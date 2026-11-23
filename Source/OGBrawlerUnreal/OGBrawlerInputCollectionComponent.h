// SPDX-License-Identifier: BUSL-1.1

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OGSimulationUnreal/InputMappingUETranslator.h"
#include "OGBrawler/SimulatableBrawlerTypes.h"
#include "OGSimulation/SimulationFrame.h"
#include "OGSimulation/SimulationTimeContext.h"
#include "OGSimulation/Network/LocalInputCache.h"
#include "OGSimulation/Mailbox.h"
#include "OGSimulationUnreal/UELatencyBudgetDiagnostics.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"

#include <atomic>
#include <memory>
#include <optional>

#include "OGBrawlerInputCollectionComponent.generated.h"

class UEnhancedInputComponent;
class AOGBrawlerUECharacter;
struct FInputActionValue;
// [T15] The AOGBrawlerSimulationManager forward declaration is gone with the manager
// argument on buildPlayerInput — this component no longer names the manager at all.

// [og-input-latency P5-15] One frame's input sample: every game-thread value buildPlayerInput
// reads, copied in one piece. The component stages it at the end of updateGameThreadCache (the
// character's Tick, TG_PrePhysics, after the controller's Enhanced Input events), and the tick
// driver's sampleInput hook (BrawlerTickDriverGameUImpl, first in startFrame, TG_StartPhysics,
// before the pump) publishes it through a TripleBuffer; buildPlayerInput, on the step's thread,
// reads only the latest published sample. This replaces the step reading the component's fields
// one by one while the game thread could be writing them (the "benign race" the caches carried:
// a step batch that outlives its frame overlaps the next frame's input events).
//
// The four derived reads below are the component's former accessor bodies, unchanged, run on
// the sample's copies instead of the live members. The component's own accessors (game thread
// only: the character's Tick, the HUD, the simulatable component) run the same code on a live
// sample built from the members at the call, so the render echo and the sim path still share
// one continuous read (simulatableBrawler::readContinuousInputFields over either source).
//
// The movement-scheme and deadzone globals are NOT in the sample: they are std::atomic, written
// on the game thread and loaded at the read, exactly as before (no race to replace).
struct BrawlerInputSample
{
	bool hasInputComponent = false;
	glm::vec3 camForward = simFrame::kForward;
	glm::vec3 mouseAim   = glm::vec3(0.f, 0.f, 0.f);
	glm::vec2 moveKeys   = glm::vec2(0.f, 0.f);
	glm::vec2 leftStick  = glm::vec2(0.f, 0.f);
	glm::vec2 rightStick = glm::vec2(0.f, 0.f);
	bool leftAttack  = false;
	bool rightAttack = false;
	bool blockLook   = false;
	bool holdGuard   = false;
	bool lastMoveInputWasGamepad = false;
#if OG_LATENCY_BUDGET_DIAGNOSTICS
	// DIAGNOSTIC ONLY: the wall time (FPlatformTime::Seconds) of the updateGameThreadCache call
	// that staged this sample, 0 before the first. The latency-budget probe's hop H1 start: it
	// travels with the sample so H1 measures the age of the input the step actually consumed.
	double sampledSeconds = 0.0;
#endif

	glm::vec2 getMoveStick() const;
	glm::vec2 getAimStick() const;
	glm::vec3 buildAimDirection() const;
	glm::vec3 buildMoveDirectionWorld() const;

private:
	glm::vec3 buildMoveDirectionWorldFor(const glm::vec3& referenceForward) const;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UOGBrawlerInputCollectionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOGBrawlerInputCollectionComponent();

	// Initializes the translator with the owning actor as outer and the default
	// game input mapping. Must be called from SetupPlayerInputComponent before
	// any addInputMappingContextForController call can actually register the IMC.
	void initializeTranslator();

	// Adds the translator's IMC to the controlling LP's Enhanced Input subsystem.
	// Idempotent. Called from BeginPlay, PossessedBy, OnRep_Controller, and
	// SetupPlayerInputComponent so the IMC lands regardless of possession ordering.
	void addInputMappingContextForController(AController* InController);

	const InputMappingUETranslator& getTranslator() const { return m_inputTranslator; }

	// Binds all Enhanced Input actions to this component. Called once from
	// AOGBrawlerUECharacter::SetupPlayerInputComponent after initializeTranslator().
	void setupBindings(UEnhancedInputComponent* ic);

	// Game-thread-only refresh entry point. Called from AOGBrawlerUECharacter::Tick
	// exactly once per frame. Resolves PCM/FollowCamera forward, mouse-aim line-plane
	// intersection, and SetShowMouseCursor — all UObject APIs unsafe from physics thread.
	// Writes m_camForwardCache and m_mouseAimCache, then stages this frame's BrawlerInputSample
	// (og-input-latency P5-15).
	// [P5-15] resolveCameraForward / resolveMouseAim ("physics-thread-safe cache reads") are
	// gone: they had no caller, and the step now reads the caches only through the sample.
	void updateGameThreadCache();

	// [og-input-latency P5-15] Publishes the sample updateGameThreadCache last staged, so the next
	// buildPlayerInput reads it. Game thread only (checkf: the TripleBuffer has one producer).
	// Called once per frame by the tick driver's sampleInput hook (BrawlerTickDriverGameUImpl),
	// first in startFrame, before the pump.
	void publishSample();

#if OG_LATENCY_BUDGET_DIAGNOSTICS
	// Diagnostics view (og-simulation's DiagnosticsConventions.md §1; og-tickdriver task 18). Read only by the
	// latency wrapper's BrawlerSessionGameUImpl hook; it exists only while OG_LATENCY_BUDGET_DIAGNOSTICS is 1
	// (every build but Shipping). inputSampledSeconds(): the sampledSeconds of the sample the latest
	// buildPlayerInput consumed (the wall time of the updateGameThreadCache call that staged it), or 0 before
	// the first. Since og-input-latency P5-15 it is written by buildPlayerInput on the step's thread and read
	// by the wrapper right after it on the same thread (atomic, relaxed): the latency-budget probe's hop H1
	// start. Until P5-15 it was the latest updateGameThreadCache call's time, written on the game thread.
	// Feeds no simulation input.
	class Diagnostics
	{
	public:
		explicit Diagnostics(const UOGBrawlerInputCollectionComponent& component) : m_component(component) {}
		double inputSampledSeconds() const { return m_component.m_consumedSampledSeconds.load(std::memory_order_relaxed); }

	private:
		const UOGBrawlerInputCollectionComponent& m_component;
	};
	Diagnostics getDiagnostics() const { return Diagnostics(*this); }
#endif

	// Direction-build helpers — GAME THREAD ONLY since og-input-latency P5-15: they read the live
	// members (through liveSample()). The step reads the same arithmetic on the published sample.
	// buildAimDirection is always unit length. buildMoveDirectionWorld is zero below the move
	// deadzone, otherwise unit length (getInputDirectionInCameraSpace normalizes).
	glm::vec3 buildAimDirection() const;
	glm::vec3 buildMoveDirectionWorld() const;
	static glm::vec3 getInputDirectionInCameraSpace(const glm::vec3& camForward, const glm::vec3& inputDirection);

	// --- The three PlayerInput sources, and how they relate (D5.4 / og-netcode-v2 T12) ---
	//
	// There are three ways a PlayerInput reaches a consumer in this project. They are NOT
	// interchangeable; picking the wrong one is a correctness bug, not a style choice.
	//
	//  1. buildPlayerInput(step, componentId, localInputCache)   — THE SIM PATH.
	//     Called once per simulation tick from the inputProvider lambda on the physics
	//     thread. Continuous fields + discrete fields (attack buttons) + the tick-stateful
	//     motion-sequence matcher (Hadouken and friends), which needs a RAW CAPTURE history
	//     and the tick number to do rising-edge detection against the previous tick's input.
	//     This is the ONLY input that is simulated and sent to the server. Rate: sim tick (60 Hz).
	//
	//  2. buildLatestVisualizationInput()                — THE RENDER ECHO (visualization only).
	//     A live re-sample of the CONTINUOUS fields only, safe to call at render-frame rate.
	//     Shares the continuous read with (1) via simulatableBrawler::readContinuousInputFields,
	//     so the two provably cannot drift; differs only in that it calls
	//     makeVisualizationPlayerInput instead of makeSimPlayerInput, leaving every discrete
	//     field neutral (triggeredActionId == inputSequence::kNoMatch, attacks false, and
	//     [movement-sim task 14] holdGuard false ⇒ the movement input flags byte all-clear).
	//     The motion matcher is NEVER invoked here — running it at render rate would misfire
	//     it, since many render frames share one "previous tick". Rate: render frame.
	//     Cosmetic only: never feed this to the simulation or to the input bundle.
	//
	//  3. SimulationNetSync::getLastRelayedInput<SimulatableBrawler>() — THE REMOTE SOURCE.
	//     The newest input RELAYED for a remote character, or nullopt if none has arrived.
	//     Returns nullopt on the authority too (relay stores exist only for remote,
	//     provider-absent ids on a predicting client). Rate: sim tick.
	//
	// T13 swaps the LOCAL character's visualization input source from (3) to (2); remote
	// proxies keep (3).
	//
	// [og-netcode-v2-input-relay T7/T8] (3) USED TO BE `CorrectionCache::getLatestInput`,
	// described here as "the correct — and only — source for REMOTE simulated proxies".
	// T7 re-pointed the viz to the relay store; T8 made that permanent by retiring the
	// server->client correction-input channel. The cache's input column is no longer fed by
	// the authority at all, so for a remote character it now holds only this client's own
	// prediction — reading it would silently hand back a guess dressed as authority. The
	// nullopt contract of (3) is deliberately unchanged, which is why the selector rule in
	// OGBrawler/BrawlerVisualizationInputSource.h needed no logic change.
	// [og-netcode-v2-input-relay T16] THE COLUMN ITSELF IS NOW GONE — `m_inputBuffer`,
	// `getInput`, `getLatestInput` and `pushPredictionInput` no longer exist on
	// StateCorrectionCache. The hazard above is therefore no longer a hazard to be
	// careful about; it is unreachable. A correction-cache slot carries state plus the
	// applied-capture-tick ref and nothing else.

	// Builds the full sim-tick PlayerInput. Called from the inputProvider lambda on the physics thread.
	//
	// [T15] `localInputCache` is this character's OWN raw capture history, keyed by capture tick, and is
	// the motion matcher's history source. It is a parameter, not something reached for: NetSync
	// binds it in collectInputAll and calls this BEFORE pushing the current tick's capture, so the
	// line holds ticks <= step.getTick() - 1 and the current sample is what this function returns.
	//
	// It replaces the old `const AOGBrawlerSimulationManager*` argument, which existed only so the
	// matcher could reach manager -> reconciliation -> correction cache. That cache stores the
	// APPLIED input keyed by APPLICATION tick, so under an input delay `d` every history read was
	// displaced by `d` — see OGBrawler/BrawlerMotionMatching.h for the full statement of the defect
	// and its exact bound. There is also no longer a "no history available" arm: the delay line
	// exists iff an input provider is registered, and this method's only caller IS that provider.
	//
	// [og-input-latency P5-15] It reads NO member the game thread writes: every live value comes
	// from the latest sample publishSample() published (acquired from the TripleBuffer, whose
	// consumer side this is; hence non-const). Before the first publish it returns the neutral
	// input, as it does for a sample staged without an input component.
	simulatableBrawler::PlayerInput buildPlayerInput(
		const SimulationTimeStep& step,
		uint32 componentId,
		const LocalInputCache<simulatableBrawler::PlayerInput>& localInputCache);

	// Render-frame-callable live sample of the CONTINUOUS input fields only. See the block
	// comment above for how this relates to buildPlayerInput and to source (3). Takes no
	// SimulationTimeStep, no componentId and no history precisely because it touches none of
	// the tick/history context the motion matcher would need — the matcher is not run.
	simulatableBrawler::PlayerInput buildLatestVisualizationInput() const;

	// Logical stick accessors. Both return the matching half of
	// dInput::stickRouting::routeSticks({m_moveKeys, m_leftStick, m_rightStick}, ...),
	// fed with g_movementScheme, g_swapMoveAndAimSticks, g_gamepadMoveStickFeedsAim,
	// g_moveStickDeadzone and g_backStopMaxMagnitude (AimRelativeSwapped's back-stick stop zone);
	// which raw source feeds which logical stick per scheme is decided
	// there and nowhere else. All three raw members are stored in the same "stick-up = -Y"
	// convention (see the handlers), so routing needs no sign flips. All consumers
	// (buildMoveDirectionWorld, buildAimDirection, buildPlayerInput, the character's camera
	// and OGBrawlerSimulatableComponent) must go through these accessors so the routing is
	// observed uniformly. [og-input-latency P5-15] The bodies are BrawlerInputSample's: these
	// two run them on a live sample (game thread), and buildPlayerInput and the sample's own
	// direction builders run them on the sample's copies of the three raw members.
	glm::vec2 getMoveStick() const;
	glm::vec2 getAimStick() const;
	// Returns the mouse look values summed since the last call and resets the sum (drained once per character Tick).
	glm::vec2 consumeLookStick() { const glm::vec2 v = m_lookStick; m_lookStick = glm::vec2(0.f, 0.f); return v; }
	bool getLeftAttack() const { return m_leftAttack; }
	bool getRightAttack() const { return m_rightAttack; }
	// Look mode: a toggle (5 / gamepad Y), not a held button since og-attackstatetransition-cleanup
	// task 12. Each pawn's component starts with it off.
	bool getBlockLook() const { return m_blockLook; }
	// The RAW guard button (Left Shift / gamepad left bumper). [movement-sim task 14] It is a sim
	// input: buildPlayerInput reads it every tick (its copy in the published input sample since
	// og-input-latency P5-15) and, since og-brawler-3rdControllerMode task 5,
	// passes it through dInput::stickRouting::guardFreezeRequested, which keeps it only while
	// the scheme's own movement input (or the actual routed move) is below the move deadzone.
	// That result goes to makeSimPlayerInput, which sets
	// brawlerMovementSimulation::kInputFlagHoldGuard — bit 0 of SyncedPlayerInput's input
	// flags byte, ON THE WIRE, sent and resimulated like any other PlayerInput field.
	// Its reader is step 1's `frozen` gate in brawlerMovementSimulation::integrate.
	// [movement-sim task 15] THE SECOND READER IS GONE. `AOGBrawlerUECharacter::Move`'s
	// suppression of the legacy CMC path was deleted with that path, so this accessor now
	// feeds the simulation and nothing else.
	bool getHoldGuard() const { return m_holdGuard; }
	bool hasInputComponent() const { return m_inputComponent != nullptr; }

private:
	InputMappingUETranslator m_inputTranslator;

	// Game-thread-written caches. Written by updateGameThreadCache() (game thread), read on the
	// game thread by the direction-build helpers and copied into the staged sample. Since
	// og-input-latency P5-15 no other thread reads them: the step reads the published copy. Until
	// then buildAimDirection() / buildMoveDirectionWorld() read them on the physics thread as a
	// "benign race" (glm::vec3 trivially copyable and aligned), the race the sample retired.
	glm::vec3 m_camForwardCache = simFrame::kForward;
	glm::vec3 m_mouseAimCache   = glm::vec3(0.f, 0.f, 0.f);

	// [og-input-latency P5-15] The per-frame input sample. m_stagedSample is game thread only:
	// written by updateGameThreadCache, copied into the mailbox by publishSample. The mailbox is a
	// TripleBuffer: one producer (the game thread, publishSample), one consumer (the step's thread,
	// buildPlayerInput), so a publish never waits on a step and a step never reads a half-written
	// sample. Held on the heap, so the slots' cache-line alignment (alignas(64)) does not
	// over-align the component's own UObject allocation.
	BrawlerInputSample m_stagedSample;
	std::unique_ptr<TripleBuffer<BrawlerInputSample>> m_sampleMailbox;

	// Fills every field of `sample` from the live members, except the diagnostic sampledSeconds.
	// Game thread only. The one place a sample is made from the members: the staged sample and
	// the GT accessors' live sample both come from it, so they cannot disagree on a field.
	void writeSampleFields(BrawlerInputSample& sample) const;
	BrawlerInputSample liveSample() const
	{
		BrawlerInputSample sample;
		writeSampleFields(sample);
		return sample;
	}

#if OG_LATENCY_BUDGET_DIAGNOSTICS
	// DIAGNOSTIC ONLY — READ BY NOTHING IN PRODUCTION. The latency-budget probe's hop H1 start of the
	// sample the latest buildPlayerInput consumed, read only through getDiagnostics(); compiled out with
	// the probe when OG_LATENCY_BUDGET_DIAGNOSTICS is 0, so a production reader does not compile there
	// (OGSimulationUnreal's docs/UELatencyBudgetDiagnostics-rationale.md §3). Until og-input-latency
	// P5-15 this was m_inputSampledSeconds, the latest updateGameThreadCache call's time.
	std::atomic<double> m_consumedSampledSeconds{ 0.0 };
#endif

	// The one write of the staged sample's sampledSeconds (the H1 start): a no-op when
	// OG_LATENCY_BUDGET_DIAGNOSTICS is 0, so its call site in updateGameThreadCache carries no #if.
	void noteInputSampled()
	{
#if OG_LATENCY_BUDGET_DIAGNOSTICS
		m_stagedSample.sampledSeconds = FPlatformTime::Seconds();
#endif
	}

	// The body of updateGameThreadCache before og-input-latency P5-15, after the owner check and the
	// stamp; updateGameThreadCache stages the sample after it, whichever of its returns it took.
	void refreshGameThreadCaches(AOGBrawlerUECharacter& ch);

	// Cached raw input state. m_moveKeys = WASD + D-pad (Move action), m_leftStick = gamepad
	// left stick (MoveStick action), m_rightStick = gamepad right stick (Aim action).
	glm::vec2 m_moveKeys   = glm::vec2(0.f, 0.f);
	glm::vec2 m_leftStick  = glm::vec2(0.f, 0.f);
	glm::vec2 m_rightStick = glm::vec2(0.f, 0.f);
	glm::vec2 m_lookStick  = glm::vec2(0.f, 0.f);
	bool m_leftAttack  = false;
	bool m_rightAttack = false;
	bool m_blockLook   = false;
	bool m_holdGuard   = false;

	// Input-device latch. true ⇒ the most recent deliberate input came from the gamepad (a
	// stick past its deadzone, or the D-pad); false ⇒ from WASD or mouse movement (or initial
	// state). Written on the game thread by onMove / onMoveStick / onAim and by
	// updateGameThreadCache (the cursor), through the dInput::stickRouting::lastInputWasGamepadAfter*
	// rules. Used by buildAimDirection and buildMoveDirectionWorld to gate the "move stick feeds
	// aim" fallback so the rule only fires in the gamepad case, leaving mouse+kbd's mouse-aim
	// behavior untouched.
	bool m_lastMoveInputWasGamepad = false;
	// Where the cursor was when the gamepad took over (dInput::stickRouting::CursorLatch). Empty
	// until the first cursor sample. Game thread only.
	std::optional<glm::vec2> m_cursorLatchAnchor;

	UEnhancedInputComponent* m_inputComponent = nullptr;

	void onMove(const FInputActionValue& Value);
	void onMoveStick(const FInputActionValue& Value);
	void onAim(const FInputActionValue& Value);
	void onLook(const FInputActionValue& Value);
	void onBlockLook(const FInputActionValue& Value);
	void onHoldGuard(const FInputActionValue& Value);
	void onLeftAttack(const FInputActionValue& Value);
	void onRightAttack(const FInputActionValue& Value);
	void onSetSchemeCameraRelative(const FInputActionValue& Value);
	void onSetSchemeAimRelative(const FInputActionValue& Value);
	void onSetSchemeMoveRelativeAim(const FInputActionValue& Value);
	void onSetSchemeAimRelativeSwapped(const FInputActionValue& Value);
	void onToggleControlsLegend(const FInputActionValue& Value);
};
