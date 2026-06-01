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
	UInputAction* LeftAttackAction  = m_inputTranslator.getAction(dInput::gameMapping::LeftAttack);
	UInputAction* RightAttackAction = m_inputTranslator.getAction(dInput::gameMapping::RightAttack);
	UInputAction* JumpAction        = m_inputTranslator.getAction(dInput::gameMapping::Jump);

	ic->BindAction(MoveAction,        ETriggerEvent::Triggered,  this, &UOGBrawlerInputCollectionComponent::onMove);
	ic->BindAction(MoveAction,        ETriggerEvent::Completed,  this, &UOGBrawlerInputCollectionComponent::onMove);
	ic->BindAction(AimAction,         ETriggerEvent::Triggered,  this, &UOGBrawlerInputCollectionComponent::onAim);
	ic->BindAction(LookAction,        ETriggerEvent::Triggered,  this, &UOGBrawlerInputCollectionComponent::onLook);
	ic->BindAction(LookAction,        ETriggerEvent::None,       this, &UOGBrawlerInputCollectionComponent::onLook);
	ic->BindAction(BlockLookAction,   ETriggerEvent::Triggered,  this, &UOGBrawlerInputCollectionComponent::onBlockLook);
	ic->BindAction(BlockLookAction,   ETriggerEvent::Completed,  this, &UOGBrawlerInputCollectionComponent::onBlockLook);
	ic->BindAction(LeftAttackAction,  ETriggerEvent::Triggered,  this, &UOGBrawlerInputCollectionComponent::onLeftAttack);
	ic->BindAction(LeftAttackAction,  ETriggerEvent::Completed,  this, &UOGBrawlerInputCollectionComponent::onLeftAttack);
	ic->BindAction(RightAttackAction, ETriggerEvent::Triggered,  this, &UOGBrawlerInputCollectionComponent::onRightAttack);
	ic->BindAction(RightAttackAction, ETriggerEvent::Completed,  this, &UOGBrawlerInputCollectionComponent::onRightAttack);

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

glm::vec3 UOGBrawlerInputCollectionComponent::buildAimDirection() const
{
	const glm::vec3 aimStick3 = glm::vec3(m_aimStick, 0.f);
	if (glm::length(aimStick3) > 0.2f)
		return getInputDirectionInCameraSpace(m_camForwardCache, aimStick3);
	if (glm::length(m_mouseAimCache) > 0.2f)
		return glm::normalize(m_mouseAimCache);
	glm::vec3 cf = m_camForwardCache;
	cf.z = 0.f;
	return glm::normalize(cf);
}

glm::vec3 UOGBrawlerInputCollectionComponent::buildMoveDirectionWorld() const
{
	if (dAttackMachineSimulation::MovementAndAimModeTest == 1)
		return getInputDirectionInAimSpace(buildAimDirection(), glm::vec3(m_moveStick, 0.f));
	return getInputDirectionInCameraSpace(m_camForwardCache, glm::vec3(m_moveStick, 0.f));
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

glm::vec3 UOGBrawlerInputCollectionComponent::getInputDirectionInAimSpace(const glm::vec3& aimDirection, const glm::vec3& inputDirection)
{
	glm::vec3 aimDirectionNormalized = aimDirection;
	aimDirectionNormalized.z = 0.f;
	aimDirectionNormalized = glm::normalize(aimDirectionNormalized);

	glm::mat4 aimRotationMatrix;
	dMathUtil::getRotationMatrix(glm::vec3(0.f, -1.f, 0.f), aimDirectionNormalized, aimRotationMatrix);

	glm::vec3 normalizedDirection = glm::normalize(inputDirection);
	glm::vec4 direction4(normalizedDirection, 0.f);
	return glm::vec3(aimRotationMatrix * direction4);
}

void UOGBrawlerInputCollectionComponent::onMove(const FInputActionValue& Value)
{
	const FVector2D v = Value.Get<FVector2D>();
	// Store (X, -Y): stick-up produces negative UE Y but positive world-forward
	// after getInputDirectionInCameraSpace rotation — matches pre-refactor setMoveInput.
	m_moveStick = glm::vec2(v.X, v.Y * -1.f);
}

void UOGBrawlerInputCollectionComponent::onAim(const FInputActionValue& Value)
{
	const FVector2D v = Value.Get<FVector2D>();
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
