// SPDX-License-Identifier: BUSL-1.1

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OGSimulationUnreal/InputMappingUETranslator.h"
#include "OGBrawler/SimulatableBrawlerTypes.h"
#include "OGSimulation/SimulationTimeContext.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"

#include "OGBrawlerInputCollectionComponent.generated.h"

class UEnhancedInputComponent;
struct FInputActionValue;

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
	static glm::vec3 getInputDirectionInAimSpace(const glm::vec3& aimDirection, const glm::vec3& inputDirection);

	// Builds the full sim-tick PlayerInput. Called from the inputProvider lambda on the physics thread.
	simulatableBrawler::PlayerInput buildPlayerInput(const SimulationTimeStep& step, uint32 componentId) const;

	// Cached raw input — single source of truth per value; no duplicates across owners.
	// x = right, y = forward; Y is stored negated to match pre-refactor setMoveInput convention
	// (stick up → positive world-forward after getInputDirectionInCameraSpace rotation).
	const glm::vec2& getMoveStick() const { return m_moveStick; }
	const glm::vec2& getAimStick() const { return m_aimStick; }
	// Returns the current mouse delta and resets it to zero (consumed each frame by character Tick).
	glm::vec2 consumeLookStick() { const glm::vec2 v = m_lookStick; m_lookStick = glm::vec2(0.f, 0.f); return v; }
	bool getLeftAttack() const { return m_leftAttack; }
	bool getRightAttack() const { return m_rightAttack; }
	bool getBlockLook() const { return m_blockLook; }
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

	UEnhancedInputComponent* m_inputComponent = nullptr;

	void onMove(const FInputActionValue& Value);
	void onAim(const FInputActionValue& Value);
	void onLook(const FInputActionValue& Value);
	void onBlockLook(const FInputActionValue& Value);
	void onLeftAttack(const FInputActionValue& Value);
	void onRightAttack(const FInputActionValue& Value);
};
