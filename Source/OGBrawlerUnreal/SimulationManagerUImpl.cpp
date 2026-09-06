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
#include "GameFramework/Character.h"
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

void ASimulationManagerUImpl::BeginPlay()
{
	Super::BeginPlay();


	UWorld* uWorld = GetWorld();
	if (uWorld == nullptr)
		checkf(false, TEXT("SimulationManagerUImpl: unexpected state"));

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
// [movement-sim T43] The movement sub-sim's OWN body. BrawlerMovementSimulation.h:98 registers its
// 30 cm sphere under this category, and PhysicsDeclaration is in the shipped composite
// (SimulatableBrawler.h), so this runs for every character in every session.
// Channel is user ruling #6, closed 2026-09-04 and lead-verified free: ch1 is `Damageable` in
// DefaultEngine.ini and ch2-5 are the four entries above.
//
// ⛔ THIS LINE IS A FIX, NOT A NEW CAPABILITY. Without it toEngineChannel(5) fell through to the
// unmapped fallback ECollisionChannel(0) - which IS ECC_WorldStatic - so
// ChaosPhysicsFactory::applyDescriptor typed every character's MovementBody sphere as STATIC
// LEVEL GEOMETRY. Harmless while nothing searched WorldStatic; LIVE from task 39 on, because a
// `worldOnly` object query searches exactly that object type and would hand the movement sim
// OTHER characters' spheres as ground. Task 40's [SpatialQuery.UnmappedCategory] category=5 line
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
        ACharacter* character = Cast<ACharacter>(owner.GetOwner());
        checkf(character != nullptr, TEXT("USimmableUpdateComponent must be attached to an ACharacter"));
        FBodyInstanceAsyncPhysicsTickHandle parentHandle =
            character->GetCapsuleComponent()->GetBodyInstanceAsyncPhysicsTickHandle();
        const BodyId parentBodyId = m_physAdapter->getBodyId(parentHandle);
        record.parentBodyId = parentBodyId;

// Stamp the authoritative capsule body id into the brawler's CharacterBindings. §10
//
// SOURCE TODAY: the engine capsule body; the planned movement sub-sim will supply it instead.
        record.simulatable->setCharacterBindings({ /*.capsuleBodyId =*/ parentBodyId });

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
// body-owning sub-simulation therefore edits NO engine file.
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

        record.bodiesCreated = true;
        return TryRegisterStatus::Pending;
    }

// Resolvability gate.
    bool allResolvable = m_physAdapter->isBodyResolvable(record.parentBodyId);
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
