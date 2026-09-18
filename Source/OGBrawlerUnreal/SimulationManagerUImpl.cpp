// SPDX-License-Identifier: BUSL-1.1
//
// ===========================================================================
// ASimulationManagerUImpl - IMPLEMENTATION. Composition root, Chaos callback,
// log router, transport adapter.
// ===========================================================================
// ORIENTATION - read this before the bodies. The header carries the shape; this
// file carries the wiring. All rationale, provenance and worked derivations are
// in docs/SimulationManagerUImpl-rationale.md (BUSL-1.1, this subtree); the
// section marks below are that document.
//
// WHAT IS IN HERE, in file order:
//   1. RouteOGMessage        - one SIMLOG string -> one LogOG* category. §4
//   2. FSimulationManagerAsyncCallback - the five PHYSICS-THREAD Chaos hooks. §1
//   3. BeginPlay / EndPlay   - the composition root: ini knobs, both role
//                              branches, every emplace and every reset. §2 §3
//   4. The OnRep listeners   - tier and floor consumption. §5
//   5. tryRegister / unregisterFromNewFramework - the registration contract. §10
//   6. The two sinks + releaseDelayedInputsForStep - the transport adapter. §7
//   7. Four probes: frame health, relay writes, connection budget, resim gate. §8
//
// THREADS. GAME THREAD unless stated. The five *_Internal hooks in section 2,
// and everything the core manager runs beneath them, are PHYSICS THREAD.
// ⛔ The only tick source legal to read on the game thread is ChaosTickMapper's
// atomic offset - never a clock, on either role. §1 §9
//
// ⛔ EVERY SESSION KNOB TAKES THE SAME FOUR STEPS, and a knob missing any one of
// them is how this tree has shipped three silently-inert settings:
//     1 INTAKE   read the ini once, before the manager exists where possible
//     2 CLAMP    or parse/validate; out of range is REPORTED, never silently fixed
//     3 SET      stamp the effective value into the one shared TimeConfig
//     4 PROVE    an UNCONDITIONAL Warning line naming the value actually stored
// ⛔ STEP 4 IS AT WARNING, NOT Log, because Config/DefaultEngine.ini sets
// LogOGNet=Warning and a Log line therefore DOES NOT EXIST on a dedicated
// server. ⛔ And it is UNCONDITIONAL, because a line that is absent both when the
// key was read and when it was not cannot tell those two cases apart. §3
//
// ⛔ NO KNOB HERE MAY BECOME A CVAR. Each is read ONCE at composition: the
// rotation width because a cadence that moves mid-run makes a probe window
// unattributable, and the resim policy because it is pushed into every
// correction cache and read unsynchronized on the landing path - which is sound
// only because it is written before any correction can land. §3
//
// PROBE VOLUME CONVENTION: per-window summaries at Warning, per-event detail at
// Verbose, and NOTHING per-tick or per-write at any verbosity. §8
//
// ⛔ THE THREE PROBE FAMILIES EACH OWN THEIR CATEGORY, because that is the only
// thing that silences a family's per-window summaries independently of its
// per-event detail; and none may be filed under `[Resim.` (which inherits
// LogOGSim=Verbose) or `[ResimCheck.` (which is split across two categories). §4
// ===========================================================================

#include "SimulationManagerUImpl.h"
#include "OGSimulation/CompilerControl.h"
#include "Runtime/Engine/Public/Net/UnrealNetwork.h"
#include "EngineUtils.h"
// [ringout task 9] The level-placed respawn points: TActorIterator over APlayerStart,
// walked ONCE in BeginPlay. See seedRingoutSpawnPointsFromLevel below.
#include "GameFramework/PlayerStart.h"
#include "OGBrawlerUnreal/SimmableUpdateComponent.h"
#include "OGBrawlerUnreal/OGBrawlerUECharacter.h"
#include "OGSimulationUnreal/SimulationTimingRelay.h"
#include "OGSimulationUnreal/SimulationConnectionRelay.h"
// The relay-ring host - this manager resolves its owner to the consuming component. §6
#include "OGSimulationUnreal/SimulationInputRelay.h"
#include "Runtime/PhysicsCore/Public/Chaos/ChaosScene.h"
#include "Runtime/Engine/Public/Physics/NetworkPhysicsComponent.h"
#include "Runtime/Experimental/Chaos/Public/PBDRigidsSolver.h"
#include "Runtime/Experimental/Chaos/Public/RewindData.h"
#include "Runtime/Experimental/Chaos/Public/PhysicsProxy/SingleParticlePhysicsProxy.h"

#include "OGBrawler/SimulatableBrawler.h"
#include "OGBrawler/DAttackRadialSimulation.h"
#include "OGBrawler/DAttackGuardSimulation.h"
#include "OGBrawler/BrawlerProjectileSimulation.h"
#include "OGBrawler/OGBrawlerLog.h"
#include "OGSimulationUnreal/UGLMTypeConversion.h"
#include "OGSimulationUnreal/ChaosPhysicsFactory.h"
#include "Components/CapsuleComponent.h"

#include "Runtime/Engine/Public/Net/NetPing.h"
#include "Runtime/Engine/Classes/Engine/NetConnection.h"
#include "Runtime/Engine/Classes/Engine/NetDriver.h"
// GConfig - the ONLY GConfig use in this codebase, deliberately confined here. §3
#include "Misc/ConfigCacheIni.h"
// relayedInputRing::kMaxDepth - the probe's stage capacity. ⚠ No clampDepth call here. §6 §11
#include "OGSimulation/RelayedInputRingCodec.h"
// resimGate:: - the policy kernel. Explicit, not leaned on through SimulationManager.h. §3
#include "OGSimulation/ResimGatePolicy.h"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

DEFINE_LOG_CATEGORY(LogOGSim);
DEFINE_LOG_CATEGORY(LogOGSimTick);
DEFINE_LOG_CATEGORY(LogOGMgmt);
DEFINE_LOG_CATEGORY(LogOGNet);
DEFINE_LOG_CATEGORY(LogOG);
DEFINE_LOG_CATEGORY(LogOGRelayProbe);
DEFINE_LOG_CATEGORY(LogOGDivergenceProbe);
DEFINE_LOG_CATEGORY(LogOGResimProbe);
DEFINE_LOG_CATEGORY(LogOGBrawler);

namespace
{
// kTierReapDeadlineDwellPeriods moved into the core coordinator, beside the reap logic.

// Routes one SIMLOG message to a LogOG* category by its leading [Tag]. §4
	void RouteOGMessage(const char* msg)
	{
		FString fmsg(msg);

// Severity meta-prefix (rare; the framework defaults to Log).
		ELogVerbosity::Type severity = ELogVerbosity::Log;
// ⛔ Tag-matching uses `body`, the FULL fmsg is logged - so a message can carry both. §4
		FString body = fmsg;
		if (fmsg.StartsWith(TEXT("[Verbose]")))
		{
			severity = ELogVerbosity::Verbose;
			body = fmsg.RightChop(9);
		}
		else if (fmsg.StartsWith(TEXT("[Warning]")))
		{
			severity = ELogVerbosity::Warning;
			body = fmsg.RightChop(9);
		}

#define EMIT_OG(cat) \
		do { \
			switch (severity) { \
				case ELogVerbosity::Warning: UE_LOG(cat, Warning, TEXT("%s"), *fmsg); break; \
				case ELogVerbosity::Verbose: UE_LOG(cat, Verbose, TEXT("%s"), *fmsg); break; \
				default:                     UE_LOG(cat, Log,     TEXT("%s"), *fmsg); break; \
			} \
		} while (0)

// ⛔ ORDER IS LOAD-BEARING: this MUST precede the `[Resim.` catch-all, its own prefix. §4
//
// [Resim.Input] is per-character-per-resim-tick and only INHERITED a rare-lifecycle prefix.
//
// ⛔ RE-ROUTED, NOT RENAMED: the tag is the string operators are told to grep for.
		if (body.StartsWith(TEXT("[Resim.Input]")))          { EMIT_OG(LogOGSimTick); }
// The RESIM-GATE family: [ResimProbe.Gate], .Chaos, .Apply, .Landing, .Request, .Stranded. §8
//
// ⛔ AHEAD OF `[Resim.`, DEFENSIVELY: widening it drops this family into LogOGSim=Verbose.
//
// ONE StartsWith COVERS THE FAMILY: a future sub-tag needs no router edit.
		else if (body.StartsWith(TEXT("[ResimProbe")))       { EMIT_OG(LogOGResimProbe); }
// LogOGSim: rare simulation lifecycle events.
		else if (body.StartsWith(TEXT("[TimeResync.")))      { EMIT_OG(LogOGSim); }
		else if (body.StartsWith(TEXT("[Resim.")))           { EMIT_OG(LogOGSim); }
		else if (body.StartsWith(TEXT("[ResimCheck.Divergence]")))     { EMIT_OG(LogOGSim); }
		else if (body.StartsWith(TEXT("[ResimCheck.PrepareRestore]"))) { EMIT_OG(LogOGSim); }
// LogOGSimTick: per-tick simulation chatter, dominates log volume.
		else if (body.StartsWith(TEXT("[ResimCheck.Check]")))      { EMIT_OG(LogOGSimTick); }
		else if (body.StartsWith(TEXT("[ResimCheck.IsSimilar]")))  { EMIT_OG(LogOGSimTick); }
		else if (body.StartsWith(TEXT("[ResimCheck.TriggerRewind]"))) { EMIT_OG(LogOGSimTick); }
		else if (body.StartsWith(TEXT("[AuthoritySimulation]")))   { EMIT_OG(LogOGSimTick); }
		else if (body.StartsWith(TEXT("[ClientPrediction]")))      { EMIT_OG(LogOGSimTick); }
		else if (body.StartsWith(TEXT("[PredictionSimulation]")))  { EMIT_OG(LogOGSimTick); }
		else if (body.StartsWith(TEXT("[PostPrediction]")))        { EMIT_OG(LogOGSimTick); }
		else if (body.StartsWith(TEXT("[CollectInput]")))          { EMIT_OG(LogOGSimTick); }
// LogOGNet: replication-channel events.
		else if (body.StartsWith(TEXT("[ServerReceive]")))              { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[ReceiveLocalInput]")))          { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[SendCorrectionStateToClients]"))) { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[SendRemoteInputToClients]")))   { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[SendLocalInputToServer]")))     { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[ReceiveCorrectionState]")))     { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[ReceiveCorrectionInput]")))     { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[InjectCorrectionState]")))      { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[InjectCorrectionInput]")))      { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[DrainOutOfOrder]")))            { EMIT_OG(LogOGNet); }
// [InputGap]/[InputDrop]/[DelayShift]/[InputStats] carry [Warning]; [Park]/[Release] do not.
		else if (body.StartsWith(TEXT("[InputGap]")))                   { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[InputDrop]")))                  { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[DelayShift]")))                 { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[InputStats]")))                 { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[Park]")))                       { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[Release]")))                    { EMIT_OG(LogOGNet); }
// The out-of-domain gate: one [InputDomain] per burst, naming a client outside our domain.
		else if (body.StartsWith(TEXT("[InputDomain]")))                { EMIT_OG(LogOGNet); }
// The relay tap: Verbose per skipped capture tick, Warning per [InputStats] window.
		else if (body.StartsWith(TEXT("[RelaySkip]")))                  { EMIT_OG(LogOGNet); }
// The relay family: [RelayProbe.Read], .Arrival (in CAPTURE ticks), .Stale, .Miss, .Delta, .Frame. §8
//
// ⚠ .Frame IS THE ONE SERVER-SIDE MEMBER: it measures the CAUSE of .Arrival's EFFECT.
//
// ⛔ Under LogOGNet it would be inseparable; unrouted its Verbose half is unreachable. §4
		else if (body.StartsWith(TEXT("[RelayProbe")))                  { EMIT_OG(LogOGRelayProbe); }
// [DivergenceProbe.Correction] per landed correction at Verbose, .Window per class at Warning. §8
//
// ⛔ THE SIGNAL IS NOT NEW - StateCorrectionCache::tryInsertingCorrectState always computed
// it; what was missing was a ROUTE, since an untagged line falls to LogOG and is silenced. §4
		else if (body.StartsWith(TEXT("[DivergenceProbe")))             { EMIT_OG(LogOGDivergenceProbe); }
// LogOGMgmt: manager / simulatable lifecycle.
		else if (body.StartsWith(TEXT("SimulationManager:"))) { EMIT_OG(LogOGMgmt); }
		else if (body.StartsWith(TEXT("tryRegister:")))       { EMIT_OG(LogOGMgmt); }
		else if (body.StartsWith(TEXT("NewFramework:")))      { EMIT_OG(LogOGMgmt); }
// LogOG: fallback for unrecognized prefixes.
		else                                                  { EMIT_OG(LogOG); }

#undef EMIT_OG
	}

}

OGSIM_OPTIMIZE_OFF


void FSimulationState2::Copy(const FSimulationState2& Value)
{
	bIsValid = Value.bIsValid;
	DeltaTime = Value.DeltaTime;
}

FName FSimulationManagerAsyncCallback::GetFNameForStatId() const
{
	const static FLazyName StaticName("FSimulationManagerAsyncCallback");
	return StaticName;
}

void FSimulationManagerAsyncCallback::OnPreSimulate_Internal()
{
	const FSimulationInput2* input = this->GetConsumerInput_Internal();

	Chaos::FPBDRigidsSolver& solver = this->GetSolver()->CastChecked();
	Chaos::FPBDRigidsEvolution* rigidsEvolution = solver.GetEvolution();

	const bool isResimulating = rigidsEvolution->IsResimming();
	const bool isFirstResimulationFrame = rigidsEvolution->IsResetting(); 
	SimulationUpdateInfo updateInfo(isResimulating, isFirstResimulationFrame);

	if (!isResimulating)
	{
		const unsigned int chaosTick = solver.GetCurrentFrame();
		const unsigned int simulationTick = [&input]() {
			if (!input->m_manager->runsPrediction())
				return input->m_manager->getServerClock().getTick();
			else
				return input->m_manager->getClientClock().getPredictionTick();
		}();
		input->m_manager->editChaosTickMapper().update((int32_t)chaosTick, (int32_t)simulationTick);
	}

	input->m_manager->onGameSimulation(updateInfo);
}

void FSimulationManagerAsyncCallback::OnPostSolve_Internal()
{
	const FSimulationInput2* input = this->GetConsumerInput_Internal();
	if (input == nullptr || input->m_manager == nullptr)
		return;

	Chaos::FPBDRigidsSolver& solver = this->GetSolver()->CastChecked();
	Chaos::FPBDRigidsEvolution* rigidsEvolution = solver.GetEvolution();

	const bool isResimulating = rigidsEvolution->IsResimming();
	const bool isFirstResimulationFrame = rigidsEvolution->IsResetting();
	SimulationUpdateInfo updateInfo(isResimulating, isFirstResimulationFrame);

	input->m_manager->onPostGameSimulation(updateInfo);
}

void FSimulationManagerAsyncCallback::ProcessInputs_Internal(int32 PhysicsStep)
{

}

void FSimulationManagerAsyncCallback::ProcessInputs_External(int32 PhysicsStep)
{

}

int32 FSimulationManagerAsyncCallback::TriggerRewindIfNeeded_Internal(int32 LastCompletedStep)
{


	ASimulationManagerUImpl* manager = m_manager;
	if (manager == nullptr)
	{
		UE_LOG(LogOGSimTick, Log, TEXT("[ResimCheck.TriggerRewind] lastCompletedStep=%d manager=null rewind=0"),
			LastCompletedStep);
		return INDEX_NONE;
	}

// Authority is the source of truth - nothing to rewind toward, so skip the sweep.
	if (!manager->runsPrediction())
		return INDEX_NONE;

	unsigned int correctionTick = manager->onCheckIsSimilar();
	const bool willRewind = correctionTick != std::numeric_limits<unsigned int>::max() && correctionTick != 0u;
	if (!willRewind)
	{
		UE_LOG(LogOGSimTick, Log, TEXT("[ResimCheck.TriggerRewind] lastCompletedStep=%d correctionTick=%u rewind=0"),
			LastCompletedStep, correctionTick);
		return INDEX_NONE;
	}

// Convert from simulation tick space back to Chaos physics tick space.
	const int32 unrealTickDifferenceAdjustedTick = manager->getChaosTickMapper().toChaosTick(static_cast<int32_t>(correctionTick));
	UE_LOG(LogOGSimTick, Log, TEXT("[ResimCheck.TriggerRewind] lastCompletedStep=%d correctionTick=%u chaosTick=%d rewind=1"),
		LastCompletedStep, correctionTick, unrealTickDifferenceAdjustedTick);

// THE REQUEST, counted after the conversion so the frame recorded is the one Chaos gets. §8
//
// ⛔ Everything past this `return` is engine-side and silent in a normal build, so this
// line and noteResimGrant are the whole visibility; `requests - grants` is the refusals.
	manager->noteResimRequest(correctionTick, LastCompletedStep, unrealTickDifferenceAdjustedTick);

// PER-EVENT DETAIL AT VERBOSE, carrying the mapper OFFSET - the discriminator: a +/-1 skew
// across Stall/Skip steps refuses a request or replays one frame short. §9
	UE_LOG(LogOGResimProbe, Verbose,
		TEXT("[ResimProbe.Request] requestedSimTick=%u requestedChaosFrame=%d lastCompletedStep=%d mapperOffset=%d"),
		correctionTick, unrealTickDifferenceAdjustedTick, LastCompletedStep,
		manager->getChaosTickMapper().toChaosTick(0));

	return unrealTickDifferenceAdjustedTick;
}

void FSimulationManagerAsyncCallback::ApplyCorrections_Internal(int32 PhysicsStep, Chaos::FSimCallbackInput* Input)
{

}

void FSimulationManagerAsyncCallback::FirstPreResimStep_Internal(int32 PhysicsStep)
{
// Server authority never resims - no rewind timeline exists there.
	if (m_manager == nullptr || !m_manager->runsPrediction())
		return;

// THE GRANT. Chaos starts at `PhysicsStep`, which can differ from ours only by being DEEPER. §8
//
// ⛔ A SHALLOW CLAMP IS STRUCTURALLY IMPOSSIBLE here: validation walks DOWN and the merge
// can only deepen, so `clampedGrants` reads 0 and a nonzero is an engine-change alarm.
//
// ⛔ BEFORE prepareResimulation, so a grant is recorded even if anything below returns
// early: `grants` and `prepares` straddle this boundary and their agreement is the check.
	m_manager->noteResimGrant(PhysicsStep);

// Convert Chaos physics step back to simulation tick for prepareResimulation.
	const uint32_t simTick = static_cast<uint32_t>(
		m_manager->getChaosTickMapper().toSimulationTick(static_cast<int32_t>(PhysicsStep)));
	m_manager->prepareResimulation(PhysicsStep, simTick);

// ⛔ At PostPushData: direct SetX/SetV/SetW is a NO-OP on ResimAsFollower bodies.
	Chaos::FPBDRigidsSolver& solver = this->GetSolver()->CastChecked();
	Chaos::FRewindData* rewindData = solver.GetRewindData();
	if (rewindData == nullptr)
		return;

	auto pushBodyState = [&](BodyId bodyId, const PhysicsBodyState& bs)
	{
		Chaos::FSingleParticlePhysicsProxy* proxy =
			solver.GetParticleProxy_PT(Chaos::FUniqueIdx{static_cast<int32>(bodyId.value)});
		if (proxy == nullptr)
			return;
		Chaos::FGeometryParticleHandle* handle = proxy->GetHandle_LowLevel();
		if (handle == nullptr)
			return;
		rewindData->SetTargetStateAtFrame(
			*handle, PhysicsStep,
			Chaos::FFrameAndPhase::EParticleHistoryPhase::PostPushData,
			uglm::toFVector(bs.position),
			uglm::toFQuat(bs.rotation),
			uglm::toFVector(bs.linearVelocity),
			uglm::toFVector(bs.angularVelocity),
			/*bShouldSleep=*/false);
	};

// BodyId lookup goes through the m_physics composite bindings (local-only).
	m_manager->editStorage().forEachSimulatable(
		[&](unsigned int /*id*/, SimulatableBrawler& simulatable)
		{
			const simulatableBrawler::State& state = simulatable.getAllState().getState();
			simulatable.getPhysicsComposite().forEach([&](const auto& decl) {
				using D = std::decay_t<decltype(decl)>;
				using S = typename D::StateType;
				pushBodyState(decl.bindings.ownBodyId,
				              D::bodyStateOf(state.template get<S>()));
			});
		});
}

ASimulationManagerUImpl* ASimulationManagerUImpl::s_instances[] = {nullptr, nullptr};

ASimulationManagerUImpl::ASimulationManagerUImpl()
{
    bReplicates = false;
}

ASimulationManagerUImpl::~ASimulationManagerUImpl()
{
	if (GetWorld() != nullptr)
		GetWorld()->GetPhysicsScene()->OnPhysSceneStep.RemoveAll(this);

// Null whichever slot points at us - avoids re-querying role during teardown.
	if (s_instances[0] == this)
	{
		s_instances[0] = nullptr;
		ISimulationTimingRelayListener::unregisterInstance(true);
		ISimulationConnectionRelayListener::unregisterInstance(true);
		ISimulationInputRelayListener::unregisterInstance(true);
	}
	if (s_instances[1] == this)
	{
		s_instances[1] = nullptr;
		ISimulationTimingRelayListener::unregisterInstance(false);
		ISimulationConnectionRelayListener::unregisterInstance(false);
		ISimulationInputRelayListener::unregisterInstance(false);
	}
}

// ⭐⭐ [ringout task 9, 2026-09-13] THE RESPAWN POINTS ARE THE LEVEL'S `APlayerStart` ACTORS.
//
// USER RULING, 2026-09-13, out of a PIE session: the authored table in
// `brawlerRingout::StaticData` is placeholder authoring at (±200, ±200, Z=200), the platform
// this mode ships on is somewhere else, and every respawn therefore dropped the fighter off
// the map. Respawn slot N is now PLAYER START N, so moving a spawn point is level design.
//
// ══ ⛔⛔ WHY THIS WRITES `StaticData` AFTER CONSTRUCTION, WHICH THE BANNER FORBIDS ══════════
// §2's ownership rule is that `StaticData` is constructed in place once and never moves. What
// that rule PROTECTS is that no tick ever sees it change: a constant the simulation reads
// mid-session is a constant two peers can disagree about and a resim can replay against the
// wrong value. This is a ONE-TIME INIT that happens before the first tick can exist, which is
// inside what the rule protects rather than an exception to it. ⛔ DO NOT "FIX" IT BACK INTO
// A CONSTRUCTOR ARGUMENT: `m_staticData` is brace-initialised in a member initializer, which
// runs during ACTOR CONSTRUCTION, and no level actor is reachable then. `APlayerStart` exists
// at `BeginPlay` and not one moment earlier. (`readMovementStaticDataCVars()` has the same
// shape for the same reason, one member above it.)
//
// ══ ⭐ AND THE ORDERING IS ENFORCED, NOT ASSUMED ═══════════════════════════════════════════
// The two `checkf`s below are the mechanical form of "exactly once, before the first
// integrate". The second is the load-bearing one: every simulation step in this process runs
// through `m_manager`, which is emplaced LATER IN THIS SAME `BeginPlay` (both role branches),
// and the physics delegates that drive it (`OnPhysScenePreTick` / `OnPhysSceneStep` /
// `OnPhysScenePostTick`) plus the async callback are bound at the very END of it. An empty
// `m_manager` is therefore proof that no tick has happened yet. ⚠ It is also why this must not
// drift below the `emplace` calls: `m_integrationLayer` holds a REFERENCE to `m_staticData`,
// so a later write would still be visible and would still compile — the assertion is what
// makes the ordering a stated requirement instead of an accident of line order.
//
// ══ ⛔ BOTH ROLES, DELIBERATELY ════════════════════════════════════════════════════════════
// Unlike `m_spawnSlots` (authority-only bookkeeping), this table is read by
// `brawlerRingout::integrate` on EVERY PEER — a client predicts its own respawn. So a client
// that skipped this would predict to the placeholder point and be corrected on every respawn.
// There is no role branch here and there must not be one.
//
// ⭐ ALL THE POLICY IS IN THE ENGINE-FREE CORE. The sort, the fallback and the (0,0,0) guard
// are `brawlerRingout::spawnPointsFromLevelPlacements`, asserted by `Ringout.SpawnPoints.*` in
// `BrawlerRingoutSimulationTest.cpp`. This function walks actors and converts a name to bytes.
void ASimulationManagerUImpl::seedRingoutSpawnPointsFromLevel(UWorld& world)
{
	checkf(!m_ringoutSpawnPointsSeeded,
		TEXT("seedRingoutSpawnPointsFromLevel: the ring-out spawn table has already been ")
		TEXT("seeded. It is a ONE-TIME init, and a second pass would merge the level over its ")
		TEXT("own previous answer rather than over the authored defaults."));

	checkf(!m_manager.has_value() && !m_integrationLayer.has_value(),
		TEXT("seedRingoutSpawnPointsFromLevel: the manager or integration layer already ")
		TEXT("exists, so a simulation tick may already have read StaticData::spawnPoints. ")
		TEXT("This seed must run BEFORE the first integrate - move it back above the role ")
		TEXT("branches in BeginPlay rather than relaxing this check."));

	std::vector<brawlerRingout::LevelSpawnPoint> placements;
	for (TActorIterator<APlayerStart> it(&world); it; ++it)
	{
		const APlayerStart* playerStart = *it;
		if (playerStart == nullptr)
			continue;

// ⛔ THE NAME, AS BYTES, AND THIS CONVERSION IS THE WHOLE REASON THE CORE TAKES A
// `std::string`. `FName`'s own ordering (`FastLess` / `FNameFastLess` / `CompareIndexes`)
// compares the NAME TABLE INDEX, which the engine documents as *"only stable during this
// process' lifetime"* - the order the string was first interned in THIS process. A server and
// a client never intern in the same order, so sorting FNames would hand two peers different
// tables while looking exactly like a deterministic sort. `GetName()` is the string saved in
// the map package; every peer that loaded the map has the same bytes.
		brawlerRingout::LevelSpawnPoint placement;
		placement.name     = TCHAR_TO_UTF8(*playerStart->GetName());
		placement.position = uglm::toGLMVec3(playerStart->GetActorLocation());
		placements.push_back(std::move(placement));
	}

// ⛔ THE FALLBACK IS THE TABLE AS IT STANDS RIGHT NOW, which - because nothing else ever
// writes it and this runs once - is exactly the authored default. So "fewer player starts than
// slots leaves the rest authored" is literally "leaves the rest alone", and ZERO player starts
// is a no-op rather than a table of origins.
	const std::array<glm::vec3, brawlerRingout::kMaxSpawnPoints> authoredFallback =
		m_staticData.m_ringoutStaticData.spawnPoints;

	m_staticData.m_ringoutStaticData.spawnPoints =
		brawlerRingout::spawnPointsFromLevelPlacements(placements, authoredFallback);

	m_ringoutSpawnPointsSeeded = true;

// ⛔ `UE_LOG`, NOT `OGBLOG_G`, AND THAT IS NOT A STYLE CHOICE. The `ogblog` sink routes
// everything to `LogOGBrawler`, which the shipped ini pins at `Warning` - which is why the
// sub-sim's own `[Ringout.respawn]` line appears in NO captured log and could not be used to
// answer "where did it teleport to?" while this task was being scoped. This is the line that
// answers that question, at Warning, on its own category, once per session per manager.
//
// Volume: one line plus `kMaxSpawnPoints` rows, at composition. Not on any tick path.
	UE_LOG(LogOGMgmt, Warning,
		TEXT("[Ringout.spawnPoints] seeded from the level: %d APlayerStart actor(s) found, ")
		TEXT("%d slot(s) filled, %d left authored (netMode=%d)"),
		static_cast<int32>(placements.size()),
		FMath::Min(static_cast<int32>(placements.size()),
			static_cast<int32>(brawlerRingout::kMaxSpawnPoints)),
		static_cast<int32>(brawlerRingout::kMaxSpawnPoints)
			- FMath::Min(static_cast<int32>(placements.size()),
				static_cast<int32>(brawlerRingout::kMaxSpawnPoints)),
		static_cast<int32>(GetNetMode()));

	for (uint32 slot = 0u; slot < brawlerRingout::kMaxSpawnPoints; ++slot)
	{
		const glm::vec3& point = m_staticData.m_ringoutStaticData.spawnPoints[slot];
		UE_LOG(LogOGMgmt, Warning,
			TEXT("[Ringout.spawnPoints]   slot %u -> (%.2f, %.2f, %.2f) [%s]"),
			slot, point.x, point.y, point.z,
			(slot < placements.size()) ? TEXT("level") : TEXT("authored"));
	}
}

void ASimulationManagerUImpl::BeginPlay()
{
	Super::BeginPlay();


	UWorld* uWorld = GetWorld();
	if (uWorld == nullptr)
		checkf(false, TEXT("SimulationManagerUImpl: unexpected state"));

	// ⭐⭐ THE SIM'S GRAVITY MUST AGREE WITH THE ENGINE'S — [movement-sim task 16].
	//
	// ⛔ THIS IS NOT A TAUTOLOGY, EVEN THOUGH THE SIM'S VALUE WAS READ FROM THE ENGINE.
	// `readMovementStaticDataCVars()` runs in a member initializer, during construction, where
	// no world exists — so it can only read the PROJECT DEFAULT
	// (`UPhysicsSettings::DefaultGravityZ`). A level is free to override gravity in its
	// `WorldSettings` (`bGlobalGravitySet` / `GlobalGravityZ`), and `UWorld::GetGravityZ()`
	// here is the first moment that override is observable. The two disagreeing means the
	// character falls at one rate while every prop in the level falls at another.
	//
	// ⚠ WHY IT MATTERS DESPITE THE BODY'S OWN GRAVITY BEING OFF (ruling #16 a): the sim applies
	// `StaticData::gravity` itself precisely BECAUSE engine gravity would double-apply on the
	// character. Everything the character shares a floor with is still on the engine's number.
	const float engineGravityZ = uWorld->GetGravityZ();
	const float simGravityZ    = m_staticData.m_movementStaticData.gravity;
	checkf(FMath::Abs(simGravityZ - engineGravityZ) < 1e-3f,
		TEXT("SimulationManagerUImpl: the movement simulation's gravity (%f cm/s^2) disagrees with ")
		TEXT("the engine's (%f cm/s^2). The sim's value is read from UPhysicsSettings::DefaultGravityZ ")
		TEXT("at construction; this world overrides gravity in its WorldSettings. Either clear that ")
		TEXT("override or teach the movement StaticData about per-level gravity."),
		simGravityZ, engineGravityZ);

// ⭐⭐ [ringout task 9] THE RING-OUT SPAWN TABLE IS SEEDED FROM THE LEVEL, HERE, ONCE.
//
// ⛔ POSITION IS LOAD-BEARING AND IS ASSERTED AT THE DEFINITION. This sits ABOVE both
// role branches, so it runs before `m_integrationLayer` and `m_manager` are emplaced and
// therefore before any tick can read `StaticData::spawnPoints`. Moving it below either
// `emplace` still COMPILES - the integration layer holds a reference, so a later write is
// still visible - which is exactly why the ordering is a `checkf` and not a comment.
//
// ⛔ NOT ROLE-GATED: a client predicts its own respawn, so it needs the same table.
	seedRingoutSpawnPointsFromLevel(*uWorld);

	FPhysScene* physScene = uWorld->GetPhysicsScene();
	if (physScene == nullptr)
		checkf(false, TEXT("SimulationManagerUImpl: unexpected state"));

	Chaos::FPhysicsSolver* solver = physScene->GetSolver();
	if (solver == nullptr)
		checkf(false, TEXT("SimulationManagerUImpl: unexpected state"));

// World-level authority. ⛔ NOT HasAuthority(): bReplicates=false makes Role always authority.
	const ENetMode worldNetMode = GetNetMode();
	const bool worldIsAuthority = (worldNetMode != NM_Client);

// ---- THE RELAY DELAY FLOOR INI OVERRIDE, step 1 of 4 -------------------
//
// Read BEFORE the manager exists, so the floor is in place for the first publish. §3
//
// ⛔ AUTHORITY ONLY, and that is CORRECTNESS: the floor is REPLICATED, so a client
// reading its own ini could disagree with the server it is meant to match.
//
// ABSENT => the compiled default. Both ini homes accepted, Game wins over Engine.
	int32 configuredRelayDelayFloorTicks = -1;      // -1 = "not present in the ini"
	if (worldIsAuthority && GConfig != nullptr)
	{
		int32 iniFloorTicks = 0;
		if (GConfig->GetInt(TEXT("OGNetcode"), TEXT("RelayDelayFloorTicks"), iniFloorTicks, GGameIni) ||
			GConfig->GetInt(TEXT("OGNetcode"), TEXT("RelayDelayFloorTicks"), iniFloorTicks, GEngineIni))
		{
			configuredRelayDelayFloorTicks = iniFloorTicks;
		}
	}

// ⛔ RETIRED: there is deliberately no relay ring depth ini intake here any more. It read
// a session-configurable retention depth for the outbound ring's replace-latest write
// path; the flush-on-poll replacement takes its capacity from `relayedInputRing::kMaxDepth`,
// a compile-time constant with no ini key, so the intake, its clamp, its setter and its
// startup proof line were all removed together. §6

// ---- THE STATE ROTATION WIDTH OVERRIDE, step 1 of 4 --------------------
//
// Second knob through the same door, same four steps. §3
//
// CONTROLS how many buffers SimulationNetSync::sendCorrectionAll writes per tick: 60*K/N Hz each.
//
// ⛔ AUTHORITY ONLY, but NOT for the floor's reason: K is never replicated, so a client
// read would have no reader - a receiver reconciles against whatever arrives.
//
// ⛔ ONE-SHOT: probe output is READ AGAINST the cadence, so no cvar. §3
//
// ⚠ ABSENT => TimeConfig::correctionRotationK. Read the value THERE, never here. §3 §11
//
// SENTINEL COLLISION, harmless and knowingly: `=-1` cannot be told from an absent key.
	int32 configuredCorrectionRotationK = -1;       // -1 = "not present in the ini"
	if (worldIsAuthority && GConfig != nullptr)
	{
		int32 iniK = 0;
		if (GConfig->GetInt(TEXT("OGNetcode"), TEXT("CorrectionRotationK"), iniK, GGameIni) ||
			GConfig->GetInt(TEXT("OGNetcode"), TEXT("CorrectionRotationK"), iniK, GEngineIni))
		{
			configuredCorrectionRotationK = iniK;
		}
	}

// ---- THE RESIM-GATE POLICY OVERRIDE, step 1 of 4 -----------------------
//
// Third knob through the same door, same four steps. §3
//
// CONTROLS which landings open the resim gate: `FrontierExact` (default) or `OnDisagreement`.
//
// ⛔ THERE IS DELIBERATELY NO `ResimCooldownTicks` KEY. A trigger-rate ceiling was built
// here and REMOVED on a user ruling: it defers acting on a correction already known to
// disagree, which is the defect this mechanism repairs. If a design document names that
// key, the document predates the ruling. The throttle is structural instead.
//
// ⚠ NOT AUTHORITY-GATED, unlike the two above: the gate exists ONLY on a predicting client.
//
// ⛔ ONE-SHOT, and here that is THREAD SAFETY: the policy is read unsynchronized at every
// correction landing, which is sound ONLY because it is written once before any land.
//
// PRESENCE IS A BOOL, NOT A SENTINEL: the value is a STRING, so nothing can collide.
	bool    hasIniResimTriggerPolicy = false;
	FString configuredResimTriggerPolicy;
	if (GConfig != nullptr)
	{
		FString iniPolicy;
		if (GConfig->GetString(TEXT("OGNetcode"), TEXT("ResimTriggerPolicy"), iniPolicy, GGameIni) ||
			GConfig->GetString(TEXT("OGNetcode"), TEXT("ResimTriggerPolicy"), iniPolicy, GEngineIni))
		{
			hasIniResimTriggerPolicy = true;
			configuredResimTriggerPolicy = iniPolicy.TrimStartAndEnd();
		}
	}

	if (worldIsAuthority)
	{
		if (s_instances[0] != nullptr)
		{
			checkf(false, TEXT("SimulationManagerUImpl: unexpected state"));
		}
		s_instances[0] = this;
		ISimulationTimingRelayListener::registerInstance(/*isAuthority=*/true, this);
		auto pctmloggerServer = [](const char* msg) { RouteOGMessage(msg); };
		auto ogblogServer = [](const char* msg) {
			FString fmsg(msg);
			if (fmsg.StartsWith(TEXT("[Verbose]")))
			{
				UE_LOG(LogOGBrawler, Verbose, TEXT("%s"), *fmsg);
			}
			else if (fmsg.StartsWith(TEXT("[Warning]")))
			{
				UE_LOG(LogOGBrawler, Warning, TEXT("%s"), *fmsg);
			}
			else
			{
				UE_LOG(LogOGBrawler, Log, TEXT("%s"), *fmsg);
			}
		};
// Adapters and integration layer emplaced here, the manager after. Authority never predicts.
		Chaos::FPBDRigidsSolver& rigidsSolverS = solver->CastChecked();
		m_physAdapter.emplace(rigidsSolverS);
		m_physReaderAdapter.emplace(rigidsSolverS);
		m_queryAdapter.emplace(uWorld, std::initializer_list<ChaosCategoryMapping>{
			{ collisionCategory::body,         ECollisionChannel::ECC_GameTraceChannel2 },
			{ collisionCategory::guard,        ECollisionChannel::ECC_GameTraceChannel3 },
			{ collisionCategory::queryRouting, ECollisionChannel::ECC_GameTraceChannel4 },
// Projectile category - its own trace channel, so projectile overlaps stay distinguishable.
			{ collisionCategory::projectile,   ECollisionChannel::ECC_GameTraceChannel5 },
// [movement-sim T39] Static level geometry. The movement sub-sim's ground/wall probe and its
// capsule sweeps search this category; no DAttack-authored shape belongs to it.
//
// ⛔ THE LOAD-BEARING EFFECT IS NOT THE RETURN VALUE, IT IS THE TABLE'S SIZE.
// ChaosSpatialQueryAdapter resizes m_toEngine to (largest mapped category + 1), and
// toObjectQueryParams iterates `cat < m_toEngine.size()`. With only categories 0-3 mapped the loop
// stopped at 4, so bit 4 was never tested, AddObjectTypesToQuery was never called, and a
// `worldOnly` search went out with EMPTY object query params - which is well-formed and matches
// NOTHING.
//
// ⚠ ECC_WorldStatic IS ECollisionChannel(0) - the very value toEngineChannel returns for an
// UNMAPPED category. So from this line on, "mapped to WorldStatic" and "never mapped" are
// INDISTINGUISHABLE by return value; only m_toEngine.size() tells them apart. Task 40 exists to
// make an unmapped category loud instead of silently channel 0. Do not read a WorldStatic result
// as proof that a mapping exists.
//
// ⚠ KEEP IN SYNC with the client-branch table below - the two tables are duplicated with no
// shared constant, and adding to one silently diverges client from server.
			{ collisionCategory::world,        ECollisionChannel::ECC_WorldStatic       },
// [movement-sim T43] The movement sub-sim's OWN body. `brawlerMovementSimulation::PhysicsSetup::body`
// (BrawlerMovementSimulation.h) registers its shape under this category, and PhysicsDeclaration is
// in the shipped composite (SimulatableBrawler.h), so this runs for every character in every
// session.
// ⚠ [movement-sim task 17] THE SHAPE IS A CAPSULE, NOT A SPHERE. Task 11 replaced the skeleton's
// 30 cm sphere with `CapsuleGeometry{42.f, 96.f}` and set `isRoot`, so the factory ADOPTS the
// pawn's own root capsule rather than creating anything. The category, and every sentence below
// about what an unmapped category would have done to it, are unaffected — only the noun was stale.
// Channel is user ruling #6, closed 2026-09-04 and lead-verified free: ch1 is `Damageable` in
// DefaultEngine.ini and ch2-5 are the four entries above.
//
// ⛔ THIS LINE IS A FIX, NOT A NEW CAPABILITY. Without it toEngineChannel(5) fell through to the
// unmapped fallback ECollisionChannel(0) - which IS ECC_WorldStatic - so
// ChaosPhysicsFactory::applyDescriptor typed every character's movement body as STATIC LEVEL
// GEOMETRY. Harmless while nothing searched WorldStatic; LIVE from task 39 on, because a
// `worldOnly` object query searches exactly that object type and would hand the movement sim
// OTHER characters' capsules as ground. Task 40's [SpatialQuery.UnmappedCategory] category=5 line
// in the 2026-09-05 18:11 run is what finally said so out loud.
//
// ⚠ TASK 13 OWNS THE FULL CHANNEL MAP. `character -> ECC_GameTraceChannel6` is the ONLY entry
// task 43 added, at this site and the client one; do not double-add it there.
//
// ⚠ The reverse map moves too: the ctor writes m_toDAttack[GTC6] = character, a slot that was
// kUnmapped before. It collides with nothing (the other five occupy WorldStatic and GTC2-GTC5)
// and it is unreachable in production today, because no shipped query volume searches
// `character` and resolveHitIdentity only ever sees channels an object query asked for.
			{ collisionCategory::character,    ECollisionChannel::ECC_GameTraceChannel6 }
		});
		m_integrationLayer.emplace(m_storage, m_staticData, *m_physAdapter, *m_queryAdapter);
		m_manager.emplace(false, solver->GetAsyncDeltaTime(), ManagerType::Params{
			*m_integrationLayer, m_netSync, m_inputResolution, m_reconciliation, m_systemsExec,
			m_storage, m_staticData, std::function<void(const char*)>(pctmloggerServer) });
		m_reconciliation.setLogger(std::function<void(const char*)>(pctmloggerServer));
// The resolution peer is a sibling now, so the composition root seeds its logger directly.
		m_inputResolution.setLogger(std::function<void(const char*)>(pctmloggerServer));
		m_netSync.setLogger(std::function<void(const char*)>(pctmloggerServer));

// Inject the game's zero input for the client input delay line. §5
//
// ⛔ SET ON THE AUTHORITY BRANCH TOO, and not as a precaution: a DEDICATED server reads this
// on every tick it substitutes an input for a remote character, and seeds each character's
// replicated applied-input with it. Deleting it makes the authority simulate - and publish
// to every peer - a zero forward vector for the whole of every join window.
//
// ⛔ ORDER IS LOAD-BEARING: this must precede every registerAuthorityOwner call.
		m_inputResolution.setNeutralInput<SimulatableBrawler>(simulatableBrawler::getZeroPlayerInput());

// THE SESSION FLOOR: stamp the clamped value into TimeConfig, then publish it on the relay. §3 §5
//
// ⛔ INTAKE POINT 1 OF 2 for the clamp. Out-of-range config is REPORTED, not silently fixed.
		if (configuredRelayDelayFloorTicks >= 0)
		{
			const int32 clampedFloor = clampRelayDelayFloorTicks(
				configuredRelayDelayFloorTicks, m_manager->getTimeConfig());
			if (clampedFloor != configuredRelayDelayFloorTicks)
			{
				UE_LOG(LogOGNet, Warning,
					TEXT("[RelayDelayFloor] ini [OGNetcode] RelayDelayFloorTicks=%d out of range, clamped to %d ticks"),
					configuredRelayDelayFloorTicks, clampedFloor);
			}
			m_manager->setRelayDelayFloorTicks(clampedFloor);
			UE_LOG(LogOGNet, Log,
				TEXT("[RelayDelayFloor] session floor = %d ticks (ini override)"), clampedFloor);

// Advisory-only - see logRelayDelayFloorAdvisory.
			logRelayDelayFloorAdvisory(clampedFloor);
		}

// ⛔ RETIRED: there is deliberately no relay ring depth clamp, setter or proof-line block
// here any more. It published a session-configurable retention depth that flush-on-poll
// had already made inert - the stage capacity is `relayedInputRing::kMaxDepth`, a
// compile-time constant - so the whole inert path went rather than keep publishing a
// number nothing on the live relay path reads. §6

// THE SESSION ROTATION WIDTH. ⛔ The intake clamp stops the proof line below from lying. §3
		if (configuredCorrectionRotationK != -1)
		{
			const int32 clampedK = correctionRotation::clampK(configuredCorrectionRotationK);
			if (clampedK != configuredCorrectionRotationK)
			{
				UE_LOG(LogOGNet, Warning,
					TEXT("[StateRotation] ini [OGNetcode] CorrectionRotationK=%d out of range, clamped to %d"),
					configuredCorrectionRotationK, clampedK);
			}
			m_manager->setCorrectionRotationK(clampedK);
		}

// STEP 4 - THE PROOF LINE. Unconditional, at Warning; the banner gives both reasons. §3
//
// ⛔ It makes the cadence checkable: per-character rate should read 60*K/N in DivergenceProbe.
//
// Volume: one line per session, authority only, at composition.
		const int32 sessionCorrectionRotationK = m_manager->getTimeConfig().correctionRotationK;
		UE_LOG(LogOGNet, Warning,
			TEXT("[StateRotation] session K = %d (%s)"),
			sessionCorrectionRotationK,
			configuredCorrectionRotationK != -1 ? TEXT("ini override") : TEXT("compiled default"));

		const int32 sessionRelayDelayFloorTicks = m_manager->getTimeConfig().relayDelayFloorTicks;
		if (ASimulationTimingRelay* timingRelay = findTimingRelay())
		{
			timingRelay->setRelayDelayFloorTicks((uint8)sessionRelayDelayFloorTicks);
		}
		else
		{
// ⛔ The relay is spawned BEFORE this manager so this write lands; reaching here means
// no client will ever learn the floor and every one will predict against the wrong delay.
			UE_LOG(LogOGNet, Warning,
				TEXT("[RelayDelayFloor] no timing relay at manager BeginPlay — session floor %d NOT published"),
				sessionRelayDelayFloorTicks);
		}

// Client tier cache on the AUTHORITY world too - a listen-server host's local player uses it. §5
//
// ⛔ No tier ever arrives on an authority world, so this stays at the no-tier fallback.
		m_replicatedTierConsumer.emplace(m_manager->getTimeConfig());
		recomputeAndPublishEffectiveInputDelay();
		ISimulationConnectionRelayListener::registerInstance(/*isAuthority=*/true, this);
			ISimulationInputRelayListener::registerInstance(/*isAuthority=*/true, this);

// ---- SERVER RECEPTION COORDINATOR -------------------------------------
//
// ⛔ AUTHORITY BRANCH ONLY, and it borrows m_manager's TimeConfig, so it must not outlive it. §7
		m_receptionCoordinator.emplace(m_manager->getTimeConfig());
		m_receptionCoordinator->setLogger(std::function<void(const char*)>(pctmloggerServer));
// Process-global sinks for templates with no logger parameter: simlog, ogblog.
		simlog::setGlobal(std::function<void(const char*)>(pctmloggerServer));
		ogblog::setGlobal(std::function<void(const char*)>(ogblogServer));
	}
	else
	{
		if (s_instances[1] != nullptr)
		{
			checkf(false, TEXT("SimulationManagerUImpl: unexpected state"));
		}
		s_instances[1] = this;
		ISimulationTimingRelayListener::registerInstance(/*isAuthority=*/false, this);
		auto pctmlogger = [](const char* msg) { RouteOGMessage(msg); };
		auto ogblogClient = [](const char* msg) {
			FString fmsg(msg);
			if (fmsg.StartsWith(TEXT("[Verbose]")))
			{
				UE_LOG(LogOGBrawler, Verbose, TEXT("%s"), *fmsg);
			}
			else if (fmsg.StartsWith(TEXT("[Warning]")))
			{
				UE_LOG(LogOGBrawler, Warning, TEXT("%s"), *fmsg);
			}
			else
			{
				UE_LOG(LogOGBrawler, Log, TEXT("%s"), *fmsg);
			}
		};
// Non-authority branch = pure client - always runs prediction.
		Chaos::FPBDRigidsSolver& rigidsSolverC = solver->CastChecked();
		m_physAdapter.emplace(rigidsSolverC);
		m_physReaderAdapter.emplace(rigidsSolverC);
		m_queryAdapter.emplace(uWorld, std::initializer_list<ChaosCategoryMapping>{
			{ collisionCategory::body,         ECollisionChannel::ECC_GameTraceChannel2 },
			{ collisionCategory::guard,        ECollisionChannel::ECC_GameTraceChannel3 },
			{ collisionCategory::queryRouting, ECollisionChannel::ECC_GameTraceChannel4 },
// Projectile category - its own trace channel, as on the authority branch.
			{ collisionCategory::projectile,   ECollisionChannel::ECC_GameTraceChannel5 },
// [movement-sim T39] Static level geometry, as on the authority branch. Both caveats are spelled
// out in full there: it is m_toEngine.size() (not the returned channel) that makes
// toObjectQueryParams test bit 4, and ECC_WorldStatic == ECollisionChannel(0) == the unmapped
// fallback. KEEP IN SYNC with the authority table above.
			{ collisionCategory::world,        ECollisionChannel::ECC_WorldStatic       },
// [movement-sim T43] The movement sub-sim's own body, as on the authority branch - and the client
// needs it for the same reason the server does: it predicts the same sub-sim. The full rationale
// (why an unmapped category silently became ECC_WorldStatic, and what the reverse map does) is
// spelled out at the authority table above. KEEP IN SYNC with it.
			{ collisionCategory::character,    ECollisionChannel::ECC_GameTraceChannel6 }
		});
		m_integrationLayer.emplace(m_storage, m_staticData, *m_physAdapter, *m_queryAdapter);
		m_manager.emplace(/*usePrediction=*/true, solver->GetAsyncDeltaTime(), ManagerType::Params{
			*m_integrationLayer, m_netSync, m_inputResolution, m_reconciliation, m_systemsExec,
			m_storage, m_staticData, std::function<void(const char*)>(pctmlogger) });
		m_reconciliation.setLogger(std::function<void(const char*)>(pctmlogger));
// The resolution peer is a sibling now, so the composition root seeds its logger directly.
		m_inputResolution.setLogger(std::function<void(const char*)>(pctmlogger));
		m_netSync.setLogger(std::function<void(const char*)>(pctmlogger));

// Fills the [0, effectiveDelay) window. ⛔ NOT PlayerInput{}: (0,0,1) forwards, load-bearing. §5
		m_inputResolution.setNeutralInput<SimulatableBrawler>(simulatableBrawler::getZeroPlayerInput());

// Establish the PRE-ARRIVAL baseline delay before any tier has replicated. §5
//
// ⛔ ServerInputDelayQueue::effectiveDelay uses the same fallback, so this keeps both ends in step.
//
// Published THROUGH the tier cache, so baseline and post-arrival share ONE derivation site.
//
// A client's floor is still 0 here; it arrives by OnRep and overwrites this on landing.
		m_replicatedTierConsumer.emplace(m_manager->getTimeConfig());
		recomputeAndPublishEffectiveInputDelay();

// Bind the tier listener, then PULL. ⛔ The property dirties only on change, so an earlier
// OnRep would never be re-notified and the channel would be silently stranded. §5
		ISimulationConnectionRelayListener::registerInstance(/*isAuthority=*/false, this);
		ISimulationInputRelayListener::registerInstance(/*isAuthority=*/false, this);
		if (ASimulationConnectionRelay* connectionRelay =
				ASimulationConnectionRelay::findLocalClientRelay(uWorld))
		{
			connectionRelay->replayLatchedTier();
		}

// Same treatment for the FLOOR, for the same reason; a missing relay means no floor yet.
		if (ASimulationTimingRelay* timingRelay = findTimingRelay())
		{
			timingRelay->replayLatchedRelayDelayFloor();
		}
// Process-global sinks as on the authority branch: simlog -> LogOG*, ogblog -> LogOGBrawler.
		simlog::setGlobal(std::function<void(const char*)>(pctmlogger));
		ogblog::setGlobal(std::function<void(const char*)>(ogblogClient));

// STEP 4 - THE PROOF LINE, which makes the other three checkable from a log, not source. §3 §4
//
// ⛔ EFFECTIVE RUNTIME VERBOSITY, not a constant, plus the per-window denominator.
//
// ⛔ ITS OWN ABSENCE IS INFORMATION: no line means NoLogging, or that this never ran.
//
// Volume: one line per session, client only, at composition.
		UE_LOG(LogOGResimProbe, Warning,
			TEXT("[ResimProbe.Session] resim-gate probe LIVE — verbosity=%s verboseDetail=%s windowSamples=%u"),
			ToString(LogOGResimProbe.GetVerbosity()),
			LogOGResimProbe.IsSuppressed(ELogVerbosity::Verbose) ? TEXT("off") : TEXT("on"),
			static_cast<uint32>(kResimGateProbeWindowSamples));
	}

// ---- APPLY THE RESIM-GATE POLICY, steps 2-4 ---------------------------
//
// ⛔ AFTER BOTH ROLE BRANCHES: duplicating apply-plus-proof is how the roles drift. §3
	{
// STEP 2 - PARSE + VALIDATE. ⛔ An unrecognised string is REPORTED and the default kept:
// a typo silently selecting the other value changes the gate on a build nobody touched. §3
		if (hasIniResimTriggerPolicy)
		{
			if (configuredResimTriggerPolicy.Equals(TEXT("OnDisagreement"), ESearchCase::IgnoreCase))
			{
				m_manager->setResimTriggerPolicy(TimeConfig::ResimTriggerPolicy::OnDisagreement);
			}
			else if (configuredResimTriggerPolicy.Equals(TEXT("FrontierExact"), ESearchCase::IgnoreCase))
			{
				m_manager->setResimTriggerPolicy(TimeConfig::ResimTriggerPolicy::FrontierExact);
			}
			else
			{
				UE_LOG(LogOGNet, Warning,
					TEXT("[ResimGate] ini [OGNetcode] ResimTriggerPolicy='%s' unrecognised ")
					TEXT("(expected FrontierExact or OnDisagreement) — keeping the compiled default"),
					*configuredResimTriggerPolicy);
				hasIniResimTriggerPolicy = false;
			}
		}

// STEP 4 - THE PROOF LINE. Unconditional, at Warning; see the banner. §3
//
// ⛔ THIS LINE IS THE BEHAVIOUR-NEUTRALITY RECEIPT: a later claim names WHICH POLICY WAS LIVE.
//
// ⛔ Values are read back from TimeConfig, so it cannot claim a setting nothing stored.
//
// ⛔ `depthPolicy` and `rateLimit` state inertness rather than falling silent. §3
//
// Volume: one line per session per role, at composition.
		const TimeConfig& sessionTimeConfig = m_manager->getTimeConfig();
		const bool policyIsOnDisagreement =
			sessionTimeConfig.resimTriggerPolicy == TimeConfig::ResimTriggerPolicy::OnDisagreement;
		const FString depthPolicyText = policyIsOnDisagreement
			? FString::Printf(TEXT("skip beyond %d ticks"), sessionTimeConfig.rollbackWindowTicks)
			: FString(TEXT("off (inert under FrontierExact)"));
		UE_LOG(LogOGNet, Warning,
			TEXT("[ResimGate] session policy = %s (%s), depthPolicy = %s, rateLimit = none (structural: one resim in flight, one pending)"),
			policyIsOnDisagreement ? TEXT("OnDisagreement") : TEXT("FrontierExact"),
			hasIniResimTriggerPolicy ? TEXT("ini override") : TEXT("compiled default"),
			*depthPolicyText);
	}

	physScene->OnPhysScenePreTick.AddUObject(this, &ASimulationManagerUImpl::OnPhysicsPreTick);
	physScene->OnPhysSceneStep.AddUObject(this, &ASimulationManagerUImpl::OnPhysicsStep);
	m_hysScenePostTickCallbackHandle = physScene->OnPhysScenePostTick.AddUObject(this, &ASimulationManagerUImpl::OnPostPhysicsStep);

	m_asyncCallback = solver->CreateAndRegisterSimCallbackObject_External<FSimulationManagerAsyncCallback>();
	m_asyncCallback->setManager(this);

	FNetworkPhysicsCallback* solverCallback = static_cast<FNetworkPhysicsCallback*>(solver->GetRewindCallback());
	if (solverCallback == nullptr)
		checkf(false, TEXT("SimulationManagerUImpl: unexpected state"));

	m_injectInputsExternalCallbackHandle = solverCallback->InjectInputsExternal.AddUObject(this, &ASimulationManagerUImpl::InjectInputs_External);
	UE_LOG(LogOGMgmt, Log, TEXT("SimulationManager: adapters, integration layer, and manager initialized"));
}

void ASimulationManagerUImpl::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
// ⛔ The coordinator borrows m_manager's TimeConfig, so it is reset BEFORE the manager. §2
	m_delayedInputComponentsById.clear();
	m_receptionCoordinator.reset();

// ⛔ Same borrow rule as the coordinator: the tier cache holds m_manager's TimeConfig. §2
	m_replicatedTierConsumer.reset();

// Clear the singleton slot before teardown so a later PIE session cannot hit the guard.
	if (s_instances[0] == this)
	{
		s_instances[0] = nullptr;
		ISimulationTimingRelayListener::unregisterInstance(/*isAuthority=*/true);
		ISimulationConnectionRelayListener::unregisterInstance(/*isAuthority=*/true);
		ISimulationInputRelayListener::unregisterInstance(/*isAuthority=*/true);
	}
	if (s_instances[1] == this)
	{
		s_instances[1] = nullptr;
		ISimulationTimingRelayListener::unregisterInstance(/*isAuthority=*/false);
		ISimulationConnectionRelayListener::unregisterInstance(/*isAuthority=*/false);
		ISimulationInputRelayListener::unregisterInstance(/*isAuthority=*/false);
	}

	if (UWorld* World = GetWorld())
	{
		if (FPhysScene* PhysScene = World->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{
				if (m_injectInputsExternalCallbackHandle.IsValid())
				{
					if (FNetworkPhysicsCallback* SolverCallback = static_cast<FNetworkPhysicsCallback*>(Solver->GetRewindCallback()))
					{
						SolverCallback->InjectInputsExternal.Remove(m_injectInputsExternalCallbackHandle);
					}
				}

				if (m_hysScenePostTickCallbackHandle.IsValid())
				{
					PhysScene->OnPhysScenePostTick.Remove(m_hysScenePostTickCallbackHandle);
				}

				if (m_asyncCallback)
				{
					Solver->UnregisterAndFreeSimCallbackObject_External(m_asyncCallback);
				}
			}
		}
	}

	Super::EndPlay(EndPlayReason);

}


// ---------------------------------------------------------------------------
// CLIENT TIER CONSUMPTION - the receive end of the per-connection tier channel. §5
//
// ⛔ PRESERVED QUIRK: the core never publishes tier 0 as a FIRST value, so a wire that never
// leaves tier 0 never calls in here - a standing client/server delay divergence, kept. §5
//
// ⛔ SECOND CONSEQUENCE: that fabricated `oldTier = 0` used to reach applyTierTransitionStall
// as a real prior tier, asking for a spurious stall on the FIRST resolution. Fixed by taking
// `hadAnyTier` explicitly. ⛔ The divergence above is UNCHANGED - it only suppresses.
// ---------------------------------------------------------------------------

void ASimulationManagerUImpl::onConnectionTierReceived(uint8_t oldTier, uint8_t newTier)
{
// ⛔ Read BEFORE applyReplicatedConnectionTier, which sets hasReceivedTier() unconditionally. §5
    const bool hadAnyTier =
        m_replicatedTierConsumer.has_value() && m_replicatedTierConsumer->hasReceivedTier();

    applyReplicatedConnectionTier(newTier);

// The (old -> new) delta IS the transition signal: the client runs no RTT sampling. §5
//
// ⛔ A fresh connection's first OnRep reports oldTier = 0, the property default, not a tier
// ever run at; `hadAnyTier`, captured above, is what tells it from a genuine prior tier 0.
    applyTierTransitionStall(oldTier, newTier, hadAnyTier);
}

void ASimulationManagerUImpl::onConnectionTierReplayed(uint8_t tier)
{
// ⛔ NO stall - the first tier ever applied, so no tick was predicted at a prior delay.
    applyReplicatedConnectionTier(tier);
}

void ASimulationManagerUImpl::applyReplicatedConnectionTier(uint8 tier)
{
    if (!m_replicatedTierConsumer.has_value())
        return;     // pre-BeginPlay ordering; the relay latches and replays at bind

// GAME THREAD, matching the cache's single-threaded contract.
    m_replicatedTierConsumer->onReplicatedTierReceived((int32)tier);
    recomputeAndPublishEffectiveInputDelay();
}

// ---------------------------------------------------------------------------
// THE RELAY-RING HOST BOUNDARY, client side. ⛔ A bridge only, because OGSimulationUnreal
// must not depend on OGBrawlerUnreal and so cannot resolve its own owner. §6
// ---------------------------------------------------------------------------

void ASimulationManagerUImpl::onInputRelayHostReady(ASimulationInputRelay& host)
{
// host -> owner -> character -> component. ⛔ Any hop may fail mid-join; the host re-asks. §6
    AActor* ownerActor = host.GetOwner();
    if (ownerActor == nullptr)
        return;

    AOGBrawlerUECharacter* character = Cast<AOGBrawlerUECharacter>(ownerActor);
    if (character == nullptr)
        return;

    if (USimmableUpdateComponent* component =
            character->FindComponentByClass<USimmableUpdateComponent>())
    {
// Idempotent on the component's side - three independent link paths can fire this.
        component->attachInputRelayHost(&host);
    }
}

// ---------------------------------------------------------------------------
// THE RELAY DELAY FLOOR, client side. ⛔ Session channel vs per-wire: either lands first. §5
// ---------------------------------------------------------------------------

void ASimulationManagerUImpl::onRelayDelayFloorReceived(uint8_t floorTicks)
{
    applyReplicatedRelayDelayFloor((uint8)floorTicks, /*payForIncrease=*/true);
}

void ASimulationManagerUImpl::onRelayDelayFloorReplayed(uint8_t floorTicks)
{
// ⛔ NO stall here - the pull runs inside BeginPlay, before the first prediction tick.
    applyReplicatedRelayDelayFloor((uint8)floorTicks, /*payForIncrease=*/false);
}

void ASimulationManagerUImpl::applyReplicatedRelayDelayFloor(uint8 floorTicks, bool payForIncrease)
{
    if (!m_manager.has_value())
        return;     // pre-BeginPlay ordering; the relay latches and replays at bind

// ⛔ INTAKE POINT 2 OF 2. Off the wire, so clamped: a corrupt byte must not outrun eviction. §3
//
// The setter clamps again; this site exists so an out-of-range value is VISIBLE.
    const TimeConfig& cfg = m_manager->getTimeConfig();
    const int32 clampedFloor = clampRelayDelayFloorTicks((int32)floorTicks, cfg);
    if (clampedFloor != (int32)floorTicks)
    {
        UE_LOG(LogOGNet, Warning,
            TEXT("[RelayDelayFloor] received floor %u out of range, clamped to %d ticks"),
            (unsigned int)floorTicks, clampedFloor);
    }

// Stamp it into the ONE shared TimeConfig, which every downstream derivation reads. §5
    m_manager->setRelayDelayFloorTicks(clampedFloor);

// Advisory-only - see logRelayDelayFloorAdvisory. Same belt-and-braces as the clamp.
    logRelayDelayFloorAdvisory(clampedFloor);

    const int32 deltaDelayTicks = recomputeAndPublishEffectiveInputDelay();

    if (!payForIncrease || deltaDelayTicks <= 0)
    {
        UE_LOG(LogOGNet, Log,
            TEXT("[RelayDelayFloor] floor = %d ticks, effective delay delta=%d, no stall"),
            clampedFloor, deltaDelayTicks);
        return;
    }

// ⛔ A floor RISE is indistinguishable from an upward tier transition to the client: the
// frontier must fall back by the difference, which the clock pays down as Stall ticks. §5
    requestInputDelayIncreaseStall(deltaDelayTicks);

    UE_LOG(LogOGNet, Warning,
        TEXT("[RelayDelayFloor] floor = %d ticks, UPWARD, requesting %d-tick prediction stall"),
        clampedFloor, deltaDelayTicks);
}

// ⛔ ADVISORY ONLY, never an assert: floor 0 is scheduled-regime-OFF, and
// classifyRelayDelayFloor (ConnectionTierTable.h) never flags it. That file has the table. §3
//
// Called from BOTH floor intake points, the same belt-and-braces shape the clamp uses.
void ASimulationManagerUImpl::logRelayDelayFloorAdvisory(int32 floorTicks)
{
    if (!m_manager.has_value())
        return;

    switch (classifyRelayDelayFloor(m_manager->getTimeConfig()))
    {
    case RelayDelayFloorAdvisory::BelowHiccupBaseline:
        UE_LOG(LogOGNet, Warning,
            TEXT("[RelayDelayFloor] floor=%d is below the hiccup-absorption baseline (>=2) and above off (0) — the one value that is neither"),
            floorTicks);
        break;
    case RelayDelayFloorAdvisory::UniformDFairnessActive:
        UE_LOG(LogOGNet, Log,
            TEXT("[RelayDelayFloor] floor=%d >= max(rttTierInputDelays) — uniform-D fairness mode active; tiers and LAN override are inert"),
            floorTicks);
        break;
    case RelayDelayFloorAdvisory::None:
        break;
    }
}

int32 ASimulationManagerUImpl::recomputeAndPublishEffectiveInputDelay()
{
    if (!m_replicatedTierConsumer.has_value())
        return 0;

// THE FORMULA, in one place: every arm lives in effectiveInputDelayTicks, as on the server. §5
//
// ⛔ GAME -> PHYSICS crossing, deliberately ONE scalar: it lands in a std::atomic<int32>
// that collectInputAll loads once per tick, so a race costs one tick of latency. §1
    const int32 delayTicks = (int32)m_replicatedTierConsumer->effectiveInputDelayTicks();
    const int32 deltaDelayTicks = delayTicks - m_lastPublishedEffectiveInputDelayTicks;

    m_lastPublishedEffectiveInputDelayTicks = delayTicks;
    publishClientEffectiveInputDelayTicks(delayTicks);

    UE_LOG(LogOGNet, Log,
        TEXT("[ConnectionTier] applied client input delay = %d ticks (tier %d, arrived=%d, floor %d)"),
        delayTicks,
        (int)m_replicatedTierConsumer->currentTierIndex(),
        m_replicatedTierConsumer->hasReceivedTier() ? 1 : 0,
        m_manager.has_value() ? (int)m_manager->getTimeConfig().relayDelayFloorTicks : 0);

    return deltaDelayTicks;
}

void ASimulationManagerUImpl::applyTierTransitionStall(uint8 oldTier, uint8 newTier, bool hadAnyTier)
{
    if (oldTier == newTier)
        return;     // an OnRep can fire for an unchanged value; nothing transitioned

    if (!m_manager.has_value())
        return;     // core manager not constructed yet; no clock to stall

// ⛔ THE DECISION is shouldStallForTierTransition in core, which has LLT coverage. §5
    const TimeConfig& cfg = m_manager->getTimeConfig();
    const int32 stallTicks = (int32)shouldStallForTierTransition(
        (int32)oldTier, (int32)newTier, hadAnyTier, cfg);

    if (stallTicks <= 0)
    {
// A downward or delay-neutral transition, which drift reaches by advancing, or a first one.
        UE_LOG(LogOGNet, Log,
            TEXT("[ConnectionTier] tier %u -> %u hadAnyTier=%d, no stall"),
            (unsigned int)oldTier, (unsigned int)newTier, hadAnyTier ? 1 : 0);
        return;
    }

// No-ops on an authority manager, the only role that can reach here with no client clock.
    requestInputDelayIncreaseStall(stallTicks);

    UE_LOG(LogOGNet, Warning,
        TEXT("[ConnectionTier] tier %u -> %u UPWARD, requesting %d-tick prediction stall"),
        (unsigned int)oldTier, (unsigned int)newTier, stallTicks);
}

ASimulationTimingRelay* ASimulationManagerUImpl::findTimingRelay()
{
    if (m_timingRelay) return m_timingRelay;
    for (TActorIterator<ASimulationTimingRelay> it(GetWorld()); it; ++it)
    {
        m_timingRelay = *it;
        UE_LOG(LogOGMgmt, Log, TEXT("SimulationManager: timing relay found"));
        return m_timingRelay;
    }
    return nullptr;
}

FSmallSimulationStateSyncBuffer& ASimulationManagerUImpl::getSyncedTimingBuffer()
{
    ASimulationTimingRelay* relay = findTimingRelay();
    checkf(relay != nullptr, TEXT("SimulationManagerUImpl::getSyncedTimingBuffer: relay not found"));
    return relay->editBuffer();
}

void ASimulationManagerUImpl::onPostSimulationGameThread()
{
    m_manager->onPostSimulationGameThread();
}

// =========================================================================================
// [ringout task 5] ⭐ THE AUTHORITY-SIDE SCORE PUSH
// =========================================================================================
//
// WHAT IT IS. One game-thread pass that copies each character's authority-side ring-out
// score out of `brawlerRingout::ScoreSystem` and onto that character's replicated
// `RingoutScore` property, from which UE replicates it to every client. It carries NO
// POLICY — who scores, how much and when is entirely task 4's law, in engine-free core.
//
// ⛔ WHY A FILE-LOCAL FUNCTION AND NOT A MEMBER. Everything it needs is passed in, so it
// adds no name to the class's surface and no member to its threading table. The class
// banner's NARROW PASSTHROUGHS list exists precisely to stop this kind of thing becoming an
// accessor; a free function that the one call site below hands two references to cannot be
// reached by anything else.
//
// ⛔ THE ROUTE TABLE IS `m_delayedInputComponentsById`, AND REUSING IT IS A REAL COUPLING —
// stated here because nothing else would state it. That map is registered in
// `USimmableUpdateComponent::tryRegisterWithNewFramework` under `if (isAuthority)` and
// erased in `unregisterFromNewFramework`, so it is exactly "the authority's live characters,
// resolvable to their actors" — which is what this push needs and what no other member is.
// ⚠ IF THAT REGISTRATION EVER NARROWS (a condition beyond `isAuthority`, a later call site,
// a role that stops registering), THIS PUSH SILENTLY LOSES THOSE CHARACTERS: their score
// never leaves the server and their scoreboard row freezes at whatever last replicated. It
// fails quiet, so it is written down rather than left to be rediscovered.
//
// ⛔⛔ THIS IS A CROSS-THREAD READ, AND THE ARGUMENT FOR IT IS THE CLASS BANNER'S THIRD
// CROSSING — read that before touching this. The short form: `ScoreSystem`'s score table is
// WRITTEN on the physics thread (`postIntegrate`, beneath `OnPreSimulate_Internal`) and read
// here on the GAME thread. What makes it the same ACCEPTED TEAR the input-history poll takes
// rather than a new hazard is that the table cannot be RESTRUCTURED under this reader: the
// only inserting and erasing calls are `onCharacterRegistered` / `onCharacterUnregistered`,
// both driven from `tryRegister` / `unregisterFromNewFramework`, both GAME THREAD. The award
// itself reaches an EXISTING entry, so it writes one naturally-aligned four-byte word and
// cannot rehash. Worst case: a scoreboard number one tick stale, on a display that decides
// nothing — which is the same bound `BrawlerColor` and this property both rest on.
//
// ⭐ THAT ARGUMENT HAS A PRECONDITION, AND THE PRECONDITION IS MACHINE-CHECKED BELOW rather
// than asserted in prose. `postIntegrate` awards through `operator[]`, which INSERTS — and
// therefore MAY REHASH — for an id the roster has not got. Task 4 chose that deliberately so
// an award to an unseeded id is a real award rather than a dropped one. It is unreachable in
// a legal session because `onCharacterRegistered` seeds every authority-registered id, and
// it seeds it BEFORE `tryRegister` returns `Ready`, which is before the route entry this
// walk reads even exists. The `checkf` states exactly that ordering, so a future change that
// breaks it fails loudly in Development instead of racing silently.
namespace
{
    void pushRingoutScoresToCharacters(
        const brawlerRingout::ScoreSystem& scoreSystem,
        const std::unordered_map<unsigned int, TWeakObjectPtr<USimmableUpdateComponent>>& routes)
    {
        for (const auto& entry : routes)
        {
            const unsigned int id = entry.first;

// A stale handle is SKIPPED, not erased. The two transport sinks prune the map when they
// meet one because they are its owners; this is a reader and pruning here would mutate the
// container mid-walk for no benefit - `unregisterFromNewFramework` erases it promptly anyway.
            USimmableUpdateComponent* component = entry.second.Get();
            if (component == nullptr)
                continue;

            AOGBrawlerUECharacter* character = Cast<AOGBrawlerUECharacter>(component->GetOwner());
            if (character == nullptr)
                continue;

// ⛔ THE NO-REHASH PRECONDITION, STATED AS A TRIPWIRE. See the banner above.
            checkf(scoreSystem.hasScoreEntry(id),
                   TEXT("Ring-out score push: id=%u is in the authority route table but has no ")
                   TEXT("ScoreSystem roster entry. The game-thread read of the score table is ")
                   TEXT("safe only because the roster is seeded by onCharacterRegistered before ")
                   TEXT("tryRegister returns Ready - an unseeded id lets postIntegrate's ")
                   TEXT("operator[] INSERT, and rehash, on the physics thread under this walk."),
                   id);

// ⛔ THE UNCHANGED-VALUE GUARD IS INSIDE THE SETTER, not here. One write site, one guard:
// a second early-out here could drift out of step with it and neither would be obviously wrong.
            character->SetAuthoritativeRingoutScore(
                static_cast<int32>(scoreSystem.scoreOf(id)));
        }
    }
}

void ASimulationManagerUImpl::OnPhysicsPreTick(FPhysScene* Scene, float DeltaTime)
{
}

void ASimulationManagerUImpl::OnPhysicsStep(FPhysScene* Scene, float DeltaTime)
{
}

void ASimulationManagerUImpl::OnPostPhysicsStep(FChaosScene* Scene)
{
// Game thread - safe to call RPCs and Unreal API here.
	onPostSimulationGameThread();

// ⛔ HasAuthority() is unreliable on non-replicated actors - use runsPrediction().
	if (m_manager.has_value() && !m_manager->runsPrediction())
	{
		if (ASimulationTimingRelay* relay = findTimingRelay())
			ServerTickClock::writeToSyncedBuffer(getServerClock(), relay->editBuffer(), 0);
	}

	updateVisualizationAll(m_storage);

// ---- [ringout task 5] THE SCORE PUSH ----------------------------------
//
// ⛔ THE GATE IS `!runsPrediction()`, THE SAME EXPRESSION TWELVE LINES ABOVE, and it is now
// the layer's ONLY role site for ring-out - [ringout task 19] deleted the BeginPlay wiring
// line, the flag it wrote, and the two-sided checkf that cross-checked them, because there is
// no longer a second gate to disagree with: brawlerRingout::ScoreSystem declares
// kRoleAffinity = AuthorityOnly and SimulationSystemsExecutor skips it off the authority.
// ⛔ NOT HasAuthority(): `bReplicates = false` pins Role to authority on every peer.
//
// ⚠ THIS GATES THE REPLICATION, NOT THE AWARD. A client whose gate here were deleted would
// push scores its ScoreSystem never computed - an empty roster, so zeroes over the replicated
// values - which is loud rather than silent. Per F26 nothing mechanical checks this line.
//
// WHY HERE, beside updateVisualizationAll. This is the one game-thread point that runs
// directly after the simulation has advanced, and harvesting a physics-side result for
// PRESENTATION is exactly what its neighbour already does. The push is a poll, not an event:
// it costs one int compare per character per pass when nothing changed, and the score
// changes at most once per death tick.
	if (m_manager.has_value() && !m_manager->runsPrediction())
	{
		pushRingoutScoresToCharacters(m_systemsExec.get<brawlerRingout::ScoreSystem>(),
			m_delayedInputComponentsById);
	}
}

TryRegisterStatus ASimulationManagerUImpl::tryRegister(
    unsigned int id,
    SimulatableBrawler simulatable,
    USimmableUpdateComponent& owner,
    BrawlerInputProviderFn inputProvider,
    bool isAuthority)
{
// Look up or insert the per-id pending record.
    auto it = m_pendingRegistrations.find(id);
    if (it == m_pendingRegistrations.end())
    {
        PendingRegistration record;
        record.simulatable.emplace(std::move(simulatable));
        record.inputProvider = std::move(inputProvider);
        record.isAuthority   = isAuthority;
        auto inserted = m_pendingRegistrations.emplace(id, std::move(record));
        it = inserted.first;
    }

    PendingRegistration& record = it->second;

    if (!record.bodiesCreated)
    {
// First-call body creation pass.
// ⭐ [movement-sim task 19] THIS CAST IS THE REGISTRATION PATH, AND IT IS NOT AN ACCESSOR SWAP.
// It used to name the engine's walking-pawn base; task 19 rebased `AOGBrawlerUECharacter` on
// `APawn`, so that base is no longer in the hierarchy and the old cast would return NULL HERE —
// on the FIRST-CALL body-creation pass — which is registration failing outright: no bodies, no
// simulation, no character. The root capsule this pass needs is declared by
// `AOGBrawlerUECharacter` itself now, so that class IS the type the contract requires.
// ⚠ `checkf` COMPILES OUT IN SHIPPING (task 36). There a wrong owner type is a null dereference
// on the very next line rather than an assert, which is why the message below names the exact
// class rather than a family.
        AOGBrawlerUECharacter* character = Cast<AOGBrawlerUECharacter>(owner.GetOwner());
        checkf(character != nullptr,
               TEXT("USimmableUpdateComponent must be attached to an AOGBrawlerUECharacter — the ")
               TEXT("first-call body pass reads that class's own root capsule"));
        FBodyInstanceAsyncPhysicsTickHandle parentHandle =
            character->GetCapsuleComponent()->GetBodyInstanceAsyncPhysicsTickHandle();
        const BodyId parentBodyId = m_physAdapter->getBodyId(parentHandle);

// ⚠ CharacterBindings is NO LONGER STAMPED HERE. [movement-sim T13] moved it BELOW the
// physics fold, because its source is now that fold's output. See the §10 banner there.

        AActor* ownerActor = owner.GetOwner();
// ⛔ Attach and parent-body are the SAME capsule, so ONE handle - two let callers desync. §10
        UPrimitiveComponent* attachParent = character->GetCapsuleComponent();
        const simulatableBrawler::StaticData& staticData = owner.getStaticData();

// The factory's parentBodyId roots every shape, so overlap() emits the capsule id.
        ChaosPhysicsFactory factory(*m_physAdapter, *m_queryAdapter, ownerActor, attachParent);

        record.simulatable->editPhysicsComposite().forEach([&](auto& decl)
        {
            using D = std::decay_t<decltype(decl)>;
// Generic — each declaration names its own slice (PhysicsDeclaration.h). Adding a
// body-owning sub-simulation therefore edits no engine file to have its body CREATED,
// BOUND, CAPTURED, REWOUND and CHECKSUMMED.
//
// ⚠ [movement-sim T13, from the task-10 review] THAT IS THE WHOLE OF THE CLAIM, and the
// earlier unqualified wording overstated it. Making that body COLLIDABLE OR QUERYABLE is
// still hand-written engine work: its `collisionCategory` needs one entry in EACH of the two
// ChaosCategoryMapping tables in BeginPlay above (authority branch and client branch,
// duplicated with no shared constant), or ChaosPhysicsFactory::applyDescriptor types the
// shape by the unmapped fallback and no object query can ask for it. Tasks 39 and 43 paid
// exactly that cost for `world` and `character`. Generic creation, hand-written collision —
// state both halves.
            const auto& subStaticData = D::staticDataOf(staticData);
            auto r = factory.createPhysicalObject(D::descriptor(), D::name);
            decl.bindings.ownBodyId        = r.bodyId;
            decl.bindings.parentBodyId     = parentBodyId;
            decl.bindings.attachmentOffset = D::attachmentOffset(subStaticData);
            decl.bindings.shapeIds         = std::move(r.shapeIds);
            FCollisionQueryParams qp;
            qp.AddIgnoredActor(ownerActor);
            for (const auto& volDesc : D::queryVolumes(subStaticData))
                decl.bindings.queryVolumeIds.push_back(
                    m_queryAdapter->registerVolume(volDesc, qp, FActorInstanceHandle(ownerActor)));
        });

#if DO_CHECK
        record.simulatable->getPhysicsComposite().forEach([&](const auto& decl)
        {
            checkf(decl.bindings.ownBodyId.value != 0,
                   TEXT("PhysicsDeclaration bindings.ownBodyId not populated by fold (id=%u)"), id);
            checkf(decl.bindings.parentBodyId.value != 0,
                   TEXT("PhysicsDeclaration bindings.parentBodyId not populated by fold (id=%u)"), id);
        });
#endif

// Stamp the authoritative capsule body id into the brawler's CharacterBindings. §10
//
// SOURCE SINCE [movement-sim T13]: the movement sub-simulation's OWN PhysicsDeclaration
// bindings, not the pawn's own capsule lookup. That is why this stamp sits AFTER the fold —
// before it, `bindings.ownBodyId` is still zero. (The `#if DO_CHECK` block above catches a
// zero id in development and test builds; being `checkf`, it is compiled out of Shipping, so
// it is a development instrument and not a Shipping-build guarantee.)
//
// ⭐ THE VALUE IS UNCHANGED, AND THAT IS THE POINT. Task 11's descriptor sets `isRoot`, so
// the factory ADOPTS the pawn's existing root capsule instead of creating a body; that is
// what makes `ownBodyId == parentBodyId == capsuleBodyId` true by construction. This
// identity is exactly what `isRoot` buys, and it is why task 11's review refused to defer
// `isRoot` to a later task. ⛔ Anyone "simplifying" `isRoot` away silently breaks this line.
        const BodyId movementOwnBodyId =
            record.simulatable->getPhysicsComposite()
                .get<brawlerMovementSimulation::PhysicsDeclaration>().bindings.ownBodyId;

// ⭐ [movement-sim task 17] THE TWO-SOURCE TRIPWIRE IS GONE. It asserted
// `movementOwnBodyId == record.parentBodyId` for as long as the two sources coexisted;
// `PendingRegistration::parentBodyId` — the record field that held the second one — is deleted
// with it, and the resolvability gate below now reads this same declaration. The identity it
// watched is UNCHANGED and still stated above: it is `isRoot` that makes it hold, not an
// assertion.
//
// ⭐ AND THE IDENTITY IS STILL ASSERTED, one layer down and by a check this task does not touch:
// `ChaosPhysicsFactory::createPhysicalObject`'s adopt-root arm ends in
// `checkf(bodyId == m_parentBodyId, …)` (`ChaosPhysicsFactory.cpp:195`), and its `m_parentBodyId`
// is derived from the SAME capsule component this function passed as the attach parent. So the
// removal drops a duplicate, not the only witness.
// ⚠ Neither was ever a Shipping-build guarantee — `checkf` compiles out there (task 36).

        record.simulatable->setCharacterBindings({ /*.capsuleBodyId =*/ movementOwnBodyId });

// THE TELEPORT SEED — spawn. BrawlerMovementSimulation.h's InitialConditions is a
// COUNTER-FREE edge: the UE layer sets `teleportPending` non-zero here, the sub-sim's first
// step consumes it and clears it back to zero in the same tick. It is ON THE WIRE (16 B) so
// that a respawn replays identically on a client and through a resim.
//
// ⛔ FIRST-CALL PASS ONLY. This branch runs once per character (guarded by
// `record.bodiesCreated`), and the record is moved wholesale into storage at registration
// below, so the seed survives to the sub-sim's first integrate. Seeding it per call would
// re-teleport the character every tick until it registers.
//
// The pose comes from the capsule component, which is where the engine has the character
// standing at spawn — this branch runs on the FIRST tryRegister call, before the sub-sim has
// integrated once, so nothing has driven the capsule yet whatever `drivesBody` says. (It says
// `true`: task 15 flipped it together with `simulatePhysics` and retired the CMC. This is
// still a READ of the engine's authoritative spawn pose, and the reason is the ORDER, not a
// second authority.)
//
// ⚠ THE CONSUMER DOES WRITE BACK. The teleport branch is documented as the ONE body write
// that ignores `drivesBody` — it calls setBodyTransform + setBodyLinearVelocity(0) on the
// capsule. Seeded from the capsule's OWN current location that is a value no-op, but it is
// a real engine call, and a future seed from any other source would MOVE the character.
        auto& movementIC = record.simulatable->editAllState().editState()
            .edit<brawlerMovementSimulation::InitialConditions>();
        movementIC.teleportPending = 1u;
        movementIC.teleportPos     =
            uglm::toGLMVec3(character->GetCapsuleComponent()->GetComponentLocation());


// THE SPAWN-SLOT SEED - ring-out, task 3. Same first-call branch, and for the same reason:
// `brawlerRingout::InitialConditions::spawnSlot` is written ONCE per character and read on
// every respawn for the life of that character. Unlike the teleport seed above it is NOT a
// counter-free edge - nothing consumes it and nothing clears it.
//
// ⛔ FIRST-CALL PASS ONLY, inherited from the branch it sits in. Re-seeding per call would
// not merely be wasteful: `acquire` is idempotent, so it would return the same index, but a
// character whose slot had been RELEASED and handed to someone else would silently change
// spawn point mid-session. Once, at registration, is the contract.
//
// ⛔⛔ AUTHORITY ONLY, AND THIS IS NOT HasAuthority(). `bReplicates = false` makes
// `GetLocalRole()` report authority on this actor in every role, which is why this file uses
// the WORLD-level test everywhere. `record.isAuthority` IS that test: the caller
// (`USimmableUpdateComponent::tryRegisterWithNewFramework`) computes it as
// `(GetNetMode() != NM_Client)` - literally the expression `BeginPlay` names
// `worldIsAuthority` - and passes it in. The `checkf` states that equivalence rather than
// leaving it to a comment, because `tryRegister` is a public entry point.
//
// ⭐ WHY A CLIENT MAY SAFELY LEAVE ITS OWN COPY AT THE DEFAULT 0, AND WHY THAT CANNOT BE
// READ BEFORE THE SERVER'S NUMBER ARRIVES. `tryRegister` runs on BOTH roles, so a client
// registers its own character locally with `spawnSlot` at its in-class initialiser, 0. That
// value is read by exactly one thing: the respawn arm of `brawlerRingout::integrate`, which
// is reached only after the character has (a) crossed the kill plane and (b) waited
// `StaticData::respawnDelayTicks` - 120 ticks, 2.0 s at 60 Hz - for `tick >= respawnAtTick`.
// The correction that carries the authority's `InitialConditions` is an ORDINARY state
// correction on the already-running rotation, arriving within a handful of ticks of
// registration and restoring the whole struct by assignment. So the client's 0 is
// overwritten two orders of magnitude before the only code that reads it can run, and a
// resim RESTORES the authority's value rather than recomputing it. There is no path on
// which the placeholder is read first: death itself cannot occur earlier than the first
// correction and still leave 120 ticks of countdown to run before the value matters.
//
// ⛔ AND ARRIVAL ORDER DECIDING THE NUMBER IS DELIBERATE - see the ⛔ banner on
// `brawlerRingout::SpawnSlotAllocator`. Clients never derive this; a replay restores it. Do
// not "fix" it into a per-peer derivation such as `GetPlayerSlotForActor`, which is per-wire
// and answers 0 for the primary pawn of every remote client.
        if (record.isAuthority)
        {
            checkf(record.isAuthority == (GetNetMode() != NM_Client),
                   TEXT("tryRegister: isAuthority disagrees with the world-level net mode; the ")
                   TEXT("spawn-slot table must be populated on the authority world only"));

            auto& ringoutIC = record.simulatable->editAllState().editState()
                .edit<brawlerRingout::InitialConditions>();

// ⛔ THE OVER-CAPACITY ANSWER IS WRITTEN THROUGH, NOT CLAMPED. `acquire` returns
// `kNoFreeSlot`, whose value is `kMaxSpawnPoints`, and the sub-sim's defensive index branch
// turns that into a `[Warning][Ringout.spawnSlot]` and no teleport seed. Clamping to 0 here
// would put the over-capacity character on top of whoever legitimately holds 0, silently.
            ringoutIC.spawnSlot = m_spawnSlots.acquire(id);

            UE_LOG(LogOGMgmt, Log,
                TEXT("tryRegister: ring-out spawnSlot=%u assigned to id=%u"),
                ringoutIC.spawnSlot, id);
        }

        record.bodiesCreated = true;
        return TryRegisterStatus::Pending;
    }

// Resolvability gate.
//
// ⭐ [movement-sim task 17] THE SOURCE IS THE MOVEMENT DECLARATION'S OWN `ownBodyId`, which is
// what `PendingRegistration::parentBodyId` used to hold and no longer exists to hold. The value
// is the same body — the descriptor's `isRoot` makes the factory ADOPT the pawn's root capsule,
// so the movement declaration's own id IS the capsule id (stated in full at the stamp above).
//
// ⚠ AND IT IS DELIBERATELY REDUNDANT WITH THE FOLD BELOW, which visits every declaration and
// therefore visits this one too. It is kept as the named, order-first read so the gate says out
// loud WHICH body a `Pending` is waiting on; it adds no guarantee the fold does not already give.
    const BodyId movementBodyId =
        record.simulatable->getPhysicsComposite()
            .get<brawlerMovementSimulation::PhysicsDeclaration>().bindings.ownBodyId;
    bool allResolvable = m_physAdapter->isBodyResolvable(movementBodyId);
    if (allResolvable)
    {
        record.simulatable->getPhysicsComposite().forEach([&](const auto& decl)
        {
            if (!m_physAdapter->isBodyResolvable(decl.bindings.ownBodyId))
                allResolvable = false;
        });
    }

    if (!allResolvable)
        return TryRegisterStatus::Pending;

// All resolvable - perform the actual registration.
    if (record.isAuthority)
    {
// `m_inputResolution` inserted - the facade gained the parameter with the peer's promotion.
        registerSimulatable<SimulatableBrawler>(
            m_storage, m_reconciliation, m_inputResolution, m_netSync,
            id, std::move(*record.simulatable),
            /*predictionOwner=*/owner,
            /*authorityOwner=*/owner);
    }
    else
    {
        registerSimulatable<SimulatableBrawler>(
            m_storage, m_reconciliation, m_inputResolution, m_netSync,
            id, std::move(*record.simulatable),
            /*owner=*/owner,
            /*inputProvider=*/std::move(record.inputProvider));
    }
    UE_LOG(LogOGMgmt, Log, TEXT("tryRegister: registered simulatable id=%u isAuthority=%d"), id, isAuthority ? 1 : 0);

// -----------------------------------------------------------------------
// THE PRE-DIET CAP FENCE, runtime half. Deleted with kPreDietCharacterCap by the diet. §10
//
// ⛔ ONCE PER OVER-CAP CHARACTER: a per-session latch would go silent after the fifth.
//
// ⛔ WARNING, not Log, and not an assert: an over-cap session still RUNS - report, do not crash. §3
    if (record.isAuthority)
    {
        m_authorityRegisteredIds.insert(id);
        const int32 registered = static_cast<int32>(m_authorityRegisteredIds.size());
        if (registered > kPreDietCharacterCap)
        {
            UE_LOG(LogOGNet, Warning,
                TEXT("[PreDietCap] character %d exceeds pre-diet cap %d — input-loss margins "
                     "unsafe (T44/T38 §16); land item 40"),
                registered, kPreDietCharacterCap);
        }
    }

// Notify the executor: the character is IN STORAGE, so
// brawlerHitRouting::System::onCharacterRegistered can index it and read capsuleBodyId. §10
//
// ⛔ The notify and the drop of the adapter's m_byRootBodyId insert land TOGETHER.
    m_manager->notifyCharacterRegistered(id);

    m_pendingRegistrations.erase(it);
    return TryRegisterStatus::Ready;
}

// sampleAndDeriveConnectionTier and tryEnqueueDelayedRemoteInput are GONE: their primitive
// acquisition moved UP to the RPC boundary, and no per-slot path remains manager-side. §7

void ASimulationManagerUImpl::noteDelayedInputComponent(
    unsigned int id, USimmableUpdateComponent& component)
{
// Routing registration for the `deliver` callback. ⛔ A plain overwrite, ONCE at register. §7 §10
    m_delayedInputComponentsById[id] = &component;
}

// Satisfies RemoteInputDeliverySink; asserted here so a signature break surfaces legibly. §7
static_assert(
    RemoteInputDeliverySink<ASimulationManagerUImpl, simulatableBrawler::PlayerInput>,
    "ASimulationManagerUImpl must satisfy RemoteInputDeliverySink so "
    "ServerReceptionCoordinator can drive remote-input delivery through it");

void ASimulationManagerUImpl::deliverRemoteInput(
    unsigned int id, uint32 captureTick, const simulatableBrawler::PlayerInput& input)
{
// Same inbound path as the RPC, ORIGINAL captureTick. ⛔ A stale handle drops both. §7 §10
    const auto it = m_delayedInputComponentsById.find(id);
    if (it == m_delayedInputComponentsById.end())
        return;

    USimmableUpdateComponent* target = it->second.Get();
    if (target == nullptr)
    {
        m_delayedInputComponentsById.erase(it);
        return;
    }

    target->deliverDelayedRemoteInput(captureTick, input);
}

// ALSO satisfies RemoteInputRelaySink - the outbound tap fired at each newer capture tick. §6
static_assert(
    RemoteInputRelaySink<ASimulationManagerUImpl, simulatableBrawler::PlayerInput>,
    "ASimulationManagerUImpl must satisfy RemoteInputRelaySink so "
    "ServerReceptionCoordinator can drive the input relay through it");

void ASimulationManagerUImpl::relayRemoteInput(
    unsigned int id, uint32 captureTick, uint8 dA,
    const simulatableBrawler::PlayerInput& input)
{
// Same register-time route and same stale-handle prune as the delivery sink.
    const auto it = m_delayedInputComponentsById.find(id);
    if (it == m_delayedInputComponentsById.end())
        return;

    USimmableUpdateComponent* target = it->second.Get();
    if (target == nullptr)
    {
        m_delayedInputComponentsById.erase(it);
        return;
    }

// ⛔ STAGE, DO NOT WRITE THE RING: the host's PreReplication publishes the burst per poll. §6
//
// ⛔ NO DEPTH IS READ HERE ANY MORE, AND THAT IS THE POINT: a depth passed to `writeLatest`
// would silently restore replace-latest on the flush path, with no compile error.
// `stageRelayedInput` has no depth parameter; the fence is Network/RelayRedundancyDepthTest.cpp.
//
// The outcome is deliberately unchecked: the stale-write arm is unreachable from here.
//
// ⛔ DUAL-WRITE FENCE DISCHARGED: m_replicatedInputSyncedBuffer is gone; this tap is the only path. §6
    target->stageRelayedInput(captureTick, dA, input);

// -----------------------------------------------------------------------
// PROBE 5 - RELAY WRITES PER GAME-THREAD FRAME. §8
// -----------------------------------------------------------------------
//
// ⚠ WHAT IT SETTLES. WHEN THIS PROBE WAS BUILT the ring shipped at depth 1, so a second
// write in one polled frame overwrote the first in server memory - indistinguishable from a
// send-path drop. Flush-on-poll removed that; the QUANTITY is still the unmeasured one. §6 §11
//
// ⛔ The relay-loss elimination chain does not cover this: these writes are PACKET-paced.
//
// ⛔ GFrameCounter, NOT AN INVOCATION COUNT: a local counter would measure the call rate.
//
// ⛔ THREE FRACTIONS, NEVER COLLAPSED: completeness, coalescing ceiling, and their product
// `deliverableX1000` - the only one comparable to the client's rate. Merged, they pick blind.
//
// VOLUME: two Warning lines per 120 WRITING FRAMES per character. ⛔ Nothing per-write.
    {
        RelayWriteWindowSummary w;
        if (m_relayWriteProbe.noteWrite(
                id, static_cast<uint64>(GFrameCounter), captureTick, w))
        {
            char line[256];

// Line 1 - THE THREE FRACTIONS. `deliverableX1000` is the headline, read against the client's
// .Arrival gap; `replaceLatestObservableX1000` keeps archived windows comparable. §8
            std::snprintf(line, sizeof(line),
                "[Warning][RelayProbe.Write] id=%u runs=%u writes=%u observableWrites=%u "
                "captureSpan=%u receivedX1000=%u observableX1000=%u deliverableX1000=%u "
                "replaceLatestObservableX1000=%u",
                w.ownerId, w.runs, w.writes, w.observableWrites, w.captureSpan,
                w.receivedX1000, w.observableX1000, w.deliverableX1000,
                w.replaceLatestObservableX1000);
            RouteOGMessage(line);

// Line 2 - THE SHAPE, plus a capture-tick range: owner ids are per-PROCESS, ticks are not.
            std::snprintf(line, sizeof(line),
                "[Warning][RelayProbe.Write] id=%u writesPerFrame p50=%u p99=%u%s "
                "max=%u emptyFrames=%u nonConsecutive=%u missedCaptureTicks=%u "
                "captures=[%u,%u] discont=%u",
                w.ownerId, w.p50, w.p99, w.p99Saturated ? "+" : "", w.maxRun,
                w.emptyFrames, w.nonConsecutiveWrites, w.missedCaptureTicks,
                w.firstCaptureTick, w.lastCaptureTick, w.discontinuities);
            RouteOGMessage(line);
        }
    }
}

// ---------------------------------------------------------------------------
// TICK ALIGNMENT. ⛔ An off-by-one here shifts EVERY player's input, silently and uniformly. §9
//
// `physicsStep` is the UPCOMING solver step; the frame counter increments at a tick's END,
// so step N's OnPreSimulate_Internal sees frame N and writes the mapper offset there,
// BEFORE onGameSimulation, whose first action on the authority is to advance the clock:
//
//     offset = K - S(K-1)     where S(K) is the sim tick simulated at step K
//     S(K)   = S(K-1) + 1     authority advance is unconditional - no Stall, Skip or
//                             resim exists on the server
//  => offset = (K - S(K)) + 1
//  => toSimulationTick(X) = X - offset = S(X) - 1
//
// ⛔ So toSimulationTick(physicsStep) names the tick BEFORE that step's: hence the `+ 1`.
//
// ⛔ WHY NOT CROSS-CHECK AGAINST THE SERVER CLOCK: reading it here IS the unsynchronized
// cross-thread read this design exists to avoid. The mapper's offset is the safe source. §1
//
// Sub-stepping: NumSteps > 1 releases per tick; NumSteps == 1 is one drain.
// ---------------------------------------------------------------------------
void ASimulationManagerUImpl::releaseDelayedInputsForStep(int32 physicsStep, int32 numSteps)
{
// Thin adapter over the drain and reap: this side supplies the tick and the callback. §7
//
// ⛔ The coordinator early-return guards ONLY the drain, never PROBE A, which runs on both. §8

    const int32 firstUpcomingSimTick =
        static_cast<int32>(m_chaosTickMapper.toSimulationTick(static_cast<int32_t>(physicsStep))) + 1;

// -----------------------------------------------------------------------
// PROBE A - SIM TICKS PER GAME-THREAD FRAME, i.e. FRAME HEALTH. BOTH ROLES. §8
// -----------------------------------------------------------------------
//
// WHAT IT SETTLED ON THE SERVER. Clients measure a ~2-tick relay arrival gap where ~1 is
// expected, and the net tick rate is a CAP on replication, not a floor - so a 60 Hz sim on
// a 30 fps server ships two ticks per poll. Ratio == gap means a HOST artefact, not netcode.
//
// ⛔ WHY THE CLIENT NEEDED IT TOO: this was the ONLY wall-clock instrument in the netcode
// surface and was server-only - yet the server never resims, so every client cost figure
// behind the shipped policy came from a cadence blind to game-thread hitching. §8
//
// ⛔ WHY HERE AND NOT OnPostPhysicsStep: that hook has no step number, so no safe tick. §9
//
// ⛔ WHY REUSING `firstUpcomingSimTick` IS SAFE ON THE CLIENT although its `+1` derivation
// assumes authority: the probe consumes only DELTAS, so a constant skew cancels, and a
// departure is counted as a `kFrameHealthDiscontinuityTicks` discontinuity. ⛔ A DIFFERENT
// tick source would not be safe. §9
//
// THE RATIO IS HOOK-INDEPENDENT - it keys on GFrameCounter - and reports its OWN cadence.
//
// ⛔ `numStepsAboveOne > 0` reads "resim ran"; `== 0` with a high ratio, "the thread hitched".
//
// ⛔ CATEGORY PER ROLE, NOT INHERITED: server on [RelayProbe.Frame], client on [ResimProbe.Frame]. §4
//
// ⛔ TWO DIFFERENT FAMILIES, not one tag with a role suffix; both carry a `role=` field too.
//
// ⚠ COMPARABLE WITH ResimGateProbe, NOT IDENTICAL: same 120 samples, different closing event. §8
//
// VOLUME UNCHANGED: window summaries at Warning, nothing per-frame, on EACH role.
    {
        FrameHealthWindowSummary frameSummary;
        const bool frameWindowClosed = m_frameHealthProbe.noteFrame(
            static_cast<uint64>(GFrameCounter),
            static_cast<uint32>(firstUpcomingSimTick),
            static_cast<uint32>(numSteps),
            static_cast<uint64>(FPlatformTime::Seconds() * 1000000.0),
            frameSummary);

        if (frameWindowClosed)
        {
// ⛔ `runsPrediction()` false covers the server AND standalone, which rides the SERVER tag.
            const bool isClient = m_manager.has_value() && m_manager->runsPrediction();
            char line[256];

            if (isClient)
            {
// Line 1 - THE RATIO, client role. p99 and max are why this exists: a mean hides a hitch.
                std::snprintf(line, sizeof(line),
                    "[Warning][ResimProbe.Frame] role=Client simTicks=%u frames=%u "
                    "meanTicksPerFrameX100=%u p50=%u p99=%u max=%u meanFrameUs=%u",
                    frameSummary.totalSimTicks, frameSummary.totalFrames,
                    frameSummary.meanTicksPerFrameX100, frameSummary.p50,
                    frameSummary.p99, frameSummary.maxTicksPerFrame,
                    frameSummary.meanFrameMicros);
                RouteOGMessage(line);

// Line 2 - cadence + sub-step cross-check, client role.
                std::snprintf(line, sizeof(line),
                    "[Warning][ResimProbe.Frame] role=Client cadence dFrame1=%u dFrame0=%u "
                    "dFrameGt1=%u discont=%u numSteps total=%u max=%u gt1=%u",
                    frameSummary.oncePerFrameSamples, frameSummary.sameFrameSamples,
                    frameSummary.skippedFrameSamples, frameSummary.discontinuities,
                    frameSummary.totalNumSteps, frameSummary.maxNumSteps,
                    frameSummary.numStepsAboveOne);
                RouteOGMessage(line);
            }
            else
            {
// Line 1 - THE RATIO, server role. ⛔ Tag unchanged, so archived-run greps keep working.
                std::snprintf(line, sizeof(line),
                    "[Warning][RelayProbe.Frame] role=Server simTicks=%u frames=%u "
                    "meanTicksPerFrameX100=%u p50=%u p99=%u max=%u meanFrameUs=%u",
                    frameSummary.totalSimTicks, frameSummary.totalFrames,
                    frameSummary.meanTicksPerFrameX100, frameSummary.p50,
                    frameSummary.p99, frameSummary.maxTicksPerFrame,
                    frameSummary.meanFrameMicros);
                RouteOGMessage(line);

// Line 2 - cadence + sub-step cross-check, server role.
                std::snprintf(line, sizeof(line),
                    "[Warning][RelayProbe.Frame] role=Server cadence dFrame1=%u dFrame0=%u "
                    "dFrameGt1=%u discont=%u numSteps total=%u max=%u gt1=%u",
                    frameSummary.oncePerFrameSamples, frameSummary.sameFrameSamples,
                    frameSummary.skippedFrameSamples, frameSummary.discontinuities,
                    frameSummary.totalNumSteps, frameSummary.maxNumSteps,
                    frameSummary.numStepsAboveOne);
                RouteOGMessage(line);
            }
        }
    }

    if (!m_receptionCoordinator.has_value())
        return;

// -----------------------------------------------------------------------
// PROBE 6 - PER-CONNECTION SEND BUDGET. §8
// -----------------------------------------------------------------------
//
// ⛔ WHY ARITHMETIC WAS NOT ENOUGH: EVERY TERM in the allowance formula is DERIVED.
//
// It measures all of them, `notReady` frames included - the frames that wrote NOTHING.
//
// ⛔ READ BEFORE TickFlush: `QueuedBits` updates at Tick's END, so this is the real credit.
//
// ⛔ CUMULATIVE COUNTERS, NOT `OutBytes`/`OutPackets`: the engine zeroes those mid-window.
//
// VOLUME: two Warning lines per 120 server frames per client connection.
    if (const UWorld* world = GetWorld())
    {
        if (const UNetDriver* netDriver = world->GetNetDriver())
        {
// The allowance denominator. ⛔ From the driver, so a config change cannot invalidate it.
            const uint32 tickRateHz =
                static_cast<uint32>(FMath::Max(1, netDriver->GetNetServerMaxTickRate()));
            const uint64 nowMicros =
                static_cast<uint64>(FPlatformTime::Seconds() * 1000000.0);

            for (UNetConnection* conn : netDriver->ClientConnections)
            {
                if (conn == nullptr)
                    continue;

// -----------------------------------------------------------
// THE CAPACITY PIN - the packet budget, measured. §8
// -----------------------------------------------------------
//
// ⛔ WHY THIS EXISTS: the DERIVED single-bunch capacity does not reproduce. The engine referees.
//
// ⛔ It only moves DOWN, so RoundVsPacketBudgetTest.cpp's literal is an UPPER BOUND.
//
// ONE-SHOT PER SESSION, at Warning: the value is a property of the build, not the connection. §3
                {
                    static bool s_loggedPacketBudget = false;
                    if (!s_loggedPacketBudget)
                    {
                        s_loggedPacketBudget = true;
                        const int32 usableBits = conn->GetMaxSingleBunchSizeBits();
                        UE_LOG(LogOGNet, Warning,
                            TEXT("[PacketBudget] usableSingleBunchBytes = %d, handlerBits = %d "
                                 "(maxPacket=%d, usableBits=%d)"),
                            (usableBits / 32) * 4, conn->MaxPacketHandlerBits,
                            conn->MaxPacket, usableBits);
                    }
                }

                ConnectionBudgetWindowSummary budget;
                const bool budgetWindowClosed = m_connectionBudgetProbe.noteSample(
                    static_cast<uint32>(conn->GetUniqueID()),
                    conn->CurrentNetSpeed,
                    conn->QueuedBits,
                    static_cast<uint64>(FMath::Max(0, conn->OutTotalBytes)),
                    static_cast<uint64>(FMath::Max(0, conn->OutTotalPackets)),
                    static_cast<uint64>(FMath::Max(0, conn->OutTotalPacketsLost)),
                    tickRateHz, nowMicros, budget);

                if (!budgetWindowClosed)
                    continue;

                char line[256];

// Line 1 - THE THROUGHPUT. ⛔ `netSpeedBps` is printed: off the ceiling, the budget is wrong.
                std::snprintf(line, sizeof(line),
                    "[Warning][RelayProbe.Budget] conn=%u frames=%u elapsedMs=%u "
                    "netSpeedBps=%d allowanceBytesPerTick=%u outBytes=%u "
                    "bytesPerTick=%u occupancyPctX10=%u",
                    budget.connectionId, budget.samples, budget.elapsedMs,
                    budget.netSpeedBps, budget.allowanceBytesPerTick,
                    budget.outBytes, budget.bytesPerSample, budget.occupancyPctX10);
                RouteOGMessage(line);

// Line 2 - THE SATURATION STATE and the REAL loss. QueuedBits is a debt counter, so `min` is
// the MOST headroom and `max` the closest to saturation. ⛔ `lost` is ack-derived: the ACTUAL
// outgoing loss, not the configured percentage.
                std::snprintf(line, sizeof(line),
                    "[Warning][RelayProbe.Budget] conn=%u queuedBits min=%d max=%d "
                    "mean=%d notReadyFrames=%u outPackets=%u bytesPerPacket=%u lost=%u",
                    budget.connectionId, budget.queuedBitsMin, budget.queuedBitsMax,
                    budget.queuedBitsMean, budget.notReadySamples,
                    budget.outPackets, budget.bytesPerPacket, budget.outPacketsLost);
                RouteOGMessage(line);
            }
        }
    }

// The per-id drain callback: answers owner liveness, then routes via deliverRemoteInput. §7
//
// ⛔ Liveness stays HERE: the drain's prune contract is a bool, the sink returns void.
    auto deliver = [this](unsigned int id, uint32 captureTick,
                          const simulatableBrawler::PlayerInput& input) -> bool
    {
        const auto it = m_delayedInputComponentsById.find(id);
        if (it == m_delayedInputComponentsById.end())
            return false;

        if (it->second.Get() == nullptr)
        {
            m_delayedInputComponentsById.erase(it);
            return false;
        }

        deliverRemoteInput(id, captureTick, input);
        return true;
    };

    m_receptionCoordinator->releaseDelayedInputs<SimulatableBrawler>(
        firstUpcomingSimTick, numSteps, deliver);

// Reap, off the arrival-gated path: once per physics frame, dwell-gated inside. §7
//
// ⛔ The tick comes from the game-thread-safe mapper, NOT the server clock. §9
    m_receptionCoordinator->reapConnections(firstUpcomingSimTick);
}

void ASimulationManagerUImpl::unregisterFromNewFramework(
    unsigned int id, USimmableUpdateComponent& owner, bool isAuthority)
{
// ⛔ Drop the routing entry BEFORE unregisterSimulatable destroys it, while still in storage. §10
//
// ⛔ The has<> guard is preserved: an unregistered character's view.get<>(id) is unsafe.
    if (m_storage.has<SimulatableBrawler>(id))
    {
        m_manager->notifyCharacterUnregistered(id);
    }

// `m_inputResolution` inserted - the facade gained the parameter with the peer's promotion.
    unregisterSimulatable<SimulatableBrawler>(
        m_storage, m_reconciliation, m_inputResolution, m_netSync,
        id,
        /*predictionOwner=*/&owner,
        /*authorityOwner=*/isAuthority ? &owner : nullptr);

// ⛔ The unregister contract that replaces the core's former GC-liveness read: drop this
// owner's claim, dedup watermark and id->component mapping here, PROMPTLY, rather than
// waiting for GC to make an engine handle stale. No-op on a pure client. §7 §10
    if (m_receptionCoordinator.has_value())
    {
        m_receptionCoordinator->forgetOwner(id);
    }
    m_delayedInputComponentsById.erase(id);

// Same unregister contract for the write probe. ⛔ A dead owner's half-open run is dropped. §8
    m_relayWriteProbe.forgetOwner(id);

// The cap's denominator, reaped so a churning session is judged on the resident roster. §10
    m_authorityRegisteredIds.erase(id);

// Same unregister contract for the ring-out spawn-slot table (task 3). ⛔ WITHOUT THIS a
// session that churns characters exhausts a four-entry table and every later join is handed
// the out-of-range value, respawning nowhere. UNGATED for the same reason the erase above is:
// nothing on the client role ever acquired, so `release` is a no-op there by construction
// rather than by a role test.
    m_spawnSlots.release(id);

// Same unregister contract for the display. ⛔ A kept ring is a leak keyed on a dead id. §10
    m_inputHistory.forgetCharacter(id);

    UE_LOG(LogOGMgmt, Log, TEXT("NewFramework: unregistered simulatable id=%u"), id);
}

void ASimulationManagerUImpl::InjectInputs_External(int32 PhysicsStep, int32 NumSteps)
{
	FSimulationInput2* asyncInput = m_asyncCallback->GetProducerInputData_External();
	asyncInput->Reset();
	asyncInput->bInitialized = true;
	asyncInput->m_world = GetWorld();
	asyncInput->m_manager = this;

// Release tier-delayed input for the upcoming step's tick(s). GAME THREAD, pre-step hook. §9
//
// ⛔ The DRAIN no-ops on a client; the frame-health probe inside it does NOT. §8
	releaseDelayedInputsForStep(PhysicsStep, NumSteps);
}

OGSIM_OPTIMIZE_ON
