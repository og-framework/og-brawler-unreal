// SPDX-License-Identifier: BUSL-1.1

#include "OGBrawlerInputCollectionComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "OGBrawler/InputMapping/GameInputMapping.h"
#include "OGBrawler/InputMapping/StickRouting.h"
#include "OGBrawlerUnreal/OGBrawlerUECharacter.h"
#include "OGBrawlerUnreal/SimulationManagerUImpl.h"
#include "OGSimulationUnreal/UGLMTypeConversion.h"
#include "OGBrawler/DAttackMachineSimulationRuntimeTweakables.h"
#include "OGSimulation/DMathUtil.h"
#include "OGBrawler/BrawlerProjectileSimulation.h"
#include "OGBrawler/BrawlerInputPackaging.h"
#include "OGBrawler/BrawlerMotionMatching.h"
#include "OGBrawler/InputSequence/InputSequence.h"
#include "OGBrawler/InputSequence/GameMotions.h"

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
	UInputAction* MoveStickAction   = m_inputTranslator.getAction(dInput::gameMapping::MoveStick);
	UInputAction* AimAction         = m_inputTranslator.getAction(dInput::gameMapping::Aim);
	UInputAction* LookAction        = m_inputTranslator.getAction(dInput::gameMapping::Look);
	UInputAction* BlockLookAction   = m_inputTranslator.getAction(dInput::gameMapping::BlockLook);
	UInputAction* HoldGuardAction   = m_inputTranslator.getAction(dInput::gameMapping::HoldGuard);
	UInputAction* LeftAttackAction  = m_inputTranslator.getAction(dInput::gameMapping::LeftAttack);
	UInputAction* RightAttackAction = m_inputTranslator.getAction(dInput::gameMapping::RightAttack);
	// [movement-sim task 15] The Jump ACTION still exists in the mapping; only its binding to
	// the engine's stock movement component is gone (ruling #1 = defer jump to a later task).
	// ⭐ [movement-sim task 19] AND THE ENGINE-SIDE CALLEE WENT WITH IT: this pawn no longer
	// derives from the engine's walking-pawn base, so there is no inherited `Jump()` left to
	// bind to even if somebody wanted to. When jump returns it becomes a bit in
	// `brawlerMovementSimulation::PlayerInput::flags` (task 21's reserved bit) — see the
	// standing input-wire rule on that type.
	UInputAction* SetSchemeCameraRelativeAction  = m_inputTranslator.getAction(dInput::gameMapping::SetSchemeCameraRelative);
	UInputAction* SetSchemeAimRelativeAction     = m_inputTranslator.getAction(dInput::gameMapping::SetSchemeAimRelative);
	UInputAction* SetSchemeMoveRelativeAimAction = m_inputTranslator.getAction(dInput::gameMapping::SetSchemeMoveRelativeAim);
	UInputAction* SetSchemeAimRelativeSwappedAction = m_inputTranslator.getAction(dInput::gameMapping::SetSchemeAimRelativeSwapped);

	ic->BindAction(MoveAction,        ETriggerEvent::Triggered,  this, &UOGBrawlerInputCollectionComponent::onMove);
	ic->BindAction(MoveAction,        ETriggerEvent::Completed,  this, &UOGBrawlerInputCollectionComponent::onMove);
	ic->BindAction(MoveStickAction,   ETriggerEvent::Triggered,  this, &UOGBrawlerInputCollectionComponent::onMoveStick);
	ic->BindAction(MoveStickAction,   ETriggerEvent::Completed,  this, &UOGBrawlerInputCollectionComponent::onMoveStick);
	ic->BindAction(AimAction,         ETriggerEvent::Triggered,  this, &UOGBrawlerInputCollectionComponent::onAim);
	// Completed too, so a released right stick reads exactly zero instead of keeping the
	// last Triggered value (it is the move stick in AimRelativeSwapped).
	ic->BindAction(AimAction,         ETriggerEvent::Completed,  this, &UOGBrawlerInputCollectionComponent::onAim);
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
	ic->BindAction(SetSchemeAimRelativeSwappedAction, ETriggerEvent::Started, this, &UOGBrawlerInputCollectionComponent::onSetSchemeAimRelativeSwapped);
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

namespace
{
	dInput::stickRouting::LogicalSticks routeRawSticks(
		const glm::vec2& moveKeys, const glm::vec2& leftStick, const glm::vec2& rightStick)
	{
		return dInput::stickRouting::routeSticks(
			dInput::stickRouting::StickSources{ moveKeys, leftStick, rightStick },
			dAttackMachineSimulation::g_movementScheme.load(),
			dAttackMachineSimulation::g_swapMoveAndAimSticks.load(),
			dAttackMachineSimulation::g_gamepadMoveStickFeedsAim.load(),
			dAttackMachineSimulation::g_moveStickDeadzone.load());
	}

	// Development-only check (compiled out of Shipping): every buildAimDirection branch must
	// return a unit vector.
	glm::vec3 checkUnitAim(const glm::vec3& aim)
	{
#if !UE_BUILD_SHIPPING
		const float aimLength = glm::length(aim);
		ensureMsgf(FMath::Abs(aimLength - 1.f) <= 1e-3f,
			TEXT("buildAimDirection returned a non-unit aim (%f, %f, %f), length %f"),
			aim.x, aim.y, aim.z, aimLength);
#endif
		return aim;
	}
} // namespace

glm::vec2 UOGBrawlerInputCollectionComponent::getMoveStick() const
{
	return routeRawSticks(m_moveKeys, m_leftStick, m_rightStick).move;
}

glm::vec2 UOGBrawlerInputCollectionComponent::getAimStick() const
{
	return routeRawSticks(m_moveKeys, m_leftStick, m_rightStick).aim;
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
		return checkUnitAim(getInputDirectionInCameraSpace(aimReference, aimStick3));
	}

	// Gamepad-only fallback: aim stick below deadzone AND move stick above its deadzone
	// AND the most recent stick/move input came from the gamepad (not WASD) AND the
	// feature is enabled. Derives aim from the move stick (camera-relative rotation).
	// Paired with the matching fallback in buildMoveDirectionWorld so aim and move
	// resolve to the same camera-relative move-stick direction. Skipped for WASD-driven
	// moves so mouse+kbd players keep their mouse aim while moving.
	const float moveDeadzone = dAttackMachineSimulation::g_moveStickDeadzone.load();
	const glm::vec3 moveStick3 = glm::vec3(getMoveStick(), 0.f);
	if (dAttackMachineSimulation::g_gamepadMoveStickFeedsAim.load()
		&& m_lastMoveInputWasGamepad
		&& glm::length(moveStick3) > moveDeadzone)
		return checkUnitAim(getInputDirectionInCameraSpace(m_camForwardCache, moveStick3));

	// Fall back to mouse aim if present, else camera forward.
	if (glm::length(m_mouseAimCache) > 0.2f)
		return checkUnitAim(glm::normalize(m_mouseAimCache));
	glm::vec3 cf = m_camForwardCache;
	cf.z = 0.f;
	return checkUnitAim(glm::normalize(cf));
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

	if (dAttackMachineSimulation::isAimRelativeFamily(dAttackMachineSimulation::g_movementScheme.load()))
	{
		// Rotate around aim except in the gamepad-fallback case where aim was derived
		// from the move stick (aim stick below deadzone, last input was gamepad). In that
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
	// Store (X, -Y): key-up (W / D-pad up) produces positive Y here, flipped to the shared
	// "up = -Y in storage" convention. No clamp here: the summed length can exceed 1 (W+D,
	// or keys plus the left stick), and dInput::stickRouting::routeSticks clamps the sum it
	// actually uses to the unit disk.
	m_moveKeys = glm::vec2(v.X, v.Y * -1.f);

	// Latch the input source for non-zero events. WASD and the gamepad D-pad both feed this
	// Move action (see GameInputMapping.cpp Move bindings; the left stick is MoveStick) — to
	// keep buildAimDirection's gamepad-only fallback from triggering on WASD, we check which
	// physical source is actually held right now: any held D-pad direction means gamepad,
	// otherwise the event came from WASD. We don't update on near-zero events (Completed
	// release) so the latch stays meaningful between input bursts.
	if (!v.IsNearlyZero())
	{
		// ⭐ [movement-sim task 19] `APawn` IS THE NARROWEST TYPE THAT ANSWERS THIS. The only
		// thing wanted from the owner is its controller, and `GetController()` is `APawn`'s.
		// ⛔ THIS IS NOT AN ACCESSOR SWAP. The previous cast named the engine's walking-pawn
		// base, which this component's owner stopped deriving from in task 19, so it would
		// return NULL on every event — `pc` null, the block below skipped, and
		// `m_lastMoveInputWasGamepad` never written from this handler. The symptom is SILENT:
		// a D-pad move would never latch gamepad and a WASD move would never clear a latch a
		// stick set (the sticks latch in onMoveStick / onAim, which need no controller), and
		// `buildAimDirection` and `buildMoveDirectionWorld` both gate the move-stick-feeds-aim
		// fallback on that flag.
		const APawn* ch = Cast<APawn>(GetOwner());
		const APlayerController* pc = ch ? Cast<APlayerController>(ch->GetController()) : nullptr;
		if (pc != nullptr)
		{
			const bool dpadMoveDown =
				pc->IsInputKeyDown(EKeys::Gamepad_DPad_Up) || pc->IsInputKeyDown(EKeys::Gamepad_DPad_Down) ||
				pc->IsInputKeyDown(EKeys::Gamepad_DPad_Left) || pc->IsInputKeyDown(EKeys::Gamepad_DPad_Right);
			m_lastMoveInputWasGamepad = dpadMoveDown;
		}
	}
}

void UOGBrawlerInputCollectionComponent::onMoveStick(const FInputActionValue& Value)
{
	const FVector2D v = Value.Get<FVector2D>();
	// Same (X, -Y) transform onMove applies: the left stick yields v.Y = +1 for stick-up.
	m_leftStick = glm::vec2(v.X, v.Y * -1.f);
	if (!v.IsNearlyZero())
		m_lastMoveInputWasGamepad = true;
}

void UOGBrawlerInputCollectionComponent::onAim(const FInputActionValue& Value)
{
	const FVector2D v = Value.Get<FVector2D>();
	// NOTE the asymmetry with onMoveStick: the two sticks arrive with opposite Y conventions.
	// The left stick yields v.Y = +1 for stick-up (negated in onMoveStick); the right stick
	// yields v.Y = -1 for stick-up (stored as-is here). Each handler compensates for its own
	// stick so that downstream code sees the same (stick-up = -Y in storage) convention. This
	// is what lets routeSticks hand either stick to either role without per-stick sign flips.
	m_rightStick = glm::vec2(v.X, v.Y);
	if (!v.IsNearlyZero())
		m_lastMoveInputWasGamepad = true;
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

void UOGBrawlerInputCollectionComponent::onSetSchemeAimRelativeSwapped(const FInputActionValue& /*Value*/)
{
	dAttackMachineSimulation::g_movementScheme = dAttackMachineSimulation::MovementScheme::AimRelativeSwapped;
}

void UOGBrawlerInputCollectionComponent::onLeftAttack(const FInputActionValue& Value)
{
	m_leftAttack = Value.Get<bool>();
}

void UOGBrawlerInputCollectionComponent::onRightAttack(const FInputActionValue& Value)
{
	m_rightAttack = Value.Get<bool>();
}

simulatableBrawler::PlayerInput UOGBrawlerInputCollectionComponent::buildPlayerInput(
    const SimulationTimeStep& step, uint32 componentId,
    const LocalInputCache<simulatableBrawler::PlayerInput>& localInputCache) const
{
	if (!hasInputComponent())
		return simulatableBrawler::getZeroPlayerInput();

	// Continuous fields via the shared core reader — the ONE source of truth, also used by
	// buildLatestVisualizationInput(). Do not re-read the accessors directly here.
	const simulatableBrawler::ContinuousInputFields continuous =
		simulatableBrawler::readContinuousInputFields(*this);

	const bool leftAttack  = getLeftAttack();
	const bool rightAttack = getRightAttack();

	// [movement-sim task 14] THE POINT WHERE holdGuard REACHES THE SIMULATION. This is the
	// field's first and only writer onto the wire: makeSimPlayerInput turns this bool into
	// brawlerMovementSimulation::kInputFlagHoldGuard (bit 0 of the movement sub-sim's input
	// flags byte), and step 1's `frozen` gate in brawlerMovementSimulation::integrate is the
	// only thing that reads it back. Before this line the gate was inert — task 51 shipped the
	// reader with no writer on purpose, so that landing the writer was one reviewable change.
	//
	// It costs ZERO new wire bytes: the flags byte already rides every ring entry (task 11
	// spent it as a `bool`, task 51 re-laid it as bits), and this only sets a bit inside it.
	//
	// ⭐ [movement-sim task 15] AND IT IS NOW THE ONLY READER. The legacy CMC path had a second
	// freeze in `AOGBrawlerUECharacter::Move` reading this same accessor; task 15 deleted it
	// along with the rest of that path. One reader, one freeze, on the sim clock.
	//
	// [3rdControllerMode task 5] THE BIT IS A CLIENT-RESOLVED FREEZE REQUEST, NOT THE RAW
	// BUTTON. dInput::stickRouting::guardFreezeRequested sets it only while the guard is held
	// AND either the scheme's own movement input (the move routeSticks returns with the
	// move-stick-feeds-aim fallback off) or the actual routed move is shorter than the move
	// deadzone. So guard + moving walks, guard alone roots, and in AimRelativeSwapped guard +
	// left stick only (the single-stick fallback, which does route the left stick to move)
	// roots while the left stick still aims. The sim cannot tell a fallback move from a real
	// one, so this is decided here. The four globals are loaded once each below, so both
	// routings inside the decision and the matcher's deadzone see one set of values.
	// readContinuousInputFields above loads them on its own, so a scheme/cvar change landing
	// between the two reads can make this tick's freeze and move disagree for that one tick.
	const dAttackMachineSimulation::MovementScheme scheme = dAttackMachineSimulation::g_movementScheme.load();
	const bool legacySwap   = dAttackMachineSimulation::g_swapMoveAndAimSticks.load();
	const bool feedsAim     = dAttackMachineSimulation::g_gamepadMoveStickFeedsAim.load();
	const float moveDeadzone = dAttackMachineSimulation::g_moveStickDeadzone.load();
	const bool holdGuard   = dInput::stickRouting::guardFreezeRequested(
		getHoldGuard(),
		dInput::stickRouting::StickSources{ m_moveKeys, m_leftStick, m_rightStick },
		scheme, legacySwap, feedsAim, moveDeadzone);

	// --- Motion-sequence matching (predicting client only) ---
	// Runs over the client's RAW CAPTURE history and produces a triggeredActionId carried on
	// the machine PlayerInput. The result replicates to the server through the normal
	// PlayerInput RPC path — same trust model as attackLeft.
	//
	// [T15] Everything below the argument list is engine-free and lives in
	// OGBrawler/BrawlerMotionMatching.h, so it is reachable from OGBrawlerTests (which links
	// OGBrawler but not this module). This function is now purely: read live fields, adapt the
	// history, call the core. DelayLineMotionHistory is the has()-gated adapter — the delay
	// line answers an absent tick with the NEUTRAL input, and matchSequence needs a nullptr.
	const uint32_t triggeredActionId = simulatableBrawler::resolveTriggeredActionId(
		simulatableBrawler::DelayLineMotionHistory(localInputCache),
		step.getTick(),
		continuous,
		leftAttack,
		rightAttack,
		moveDeadzone,
		kGameMotions);

	const simulatableBrawler::PlayerInput packed = simulatableBrawler::makeSimPlayerInput(
		continuous, leftAttack, rightAttack, triggeredActionId,
		// [movement-sim task 52] Named, not positional. Task 14 passed this bool as a trailing
		// defaulted argument; the parameter is now a required InputFlagFields, so a future flag
		// is `{.holdGuard = holdGuard, .jump = jump}` -- it cannot be transposed with holdGuard
		// and it cannot be forgotten by leaving the argument off.
		simulatableBrawler::InputFlagFields{.holdGuard = holdGuard});

	// [movement-sim task 14] `movementFlags` is read back off the PACKED composite rather than
	// re-printed from the `holdGuard` bool above, so this observes the wire field the sim will
	// actually consume, not the packer's input. That makes the task's PIE acceptance check
	// (holding guard with no movement input raises the holdGuard bit; since
	// og-brawler-3rdControllerMode task 5, holding it while moving does not) answerable from a
	// shipped log line with no temporary instrumentation to add and remove. ⚠ LogOGSimTick defaults to Warning in
	// Config/DefaultEngine.ini — this line is per-tick chatter and is silent until someone
	// runs `Log LogOGSimTick Log` in the console.
	UE_LOG(LogOGSimTick, Log,
		TEXT("[ClientPrediction] id=%u tick=%u attackLeft=%d triggeredActionId=%u movementFlags=0x%02X"),
		componentId, step.getTick(), leftAttack ? 1 : 0, triggeredActionId,
		static_cast<uint32>(packed.get<brawlerMovementSimulation::PlayerInput>().flags));

	return packed;
}

simulatableBrawler::PlayerInput UOGBrawlerInputCollectionComponent::buildLatestVisualizationInput() const
{
	// Same cold-path guard as buildPlayerInput: with no input component bound there is nothing
	// live to sample, and the neutral input is the honest answer.
	if (!hasInputComponent())
		return simulatableBrawler::getZeroPlayerInput();

	// Continuous read shared with buildPlayerInput; visualization packer leaves every discrete
	// field neutral. The motion matcher is deliberately not reachable from here — there is no
	// step, no componentId and no manager in scope to run it with.
	// [movement-sim task 14] holdGuard joined that neutral set and is deliberately NOT read
	// here, even though getHoldGuard() is in scope and cheap: it is a discrete button, so the
	// discrete-field-neutral rule (see the three-sources block in the header) applies, and a
	// render-rate echo of a guard press would be a discrete edge leaking onto a cosmetic path.
	return simulatableBrawler::makeVisualizationPlayerInput(
		simulatableBrawler::readContinuousInputFields(*this));
}
