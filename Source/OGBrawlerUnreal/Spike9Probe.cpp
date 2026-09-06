// SPDX-License-Identifier: BUSL-1.1
//
// ⛔ TEMPORARY — brawler-movement-simulation TASK 9 RESEARCH SPIKE, PHASE 1.
//    Reverted in PHASE 2 by deleting this file and Spike9Probe.h. Nothing
//    includes either of them; no .Build.cs was edited.
//
// ⭐ TASK 44 (2026-09-05): scenario trigger moved off the listen server. See the
//    [task 44] block at the top of ASpike9ProbeDirector::Tick.

#include "Spike9Probe.h"

#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Runtime/Engine/Public/Net/UnrealNetwork.h"
#include "Runtime/Engine/Classes/Engine/NetConnection.h"
#include "Runtime/Engine/Classes/Engine/NetDriver.h"

#include "PhysicsPublic.h"
#include "Runtime/PhysicsCore/Public/PhysicsInterfaceDeclaresCore.h"
#include "Runtime/Engine/Public/Physics/Experimental/PhysScene_Chaos.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Runtime/Experimental/Chaos/Public/Chaos/SimCallbackInput.h"
#include "Runtime/Experimental/Chaos/Public/Chaos/SimCallbackObject.h"
#include "Runtime/Experimental/Chaos/Public/PBDRigidsSolver.h"

#include "OGSimulationUnreal/ChaosPhysicsBodyAdapter.h"
#include "OGSimulationUnreal/ChaosSpatialQueryAdapter.h"
#include "OGSimulationUnreal/ChaosPhysicsFactory.h"
#include "OGSimulationUnreal/UGLMTypeConversion.h"
#include "OGBrawler/CollisionCategoryConstants.h"

#include <atomic>
#include <optional>
#include <vector>

DEFINE_LOG_CATEGORY(LogOGSpike9);

// ===========================================================================
// 0. CONSTANTS AND CVARS
// ===========================================================================

namespace
{
// The probe capsule matches AOGBrawlerUECharacter's authored capsule
// (OGBrawlerUECharacter.cpp:69 InitCapsuleSize(42.f, 96.0f)) so that the
// adopt-root agreement checkf in ChaosPhysicsFactory.cpp passes and so the
// numbers transfer to the real character without rescaling.
constexpr float kCapsuleRadius     = 42.f;
constexpr float kCapsuleHalfHeight = 96.f;

// Scenario ids deliberately equal the SPIKE QUESTION NUMBERS.
constexpr int32 kScenarioOff   = 0;
constexpr int32 kScenarioWall  = 1;   // q1  — world push-out, determinism, correction convergence
constexpr int32 kScenarioHover = 3;   // q3+q4 — hover servo and the physics-thread probe sweep
constexpr int32 kScenarioPair  = 6;   // q6  — two BLOCKING capsules, 50/50 split, ride-up normal

int32 GScenario      = kScenarioOff;
int32 GResimPolicy   = 0;      // 0 = ReplayRecordedHistory (rev-6 default), 1 = Resimulate
float GSpeed         = 200.f;  // cm/s along the script direction
int32 GDuration      = 300;    // probe steps before auto-stop (300 @ 60 Hz = 5 s)
int32 GCorrectStep   = 150;    // q1(iii): step at which the synthetic sim-side correction fires; 0 = never
float GCorrectOffset = -50.f;  // cm along the script direction (negative = back out of the wall)
float GRideHeight    = 10.f;   // q3 (architecture §3.3 step 4)
float GSnapDistance  = 40.f;   // q3: probe length beyond rideHeight
float GMaxSnapSpeed  = 400.f;  // q3: the clamp that "catches" a drop
float GTerminalFall  = 2000.f; // q3: airborne clamp
float GPairSeparation= 300.f;  // q6: B starts this far along +forward from A
float GPairZOffset   = 0.f;    // q6(iii): B's start height relative to A — the RIDE-UP knob
float GDropCm        = 0.f;    // q3: at GDropStep, lift the commanded position this far so it falls back
int32 GDropStep      = 0;      // 0 = no drop
int32 GLogEvery      = 1;      // log 1 line in N (raise it if the log is too hot)
int32 GResetOrigin   = 0;      // set to 1 to forget the latched origin before the next start
// [task 45] WHICH post-solve read step 6' adopts. 0 = the production
// ChaosPhysicsBodyAdapter::captureBodyState() (PT `GetX()`), which is what runs
// 19:56 and 20:11 measured; 1 = the probe's own read of the rigid handle's
// `GetP()`. See the [task45-capture] block in PostSolve_PT.
int32 GCaptureSource = 0;

FAutoConsoleVariableRef CVarSpike9Scenario(TEXT("og.spike9.scenario"), GScenario,
	TEXT("Task-9 probe. 0 = off, 1 = q1 wall push-out, 3 = q3/q4 hover+sweep, 6 = q6 two blocking capsules."), ECVF_Default);
FAutoConsoleVariableRef CVarSpike9ResimPolicy(TEXT("og.spike9.resimPolicy"), GResimPolicy,
	TEXT("0 = ReplayRecordedHistory (EResimType::ResimAsFollower, the rev-6 default), 1 = Resimulate (FullResim)."), ECVF_Default);
FAutoConsoleVariableRef CVarSpike9Speed(TEXT("og.spike9.speed"), GSpeed,
	TEXT("Scripted speed in cm/s."), ECVF_Default);
FAutoConsoleVariableRef CVarSpike9Duration(TEXT("og.spike9.duration"), GDuration,
	TEXT("Probe steps before the run auto-stops (60 = 1 s)."), ECVF_Default);
FAutoConsoleVariableRef CVarSpike9CorrectStep(TEXT("og.spike9.correctStep"), GCorrectStep,
	TEXT("q1(iii): probe step at which a synthetic SIM-SIDE correction displaces the commanded position. 0 = never."), ECVF_Default);
FAutoConsoleVariableRef CVarSpike9CorrectOffset(TEXT("og.spike9.correctOffset"), GCorrectOffset,
	TEXT("q1(iii): size of that correction, cm along the script direction."), ECVF_Default);
FAutoConsoleVariableRef CVarSpike9RideHeight(TEXT("og.spike9.rideHeight"), GRideHeight,
	TEXT("q3 hover clearance target, cm."), ECVF_Default);
FAutoConsoleVariableRef CVarSpike9SnapDistance(TEXT("og.spike9.snapDistance"), GSnapDistance,
	TEXT("q3 probe length beyond rideHeight, cm."), ECVF_Default);
FAutoConsoleVariableRef CVarSpike9MaxSnapSpeed(TEXT("og.spike9.maxSnapSpeed"), GMaxSnapSpeed,
	TEXT("q3 clearance-servo clamp, cm/s."), ECVF_Default);
FAutoConsoleVariableRef CVarSpike9TerminalFall(TEXT("og.spike9.terminalFall"), GTerminalFall,
	TEXT("q3 airborne fall clamp, cm/s."), ECVF_Default);
FAutoConsoleVariableRef CVarSpike9PairSeparation(TEXT("og.spike9.pairSeparation"), GPairSeparation,
	TEXT("q6 start separation of the two capsules, cm."), ECVF_Default);
FAutoConsoleVariableRef CVarSpike9PairZOffset(TEXT("og.spike9.pairZOffset"), GPairZOffset,
	TEXT("q6(iii) RIDE-UP knob: B's start height relative to A, cm. 0 = level."), ECVF_Default);
FAutoConsoleVariableRef CVarSpike9DropCm(TEXT("og.spike9.dropCm"), GDropCm,
	TEXT("q3: lift the commanded position this far at og.spike9.dropStep so the servo has to catch a drop."), ECVF_Default);
FAutoConsoleVariableRef CVarSpike9DropStep(TEXT("og.spike9.dropStep"), GDropStep,
	TEXT("q3: probe step at which the drop is applied. 0 = no drop."), ECVF_Default);
FAutoConsoleVariableRef CVarSpike9LogEvery(TEXT("og.spike9.logEvery"), GLogEvery,
	TEXT("Emit one per-tick line every N probe steps."), ECVF_Default);
FAutoConsoleVariableRef CVarSpike9ResetOrigin(TEXT("og.spike9.reset"), GResetOrigin,
	TEXT("Set to 1 to forget the latched origin so the NEXT start re-latches from the local pawn."), ECVF_Default);
FAutoConsoleVariableRef CVarSpike9CaptureSource(TEXT("og.spike9.captureSource"), GCaptureSource,
	TEXT("[task 45] Which post-solve read step 6' adopts. 0 = captureBodyState() (production, PT GetX()); 1 = the rigid handle's GetP() (the SOLVED end-of-step position)."), ECVF_Default);

const TCHAR* peerTag(const UWorld* World)
{
	if (!World) return TEXT("???");
	switch (World->GetNetMode())
	{
		case NM_DedicatedServer: return TEXT("SRV-DED");
		case NM_ListenServer:    return TEXT("SRV-LSN");
		case NM_Client:          return TEXT("CLIENT");
		case NM_Standalone:      return TEXT("STANDALONE");
		default:                 return TEXT("???");
	}
}

FString v3(const FVector& V)
{
	return FString::Printf(TEXT("(%.6f,%.6f,%.6f)"), V.X, V.Y, V.Z);
}

// ---------------------------------------------------------------------------
// ⭐ THE FIX FOR THE DEFECT THAT VOIDED RUN 1.
//
// Half of this probe's per-tick numbers are ORIGIN-RELATIVE and half are WORLD
// space, and until now both printed as a bare `(x,y,z)`. A reader who cannot
// tell them apart reads `[Spike9.Tick] cmd=(0.000000,0.000000,10.000000)` as
// "the body is 10 cm above the WORLD origin" and concludes the capsule
// teleported 13 m - when in fact it was 10 cm above a latched origin at
// (1088.6, 784.9, 98.2) and the run was correct. That misreading is what a
// whole PIE session was thrown away over.
//
// From here on the frame is IN THE TEXT: world space prints `(x,y,z)`,
// origin-relative prints `rel(x,y,z)`, and the [Spike9.Init] banner says so.
// ---------------------------------------------------------------------------
FString v3rel(const FVector& V, const FVector& Origin)
{
	const FVector R = V - Origin;
	return FString::Printf(TEXT("rel(%.6f,%.6f,%.6f)"), R.X, R.Y, R.Z);
}

// ⭐ THE FIX FOR THE DEFECT THAT MADE q2 UNANSWERABLE. The first run had no
// client peer at all - PIE launched a single process - and nothing in any log
// said so. `clients=0` on the AUTHORITY is now on the [Spike9.Init],
// [Spike9.Start], [Spike9.Wait] and [Spike9.Stop] lines, so "there is no second
// peer" is visible before the user spends 35 minutes producing half a dataset.
// -1 means "no net driver" (standalone).
int32 clientConns(const UWorld* World)
{
	if (!World) return -1;
	const UNetDriver* Driver = World->GetNetDriver();
	return Driver ? Driver->ClientConnections.Num() : -1;
}

// ---------------------------------------------------------------------------
// [task 44] KNOB MARSHALLING.
//
// The cvars above are per PROCESS. The console that arms a run is now a CLIENT
// and the authority is a DEDICATED SERVER, so the values have to be captured
// into a struct, sent, and applied on the far side. These two functions are the
// only place that mapping exists — add a cvar, add it to BOTH.
// ---------------------------------------------------------------------------
FSpike9Knobs captureKnobs()
{
	FSpike9Knobs K;
	K.ResimPolicy    = GResimPolicy;
	K.Speed          = GSpeed;
	K.Duration       = GDuration;
	K.CorrectStep    = GCorrectStep;
	K.CorrectOffset  = GCorrectOffset;
	K.RideHeight     = GRideHeight;
	K.SnapDistance   = GSnapDistance;
	K.MaxSnapSpeed   = GMaxSnapSpeed;
	K.TerminalFall   = GTerminalFall;
	K.PairSeparation = GPairSeparation;
	K.PairZOffset    = GPairZOffset;
	K.DropCm         = GDropCm;
	K.DropStep       = GDropStep;
	K.LogEvery       = GLogEvery;
	K.CaptureSource  = GCaptureSource;   // [task 45]
	return K;
}

// ⚠ og.spike9.scenario is deliberately NOT in here. That cvar is the REQUEST on
// a client and the ARMED STATE on the authority; letting a replicated knob set
// write it would make a follower re-request its own run.
void applyKnobs(const FSpike9Knobs& K)
{
	GResimPolicy    = K.ResimPolicy;
	GSpeed          = K.Speed;
	GDuration       = K.Duration;
	GCorrectStep    = K.CorrectStep;
	GCorrectOffset  = K.CorrectOffset;
	GRideHeight     = K.RideHeight;
	GSnapDistance   = K.SnapDistance;
	GMaxSnapSpeed   = K.MaxSnapSpeed;
	GTerminalFall   = K.TerminalFall;
	GPairSeparation = K.PairSeparation;
	GPairZOffset    = K.PairZOffset;
	GDropCm         = K.DropCm;
	GDropStep       = K.DropStep;
	GLogEvery       = K.LogEvery;
	GCaptureSource  = K.CaptureSource;   // [task 45]
}

bool isValidScenario(int32 Scenario)
{
	return Scenario == kScenarioOff || Scenario == kScenarioWall
	    || Scenario == kScenarioHover || Scenario == kScenarioPair;
}

// The probe capsule is placed this far in front of the reference pawn. Was a
// bare 200.f literal at the single latch site; it is now read from two.
constexpr float kOriginAheadCm = 200.f;

// The AUTHORITY's own local controller, or nullptr. ⛔ NOT
// UWorld::GetFirstPlayerController(): on a DEDICATED server that returns a
// REMOTE client's controller, which is precisely the arbitrary referent this
// task exists to remove.
APlayerController* localController(UWorld* World)
{
	if (!World) return nullptr;
	for (auto It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (IsValid(PC) && PC->IsLocalController()) return PC;
	}
	return nullptr;
}
} // namespace

// ===========================================================================
// 1. THE PROBE CHARACTER (q2's subject)
// ===========================================================================

ASpike9ProbeCharacter::ASpike9ProbeCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	GetCapsuleComponent()->InitCapsuleSize(kCapsuleRadius, kCapsuleHalfHeight);

	// q2's configuration, applied as early as possible so no CMC tick ever runs.
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->PrimaryComponentTick.bCanEverTick        = false;
		CMC->PrimaryComponentTick.bStartWithTickEnabled = false;
		CMC->SetIsReplicated(false);
	}

	bReplicates = true;
	SetReplicateMovement(false);

	// No skeletal mesh asset is assigned: the probe is drawn with
	// DrawDebugCapsule instead, which is cheaper and unambiguous on screen.
}

void ASpike9ProbeCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->SetMovementMode(MOVE_None);
		CMC->SetComponentTickEnabled(false);
		CMC->StopMovementImmediately();
	}

	UE_LOG(LogOGSpike9, Warning,
		TEXT("[Warning][Spike9.Char] peer=%s idx=%d role=%d remoteRole=%d replicateMovement=%d cmcMode=%d cmcTick=%d ")
		TEXT("capsuleR=%.3f capsuleHH=%.3f pos=%s"),
		peerTag(GetWorld()), ProbeIndex, (int32)GetLocalRole(), (int32)GetRemoteRole(),
		IsReplicatingMovement() ? 1 : 0,
		GetCharacterMovement() ? (int32)GetCharacterMovement()->MovementMode : -1,
		GetCharacterMovement() ? (GetCharacterMovement()->IsComponentTickEnabled() ? 1 : 0) : -1,
		GetCapsuleComponent()->GetUnscaledCapsuleRadius(),
		GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(),
		*v3(GetActorLocation()));
}

void ASpike9ProbeCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASpike9ProbeCharacter, ProbeIndex);
	DOREPLIFETIME(ASpike9ProbeCharacter, ProbeScenario);
	DOREPLIFETIME(ASpike9ProbeCharacter, ScriptOrigin);
	DOREPLIFETIME(ASpike9ProbeCharacter, ScriptForward);
	DOREPLIFETIME(ASpike9ProbeCharacter, ProbeKnobs);
}

// ===========================================================================
// 1.5 THE REQUEST RELAY  [task 44]
//
// The whole client -> authority hop, in one small actor. It holds no state and
// replicates no property: it exists so that a Server RPC has an owning
// connection to travel on.
// ===========================================================================

ASpike9ProbeRelay::ASpike9ProbeRelay()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	// Owner-only relevancy, so a client holds exactly ONE relay — its own — and
	// "find the local relay" is unambiguous there. (An authority holds N, one
	// per remote client, and never looks one up by that route.)
	bOnlyRelevantToOwner = true;
	// It carries no replicated PROPERTY at all, so this governs an empty pass.
	SetNetUpdateFrequency(2.f);
}

void ASpike9ProbeRelay::ServerRequestScenario_Implementation(const FSpike9ScenarioRequest& Request)
{
	UWorld* World = GetWorld();
	if (!World) return;

	for (TActorIterator<ASpike9ProbeDirector> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			It->applyScenarioRequest(Request, GetOwner());
			return;
		}
	}

	UE_LOG(LogOGSpike9, Warning,
		TEXT("[Warning][Spike9.Request] peer=%s a scenario request ARRIVED but there is no ASpike9ProbeDirector ")
		TEXT("in this world — the server is not running the probe build."),
		peerTag(World));
}

// ===========================================================================
// 2. THE PHYSICS-THREAD TAP
// ===========================================================================

// Marshalled input/output mirror FSimulationInput2 / FSimulationState2 exactly
// (SimulationManagerUImpl.h:173-208) because that shape is proven in this
// project; the director pointer is set directly like setManager(), which
// TriggerRewindIfNeeded_Internal already relies on.
struct FSpike9Input : public Chaos::FSimCallbackInput
{
	virtual ~FSpike9Input() {}
	void Reset() { bInitialized = false; }
	bool bInitialized = false;
};

struct FSpike9Output : public Chaos::FSimCallbackOutput
{
	virtual ~FSpike9Output() {}
	void Reset() {}
};

class FSpike9AsyncCallback : public Chaos::TSimCallbackObject<
	FSpike9Input,
	FSpike9Output,
	Chaos::ESimCallbackOptions::Presimulate | Chaos::ESimCallbackOptions::PostSolve>
{
public:
	virtual FName GetFNameForStatId() const override
	{
		const static FLazyName StaticName("FSpike9AsyncCallback");
		return StaticName;
	}

	void setDirector(ASpike9ProbeDirector* InDirector) { Director = InDirector; }

private:
	virtual void OnPreSimulate_Internal() override
	{
		if (Director) Director->PreSimulate_PT();
	}
	virtual void OnPostSolve_Internal() override
	{
		if (Director) Director->PostSolve_PT();
	}

	ASpike9ProbeDirector* Director = nullptr;
};

// ===========================================================================
// 3. PROBE STATE
// ===========================================================================

struct FSpike9Body
{
	ASpike9ProbeCharacter* Character = nullptr;
	int32     Index    = 0;
	BodyId    Body;
	QueryVolumeId WorldVolume;      // searchCategories = worldOnly     (q3/q4)
	QueryVolumeId CharacterVolume;  // searchCategories = character     (q6 iii)
	bool      VolumesRegistered = false;

	// --- script state (physics thread only, after arming) -------------------
	FVector Origin  = FVector::ZeroVector;
	FVector Forward = FVector(1.f, 0.f, 0.f);
	FVector Dir     = FVector(1.f, 0.f, 0.f);   // travel direction (B walks backwards in q6)

	FVector CommandedPos = FVector::ZeroVector;  // State.positionCmd
	FVector CommandedVel = FVector::ZeroVector;  // State.velocityCmd
	FVector SimPos       = FVector::ZeroVector;  // the sim's OWN position (rev 6: the sim owns it)
	FVector SimVel       = FVector::ZeroVector;  // the sim's OWN velocity (rev 6: never read back)

	// --- per-step measurements ----------------------------------------------
	FVector LastPushOut  = FVector::ZeroVector;
	FVector LastCapture  = FVector::ZeroVector;   // the one step 6' ADOPTS (see GCaptureSource)
	FVector LastCaptureV = FVector::ZeroVector;
	// ⭐ [task 45] THE TWO POST-SOLVE READS, KEPT SEPARATE.
	// `LastCaptureX` is `captureBodyState().position`, i.e. the PT `GetX()` the
	// PRODUCTION adapter returns. `LastCaptureP` is the rigid handle's `GetP()`.
	// At OnPostSolve_Internal time those are DIFFERENT quantities — X is still
	// start-of-step, P is the solved end-of-step position — and conflating them
	// is the whole of task 45. `LastCapture` is whichever one the run adopted.
	FVector LastCaptureX = FVector::ZeroVector;
	FVector LastCaptureP = FVector::ZeroVector;
	bool    HaveCaptureP = false;
	// The value the probe's OWN bookkeeping advanced SimPos to at the end of the
	// previous PreSimulate (x += v*dt), recorded BEFORE step 6' overwrites it.
	FVector SimPosPredicted = FVector::ZeroVector;
	float   LastClearance = -1.f;
	float   Mass          = 0.f;

	// --- aggregates for the [Spike9.Stop] summary ---------------------------
	float   MaxPushOut        = 0.f;
	float   MaxPushOutZ       = 0.f;
	int32   PushOutTicks      = 0;
	int32   CorrectionStep    = -1;
	int32   ConvergedAtStep   = -1;
	// ⚠ -1 means THE SERVO NEVER RAN. It used to initialise to 0, which is
	// also the value a PERFECT servo reports - so the first run's
	// `worstClearanceErr=0.000000` was read as a success when the `bFloor`
	// branch had in fact never once been entered. A "never ran" that is
	// indistinguishable from "ran perfectly" is a reporting defect; the
	// companion `floorTicks`/`airborneTicks`/`servoRan` fields on [Spike9.Stop]
	// make the distinction structural rather than a convention about zero.
	float   WorstClearanceErr = -1.f;
	int32   FloorTicks        = 0;    // steps the clearance servo actually ran
	int32   AirborneTicks     = 0;    // steps that took the authored-gravity branch
	FVector StartXY           = FVector::ZeroVector;
	float   MaxLateralCreep   = 0.f;
	int32   SweepHitTicks     = 0;
	int32   SweepNormalTicks  = 0;    // sweeps that returned a non-zero normal (q4's answer)
	int32   StartPenTicks     = 0;
	int32   PenDepthTicks     = 0;    // startPenetrating hits that carried a non-zero depth
	// ⭐ Steps whose capture landed EXACTLY on the command, i.e. the solve did
	// not integrate v*dt at all. On such a step the "push-out" is identically
	// -cmdV*dt and is NOT contact - which is precisely how run 1's
	// pushOutTicks=299/300 came to read as sustained floor contact while
	// sweepHitTicks=0. See the discriminator in PreSimulate_PT.
	int32   NoIntegrateTicks  = 0;
	// ⭐ [task 45] THE DISCRIMINATOR `noIntegrateTicks` WAS BELIEVED TO BE.
	// `noIntegrateTicks` counts "the ADOPTED capture came back on the command".
	// This counts "the SOLVED position `GetP()` came back on the command", which
	// is the only one of the two that means "the engine did not move the body".
	// If noIntegrateTicks == steps and noIntegratePTicks == 0, the engine
	// integrated perfectly and the probe read the wrong field.
	int32   NoIntegratePTicks = 0;
	int32   CaptureFrameTicks = 0;    // ticks on which GetP() was readable at all
	float   MaxXPDelta        = 0.f;  // worst |GetP() - GetX()| seen at post-solve
};

struct FSpike9Impl
{
	// Owned adapters. Deliberately the probe's OWN instances: the shipped ones
	// (SimulationManagerUImpl.cpp:542/678) map only categories 0..3, so `world`
	// and `character` resolve to ECollisionChannel(0) there — see
	// research_spike_9.md finding F1.
	std::optional<ChaosPhysicsBodyAdapter>  BodyAdapter;
	std::optional<ChaosSpatialQueryAdapter> QueryAdapter;
	Chaos::FPBDRigidsSolver*                Solver   = nullptr;
	FSpike9AsyncCallback*                   Callback = nullptr;

	std::vector<FSpike9Body> Bodies;

	// GT -> PT handshake. Written on the game thread, read on the physics
	// thread; release/acquire, and nothing in Bodies is mutated on GT while it
	// is true.
	std::atomic<bool> Driving{false};

	int32 ActiveScenario = kScenarioOff;
	int32 Step           = 0;
	float Dt             = 1.f / 60.f;
	int32 LastChaosFrame = -1;
	bool  WasResim       = false;
	int32 ResimSteps     = 0;
	int32 ResimEpisodes  = 0;

	// Cached on the GAME thread so the physics-thread script never touches
	// UObject state (AActor::GetWorld() walks the outer chain).
	const TCHAR* PeerTag  = TEXT("???");
	float        GravityZ = -980.f;

	// The origin is LATCHED for the whole session so that two runs of the same
	// scenario are byte-comparable — that is literally what q1(ii) asks for.
	bool    HasLatchedOrigin = false;
	FVector LatchedOrigin    = FVector::ZeroVector;
	FVector LatchedForward   = FVector(1.f, 0.f, 0.f);

	// ⭐ Sticky. Set if the origin a body is DRIVEN from ever disagrees with the
	// origin the run LATCHED, and then repeated on every [Spike9.Stop] line, so a
	// divergence cannot be missed by scrolling past one line.
	bool    OriginDiverged   = false;

	// Throttle for [Spike9.Wait]: one line per gate per ~60 game ticks (~1 s).
	int32   WaitLogCounter   = 0;

	// ---------------- [task 44] the request channel -------------------------
	// AUTHORITY: the origin carried by the most recent client request. Kept
	// separate from LatchedOrigin so that "a request arrived" and "an origin is
	// latched for this session" stay two distinct facts — the determinism PAIR
	// in RUN 1 is a second request that must NOT re-latch.
	bool    HasRequestedOrigin = false;
	FVector RequestedOrigin    = FVector::ZeroVector;
	FVector RequestedForward   = FVector(1.f, 0.f, 0.f);

	// CLIENT: the last scenario value actually sent to the authority. The cvar
	// is sent on a CHANGE, so this is what makes it a request and not a
	// per-tick flood.
	int32   LastRequestedScenario = kScenarioOff;
	// CLIENT: `og.spike9.reset 1` is typed as its own console line, so it is
	// held here until the next scenario request carries it across.
	bool    PendingResetRequest   = false;

	int32   RelayMaintainCounter  = 0;
	int32   RequestLogCounter     = 0;

	// GAME-THREAD control sweep tally (scenario 3). Kept on the Impl, not on
	// FSpike9Body, so the physics thread and the game thread never write to the
	// same struct.
	int32   GtSweepSamples   = 0;
	int32   GtSweepHits      = 0;

	// Set when a scenario is stopped, so the characters are destroyed one game
	// tick AFTER the physics thread has been disarmed.
	int32 RootLogCounter  = 0;
	bool  PendingTeardown = false;
	int32 TeardownDelay   = 0;
	int32 SpawnedScenario = kScenarioOff;

	// PHYSICS THREAD. A body whose particle has gone (actor destroyed, or a
	// client's replicated copy removed) must be SKIPPED, not passed to the
	// adapter: ChaosPhysicsBodyAdapter fires ensureAlwaysMsgf on an unresolved
	// BodyId, and such an ensure in the log would read exactly like q2 evidence.
	bool proxyAlive(BodyId Id) const
	{
		return Solver && Solver->GetParticleProxy_PT(Chaos::FUniqueIdx{ (int32)Id.value }) != nullptr;
	}

	// ⭐⭐ [task45-capture] THE SOLVED END-OF-STEP POSITION, READ IN THE PROBE.
	//
	// `ChaosPhysicsBodyAdapter::captureBodyState()` reads `ptApi->GetX()`. In
	// UE 5.6 `FPBDRigidsEvolutionGBF::AdvanceOneTimeStepImpl` the order is:
	//
	//     Integrate(Dt)            -> SetTransformPQCom(XCom + V*Dt, ...), SetV(V)
	//                                 ** writes P/Q and V; leaves X/R alone **
	//     ... constraint solve ...
	//     PostSolveCallback(Dt)    -> FSpike9AsyncCallback::OnPostSolve_Internal
	//     ParticleUpdatePosition() -> Particle.SetX(Particle.GetP())
	//                                 ** the X <- P commit, AFTER the callback **
	//
	// So at OnPostSolve_Internal time `GetX()` is still the START-of-step
	// position — literally what setBodyTransform() wrote in OnPreSimulate — and
	// `GetP()` is the position the step actually produced. Reading X there is
	// structurally incapable of observing the step it is meant to capture.
	//
	// ⛔ This helper exists so the PROBE can read P without touching the shipped
	// adapter. Whether the shipped `captureBodyState()` has the same problem is
	// NOT this task's to decide or fix — see impl_notes_seam_45.md §7.
	//
	// `GetHandle_LowLevel()` is the public escape hatch on
	// FSingleParticlePhysicsProxy; `FRigidBodyHandle_Internal` exposes no GetP().
	bool readSolvedPT(BodyId Id, FVector& OutP) const
	{
		if (!Solver) return false;
		auto* Proxy = Solver->GetParticleProxy_PT(Chaos::FUniqueIdx{ (int32)Id.value });
		if (!Proxy) return false;
		auto* Handle = Proxy->GetHandle_LowLevel();
		if (!Handle) return false;
		auto* Rigid = Handle->CastToRigidParticle();
		if (!Rigid) return false;
		OutP = FVector(Rigid->GetP());
		return true;
	}
};

// ===========================================================================
// 4. THE DIRECTOR
// ===========================================================================

ASpike9ProbeDirector::ASpike9ProbeDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup    = ETickingGroup::TG_PrePhysics;
	bReplicates = false;
}

void ASpike9ProbeDirector::BeginPlay()
{
	Super::BeginPlay();

	Impl = new FSpike9Impl();

	UWorld* World = GetWorld();
	FPhysScene* PhysScene = World ? World->GetPhysicsScene() : nullptr;
	Chaos::FPhysicsSolver* Solver = PhysScene ? PhysScene->GetSolver() : nullptr;
	if (!Solver)
	{
		UE_LOG(LogOGSpike9, Warning, TEXT("[Warning][Spike9.Init] peer=%s NO SOLVER — probe unavailable this session."),
			peerTag(World));
		return;
	}

	Impl->Solver   = &Solver->CastChecked();
	Impl->Dt       = Solver->GetAsyncDeltaTime() > 0.f ? Solver->GetAsyncDeltaTime() : (1.f / 60.f);
	Impl->PeerTag  = peerTag(World);
	Impl->GravityZ = World->GetGravityZ();

	// ⭐ The probe's OWN category mapping — it is what makes `world` and
	// `character` mean anything at all (the shipped adapter maps neither).
	// `character -> ECC_GameTraceChannel6` is user ruling #6, closed 2026-09-04.
	Impl->QueryAdapter.emplace(World, std::initializer_list<ChaosCategoryMapping>{
		{ collisionCategory::body,         ECollisionChannel::ECC_GameTraceChannel2 },
		{ collisionCategory::guard,        ECollisionChannel::ECC_GameTraceChannel3 },
		{ collisionCategory::queryRouting, ECollisionChannel::ECC_GameTraceChannel4 },
		{ collisionCategory::projectile,   ECollisionChannel::ECC_GameTraceChannel5 },
		{ collisionCategory::world,        ECollisionChannel::ECC_WorldStatic       },
		{ collisionCategory::character,    ECollisionChannel::ECC_GameTraceChannel6 }
	});
	Impl->BodyAdapter.emplace(*Impl->Solver);

	Impl->Callback = Solver->CreateAndRegisterSimCallbackObject_External<FSpike9AsyncCallback>();
	Impl->Callback->setDirector(this);

	UE_LOG(LogOGSpike9, Warning,
		TEXT("[Warning][Spike9.Init] peer=%s READY dt=%.6f gravityZ=%.3f clients=%d — dormant until a scenario is requested. ")
		TEXT("TYPE `og.spike9.scenario N` IN A CLIENT WINDOW: the cvar is a request that is relayed to the authority ")
		TEXT("(look for [Spike9.Request] SENT on the client and RECEIVED on the server). ")
		TEXT("LEGEND: a bare (x,y,z) is WORLD space; rel(x,y,z) is relative to that body's own origin. ")
		TEXT("On the SERVER, clients=0 means no client has joined yet and nothing can be requested."),
		peerTag(World), Impl->Dt, World ? World->GetGravityZ() : 0.f, clientConns(World));
}

void ASpike9ProbeDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Impl)
	{
		Impl->Driving.store(false, std::memory_order_release);
		if (Impl->Callback)
		{
			Impl->Callback->setDirector(nullptr);
			if (UWorld* World = GetWorld())
				if (FPhysScene* PhysScene = World->GetPhysicsScene())
					if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
						Solver->UnregisterAndFreeSimCallbackObject_External(Impl->Callback);
			Impl->Callback = nullptr;
		}
		delete Impl;
		Impl = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------
// 4.1 GAME THREAD: spawn, stand up bodies, arm/disarm, draw.
// ---------------------------------------------------------------------------

void ASpike9ProbeDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Impl || !Impl->Solver) return;

	UWorld* World = GetWorld();
	if (!World) return;

	const bool bAuthority = (World->GetNetMode() != NM_Client);

	// =======================================================================
	// ⭐⭐ [task 44] THE TRIGGER — AND WHY THIS PROBE NO LONGER NEEDS A LISTEN
	//     SERVER.
	//
	// The old flow needed the AUTHORITY to have both a console and a local pawn,
	// which is only true on a listen server or in standalone. That put an
	// untested network configuration underneath a physics measurement. The new
	// flow runs the project's NORMAL configuration (dedicated server + N
	// clients) and moves the console to where the user already is:
	//
	//   CLIENT window:  og.spike9.scenario 3
	//     -> the cvar is a REQUEST, not a command
	//     -> ASpike9ProbeRelay::ServerRequestScenario — a Server RPC on an actor
	//        the server spawned and OWNED BY THIS CLIENT'S PlayerController,
	//        which is what makes the RPC routable at all
	//     -> the authority adopts the knobs, latches the origin from the pawn
	//        transform the request carried, and arms the scenario
	//     -> ProbeScenario / ScriptOrigin / ProbeKnobs replicate back out and
	//        every peer, the requester included, follows exactly as before.
	//
	// Nothing outside Spike9Probe.{h,cpp} is touched: the relay is spawned by
	// this director, owned by a PlayerController that already exists, and reaped
	// when that controller goes.
	// =======================================================================
	if (bAuthority && World->GetNetMode() != NM_Standalone)
	{
		maintainRequestRelays(*World);
	}

	// `og.spike9.reset 1` is its own console line, typed one or more frames
	// before the scenario command. On a CLIENT it cannot be consumed locally —
	// there is no latch here to clear — so it is held and travels with the next
	// request.
	if (GResetOrigin != 0)
	{
		GResetOrigin = 0;
		if (bAuthority)
		{
			Impl->HasLatchedOrigin = false;
			UE_LOG(LogOGSpike9, Warning, TEXT("[Warning][Spike9.Reset] peer=%s origin latch cleared."), peerTag(World));
		}
		else
		{
			Impl->PendingResetRequest = true;
			UE_LOG(LogOGSpike9, Warning,
				TEXT("[Warning][Spike9.Reset] peer=%s reset ARMED — it travels with the next `og.spike9.scenario` request."),
				peerTag(World));
		}
	}

	if (!bAuthority)
	{
		sendScenarioRequestIfChanged(*World);
	}

	// ⭐ ONE COMMAND, NOT TWO. A client derives the running scenario from the
	// replicated probe characters rather than from its own cvar, so a run can
	// never be silently half-armed. ⭐ [task 44] It now also adopts the
	// AUTHORITY's knob set, which is what makes q1(ii)'s server-vs-client diff a
	// comparison of two runs of the SAME script — before this, a knob typed on
	// one peer was simply never seen by the other.
	int32 EffScenario = GScenario;
	if (!bAuthority)
	{
		EffScenario = kScenarioOff;
		for (TActorIterator<ASpike9ProbeCharacter> It(World); It; ++It)
			if (IsValid(*It) && It->ProbeScenario != kScenarioOff)
			{
				EffScenario = It->ProbeScenario;
				applyKnobs(It->ProbeKnobs);
				break;
			}
	}

	// ⭐ NO SILENT WAITING - the second half of the run-1 post-mortem.
	//
	// The client log from run 1 contained exactly ONE probe line, the
	// [Spike9.Init] banner, and nothing else. That single symptom is consistent
	// with at least five different causes (no client process; the actor not
	// replicated; the property not replicated; the body never became resolvable;
	// the subsystem never ticked) and the log could not tell them apart. Every
	// gate between "the user typed the command" and "[Spike9.Start]" now names
	// ITSELF, once a second, on BOTH peers, and carries the client census. A
	// second void run cannot end in silence.
	auto LogWait = [&](const TCHAR* Reason, int32 FoundChars)
	{
		if (Impl->WaitLogCounter++ % 60 != 0) return;
		UE_LOG(LogOGSpike9, Warning,
			TEXT("[Warning][Spike9.Wait] peer=%s reason=%s authority=%d localCvar=%d effScenario=%d ")
			TEXT("foundChars=%d wantedChars=%d clients=%d ")
			TEXT("(a CLIENT ignores its own og.spike9.scenario and follows the replicated ProbeScenario)"),
			peerTag(World), Reason, bAuthority ? 1 : 0, GScenario, EffScenario,
			FoundChars, (EffScenario == kScenarioPair) ? 2 : 1, clientConns(World));
	};

	// ---------------- STOP ---------------------------------------------------
	if (EffScenario == kScenarioOff && Impl->ActiveScenario != kScenarioOff)
	{
		Impl->Driving.store(false, std::memory_order_release);
		for (const FSpike9Body& B : Impl->Bodies)
		{
			UE_LOG(LogOGSpike9, Warning,
				TEXT("[Warning][Spike9.Stop] q=%d peer=%s idx=%d steps=%d resimSteps=%d resimEpisodes=%d clients=%d ")
				TEXT("originDiverged=%d maxPushOut=%.6f maxPushOutZ=%.6f pushOutTicks=%d noIntegrateTicks=%d ")
				// ⭐⭐ [task 45] READ THESE FOUR TOGETHER OR NOT AT ALL.
				// `noIntegrateTicks` == "the ADOPTED capture came back on the
				// command"; `noIntegrateP` == "the SOLVED position GetP() came
				// back on the command". Only the second means the engine did not
				// move the body. `maxXPDelta` is the worst |GetP() - GetX()| at
				// post-solve: a NON-ZERO value proves the two reads are different
				// quantities. `captureSource` says which one this run adopted.
				TEXT("noIntegrateP=%d maxXPDelta=%.6f capturePTicks=%d captureSource=%d ")
				TEXT("correctionStep=%d convergedAtStep=%d ")
				TEXT("servoRan=%d floorTicks=%d airborneTicks=%d worstClearanceErr=%.6f maxLateralCreep=%.6f ")
				TEXT("sweepHitTicks=%d sweepNormalTicks=%d gtSweepSamples=%d gtSweepHits=%d ")
				TEXT("startPenTicks=%d penDepthTicks=%d mass=%.4f"),
				Impl->ActiveScenario, peerTag(World), B.Index, Impl->Step, Impl->ResimSteps, Impl->ResimEpisodes,
				clientConns(World), Impl->OriginDiverged ? 1 : 0,
				B.MaxPushOut, B.MaxPushOutZ, B.PushOutTicks, B.NoIntegrateTicks,
				B.NoIntegratePTicks, B.MaxXPDelta, B.CaptureFrameTicks, GCaptureSource,
				B.CorrectionStep, B.ConvergedAtStep,
				(B.FloorTicks > 0) ? 1 : 0, B.FloorTicks, B.AirborneTicks,
				B.WorstClearanceErr, B.MaxLateralCreep, B.SweepHitTicks, B.SweepNormalTicks,
				Impl->GtSweepSamples, Impl->GtSweepHits,
				B.StartPenTicks, B.PenDepthTicks, B.Mass);
		}
		Impl->ActiveScenario  = kScenarioOff;
		// ⭐ [task 44] RE-ARM. The authority ends a run by writing its OWN
		// og.spike9.scenario back to 0 (the duration auto-stop). That does not
		// touch a client's cvar, so without this the user's next
		// `og.spike9.scenario 1` would be 1 -> 1, no edge, and NO REQUEST WOULD
		// BE SENT — and RUN 1's determinism PAIR is exactly that second command.
		// LastRequestedScenario is cleared in the same breath so this write is
		// not itself mistaken for the user typing 0.
		if (!bAuthority)
		{
			GScenario                   = kScenarioOff;
			Impl->LastRequestedScenario = kScenarioOff;
			UE_LOG(LogOGSpike9, Warning,
				TEXT("[Warning][Spike9.Rearm] peer=%s the run ended on the authority; og.spike9.scenario is back to 0 here, ")
				TEXT("so retyping the same scenario number starts a NEW run."),
				peerTag(World));
		}
		Impl->PendingTeardown = true;
		// Let the physics thread observe the disarm before the particles go.
		Impl->TeardownDelay   = 5;
		return;
	}

	// ---------------- TEARDOWN (one tick after disarming) --------------------
	if (Impl->PendingTeardown)
	{
		if (Impl->TeardownDelay-- > 0) return;
		if (bAuthority)
		{
			for (const FSpike9Body& B : Impl->Bodies)
				if (IsValid(B.Character)) B.Character->Destroy();
		}
		Impl->Bodies.clear();
		Impl->Step            = 0;
		Impl->ResimSteps      = 0;
		Impl->ResimEpisodes   = 0;
		Impl->SpawnedScenario = kScenarioOff;
		Impl->PendingTeardown = false;
		Impl->WaitLogCounter  = 0;
		Impl->GtSweepSamples  = 0;
		Impl->GtSweepHits     = 0;
		// ⚠ OriginDiverged is deliberately NOT reset: it is a defect flag for the
		// whole session, not for one run.
		return;
	}

	// ---------------- ALREADY RUNNING ---------------------------------------
	if (Impl->Driving.load(std::memory_order_acquire))
	{
		// Auto-stop. Only the AUTHORITY writes the cvar; a client stops when the
		// characters it is following go away, which is the same edge.
		if (GDuration > 0 && Impl->Step >= GDuration && bAuthority)
		{
			GScenario = kScenarioOff;
			return;
		}
		// On-screen feedback: commanded (green) versus captured (red).
		//
		// ⭐⭐ [task 45] THE THIRD CAPSULE IS THE POINT. Green (command) and red
		// (the ADOPTED capture) coincided in runs 19:56 and 20:11, which reads as
		// "the body never moves". BLUE is the rigid handle's `GetP()` — the
		// position the step actually SOLVED to — and it is drawn LAST so it wins
		// the overdraw. If blue sits a servo-step BELOW the coincident green/red
		// pair, the engine is integrating and the probe is reading the wrong
		// field. If all three coincide, the engine really is not moving the body.
		// One glance replaces a log grep; the numbers are still on [Spike9.Tick].
		for (const FSpike9Body& B : Impl->Bodies)
		{
			DrawDebugCapsule(World, B.CommandedPos, kCapsuleHalfHeight, kCapsuleRadius,
			                 FQuat::Identity, FColor::Green, false, -1.f, 0, 1.f);
			DrawDebugCapsule(World, B.LastCapture, kCapsuleHalfHeight, kCapsuleRadius,
			                 FQuat::Identity, FColor::Red, false, -1.f, 0, 1.f);
			if (B.HaveCaptureP)
				DrawDebugCapsule(World, B.LastCaptureP, kCapsuleHalfHeight, kCapsuleRadius,
				                 FQuat::Identity, FColor::Blue, false, -1.f, 0, 2.f);
		}

		// ⭐ q2's SECOND HALF, and it can only be measured HERE. Every other probe
		// line reads the PHYSICS-THREAD particle. This one reads the GAME-THREAD
		// root component of the ACharacter and compares the two: that difference
		// IS "does the root component follow the physics-thread body". It is taken
		// on BOTH peers, which is the rest of what q2 asks.
		//
		// ⚠ A few cm of lag is EXPECTED and is not a failure — the game thread
		// reads an interpolated/one-frame-behind transform, and this sample is not
		// synchronised with the physics step. A LARGE or GROWING delta, or a root
		// that does not move at all, is the failure.
		++Impl->RootLogCounter;
		if (GLogEvery > 0 && (Impl->RootLogCounter % FMath::Max(1, GLogEvery * 5)) == 0)
		{
			for (const FSpike9Body& B : Impl->Bodies)
			{
				if (!IsValid(B.Character)) continue;
				const FVector RootPos = B.Character->GetActorLocation();
				const FString RootCapP = B.HaveCaptureP
					? v3rel(B.LastCaptureP, B.Origin) : FString(TEXT("rel(?,?,?)"));
				UE_LOG(LogOGSpike9, Warning,
					TEXT("[Warning][Spike9.Root] q=%d peer=%s idx=%d gtSample=%d step=%d ")
					// ⭐ [task 45] `ptBodyP` is the SOLVED position; `ptBody` is the
					// adopted capture. |root-bodyP| is the number that says whether
					// the GAME-THREAD root agrees with the SOLVE (it should) rather
					// than with the probe's read (it need not).
					TEXT("rootWorld=%s rootActor=%s ptBody=%s ptBodyP=%s |root-body|=%.6f ")
					TEXT("|root-bodyP|=%.6f rootRot=%s"),
					Impl->ActiveScenario, peerTag(World), B.Index, Impl->RootLogCounter, Impl->Step,
					*v3(RootPos), *v3rel(RootPos, B.Origin), *v3rel(B.LastCapture, B.Origin),
					*RootCapP, (float)(RootPos - B.LastCapture).Size(),
					B.HaveCaptureP ? (float)(RootPos - B.LastCaptureP).Size() : -1.f,
					*B.Character->GetActorRotation().ToCompactString());

				// ⭐ GAME-THREAD CONTROL FOR [Spike9.Sweep], and it is the difference
				// between a q4 ANSWER and a broken instrument.
				//
				// Run 1 reported sweepHitTicks=0 for all 300 steps while the capsule
				// sat 10 cm above a floor that is unambiguously there. That is either
				// "the physics-thread probe sweep genuinely cannot see static world
				// geometry" (a first-class q4/q3 finding) or "the probe asked the
				// wrong question". This line asks the SAME question - same capsule,
				// same pose, same length, same WorldStatic object type - through the
				// plain engine API on the GAME thread, touching no adapter code and
				// no physics-thread path.
				//
				//   gtBlocked=1 while [Spike9.Sweep] says blocked=0  -> the geometry is
				//     there and the PHYSICS-THREAD query path is what loses it.
				//   gtBlocked=0 as well                              -> there is nothing
				//     under the capsule and the hover scenario was set up on thin air.
				//
				// Deliberately NOT routed through ChaosSpatialQueryAdapter: its
				// game-thread hit filter dereferences a hit's proxy without a null
				// check, and a crash here would cost another whole session.
				if (Impl->ActiveScenario == kScenarioHover && B.Index == 0)
				{
					const float  GtLen = GRideHeight + GSnapDistance;
					const FVector GtStart = B.CommandedPos;
					const FVector GtEnd   = GtStart - FVector(0.f, 0.f, GtLen);

					FCollisionQueryParams GtParams;
					GtParams.bTraceComplex = false;
					GtParams.AddIgnoredActor(B.Character);

					FHitResult GtHit;
					const bool bGtBlocked = World->SweepSingleByObjectType(
						GtHit, GtStart, GtEnd, FQuat::Identity,
						FCollisionObjectQueryParams(ECollisionChannel::ECC_WorldStatic),
						FCollisionShape::MakeCapsule(kCapsuleRadius, kCapsuleHalfHeight),
						GtParams);

					++Impl->GtSweepSamples;
					if (bGtBlocked) ++Impl->GtSweepHits;

					UE_LOG(LogOGSpike9, Warning,
						TEXT("[Warning][Spike9.GTSweep] q=4 peer=%s idx=%d step=%d GAME-THREAD CONTROL ")
						TEXT("gtBlocked=%d gtFrac=%.6f gtClearance=%.6f gtN=%s |gtN|=%.6f gtImpactWorld=%s ")
						TEXT("gtStartPen=%d gtPenDepth=%.6f sweepLen=%.3f fromWorld=%s hitActor=%s"),
						peerTag(World), B.Index, Impl->Step,
						bGtBlocked ? 1 : 0, (float)GtHit.Time,
						bGtBlocked ? (float)GtHit.Time * GtLen : -1.f,
						*v3(GtHit.ImpactNormal), (float)GtHit.ImpactNormal.Size(),
						*v3(GtHit.ImpactPoint), GtHit.bStartPenetrating ? 1 : 0,
						(float)GtHit.PenetrationDepth, GtLen, *v3(GtStart),
						*GetNameSafe(GtHit.GetActor()));
				}
			}
		}
		return;
	}

	if (EffScenario == kScenarioOff) return;

	// ---------------- START: spawn (authority only) --------------------------
	const int32 WantedBodies = (EffScenario == kScenarioPair) ? 2 : 1;

	if (bAuthority && Impl->SpawnedScenario != GScenario)
	{
		if (!Impl->HasLatchedOrigin)
		{
			// ⭐⭐ [task 44] THE ORIGIN RULE — the second half of what this task
			// exists to fix.
			//
			// ⛔ WHAT WAS DELETED, AND WHY. The old code asked
			// `GetFirstPlayerController()->GetPawn()` and then, if that was null,
			// took ANY non-probe pawn in the world. On a DEDICATED server both
			// branches are wrong: the first returns a REMOTE client's controller
			// (the list is not local-only), and the second silently picks an
			// arbitrary player out of N. Either way the capsule lands in front of
			// someone nobody chose, and no log line would have said so.
			//
			// THE RULE, in order, and there is no third case:
			//   1. the REQUESTING CLIENT's own pawn transform, carried in the RPC.
			//      That is the frame the user is looking at as they type, which is
			//      the entire affordance ("walk up to a wall, then arm it"). It is
			//      a client-side transform, and that is not a defect: the origin is
			//      a reference FRAME, it is latched exactly ONCE per session, and
			//      it is replicated straight back out on ScriptOrigin, so every
			//      peer drives from identical numbers — which the [Spike9.DEFECT]
			//      guard below then re-checks per body, per run.
			//   2. the AUTHORITY's own LOCAL controller's pawn — standalone PIE,
			//      and a listen server if anyone ever runs one again. Unchanged.
			//   3. refuse, by name, in the log. Guessing is what produced the
			//      fallback deleted above.
			const TCHAR* OriginSource = TEXT("none");
			if (Impl->HasRequestedOrigin)
			{
				Impl->LatchedOrigin  = Impl->RequestedOrigin;
				Impl->LatchedForward = Impl->RequestedForward;
				OriginSource         = TEXT("requesting-client-pawn");
			}
			else if (APlayerController* PC = localController(World))
			{
				APawn* RefPawn = PC->GetPawn();
				if (!RefPawn)
				{
					LogWait(TEXT("NO_PAWN__possess_a_character_then_retype_the_command"), 0);
					return;
				}
				Impl->LatchedOrigin  = RefPawn->GetActorLocation() + RefPawn->GetActorForwardVector() * kOriginAheadCm;
				Impl->LatchedForward = RefPawn->GetActorForwardVector().GetSafeNormal2D();
				OriginSource         = TEXT("authority-local-pawn");
			}
			else
			{
				LogWait(TEXT("NO_ORIGIN__no_client_request_and_no_local_pawn__type_og.spike9.scenario_in_a_CLIENT_window"), 0);
				return;
			}
			Impl->HasLatchedOrigin = true;
			UE_LOG(LogOGSpike9, Warning,
				TEXT("[Warning][Spike9.Latch] peer=%s source=%s origin=%s forward=%s (latched; `og.spike9.reset 1` to re-latch)"),
				peerTag(World), OriginSource, *v3(Impl->LatchedOrigin), *v3(Impl->LatchedForward));
		}

		for (int32 i = 0; i < WantedBodies; ++i)
		{
			FVector SpawnPos = Impl->LatchedOrigin;
			if (i == 1)
				SpawnPos += Impl->LatchedForward * GPairSeparation + FVector(0.f, 0.f, GPairZOffset);

			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ASpike9ProbeCharacter* Ch = World->SpawnActor<ASpike9ProbeCharacter>(
				ASpike9ProbeCharacter::StaticClass(), SpawnPos, FRotator::ZeroRotator, Params);
			if (!Ch)
			{
				UE_LOG(LogOGSpike9, Warning, TEXT("[Warning][Spike9.Start] peer=%s SpawnActor FAILED for idx=%d"),
					peerTag(World), i);
				return;
			}
			Ch->ProbeIndex    = i;
			Ch->ProbeScenario = GScenario;
			Ch->ScriptOrigin  = Impl->LatchedOrigin;
			Ch->ScriptForward = Impl->LatchedForward;
			// [task 44] The script every FOLLOWING peer must run. Without this a
			// client would follow with its own (default) knobs.
			Ch->ProbeKnobs    = captureKnobs();
		}
		Impl->SpawnedScenario = GScenario;
		return;   // let replication and body init settle for a tick
	}

	// ---------------- START: collect characters (both peers) -----------------
	std::vector<ASpike9ProbeCharacter*> Found;
	for (TActorIterator<ASpike9ProbeCharacter> It(World); It; ++It)
		if (IsValid(*It) && It->ProbeScenario != kScenarioOff) Found.push_back(*It);

	if ((int32)Found.size() < WantedBodies)
	{
		// On a CLIENT this is the state that produced run 1's one-line log.
		LogWait(TEXT("WAIT_REPLICATED_CHARACTERS"), (int32)Found.size());
		return;
	}

	// ---------------- START: stand the bodies up ----------------------------
	if (Impl->Bodies.empty())
	{
		PhysicalObjectDescriptor Descriptor{
			BodyDescriptor{
				/*simulatePhysics*/ true,
				/*enableGravity  */ false,      // rev-6 ruling #16(a): gravity is AUTHORED by the sim
				/*isRoot         */ true,       // adopt the ACharacter's own capsule — q2
				/*lockRotation   */ true,
				(GResimPolicy == 1) ? BodyResimPolicy::Resimulate : BodyResimPolicy::ReplayRecordedHistory
			},
			{ ShapeDescriptor{
				CapsuleGeometry{ kCapsuleRadius, kCapsuleHalfHeight },
				CollisionCategories::single(collisionCategory::character),
				collisionCategory::worldAndCharacter          // ruling #5: BLOCK world AND other characters
			} }
		};

		bool bAllReady = true;
		for (ASpike9ProbeCharacter* Ch : Found)
		{
			UCapsuleComponent* Capsule = Ch->GetCapsuleComponent();
			FBodyInstance* BI = Capsule ? Capsule->GetBodyInstance() : nullptr;
			if (!Capsule || !Capsule->IsRegistered() || !BI || !BI->IsValidBodyInstance())
				{ bAllReady = false; continue; }
			if (Capsule->GetBodyInstanceAsyncPhysicsTickHandle().Proxy == nullptr)
				{ bAllReady = false; continue; }

			// ⚠ DELIBERATE DEVIATION FROM THE SHIPPED ADOPT-ROOT ORDER, and it is
			// the one thing in this probe that is not a pure exercise of task 8.
			//
			// ChaosPhysicsFactory's CONSTRUCTOR latches m_parentBodyId from the
			// capsule's Chaos particle, and only LATER does applyDescriptor call
			// SetSimulatePhysics(true) — after which createPhysicalObject asserts
			// `checkf(bodyId == m_parentBodyId)`. If Chaos were to re-create the
			// particle on the kinematic->dynamic switch, that checkf would fire
			// and take the user's whole PIE session with it. I cannot rule that
			// out without reading the engine tree, which is out of bounds. So the
			// switch is made HERE, one game tick before the factory is built, and
			// the factory then sees a stable dynamic particle; applyDescriptor's
			// own SetSimulatePhysics(true) becomes a no-op and every other part of
			// the adopt-root path (agreement check, bodyId identity checkf,
			// registerShape(nullopt), rotation locks, blocking walk, SetResimType)
			// still runs exactly as shipped.
			//
			// ⭐ THE ID-STABILITY QUESTION IS NOT DODGED, IT IS MEASURED: the
			// bodyId before and after the switch is logged below, and task 11 needs
			// that answer to decide whether the shipped ctor order is safe.
			if (!Capsule->IsSimulatingPhysics())
			{
				const uint32 IdBefore = Impl->BodyAdapter->getBodyId(
					Capsule->GetBodyInstanceAsyncPhysicsTickHandle()).value;
				Capsule->SetSimulatePhysics(true);
				const uint32 IdAfter = Capsule->GetBodyInstanceAsyncPhysicsTickHandle().Proxy
					? Impl->BodyAdapter->getBodyId(Capsule->GetBodyInstanceAsyncPhysicsTickHandle()).value : 0u;
				UE_LOG(LogOGSpike9, Warning,
					TEXT("[Warning][Spike9.Adopt] peer=%s idx=%d SetSimulatePhysics(true) on the ACharacter ROOT capsule: ")
					TEXT("bodyIdBefore=%u bodyIdAfter=%u stable=%d"),
					peerTag(World), Ch->ProbeIndex, IdBefore, IdAfter, (IdBefore == IdAfter) ? 1 : 0);
				bAllReady = false;   // give the switch a tick to settle
			}
		}
		if (!bAllReady)
		{
			LogWait(TEXT("WAIT_BODY_INSTANCE_OR_ADOPT"), (int32)Found.size());
			return;
		}

		for (ASpike9ProbeCharacter* Ch : Found)
		{
			ChaosPhysicsFactory Factory(*Impl->BodyAdapter, *Impl->QueryAdapter, Ch, Ch->GetCapsuleComponent());
			const auto Result = Factory.createPhysicalObject(Descriptor, "Spike9Capsule");

			FSpike9Body B;
			B.Character = Ch;
			B.Index     = Ch->ProbeIndex;
			B.Body      = Result.bodyId;
			B.Origin    = Ch->ScriptOrigin;
			B.Forward   = Ch->ScriptForward.GetSafeNormal2D();

			// A walks along +forward; B (q6 only) walks back along -forward.
			B.Dir = (B.Index == 1) ? -B.Forward : B.Forward;

			FVector Start = B.Origin;
			if (B.Index == 1) Start += B.Forward * GPairSeparation + FVector(0.f, 0.f, GPairZOffset);
			if (EffScenario == kScenarioHover) Start += FVector(0.f, 0.f, GRideHeight);

			B.SimPos       = Start;
			B.CommandedPos = Start;
			B.StartXY      = Start;

			// q3/q4: a capsule query volume of the SAME size, so `fraction * L`
			// is the literal clearance the architecture §3.3 step-2 probe wants.
			FCollisionQueryParams QP;
			QP.AddIgnoredActor(Ch);

			QueryVolumeDescriptor WorldVol;
			WorldVol.geometry         = CapsuleGeometry{ kCapsuleRadius, kCapsuleHalfHeight };
			WorldVol.searchCategories = collisionCategory::worldOnly;
			WorldVol.traceCategory    = collisionCategory::world;
			B.WorldVolume = Impl->QueryAdapter->registerVolume(WorldVol, QP, FActorInstanceHandle(Ch));

			QueryVolumeDescriptor CharVol;
			CharVol.geometry         = CapsuleGeometry{ kCapsuleRadius, kCapsuleHalfHeight };
			CharVol.searchCategories = CollisionCategories::single(collisionCategory::character);
			CharVol.traceCategory    = collisionCategory::character;
			B.CharacterVolume = Impl->QueryAdapter->registerVolume(CharVol, QP, FActorInstanceHandle(Ch));
			B.VolumesRegistered = true;

			Impl->Bodies.push_back(B);
		}
		return;   // one more game tick for the bodies to become resolvable
	}

	// ---------------- START: resolvability gate, then ARM --------------------
	for (const FSpike9Body& B : Impl->Bodies)
		if (!Impl->BodyAdapter->isBodyResolvable(B.Body))
		{
			LogWait(TEXT("WAIT_BODY_RESOLVABLE"), (int32)Impl->Bodies.size());
			return;
		}

	Impl->WaitLogCounter = 0;
	Impl->GtSweepSamples = 0;
	Impl->GtSweepHits    = 0;
	Impl->Step           = 0;
	Impl->ResimSteps     = 0;
	Impl->ResimEpisodes  = 0;
	Impl->ActiveScenario = EffScenario;
	Impl->WasResim       = false;

	for (const FSpike9Body& B : Impl->Bodies)
	{
		// ⭐ THE LATCHED-vs-DRIVEN GUARD.
		//
		// `origin=` used to print B.Origin alone - the value the body is actually
		// driven from - and the latched origin was only ever printed on a separate
		// [Spike9.Latch] line, one scroll away. Nothing compared them. If the two
		// had ever diverged the log would have looked completely normal, which is
		// the failure mode that cost the user a run (it turned out to be a
		// misreading rather than a divergence, but the log could not have told the
		// difference either way). Both are now on the SAME line, with their delta,
		// and a divergence is an [Error] plus a sticky flag on every [Spike9.Stop].
		//
		// A CLIENT never latches an origin - it inherits ScriptOrigin by
		// replication - so originMatch=-1 there means "not applicable", not "bad".
		const bool  bHaveLatch  = Impl->HasLatchedOrigin;
		const float OriginDelta = bHaveLatch ? (float)(B.Origin - Impl->LatchedOrigin).Size() : -1.f;
		const int32 OriginMatch = bHaveLatch ? ((OriginDelta <= 0.01f) ? 1 : 0) : -1;
		if (OriginMatch == 0)
		{
			Impl->OriginDiverged = true;
			UE_LOG(LogOGSpike9, Error,
				TEXT("[Error][Spike9.DEFECT] peer=%s idx=%d ORIGIN DIVERGENCE — the body is DRIVEN from ")
				TEXT("bodyOriginWorld=%s but this run LATCHED latchedOriginWorld=%s (|delta|=%.6f cm). ")
				TEXT("Every rel(...) number in this run is measured against bodyOriginWorld. ")
				TEXT("STOP THE RUN AND REPORT THIS — the dataset is not comparable with any other run."),
				peerTag(World), B.Index, *v3(B.Origin), *v3(Impl->LatchedOrigin), OriginDelta);
		}

		UE_LOG(LogOGSpike9, Warning,
			TEXT("[Warning][Spike9.Start] q=%d peer=%s idx=%d bodyId=%u dt=%.6f gravityZ=%.4f clients=%d ")
			TEXT("bodyOriginWorld=%s latchedOriginWorld=%s originDelta=%.6f originMatch=%d ")
			TEXT("forward=%s dir=%s startWorld=%s speed=%.3f duration=%d ")
			TEXT("resimPolicy=%s capsuleR=%.3f capsuleHH=%.3f rideHeight=%.3f pairSep=%.3f pairZOffset=%.3f ")
			TEXT("correctStep=%d correctOffset=%.3f dropStep=%d dropCm=%.3f captureSource=%s"),
			EffScenario, peerTag(World), B.Index, B.Body.value, Impl->Dt, World->GetGravityZ(),
			clientConns(World),
			*v3(B.Origin), bHaveLatch ? *v3(Impl->LatchedOrigin) : TEXT("(not-latched-on-this-peer)"),
			OriginDelta, OriginMatch,
			*v3(B.Forward), *v3(B.Dir), *v3(B.SimPos), GSpeed, GDuration,
			(GResimPolicy == 1) ? TEXT("Resimulate/FullResim") : TEXT("ReplayRecordedHistory/ResimAsFollower"),
			kCapsuleRadius, kCapsuleHalfHeight, GRideHeight, GPairSeparation, GPairZOffset,
			GCorrectStep, GCorrectOffset, GDropStep, GDropCm,
			// [task 45] A run is not readable without knowing which post-solve read
			// it adopted. `og.spike9.captureSource 1` is the FIX ARM.
			(GCaptureSource == 1) ? TEXT("1/GetP-solved") : TEXT("0/captureBodyState-GetX"));
	}

	Impl->Driving.store(true, std::memory_order_release);
}

// ---------------------------------------------------------------------------
// 4.1b [task 44] THE REQUEST CHANNEL: maintain, send, apply.
// ---------------------------------------------------------------------------

// AUTHORITY. One relay per REMOTE PlayerController, spawned on demand and reaped
// when its owner goes.
//
// ⚠ A LOCAL controller is deliberately skipped: on a listen server or in
// standalone the host already types into the authority's own console, and a
// second path into the same globals would only be a way to get them out of step.
//
// ⚠ Spawn-on-demand rather than a GameMode hook, and reap on a timer rather than
// on logout, for the two reasons ASimulationConnectionRelay documents: PostLogin
// is not called for seamless-travel players, and destroying a PlayerController
// does NOT destroy the actors it owns — it only clears their Owner, leaving an
// owner-less bOnlyRelevantToOwner actor that is relevant to nobody.
void ASpike9ProbeDirector::maintainRequestRelays(UWorld& World)
{
	if (!Impl) return;
	// ~1 Hz. Nothing here is urgent: a client that joins mid-session waits at
	// most a second for its channel, and [Spike9.Wait] names that state while it
	// does.
	if (Impl->RelayMaintainCounter++ % 60 != 0) return;

	TArray<ASpike9ProbeRelay*> Dead;
	for (TActorIterator<ASpike9ProbeRelay> It(&World); It; ++It)
		if (IsValid(*It) && !IsValid(It->GetOwner())) Dead.Add(*It);
	for (ASpike9ProbeRelay* Relay : Dead)
	{
		UE_LOG(LogOGSpike9, Warning,
			TEXT("[Warning][Spike9.Relay] peer=%s reaping a request relay whose owning PlayerController is gone."),
			peerTag(&World));
		Relay->Destroy();
	}

	for (auto It = World.GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!IsValid(PC) || PC->IsLocalController()) continue;

		bool bHasRelay = false;
		for (TActorIterator<ASpike9ProbeRelay> R(&World); R; ++R)
			if (IsValid(*R) && R->GetOwner() == PC) { bHasRelay = true; break; }
		if (bHasRelay) continue;

		FActorSpawnParameters Params;
		Params.Owner = PC;
		ASpike9ProbeRelay* Relay = World.SpawnActor<ASpike9ProbeRelay>(
			ASpike9ProbeRelay::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);

		UE_LOG(LogOGSpike9, Warning,
			TEXT("[Warning][Spike9.Relay] peer=%s %s a request relay for %s — that client can now type `og.spike9.scenario`."),
			peerTag(&World), Relay ? TEXT("SPAWNED") : TEXT("FAILED to spawn"), *GetNameSafe(PC));
	}
}

// CLIENT. The local `og.spike9.scenario` cvar is a REQUEST, sent on a CHANGE of
// value so nothing floods the wire. Every failure to send is named in the log
// and, where it is recoverable, retried by itself on the next tick.
void ASpike9ProbeDirector::sendScenarioRequestIfChanged(UWorld& World)
{
	if (!Impl) return;
	if (GScenario == Impl->LastRequestedScenario) return;

	if (!isValidScenario(GScenario))
	{
		UE_LOG(LogOGSpike9, Warning,
			TEXT("[Warning][Spike9.Request] peer=%s REFUSED to send scenario=%d — the only valid values are 0, 1, 3 and 6."),
			peerTag(&World), GScenario);
		Impl->LastRequestedScenario = GScenario;   // say it once, not once a tick
		return;
	}

	APlayerController* PC = World.GetFirstPlayerController();
	APawn* RefPawn = PC ? PC->GetPawn() : nullptr;
	if (GScenario != kScenarioOff && !RefPawn)
	{
		// NOT sent, and LastRequestedScenario is NOT advanced, so this retries
		// itself the moment the pawn exists.
		if (Impl->RequestLogCounter++ % 60 == 0)
			UE_LOG(LogOGSpike9, Warning,
				TEXT("[Warning][Spike9.Wait] peer=%s reason=NO_LOCAL_PAWN__the_request_carries_YOUR_pawn_transform_as_the_origin ")
				TEXT("wantScenario=%d"),
				peerTag(&World), GScenario);
		return;
	}

	ASpike9ProbeRelay* Relay = nullptr;
	for (TActorIterator<ASpike9ProbeRelay> It(&World); It; ++It)
	{
		// Owner AND connection must BOTH have resolved. A Server RPC on an actor
		// whose Owner has not replicated yet is dropped by the engine, which
		// would look exactly like a probe that ignores the console.
		if (IsValid(*It) && IsValid(It->GetOwner()) && It->GetNetConnection() != nullptr)
		{
			Relay = *It;
			break;
		}
	}
	if (!Relay)
	{
		if (Impl->RequestLogCounter++ % 60 == 0)
			UE_LOG(LogOGSpike9, Warning,
				TEXT("[Warning][Spike9.Wait] peer=%s reason=WAIT_REQUEST_RELAY__no_owned_request_channel_on_this_client_yet ")
				TEXT("wantScenario=%d (the server spawns one per client about once a second; if it never arrives, the ")
				TEXT("server is not running this probe build)"),
				peerTag(&World), GScenario);
		return;
	}

	FSpike9ScenarioRequest Request;
	Request.Scenario     = GScenario;
	Request.bResetOrigin = Impl->PendingResetRequest;
	Request.Knobs        = captureKnobs();
	if (RefPawn)
	{
		Request.PawnLocation = RefPawn->GetActorLocation();
		Request.PawnForward  = RefPawn->GetActorForwardVector();
	}

	Relay->ServerRequestScenario(Request);
	Impl->LastRequestedScenario = GScenario;
	Impl->PendingResetRequest   = false;
	Impl->RequestLogCounter     = 0;

	UE_LOG(LogOGSpike9, Warning,
		TEXT("[Warning][Spike9.Request] peer=%s SENT scenario=%d reset=%d pawnWorld=%s forward=%s relay=%s ")
		TEXT("— the AUTHORITY's [Spike9.Request] RECEIVED line is the other half of this handshake; if it is ")
		TEXT("missing from the server log, the RPC did not route."),
		peerTag(&World), Request.Scenario, Request.bResetOrigin ? 1 : 0,
		*v3(Request.PawnLocation), *v3(Request.PawnForward), *GetNameSafe(Relay));
}

// AUTHORITY. The far end of the hop.
void ASpike9ProbeDirector::applyScenarioRequest(const FSpike9ScenarioRequest& Request, const AActor* Requester)
{
	UWorld* World = GetWorld();
	if (!Impl || !World) return;

	if (World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogOGSpike9, Error,
			TEXT("[Error][Spike9.Request] peer=%s a scenario request was delivered to a NON-AUTHORITY peer and IGNORED."),
			peerTag(World));
		return;
	}
	if (!isValidScenario(Request.Scenario))
	{
		UE_LOG(LogOGSpike9, Warning,
			TEXT("[Warning][Spike9.Request] peer=%s REJECTED scenario=%d from=%s — valid values are 0, 1, 3 and 6."),
			peerTag(World), Request.Scenario, *GetNameSafe(Requester));
		return;
	}

	applyKnobs(Request.Knobs);

	if (Request.bResetOrigin)
	{
		Impl->HasLatchedOrigin = false;
	}

	if (Request.Scenario != kScenarioOff)
	{
		const FVector Forward = Request.PawnForward.GetSafeNormal2D();
		if (Forward.IsNearlyZero())
		{
			UE_LOG(LogOGSpike9, Warning,
				TEXT("[Warning][Spike9.Request] peer=%s REJECTED scenario=%d from=%s — the request carried a degenerate ")
				TEXT("pawn forward %s, so there is no direction to walk the capsule along."),
				peerTag(World), Request.Scenario, *GetNameSafe(Requester), *v3(Request.PawnForward));
			return;
		}
		Impl->RequestedOrigin    = Request.PawnLocation + Forward * kOriginAheadCm;
		Impl->RequestedForward   = Forward;
		Impl->HasRequestedOrigin = true;
	}

	// LAST: everything the run needs is in place before the scenario is armed.
	GScenario = Request.Scenario;

	UE_LOG(LogOGSpike9, Warning,
		TEXT("[Warning][Spike9.Request] peer=%s RECEIVED scenario=%d reset=%d from=%s pawnWorld=%s requestOriginWorld=%s ")
		TEXT("speed=%.3f duration=%d rideHeight=%.3f dropStep=%d dropCm=%.3f pairZOffset=%.3f resimPolicy=%d ")
		TEXT("captureSource=%d ")
		TEXT("— those knobs are now the AUTHORITY's and replicate to every peer on ProbeKnobs."),
		peerTag(World), Request.Scenario, Request.bResetOrigin ? 1 : 0, *GetNameSafe(Requester),
		*v3(Request.PawnLocation),
		Impl->HasRequestedOrigin ? *v3(Impl->RequestedOrigin) : TEXT("(none)"),
		GSpeed, GDuration, GRideHeight, GDropStep, GDropCm, GPairZOffset, GResimPolicy,
		GCaptureSource);   // [task 45]
}

// ---------------------------------------------------------------------------
// 4.2 PHYSICS THREAD: the fixed deterministic script.
//     Mirrors architecture §3.3 steps 6', 2, 4, 5 exactly.
// ---------------------------------------------------------------------------

void ASpike9ProbeDirector::PreSimulate_PT()
{
	if (!Impl || !Impl->Driving.load(std::memory_order_acquire)) return;
	if (!Impl->Solver || !Impl->BodyAdapter || !Impl->QueryAdapter) return;

	Chaos::FPBDRigidsEvolution* Evolution = Impl->Solver->GetEvolution();
	const bool bResim      = Evolution && Evolution->IsResimming();
	const bool bFirstResim = Evolution && Evolution->IsResetting();
	const int32 ChaosFrame = (int32)Impl->Solver->GetCurrentFrame();

	if (bResim)
	{
		++Impl->ResimSteps;
		if (!Impl->WasResim) ++Impl->ResimEpisodes;
	}
	Impl->WasResim       = bResim;
	Impl->LastChaosFrame = ChaosFrame;

	// ⭐ A CHAOS REPLAY STEP DOES NOT ADVANCE THE SCRIPT. It re-issues the SAME
	// command — which is precisely the revision-6 claim under test: the body is
	// re-placed from State every tick, replay or not. Advancing the script here
	// would (a) misrepresent a replay as new simulation and (b) make q1(ii)
	// non-reproducible, since two runs see different numbers of replay steps.
	if (bResim)
	{
		for (const FSpike9Body& B : Impl->Bodies)
		{
			if (!Impl->proxyAlive(B.Body)) continue;
			Impl->BodyAdapter->setBodyTransform(
				B.Body, uglm::toGLMMat4(FTransform(FQuat::Identity, B.CommandedPos)));
			Impl->BodyAdapter->setBodyLinearVelocity(B.Body, uglm::toGLMVec3(B.CommandedVel));
			UE_LOG(LogOGSpike9, Warning,
				TEXT("[Warning][Spike9.Replay] q=%d peer=%s idx=%d step=%d chaos=%d firstResim=%d ")
				TEXT("re-placed cmd=%s cmdV=%s"),
				Impl->ActiveScenario, Impl->PeerTag, B.Index, Impl->Step, ChaosFrame,
				bFirstResim ? 1 : 0, *v3rel(B.CommandedPos, B.Origin), *v3(B.CommandedVel));
		}
		return;
	}

	const int32 Step = Impl->Step;
	const float Dt   = Impl->Dt;
	const float L    = GRideHeight + GSnapDistance;

	for (FSpike9Body& B : Impl->Bodies)
	{
		if (!Impl->proxyAlive(B.Body)) continue;

		// ---- step 6': ADOPT the engine's positional push-out ---------------
		// predicted = what x += v*dt would have given from LAST tick's command.
		const FVector Predicted = B.CommandedPos + B.CommandedVel * Dt;
		const FVector PushOut   = B.LastCapture - Predicted;
		B.LastPushOut = PushOut;

		if (Step > 0)
		{
			const float Mag = (float)PushOut.Size();
			if (Mag > B.MaxPushOut)  B.MaxPushOut  = Mag;
			if (FMath::Abs((float)PushOut.Z) > B.MaxPushOutZ) B.MaxPushOutZ = FMath::Abs((float)PushOut.Z);
			if (Mag > 0.05f) ++B.PushOutTicks;

			// ⭐⭐ [task 45] WHAT THIS COUNTER ACTUALLY COUNTS — READ THIS BEFORE
			// QUOTING IT. Substituting the definitions above,
			//     PushOut + Integration
			//   = (LastCapture - CommandedPos - CommandedVel*Dt) + CommandedVel*Dt
			//   = LastCapture - CommandedPos
			// so the test below is EXACTLY
			//     |LastCapture - CommandedPos| < 1e-3, given a non-trivial v*dt.
			//
			// ⛔ That is "THE ADOPTED CAPTURE CAME BACK ON THE COMMAND", NOT "the
			// body did not move". The two are only the same thing if the adopted
			// capture is a post-solve position — and with GCaptureSource==0 it is
			// `captureBodyState()`'s PT `GetX()`, which at OnPostSolve_Internal
			// time is still START-of-step (see FSpike9Impl::readSolvedPT). Under
			// that read the test is satisfied by construction on every tick, no
			// matter what the engine did.
			// `noIntegratePTicks` (PostSolve_PT) is the version that can tell.
			const FVector Integration = B.CommandedVel * Dt;
			if (Integration.Size() > 1e-3 && (PushOut + Integration).Size() < 1e-3)
				++B.NoIntegrateTicks;

			// ⭐⭐ [task 45] QUESTION 2, MADE STRUCTURAL RATHER THAN DERIVED.
			// Three facts the old logging conflated, side by side and unreduced:
			//   simPosPredicted — what the probe's OWN bookkeeping advanced to at
			//                     the end of last PreSimulate (x += v*dt);
			//   capX / capP     — what the two post-solve reads returned;
			//   simPosAfter     — what step 6' leaves SimPos at, which is the
			//                     position the NEXT setBodyTransform will write.
			// `discarded` is the displacement the probe throws away here. If it
			// equals |cmdV*dt| every tick, step 6' is undoing the servo, and the
			// body is teleported back to where it started on the next PreSimulate.
			if (GLogEvery > 0 && (Step % GLogEvery) == 0)
			{
				const FString AdoptCapP = B.HaveCaptureP
					? v3rel(B.LastCaptureP, B.Origin) : FString(TEXT("rel(?,?,?)"));
				UE_LOG(LogOGSpike9, Warning,
					TEXT("[Warning][Spike9.Adopt6] q=%d peer=%s idx=%d step=%d chaos=%d src=%d ")
					TEXT("simPosPredicted=%s capX=%s capP=%s haveP=%d simPosAfter=%s ")
					TEXT("discarded=%.6f |push|=%.6f"),
					Impl->ActiveScenario, Impl->PeerTag, B.Index, Step, ChaosFrame, GCaptureSource,
					*v3rel(B.SimPosPredicted, B.Origin), *v3rel(B.LastCaptureX, B.Origin),
					*AdoptCapP, B.HaveCaptureP ? 1 : 0, *v3rel(B.LastCapture, B.Origin),
					(float)(B.LastCapture - B.SimPosPredicted).Size(), Mag);
			}

			// rev 6: the SIM adopts the push-out into its own position, and
			// kills only the into-contact component of its own velocity.
			B.SimPos = B.LastCapture;
			if (Mag > 0.05f)
			{
				const FVector N = PushOut.GetSafeNormal();
				B.SimVel -= FVector::DotProduct(B.SimVel, N) * N;
			}
		}

		// ---- q1(iii): the SYNTHETIC SIM-SIDE CORRECTION ---------------------
		// This is what an authoritative correction does under rev 6: it rewrites
		// State.position. Nothing tells Chaos. Convergence of the BODY is then
		// purely a consequence of re-placing it from State every tick — which is
		// exactly the property rev 6 claims and rev 5 did not have.
		if (GCorrectStep > 0 && Step == GCorrectStep && B.Index == 0)
		{
			const FVector Before = B.SimPos;
			B.SimPos += B.Dir * GCorrectOffset;
			B.CorrectionStep  = Step;
			B.ConvergedAtStep = -1;
			UE_LOG(LogOGSpike9, Warning,
				TEXT("[Warning][Spike9.Correct] q=%d peer=%s idx=%d step=%d chaos=%d resim=%d ")
				TEXT("offsetCm=%.3f simPosBefore=%s simPosAfter=%s"),
				Impl->ActiveScenario, Impl->PeerTag, B.Index, Step, ChaosFrame, bResim ? 1 : 0,
				GCorrectOffset, *v3rel(Before, B.Origin), *v3rel(B.SimPos, B.Origin));
		}

		// ---- q3: the DROP that the clearance clamp has to catch -------------
		if (GDropStep > 0 && Step == GDropStep && GDropCm != 0.f)
		{
			B.SimPos.Z += GDropCm;
			UE_LOG(LogOGSpike9, Warning,
				TEXT("[Warning][Spike9.Drop] q=%d peer=%s idx=%d step=%d liftedCm=%.3f"),
				Impl->ActiveScenario, Impl->PeerTag, B.Index, Step, GDropCm);
		}

		// ---- step 2: ATTACH probe (q4 lives entirely on this call) ----------
		float Clearance = -1.f;
		FVector GroundNormal = FVector::ZeroVector;
		if (Impl->ActiveScenario == kScenarioHover)
		{
			const glm::mat4 Pose = uglm::toGLMMat4(FTransform(FQuat::Identity, B.SimPos));
			const SweepHit Hit = Impl->QueryAdapter->sweep(
				B.WorldVolume, Pose, glm::vec3(0.f, 0.f, -L));

			GroundNormal = uglm::toFVector(Hit.normal);
			Clearance    = Hit.blocked ? Hit.fraction * L : -1.f;
			B.LastClearance = Clearance;

			if (Hit.blocked)
			{
				++B.SweepHitTicks;
				if (!GroundNormal.IsNearlyZero()) ++B.SweepNormalTicks;
				if (Hit.startPenetrating)
				{
					++B.StartPenTicks;
					if (Hit.penetrationDepth != 0.f) ++B.PenDepthTicks;
				}
			}

			if (GLogEvery > 0 && (Step % GLogEvery) == 0)
			{
				// ⭐ q4's WHOLE ANSWER is on this line: `n=` is FHitResult::ImpactNormal
				// as translated by ChaosSpatialQueryAdapter.cpp:200, taken in
				// PHYSICS-THREAD context with bFindInitialOverlaps = true
				// (ChaosSpatialQueryAdapter.cpp:161).
				UE_LOG(LogOGSpike9, Warning,
					TEXT("[Warning][Spike9.Sweep] q=4 peer=%s idx=%d step=%d chaos=%d resim=%d ")
					TEXT("blocked=%d frac=%.6f n=%s |n|=%.6f impact=%s startPen=%d penDepth=%.6f ")
					TEXT("sweepLen=%.3f clearance=%.6f hitBodyId=%u"),
					Impl->PeerTag, B.Index, Step, ChaosFrame, bResim ? 1 : 0,
					Hit.blocked ? 1 : 0, Hit.fraction, *v3(GroundNormal), (float)GroundNormal.Size(),
					*v3(uglm::toFVector(Hit.impactPoint)), Hit.startPenetrating ? 1 : 0,
					Hit.penetrationDepth, L, Clearance, Hit.bodyId.value);
			}
		}

		// ---- q6(iii): capsule-vs-capsule contact normal ---------------------
		if (Impl->ActiveScenario == kScenarioPair && B.Index == 0 &&
		    GLogEvery > 0 && (Step % GLogEvery) == 0)
		{
			const glm::mat4 Pose = uglm::toGLMMat4(FTransform(FQuat::Identity, B.SimPos));
			const SweepHit Hit = Impl->QueryAdapter->sweep(
				B.CharacterVolume, Pose, uglm::toGLMVec3(B.Dir * 20.f));
			const FVector N = uglm::toFVector(Hit.normal);
			UE_LOG(LogOGSpike9, Warning,
				TEXT("[Warning][Spike9.PairNormal] q=6 peer=%s step=%d chaos=%d blocked=%d frac=%.6f ")
				TEXT("n=%s nZ=%.6f startPen=%d penDepth=%.6f hitBodyId=%u"),
				Impl->PeerTag, Step, ChaosFrame, Hit.blocked ? 1 : 0, Hit.fraction,
				*v3(N), N.Z, Hit.startPenetrating ? 1 : 0, Hit.penetrationDepth, Hit.bodyId.value);
		}

		// ---- steps 3 + 4: the model, then the vertical law ------------------
		FVector Vel = FVector::ZeroVector;
		if (Impl->ActiveScenario == kScenarioWall || Impl->ActiveScenario == kScenarioPair)
		{
			Vel = B.Dir * GSpeed;                            // pure horizontal script
		}
		else if (Impl->ActiveScenario == kScenarioHover)
		{
			// §3.3 step 4. Floor: dead-beat clearance servo (ruling #13).
			// Airborne: AUTHORED gravity (ruling #16 a). Lateral velocity is
			// zero, so any XY movement is CREEP — which is what q3 measures.
			// ⛔ DELIBERATELY TOLERANT OF AN UNPOPULATED NORMAL. If q4's answer is
			// "ImpactNormal is NOT populated in physics-thread context", a strict
			// `normal.Z >= cosMaxSlope` test would make the capsule fall forever and
			// q3 would report a hover failure that is really a q4 failure. The
			// [Spike9.Sweep] line still records |n| honestly, so q4 stays measurable.
			const bool bFloor = (Clearance >= 0.f) &&
			                    (GroundNormal.IsNearlyZero() || GroundNormal.Z >= 0.7f);
			if (bFloor)
			{
				++B.FloorTicks;
				const float Err = GRideHeight - Clearance;
				// ⚠ `< 0.f ||` is the whole point: WorstClearanceErr starts at -1
				// ("never ran"), so the FIRST servo step must overwrite it even when
				// its error is 0. Without that, a servo that ran perfectly and a servo
				// that never ran would both report 0.000000 - which is exactly how run
				// 1's worstClearanceErr=0.000000 was nearly recorded as a q3 success
				// when the bFloor branch had never once been entered.
				if (B.WorstClearanceErr < 0.f || FMath::Abs(Err) > B.WorstClearanceErr)
					B.WorstClearanceErr = FMath::Abs(Err);
				Vel.Z = FMath::Clamp(Err / Dt, -GMaxSnapSpeed, GMaxSnapSpeed);
			}
			else
			{
				++B.AirborneTicks;
				Vel.Z = FMath::Max(B.SimVel.Z + Impl->GravityZ * Dt, -GTerminalFall);
			}
		}

		// ---- step 5: WRITE (SetX + SetV, every tick, rev-6 ruling #14 c) ----
		// The sim OWNS position and velocity (ruling #14 c). The engine returns
		// only a positional push-out, adopted by step 6' at the top of next tick.
		B.SimVel       = Vel;
		B.CommandedPos = B.SimPos;
		B.CommandedVel = B.SimVel;

		Impl->BodyAdapter->setBodyTransform(
			B.Body, uglm::toGLMMat4(FTransform(FQuat::Identity, B.CommandedPos)));
		Impl->BodyAdapter->setBodyLinearVelocity(B.Body, uglm::toGLMVec3(B.CommandedVel));

		// Advance the sim's own position for the NEXT tick (x += v*dt).
		B.SimPos = B.CommandedPos + B.CommandedVel * Dt;
		// ⭐ [task 45] Remembered so the next tick's step-6' can print what this
		// advance asked for BESIDE what the adopt replaced it with. Without this
		// the advance is invisible: step 6' overwrites SimPos before anything
		// logs it, and "the probe advanced" was only ever inferred from cmdV.
		B.SimPosPredicted = B.SimPos;
	}
}

void ASpike9ProbeDirector::PostSolve_PT()
{
	if (!Impl || !Impl->Driving.load(std::memory_order_acquire)) return;
	if (!Impl->Solver || !Impl->BodyAdapter) return;

	Chaos::FPBDRigidsEvolution* Evolution = Impl->Solver->GetEvolution();
	const bool bResim      = Evolution && Evolution->IsResimming();
	const int32 ChaosFrame = (int32)Impl->Solver->GetCurrentFrame();
	const int32 Step       = Impl->Step;

	// ⭐ A REPLAY STEP IS CAPTURED (its solve moved the body, and the next real
	// step's step-6' adopt must see that) but is NOT counted as a script step and
	// contributes to no aggregate — see the matching guard in PreSimulate_PT.
	if (bResim)
	{
		for (FSpike9Body& B : Impl->Bodies)
		{
			if (!Impl->proxyAlive(B.Body)) continue;
			const PhysicsBodyState RS = Impl->BodyAdapter->captureBodyState(B.Body);
			B.LastCaptureX = uglm::toFVector(RS.position);
			B.LastCaptureV = uglm::toFVector(RS.linearVelocity);
			B.HaveCaptureP = Impl->readSolvedPT(B.Body, B.LastCaptureP);
			B.LastCapture  = (GCaptureSource == 1 && B.HaveCaptureP) ? B.LastCaptureP : B.LastCaptureX;
			const FString ReplayCapP = B.HaveCaptureP ? v3rel(B.LastCaptureP, B.Origin) : FString(TEXT("rel(?,?,?)"));
			UE_LOG(LogOGSpike9, Warning,
				TEXT("[Warning][Spike9.Replay] q=%d peer=%s idx=%d step=%d chaos=%d POSTSOLVE ")
				TEXT("cmd=%s capX=%s capP=%s haveP=%d src=%d err=%.6f"),
				Impl->ActiveScenario, Impl->PeerTag, B.Index, Step, ChaosFrame,
				*v3rel(B.CommandedPos, B.Origin), *v3rel(B.LastCaptureX, B.Origin),
				*ReplayCapP, B.HaveCaptureP ? 1 : 0, GCaptureSource,
				(float)(B.LastCapture - B.CommandedPos).Size());
		}
		return;
	}

	for (FSpike9Body& B : Impl->Bodies)
	{
		if (!Impl->proxyAlive(B.Body)) continue;
		const PhysicsBodyState S = Impl->BodyAdapter->captureBodyState(B.Body);
		B.LastCaptureX = uglm::toFVector(S.position);
		B.LastCaptureV = uglm::toFVector(S.linearVelocity);

		// ⭐⭐ [task45-capture] BOTH READS, EVERY TICK. See FSpike9Impl::readSolvedPT.
		B.HaveCaptureP = Impl->readSolvedPT(B.Body, B.LastCaptureP);
		B.LastCapture  = (GCaptureSource == 1 && B.HaveCaptureP) ? B.LastCaptureP : B.LastCaptureX;
		if (B.HaveCaptureP)
		{
			++B.CaptureFrameTicks;
			const float XP = (float)(B.LastCaptureP - B.LastCaptureX).Size();
			if (XP > B.MaxXPDelta) B.MaxXPDelta = XP;
			// The counter `noIntegrateTicks` was BELIEVED to be: did the SOLVED
			// position land on the command? Only this one can say "the engine did
			// not move the body"; noIntegrateTicks can only say "the adopted read
			// did not move".
			if (Step > 0 && (B.CommandedVel * Impl->Dt).Size() > 1e-3 &&
			    (B.LastCaptureP - B.CommandedPos).Size() < 1e-3)
				++B.NoIntegratePTicks;
		}

		if (B.Mass == 0.f)
			if (auto* Proxy = Impl->Solver->GetParticleProxy_PT(Chaos::FUniqueIdx{ (int32)B.Body.value }))
				if (auto* Api = Proxy->GetPhysicsThreadAPI()) B.Mass = (float)Api->M();

		// The IMMEDIATE, SAME-STEP push-out: what the solve did to the position
		// we commanded this very tick. q1(i) and q6(i) both read this.
		const FVector SameStepPush = B.LastCapture - (B.CommandedPos + B.CommandedVel * Impl->Dt);

		const float Creep = (float)FVector(B.LastCapture.X - B.StartXY.X,
		                                   B.LastCapture.Y - B.StartXY.Y, 0.f).Size();
		if (Impl->ActiveScenario == kScenarioHover && Creep > B.MaxLateralCreep)
			B.MaxLateralCreep = Creep;

		// q1(iii): first step at which the body has caught up with the corrected
		// command again (contact-free tolerance 0.5 cm).
		if (B.CorrectionStep >= 0 && B.ConvergedAtStep < 0 && Step > B.CorrectionStep)
			if ((B.LastCapture - B.CommandedPos).Size() < 0.5)
				B.ConvergedAtStep = Step;

		if (GLogEvery > 0 && (Step % GLogEvery) == 0)
		{
			// ⭐ `cmdWorld=` is the anchor that makes the rest of this line
			// unmisreadable. `cmd`/`cap` are rel(...) to the body's own origin;
			// `cmdWorld` is where the body actually IS. Reading a relative
			// `cmd=(0,0,10)` as a world position is what voided run 1.
			// ⭐⭐ [task 45] `capX` / `capP` ARE THE HEADLINE.
			// `capX` is `captureBodyState()`'s PT `GetX()` — what runs 19:56 and
			// 20:11 called `cap`. `capP` is the rigid handle's `GetP()`, the
			// position this step actually SOLVED to. `xp` is their distance and
			// `predicted` is what free integration (x += v*dt) asks for.
			//   capX == cmd  and  capP == predicted   ->  the engine integrated and
			//                                             the capture read the wrong
			//                                             field (task 45's finding);
			//   capX == capP == cmd                   ->  the engine really did not
			//                                             move the body.
			// Those two are indistinguishable on the OLD line, which is why the
			// spike stalled for six wrong calls.
			const FVector Predicted = B.CommandedPos + B.CommandedVel * Impl->Dt;
			const FString CapP = B.HaveCaptureP ? v3rel(B.LastCaptureP, B.Origin) : FString(TEXT("rel(?,?,?)"));
			UE_LOG(LogOGSpike9, Warning,
				TEXT("[Warning][Spike9.Tick] q=%d peer=%s idx=%d step=%d chaos=%d resim=%d ")
				TEXT("cmdWorld=%s cmd=%s cmdV=%s cap=%s capX=%s capP=%s haveP=%d src=%d ")
				TEXT("predicted=%s xp=%.6f pErr=%.6f capV=%s push=%s |push|=%.6f pushZ=%.6f ")
				TEXT("err=%.6f clearance=%.6f mass=%.4f"),
				Impl->ActiveScenario, Impl->PeerTag, B.Index, Step, ChaosFrame, bResim ? 1 : 0,
				*v3(B.CommandedPos),
				*v3rel(B.CommandedPos, B.Origin), *v3(B.CommandedVel),
				*v3rel(B.LastCapture, B.Origin),
				*v3rel(B.LastCaptureX, B.Origin), *CapP, B.HaveCaptureP ? 1 : 0, GCaptureSource,
				*v3rel(Predicted, B.Origin),
				B.HaveCaptureP ? (float)(B.LastCaptureP - B.LastCaptureX).Size() : -1.f,
				B.HaveCaptureP ? (float)(B.LastCaptureP - Predicted).Size() : -1.f,
				*v3(B.LastCaptureV),
				*v3(SameStepPush), (float)SameStepPush.Size(), (float)SameStepPush.Z,
				(float)(B.LastCapture - B.CommandedPos).Size(), B.LastClearance, B.Mass);
		}
	}

	// q6(i): the 50/50 split, in the SAME step's capture.
	if (Impl->ActiveScenario == kScenarioPair && Impl->Bodies.size() == 2 &&
	    GLogEvery > 0 && (Step % GLogEvery) == 0)
	{
		const FSpike9Body& A = Impl->Bodies[0];
		const FSpike9Body& Bb = Impl->Bodies[1];
		const FVector PushA = A.LastCapture - (A.CommandedPos + A.CommandedVel * Impl->Dt);
		const FVector PushB = Bb.LastCapture - (Bb.CommandedPos + Bb.CommandedVel * Impl->Dt);
		const float   MagA  = (float)PushA.Size();
		const float   MagB  = (float)PushB.Size();
		const float   Ratio = (MagA + MagB) > 1e-4f ? MagA / (MagA + MagB) : -1.f;
		const FVector Sep   = Bb.LastCapture - A.LastCapture;
		UE_LOG(LogOGSpike9, Warning,
			TEXT("[Warning][Spike9.Pair] q=6 peer=%s step=%d chaos=%d sep=%s |sepXY|=%.6f sepZ=%.6f ")
			TEXT("centreDist=%.6f overlap=%.6f pushA=%s pushB=%s |pushA|=%.6f |pushB|=%.6f ")
			TEXT("shareA=%.6f pushAZ=%.6f pushBZ=%.6f massA=%.4f massB=%.4f"),
			Impl->PeerTag, Step, ChaosFrame, *v3(Sep),
			(float)FVector(Sep.X, Sep.Y, 0.f).Size(), (float)Sep.Z,
			(float)Sep.Size(), (float)(2.f * kCapsuleRadius - FVector(Sep.X, Sep.Y, 0.f).Size()),
			*v3(PushA), *v3(PushB), MagA, MagB, Ratio, (float)PushA.Z, (float)PushB.Z,
			A.Mass, Bb.Mass);
	}

	++Impl->Step;
}

// ===========================================================================
// 5. THE SUBSYSTEM
// ===========================================================================

bool USpike9ProbeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void USpike9ProbeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	InWorld.SpawnActor<ASpike9ProbeDirector>();
}
