// SPDX-License-Identifier: BUSL-1.1

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OGSimulationUnreal/InputMappingUETranslator.h"
#include "OGBrawler/SimulatableBrawlerTypes.h"
#include "OGSimulation/SimulationTimeContext.h"
#include "OGSimulation/Network/ClientInputDelayLine.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"

#include "OGBrawlerInputCollectionComponent.generated.h"

class UEnhancedInputComponent;
struct FInputActionValue;
// [T15] The ASimulationManagerUImpl forward declaration is gone with the manager
// argument on buildPlayerInput — this component no longer names the manager at all.

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
	// Writes m_camForwardCache and m_mouseAimCache consumed by the direction-build helpers.
	void updateGameThreadCache();

	// Physics-thread-safe cache reads. Valid after the first updateGameThreadCache() call.
	glm::vec3 resolveCameraForward() const { return m_camForwardCache; }
	glm::vec3 resolveMouseAim() const { return m_mouseAimCache; }

	// Direction-build helpers — physics-thread-safe (read caches + statics only).
	glm::vec3 buildAimDirection() const;
	glm::vec3 buildMoveDirectionWorld() const;
	static glm::vec3 getInputDirectionInCameraSpace(const glm::vec3& camForward, const glm::vec3& inputDirection);

	// --- The three PlayerInput sources, and how they relate (D5.4 / og-netcode-v2 T12) ---
	//
	// There are three ways a PlayerInput reaches a consumer in this project. They are NOT
	// interchangeable; picking the wrong one is a correctness bug, not a style choice.
	//
	//  1. buildPlayerInput(step, componentId, delayLine)   — THE SIM PATH.
	//     Called once per simulation tick from the inputProvider lambda on the physics
	//     thread. Continuous fields + discrete fields (attack buttons) + the tick-stateful
	//     motion-sequence matcher (Hadouken and friends), which needs a RAW CAPTURE history
	//     and the tick number to do rising-edge detection against the previous tick's input.
	//     This is the ONLY input that is simulated and replicated. Rate: sim tick (60 Hz).
	//
	//  2. buildLatestVisualizationInput()                — THE RENDER ECHO (visualization only).
	//     A live re-sample of the CONTINUOUS fields only, safe to call at render-frame rate.
	//     Shares the continuous read with (1) via simulatableBrawler::readContinuousInputFields,
	//     so the two provably cannot drift; differs only in that it calls
	//     makeVisualizationPlayerInput instead of makeSimPlayerInput, leaving every discrete
	//     field neutral (triggeredActionId == inputSequence::kNoMatch, attacks false).
	//     The motion matcher is NEVER invoked here — running it at render rate would misfire
	//     it, since many render frames share one "previous tick". Rate: render frame.
	//     Cosmetic only: never feed this to the simulation or to the input RPC.
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
	// [T15] `delayLine` is this character's OWN raw capture history, keyed by capture tick, and is
	// the motion matcher's history source. It is a parameter, not something reached for: NetSync
	// binds it in collectInputAll and calls this BEFORE pushing the current tick's capture, so the
	// line holds ticks <= step.getTick() - 1 and the current sample is what this function returns.
	//
	// It replaces the old `const ASimulationManagerUImpl*` argument, which existed only so the
	// matcher could reach manager -> reconciliation -> correction cache. That cache stores the
	// APPLIED input keyed by APPLICATION tick, so under an input delay `d` every history read was
	// displaced by `d` — see OGBrawler/BrawlerMotionMatching.h for the full statement of the defect
	// and its exact bound. There is also no longer a "no history available" arm: the delay line
	// exists iff an input provider is registered, and this method's only caller IS that provider.
	simulatableBrawler::PlayerInput buildPlayerInput(
		const SimulationTimeStep& step,
		uint32 componentId,
		const ClientInputDelayLine<simulatableBrawler::PlayerInput>& delayLine) const;

	// Render-frame-callable live sample of the CONTINUOUS input fields only. See the block
	// comment above for how this relates to buildPlayerInput and to source (3). Takes no
	// SimulationTimeStep, no componentId and no history precisely because it touches none of
	// the tick/history context the motion matcher would need — the matcher is not run.
	simulatableBrawler::PlayerInput buildLatestVisualizationInput() const;

	// Logical stick accessors. Both raw members end up with the same "stick-up = -Y in
	// storage" convention but get there differently: onMove explicitly negates v.Y (left
	// stick raw is +Y for stick-up); onAim stores raw (right stick raw is already -Y for
	// stick-up per the IMC asset). With both members in the same storage convention, the
	// swap is a straight pointer swap with no sign flips. When g_swapMoveAndAimSticks is
	// true, getMoveStick() returns the raw aim stick and getAimStick() returns the raw
	// move stick. All consumers (Move, buildMoveDirectionWorld, buildAimDirection,
	// buildPlayerInput) must route through these accessors so the swap is observed uniformly.
	glm::vec2 getMoveStick() const;
	glm::vec2 getAimStick() const;
	// Returns the current mouse delta and resets it to zero (consumed each frame by character Tick).
	glm::vec2 consumeLookStick() { const glm::vec2 v = m_lookStick; m_lookStick = glm::vec2(0.f, 0.f); return v; }
	bool getLeftAttack() const { return m_leftAttack; }
	bool getRightAttack() const { return m_rightAttack; }
	bool getBlockLook() const { return m_blockLook; }
	bool getHoldGuard() const { return m_holdGuard; }
	bool hasInputComponent() const { return m_inputComponent != nullptr; }

private:
	InputMappingUETranslator m_inputTranslator;

	// Game-thread-written caches. Written by updateGameThreadCache() (game thread),
	// read by buildAimDirection() / buildMoveDirectionWorld() (physics thread).
	// glm::vec3 is trivially copyable and aligned — same benign-race pattern as the
	// pre-refactor m_camForward on SimmableUpdateComponent.
	glm::vec3 m_camForwardCache = glm::vec3(1.f, 0.f, 0.f);
	glm::vec3 m_mouseAimCache   = glm::vec3(0.f, 0.f, 0.f);

	// Cached raw input state
	glm::vec2 m_moveStick  = glm::vec2(0.f, 0.f);
	glm::vec2 m_aimStick   = glm::vec2(0.f, 0.f);
	glm::vec2 m_lookStick  = glm::vec2(0.f, 0.f);
	bool m_leftAttack  = false;
	bool m_rightAttack = false;
	bool m_blockLook   = false;
	bool m_holdGuard   = false;

	// Source-of-move latch updated each onMove call. true ⇒ most recent non-zero move
	// input came from the gamepad left stick; false ⇒ from WASD (or initial state).
	// Used by buildAimDirection to gate the "move stick feeds aim" fallback so the rule
	// only fires in the gamepad case, leaving mouse+kbd's mouse-aim behavior untouched.
	bool m_lastMoveInputWasGamepad = false;

	UEnhancedInputComponent* m_inputComponent = nullptr;

	glm::vec3 buildMoveDirectionWorldFor(const glm::vec3& referenceForward) const;

	void onMove(const FInputActionValue& Value);
	void onAim(const FInputActionValue& Value);
	void onLook(const FInputActionValue& Value);
	void onBlockLook(const FInputActionValue& Value);
	void onHoldGuard(const FInputActionValue& Value);
	void onLeftAttack(const FInputActionValue& Value);
	void onRightAttack(const FInputActionValue& Value);
	void onSetSchemeCameraRelative(const FInputActionValue& Value);
	void onSetSchemeAimRelative(const FInputActionValue& Value);
	void onSetSchemeMoveRelativeAim(const FInputActionValue& Value);
};
