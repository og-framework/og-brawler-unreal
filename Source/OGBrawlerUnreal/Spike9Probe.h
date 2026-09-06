#pragma once

// SPDX-License-Identifier: BUSL-1.1
//
// ============================================================================
//  ⛔ TEMPORARY — brawler-movement-simulation TASK 9 RESEARCH SPIKE, PHASE 1.
//
//  This whole file pair (Spike9Probe.h / Spike9Probe.cpp) is a PROBE. It is
//  reverted in task 9 PHASE 2, after the user's PIE run, by deleting both
//  files. It is referenced by NOTHING: no existing file includes it, no
//  existing file names any symbol in it, and no .Build.cs was edited (the UBT
//  module globs its own Source directory). Deleting the two files therefore
//  restores the tree exactly.
//
//  It exists to answer the five live spike questions (q1, q2, q3, q4, q6),
//  every one of which needs a running game, which no agent can produce.
//
//  ⭐ TASK 44 (2026-09-05): the probe no longer requires a LISTEN SERVER. A
//  scenario is requested from a CLIENT console and relayed to the authority
//  (ASpike9ProbeRelay), so the spike runs under the project's normal, tested
//  dedicated-server configuration. Still two files; still deleted by phase 2.
//
//  See: impl/research_spike_9.md  (hunk inventory + verdicts)
//       impl/spike_9_PIE_CHECKLIST.md  (how the user drives it)
// ============================================================================

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/Info.h"
#include "Subsystems/WorldSubsystem.h"

#include "Spike9Probe.generated.h"

// Its own category so the probe is separable from every shipped instrument.
// Compile-time default `Log` and NO Config/DefaultEngine.ini entry, so the
// category is unsuppressed out of the box; every probe line is additionally
// emitted at Warning severity AND carries a literal "[Warning]" token, which is
// the belt-and-braces the brief requires (Config/DefaultEngine.ini pins every
// LogOG* category to Warning, and SimulationManagerUImpl.cpp:127 EMIT_OG picks
// severity from that in-message token — a probe line written the obvious way is
// silently suppressed).
OGBRAWLERUNREAL_API DECLARE_LOG_CATEGORY_EXTERN(LogOGSpike9, Log, All);

// ---------------------------------------------------------------------------
// [task 44] THE TUNING KNOBS, AS DATA.
//
// Every knob below used to exist ONLY as a process-local cvar, which was fine
// while the console that armed a run and the authority that executed it were the
// same process. They no longer are: the console is a CLIENT and the authority is
// a DEDICATED SERVER, so the knob VALUES have to travel with the request, and
// then out to every following peer.
//
// Why they also replicate (on ASpike9ProbeCharacter::ProbeKnobs): q1(ii) diffs
// the server's trajectory against a client's. That comparison is only meaningful
// if both peers ran the SAME script. Before this task a knob typed on the
// authority was never seen by the client at all, and the two peers silently ran
// different speeds/durations — a defect that would have read as non-determinism.
//
// ⚠ Plain UPROPERTY members only, and deliberately NO custom NetSerialize: Iris
// resolves custom struct serializers through a name registry, so an unregistered
// one is silently bypassed. There is nothing here for it to bypass.
// ---------------------------------------------------------------------------
USTRUCT()
struct FSpike9Knobs
{
	GENERATED_BODY()

	UPROPERTY() int32 ResimPolicy    = 0;
	// [task 45] 0 = ChaosPhysicsBodyAdapter::captureBodyState() (production, reads
	// the PT `GetX()`); 1 = the probe's OWN read of the rigid handle's `GetP()`.
	// See the [task45-capture] block in Spike9Probe.cpp for why those differ at
	// OnPostSolve_Internal time.
	UPROPERTY() int32 CaptureSource  = 0;
	UPROPERTY() float Speed          = 200.f;
	UPROPERTY() int32 Duration       = 300;
	UPROPERTY() int32 CorrectStep    = 150;
	UPROPERTY() float CorrectOffset  = -50.f;
	UPROPERTY() float RideHeight     = 10.f;
	UPROPERTY() float SnapDistance   = 40.f;
	UPROPERTY() float MaxSnapSpeed   = 400.f;
	UPROPERTY() float TerminalFall   = 2000.f;
	UPROPERTY() float PairSeparation = 300.f;
	UPROPERTY() float PairZOffset    = 0.f;
	UPROPERTY() float DropCm         = 0.f;
	UPROPERTY() int32 DropStep       = 0;
	UPROPERTY() int32 LogEvery       = 1;
};

// ---------------------------------------------------------------------------
// [task 44] ONE client -> server message: "run this scenario, here, with these
// knobs". `PawnLocation`/`PawnForward` are the REQUESTING CLIENT's own pawn
// transform at the moment the command was typed — see the origin rule in
// Spike9Probe.cpp's latch block for why that, and nothing else, is the referent.
// ---------------------------------------------------------------------------
USTRUCT()
struct FSpike9ScenarioRequest
{
	GENERATED_BODY()

	UPROPERTY() int32        Scenario     = 0;
	UPROPERTY() bool         bResetOrigin = false;
	UPROPERTY() FVector      PawnLocation = FVector::ZeroVector;
	UPROPERTY() FVector      PawnForward  = FVector(1.f, 0.f, 0.f);
	UPROPERTY() FSpike9Knobs Knobs;
};

// ---------------------------------------------------------------------------
// q2's subject: a plain ACharacter whose ROOT CAPSULE is the simulating body.
// CMC is put in MOVE_None with its tick off and movement replication is off, so
// the only thing that moves the root is the physics thread. Everything the
// engine complains about while that is true is q2's answer.
// ---------------------------------------------------------------------------
UCLASS()
class ASpike9ProbeCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASpike9ProbeCharacter();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Server-authored, replicated so the CLIENT's director can drive the same
	// script against its own copy. Deliberately tiny.
	UPROPERTY(Replicated) int32   ProbeIndex   = 0;
	UPROPERTY(Replicated) int32   ProbeScenario = 0;
	UPROPERTY(Replicated) FVector ScriptOrigin  = FVector::ZeroVector;
	UPROPERTY(Replicated) FVector ScriptForward = FVector(1.f, 0.f, 0.f);
	// [task 44] The knob set the AUTHORITY actually ran with. A following peer
	// adopts it verbatim, so every peer executes the identical script.
	UPROPERTY(Replicated) FSpike9Knobs ProbeKnobs;
};

// ---------------------------------------------------------------------------
// [task 44] THE REQUEST CHANNEL — one per remote client, spawned BY THE SERVER
// and OWNED BY THAT CLIENT'S PlayerController.
//
// Ownership is not decoration: `AActor::GetNetConnection()` walks the Owner
// chain, and an actor with no owning connection cannot carry a Server RPC at
// all. That is the entire reason this actor exists rather than putting the RPC
// on the probe character (which the server spawns unowned, and only AFTER a
// scenario has already started — the chicken-and-egg this class breaks).
//
// Modelled on `ASimulationConnectionRelay` (OGSimulationUnreal), the shipped
// per-connection owned actor — a COPY of a proven pattern, not a dependency:
// this probe adds no include of it and no module reference.
// ---------------------------------------------------------------------------
UCLASS()
class ASpike9ProbeRelay : public AInfo
{
	GENERATED_BODY()

public:
	ASpike9ProbeRelay();

	// CLIENT -> SERVER. Reliable, because a dropped "start the scenario" would
	// look exactly like a probe that is broken.
	UFUNCTION(Server, Reliable)
	void ServerRequestScenario(const FSpike9ScenarioRequest& Request);
};

// Physics-thread tap; fully defined in the .cpp so no Chaos header reaches UHT.
class FSpike9AsyncCallback;
// Every non-UObject member (adapters, body ids, per-tick script state) lives
// here, out of the UHT-parsed header.
struct FSpike9Impl;

// ---------------------------------------------------------------------------
// One per peer. Spawned unconditionally at world begin-play (like
// ASimulationManagerUImpl) and completely DORMANT until a scenario is armed: it
// creates no body, registers no callback and logs nothing until then.
//
// [task 44] On the AUTHORITY the scenario is armed by a client's request (or by
// a local `og.spike9.scenario` in standalone PIE); on a CLIENT the local cvar is
// only ever a REQUEST, and what the client FOLLOWS is the replicated
// ASpike9ProbeCharacter::ProbeScenario.
// ---------------------------------------------------------------------------
UCLASS()
class ASpike9ProbeDirector : public AActor
{
	GENERATED_BODY()

public:
	ASpike9ProbeDirector();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	// PHYSICS THREAD. Called by FSpike9AsyncCallback.
	void PreSimulate_PT();
	void PostSolve_PT();

	// [task 44] AUTHORITY. Applies a request that arrived from a client's relay:
	// adopts the knobs, optionally clears the origin latch, records the origin
	// the request carried, and arms the scenario. Public because
	// ASpike9ProbeRelay is the caller.
	void applyScenarioRequest(const FSpike9ScenarioRequest& Request, const AActor* Requester);

private:
	// [task 44] AUTHORITY, ~1 Hz: one relay per remote PlayerController, reaped
	// when its owner goes.
	void maintainRequestRelays(UWorld& World);
	// [task 44] CLIENT: send the local cvar as a request when it CHANGES.
	void sendScenarioRequestIfChanged(UWorld& World);

	FSpike9Impl* Impl = nullptr;
};

// ---------------------------------------------------------------------------
// Spawns the director. Mirrors USimulationManagerSubsystem exactly.
// ---------------------------------------------------------------------------
UCLASS()
class USpike9ProbeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
};
