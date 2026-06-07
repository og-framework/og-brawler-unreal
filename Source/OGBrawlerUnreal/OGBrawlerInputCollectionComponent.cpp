// SPDX-License-Identifier: BUSL-1.1

#include "OGBrawlerInputCollectionComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "OGBrawler/InputMapping/GameInputMapping.h"
#include "OGBrawlerUnreal/OGBrawlerUECharacter.h"
#include "OGBrawlerUnreal/SimulationManagerUImpl.h"
#include "OGSimulationUnreal/UGLMTypeConversion.h"
#include "OGBrawler/DAttackMachineSimulationRuntimeTweakables.h"
#include "OGSimulation/DMathUtil.h"

UOGBrawlerInputCollectionComponent::UOGBrawlerInputCollectionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UOGBrawlerInputCollectionComponent::initializeTranslator()
{
	m_inputTranslator.initialize(GetOwner(), dInput::gameMapping::buildDefaultContext());
}

void UOGBrawlerInputCollectionComponent::addInputMappingContextForController(AController* InController)
{
	APlayerController* pc = Cast<APlayerController>(InController);
	if (pc == nullptr) return;
	UEnhancedInputLocalPlayerSubsystem* subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(pc->GetLocalPlayer());
	if (subsystem == nullptr) return;
	m_inputTranslator.addToSubsystem(subsystem, 0);
}

void UOGBrawlerInputCollectionComponent::setupBindings(UEnhancedInputComponent* ic)
{
	m_inputComponent = ic;

	UInputAction* MoveAction        = m_inputTranslator.getAction(dInput::gameMapping::Move);
	UInputAction* AimAction         = m_inputTranslator.getAction(dInput::gameMapping::Aim);
	UInputAction* LookAction        = m_inputTranslator.getAction(dInput::gameMapping::Look);
	UInputAction* BlockLookAction   = m_inputTranslator.getAction(dInput::gameMapping::BlockLook);
	UInputAction* HoldGuardAction   = m_inputTranslator.getAction(dInput::gameMapping::HoldGuard);
	UInputAction* LeftAttackAction  = m_inputTranslator.getAction(dInput::gameMapping::LeftAttack);
	UInputAction* RightAttackAction = m_inputTranslator.getAction(dInput::gameMapping::RightAttack);
	UInputAction* JumpAction        = m_inputTranslator.getAction(dInput::gameMapping::Jump);
	UInputAction* SetSchemeCameraRelativeAction  = m_inputTranslator.getAction(dInput::gameMapping::SetSchemeCameraRelative);
	UInputAction* SetSchemeAimRelativeAction     = m_inputTranslator.getAction(dInput::gameMapping::SetSchemeAimRelative);
	UInputAction* SetSchemeMoveRelativeAimAction = m_inputTranslator.getAction(dInput::gameMapping::SetSchemeMoveRelativeAim);

	ic->BindAction(MoveAction,        ETriggerEvent::Triggered,  this, &UOGBrawlerInputCollectionComponent::onMove);
	ic->BindAction(MoveAction,        ETriggerEvent::Completed,  this, &UOGBrawlerInputCollectionComponent::onMove);
	ic->BindAction(AimAction,         ETriggerEvent::Triggered,  this, &UOGBrawlerInputCollectionComponent::onAim);
	ic->BindAction(LookAction,        ETriggerEvent::Triggered,  this, &UOGBrawlerInputCollectionComponent::onLook);
	ic->BindAction(LookAction,        ETriggerEvent::None,       this, &UOGBrawlerInputCollectionComponent::onLook);
	ic->BindAction(BlockLookAction,   ETriggerEvent::Triggered,  this, &UOGBrawlerInputCollectionComponent::onBlockLook);
	ic->BindAction(BlockLookAction,   ETriggerEvent::Completed,  this, &UOGBrawlerInputCollectionComponent::onBlockLook);
	ic->BindAction(HoldGuardAction,   ETriggerEvent::Triggered,  this, &UOGBrawlerInputCollectionComponent::onHoldGuard);
	ic->BindAction(HoldGuardAction,   ETriggerEvent::Completed,  this, &UOGBrawlerInputCollectionComponent::onHoldGuard);
	ic->BindAction(LeftAttackAction,  ETriggerEvent::Triggered,  this, &UOGBrawlerInputCollectionComponent::onLeftAttack);
	ic->BindAction(LeftAttackAction,  ETriggerEvent::Completed,  this, &UOGBrawlerInputCollectionComponent::onLeftAttack);
	ic->BindAction(RightAttackAction, ETriggerEvent::Triggered,  this, &UOGBrawlerInputCollectionComponent::onRightAttack);
	ic->BindAction(RightAttackAction, ETriggerEvent::Completed,  this, &UOGBrawlerInputCollectionComponent::onRightAttack);
	// Movement-scheme switches — fire on Started (single edge per press, no auto-repeat).
	ic->BindAction(SetSchemeCameraRelativeAction,  ETriggerEvent::Started, this, &UOGBrawlerInputCollectionComponent::onSetSchemeCameraRelative);
	ic->BindAction(SetSchemeAimRelativeAction,     ETriggerEvent::Started, this, &UOGBrawlerInputCollectionComponent::onSetSchemeAimRelative);
	ic->BindAction(SetSchemeMoveRelativeAimAction, ETriggerEvent::Started, this, &UOGBrawlerInputCollectionComponent::onSetSchemeMoveRelativeAim);

	if (ACharacter* ch = Cast<ACharacter>(GetOwner()))
	{
		ic->BindAction(JumpAction, ETriggerEvent::Started,   ch, &ACharacter::Jump);
		ic->BindAction(JumpAction, ETriggerEvent::Completed, ch, &ACharacter::StopJumping);
	}
}

void UOGBrawlerInputCollectionComponent::updateGameThreadCache()
{
	AOGBrawlerUECharacter* ch = Cast<AOGBrawlerUECharacter>(GetOwner());
	if (ch == nullptr)
		return;

	// Resolve camera forward: PCM with FollowCamera fallback.
	// Single home for the resolution previously duplicated in OGBrawlerUECharacter::Tick
	// and OGBrawlerUECharacter::Move (pain point F).
	if (const APlayerController* pc = Cast<APlayerController>(ch->GetController()))
	{
		if (pc->PlayerCameraManager != nullptr)
			m_camForwardCache = uglm::toGLMVec3(pc->PlayerCameraManager->GetCameraRotation().Vector());
		else
			m_camForwardCache = uglm::toGLMVec3(ch->GetFollowCamera()->GetForwardVector());
	}
	else
	{
		m_camForwardCache = uglm::toGLMVec3(ch->GetFollowCamera()->GetForwardVector());
	}

	// Mouse cursor visibility + mouse-aim resolution.
	APlayerController* pc = Cast<APlayerController>(ch->GetController());
	if (pc == nullptr)
	{
		m_mouseAimCache = glm::vec3(0.f, 0.f, 0.f);
		return;
	}

	pc->SetShowMouseCursor(!m_blockLook);

	if (m_blockLook)
	{
		m_mouseAimCache = glm::vec3(0.f, 0.f, 0.f);
		return;
	}

	// Line-plane intersection: project mouse onto z=0 plane through the capsule center.
	// Previously in AOGBrawlerUECharacter::Tick:310-325.
	FVector worldLocation;
	FVector worldDirection;
	pc->DeprojectMousePositionToWorld(worldLocation, worldDirection);

	const FVector planeNormal = FVector(0.f, 0.f, 1.f);
	const FVector planePoint  = ch->GetCapsuleComponent()->GetComponentTransform().GetTranslation();
	const FVector planePointToLinePoint = worldLocation - planePoint;
	const float dotProduct = FVector::DotProduct(planeNormal, worldDirection);
	if (FMath::Abs(dotProduct) < KINDA_SMALL_NUMBER)
	{
		m_mouseAimCache = glm::vec3(0.f, 0.f, 0.f);
		return;
	}
	const float distance = FVector::DotProduct(planeNormal, planePointToLinePoint) / dotProduct;
	const FVector intersectionPoint = worldLocation - distance * worldDirection;
	const FVector aimVec = intersectionPoint - planePoint;
	if (aimVec.IsNearlyZero())
	{
		m_mouseAimCache = glm::vec3(0.f, 0.f, 0.f);
		return;
	}
	m_mouseAimCache = glm::normalize(uglm::toGLMVec3(aimVec));
}

glm::vec2 UOGBrawlerInputCollectionComponent::getMoveStick() const
{
	return dAttackMachineSimulation::g_swapMoveAndAimSticks.load() ? m_aimStick : m_moveStick;
}

glm::vec2 UOGBrawlerInputCollectionComponent::getAimStick() const
{
	return dAttackMachineSimulation::g_swapMoveAndAimSticks.load() ? m_moveStick : m_aimStick;
}

glm::vec3 UOGBrawlerInputCollectionComponent::buildAimDirection() const
{
	const float aimDeadzone = dAttackMachineSimulation::g_aimStickDeadzone.load();
	const glm::vec3 aimStick3 = glm::vec3(getAimStick(), 0.f);
	if (glm::length(aimStick3) > aimDeadzone)
	{
		// MoveRelativeAim: aim stick rotates around the current move direction (so
		// aim-stick-up means aim direction equals move direction). When no movement is
		// happening, fall back to camera-forward as the reference so aim is still usable
		// — that matches CameraRelative/AimRelative aim-stick behavior when idle.
		glm::vec3 aimReference = m_camForwardCache;
		if (dAttackMachineSimulation::g_movementScheme == dAttackMachineSimulation::MovementScheme::MoveRelativeAim)
		{
			const glm::vec3 moveDir = buildMoveDirectionWorld();
			if (glm::length(moveDir) > KINDA_SMALL_NUMBER)
				aimReference = moveDir;
		}
		return getInputDirectionInCameraSpace(aimReference, aimStick3);
	}

	// Gamepad-only fallback: aim stick below deadzone AND move stick above its deadzone
	// AND the most recent move input came from the gamepad left stick (not WASD) AND the
	// feature is enabled. Derives aim from the move stick (camera-relative rotation).
	// Paired with the matching fallback in buildMoveDirectionWorld so aim and move
	// resolve to the same camera-relative move-stick direction. Skipped for WASD-driven
	// moves so mouse+kbd players keep their mouse aim while moving.
	const float moveDeadzone = dAttackMachineSimulation::g_moveStickDeadzone.load();
	const glm::vec3 moveStick3 = glm::vec3(getMoveStick(), 0.f);
	if (dAttackMachineSimulation::g_gamepadMoveStickFeedsAim.load()
		&& m_lastMoveInputWasGamepad
		&& glm::length(moveStick3) > moveDeadzone)
		return getInputDirectionInCameraSpace(m_camForwardCache, moveStick3);

	// Fall back to mouse aim if present, else camera forward.
	if (glm::length(m_mouseAimCache) > 0.2f)
		return glm::normalize(m_mouseAimCache);
	glm::vec3 cf = m_camForwardCache;
	cf.z = 0.f;
	return glm::normalize(cf);
}

glm::vec3 UOGBrawlerInputCollectionComponent::buildMoveDirectionWorldFor(const glm::vec3& referenceForward) const
{
	return getInputDirectionInCameraSpace(referenceForward, glm::vec3(getMoveStick(), 0.f));
}

glm::vec3 UOGBrawlerInputCollectionComponent::buildMoveDirectionWorld() const
{
	// Below the move-stick deadzone there is no meaningful direction — return zero so
	// callers (sim PlayerInput, CMC Move) do not normalize a near-zero vector into NaN.
	// Sim consumers already handle a zero moveDirectionWorld via the existing
	// length-of-moveDirection safety check in integrate3.
	const float moveDeadzone = dAttackMachineSimulation::g_moveStickDeadzone.load();
	if (glm::length(getMoveStick()) < moveDeadzone)
		return glm::vec3(0.f, 0.f, 0.f);

	if (dAttackMachineSimulation::g_movementScheme == dAttackMachineSimulation::MovementScheme::AimRelative)
	{
		// Rotate around aim except in the gamepad-fallback case where aim was derived
		// from the move stick (aim stick below deadzone, last move was gamepad). In that
		// one case rotating around it would be a self-reference, so fall through to
		// camera-relative — matching buildAimDirection's gamepad fallback so the move
		// direction lines up with the (move-stick-derived) aim direction.
		const float aimDeadzone = dAttackMachineSimulation::g_aimStickDeadzone.load();
		const bool aimStickActive = glm::length(getAimStick()) > aimDeadzone;
		const bool gamepadAimFallback =
			dAttackMachineSimulation::g_gamepadMoveStickFeedsAim.load()
			&& !aimStickActive
			&& m_lastMoveInputWasGamepad;
		if (!gamepadAimFallback)
			return buildMoveDirectionWorldFor(buildAimDirection());
	}
	return buildMoveDirectionWorldFor(m_camForwardCache);
}

glm::vec3 UOGBrawlerInputCollectionComponent::getInputDirectionInCameraSpace(const glm::vec3& camForward, const glm::vec3& inputDirection)
{
	glm::vec3 camForwardNormalized = camForward;
	camForwardNormalized.z = 0.f;
	camForwardNormalized = glm::normalize(camForwardNormalized);

	glm::mat4 camRotationMatrix;
	dMathUtil::getRotationMatrix(glm::vec3(0.f, -1.f, 0.f), camForwardNormalized, camRotationMatrix);

	glm::vec3 normalizedDirection = glm::normalize(inputDirection);
	glm::vec4 direction4(normalizedDirection, 0.f);
	return glm::vec3(camRotationMatrix * direction4);
}

void UOGBrawlerInputCollectionComponent::onMove(const FInputActionValue& Value)
{
	const FVector2D v = Value.Get<FVector2D>();
	// Store (X, -Y): stick-up produces negative UE Y but positive world-forward
	// after getInputDirectionInCameraSpace rotation — matches pre-refactor setMoveInput.
	m_moveStick = glm::vec2(v.X, v.Y * -1.f);

	// Latch the input source for non-zero events. WASD and the gamepad left stick both
	// feed this same Move action (see GameInputMapping.cpp Move bindings) — to keep
	// buildAimDirection's gamepad-only fallback from triggering on WASD, we check whether
	// any WASD key is actually held down right now. If yes → keyboard; if no but value
	// is non-zero → must be the gamepad. We don't update on near-zero events (Completed
	// release) so the latch stays meaningful between input bursts.
	if (!v.IsNearlyZero())
	{
		const ACharacter* ch = Cast<ACharacter>(GetOwner());
		const APlayerController* pc = ch ? Cast<APlayerController>(ch->GetController()) : nullptr;
		if (pc != nullptr)
		{
			const bool kbdMoveDown =
				pc->IsInputKeyDown(EKeys::W) || pc->IsInputKeyDown(EKeys::A) ||
				pc->IsInputKeyDown(EKeys::S) || pc->IsInputKeyDown(EKeys::D);
			m_lastMoveInputWasGamepad = !kbdMoveDown;
		}
	}
}

void UOGBrawlerInputCollectionComponent::onAim(const FInputActionValue& Value)
{
	const FVector2D v = Value.Get<FVector2D>();
	// NOTE the asymmetry with onMove: the project's IMC asset emits opposite Y conventions
	// for the two sticks. The left (move) stick yields v.Y = +1 for stick-up (negated below
	// in onMove); the right (aim) stick yields v.Y = -1 for stick-up (stored as-is here).
	// Each handler compensates for its own stick so that downstream code sees the same
	// (stick-up = -Y in storage) convention. This is what lets the swap accessor work as a
	// straight pointer swap without per-stick sign flips.
	m_aimStick = glm::vec2(v.X, v.Y);
}

void UOGBrawlerInputCollectionComponent::onLook(const FInputActionValue& Value)
{
	const FVector2D v = Value.Get<FVector2D>();
	m_lookStick = glm::vec2(v.X, v.Y);
}

void UOGBrawlerInputCollectionComponent::onBlockLook(const FInputActionValue& Value)
{
	m_blockLook = Value.Get<bool>();
}

void UOGBrawlerInputCollectionComponent::onHoldGuard(const FInputActionValue& Value)
{
	m_holdGuard = Value.Get<bool>();
}

void UOGBrawlerInputCollectionComponent::onSetSchemeCameraRelative(const FInputActionValue& /*Value*/)
{
	dAttackMachineSimulation::g_movementScheme = dAttackMachineSimulation::MovementScheme::CameraRelative;
}

void UOGBrawlerInputCollectionComponent::onSetSchemeAimRelative(const FInputActionValue& /*Value*/)
{
	dAttackMachineSimulation::g_movementScheme = dAttackMachineSimulation::MovementScheme::AimRelative;
}

void UOGBrawlerInputCollectionComponent::onSetSchemeMoveRelativeAim(const FInputActionValue& /*Value*/)
{
	dAttackMachineSimulation::g_movementScheme = dAttackMachineSimulation::MovementScheme::MoveRelativeAim;
}

void UOGBrawlerInputCollectionComponent::onLeftAttack(const FInputActionValue& Value)
{
	m_leftAttack = Value.Get<bool>();
}

void UOGBrawlerInputCollectionComponent::onRightAttack(const FInputActionValue& Value)
{
	m_rightAttack = Value.Get<bool>();
}

simulatableBrawler::PlayerInput UOGBrawlerInputCollectionComponent::buildPlayerInput(const SimulationTimeStep& step, uint32 componentId) const
{
	if (!hasInputComponent())
		return simulatableBrawler::getZeroPlayerInput();

	const glm::vec3 aimDirection = buildAimDirection();
	const glm::vec2 moveStick = getMoveStick();
	const glm::vec3 moveDirectionWorld = buildMoveDirectionWorld();
	const bool leftAttack  = getLeftAttack();
	const bool rightAttack = getRightAttack();
	UE_LOG(LogOGSimTick, Log,
		TEXT("[ClientPrediction] id=%u tick=%u attackLeft=%d"),
		componentId, step.getTick(), leftAttack ? 1 : 0);
	return simulatableBrawler::PlayerInput(
		dAttackRadialSimulation::PlayerInput(aimDirection, leftAttack, rightAttack),
		dAttackMachineSimulation::PlayerInput(aimDirection, leftAttack, rightAttack, moveStick, moveDirectionWorld),
		dAttackGuardSimulation::PlayerInput(aimDirection));
}
