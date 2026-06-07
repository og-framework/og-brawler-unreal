// SPDX-License-Identifier: BUSL-1.1

#include "HAL/IConsoleManager.h"
#include "Math/UnrealMathUtility.h"
#include "OGBrawler/DAttackMachineSimulationRuntimeTweakables.h"

namespace
{
	int32 GMovementSchemeInt = static_cast<int32>(dAttackMachineSimulation::MovementScheme::AimRelative);

	void OnMovementSchemeChanged(IConsoleVariable* Var)
	{
		const int32 Val = Var->GetInt();
		if (Val == static_cast<int32>(dAttackMachineSimulation::MovementScheme::CameraRelative))
		{
			dAttackMachineSimulation::g_movementScheme = dAttackMachineSimulation::MovementScheme::CameraRelative;
		}
		else if (Val == static_cast<int32>(dAttackMachineSimulation::MovementScheme::AimRelative))
		{
			dAttackMachineSimulation::g_movementScheme = dAttackMachineSimulation::MovementScheme::AimRelative;
		}
		else if (Val == static_cast<int32>(dAttackMachineSimulation::MovementScheme::MoveRelativeAim))
		{
			dAttackMachineSimulation::g_movementScheme = dAttackMachineSimulation::MovementScheme::MoveRelativeAim;
		}
		// Silently ignore invalid values — the atomic is not updated.
	}

	static FAutoConsoleVariableRef CVarMovementScheme(
		TEXT("OGBrawler.MovementScheme"),
		GMovementSchemeInt,
		TEXT("0 = CameraRelative, 1 = AimRelative, 2 = MoveRelativeAim"),
		FConsoleVariableDelegate::CreateStatic(&OnMovementSchemeChanged),
		ECVF_Default);

	// Stick deadzones — UE CVars mirror the sim-side tweakables. Clamped to [0, 1].
	float GMoveStickDeadzone = 0.15f;
	float GAimStickDeadzone  = 0.2f;

	void OnMoveStickDeadzoneChanged(IConsoleVariable* Var)
	{
		dAttackMachineSimulation::g_moveStickDeadzone = FMath::Clamp(Var->GetFloat(), 0.f, 1.f);
	}

	void OnAimStickDeadzoneChanged(IConsoleVariable* Var)
	{
		dAttackMachineSimulation::g_aimStickDeadzone = FMath::Clamp(Var->GetFloat(), 0.f, 1.f);
	}

	static FAutoConsoleVariableRef CVarMoveStickDeadzone(
		TEXT("OGBrawler.MoveStickDeadzone"),
		GMoveStickDeadzone,
		TEXT("Magnitude threshold below which the move stick is treated as neutral. [0, 1], default 0.15."),
		FConsoleVariableDelegate::CreateStatic(&OnMoveStickDeadzoneChanged),
		ECVF_Default);

	static FAutoConsoleVariableRef CVarAimStickDeadzone(
		TEXT("OGBrawler.AimStickDeadzone"),
		GAimStickDeadzone,
		TEXT("Magnitude threshold below which the aim stick is treated as neutral (falls back to mouse aim). [0, 1], default 0.2."),
		FConsoleVariableDelegate::CreateStatic(&OnAimStickDeadzoneChanged),
		ECVF_Default);

	// Swap which physical stick feeds which logic.
	int32 GSwapMoveAndAimSticks = 0;

	void OnSwapMoveAndAimSticksChanged(IConsoleVariable* Var)
	{
		dAttackMachineSimulation::g_swapMoveAndAimSticks = (Var->GetInt() != 0);
	}

	static FAutoConsoleVariableRef CVarSwapMoveAndAimSticks(
		TEXT("OGBrawler.SwapMoveAndAimSticks"),
		GSwapMoveAndAimSticks,
		TEXT("0 = left stick = move, right stick = aim (default). 1 = swapped: right stick feeds the move direction, left stick feeds the aim direction."),
		FConsoleVariableDelegate::CreateStatic(&OnSwapMoveAndAimSticksChanged),
		ECVF_Default);

	// Character walk speed. Applied to CharacterMovementComponent::MaxWalkSpeed each Tick.
	float GMoveSpeed = 100.f;

	void OnMoveSpeedChanged(IConsoleVariable* Var)
	{
		dAttackMachineSimulation::g_moveSpeed = FMath::Max(0.f, Var->GetFloat());
	}

	static FAutoConsoleVariableRef CVarMoveSpeed(
		TEXT("OGBrawler.MoveSpeed"),
		GMoveSpeed,
		TEXT("Character walk speed in UE units (cm) per second. Drives CharacterMovementComponent::MaxWalkSpeed each Tick. Default 100."),
		FConsoleVariableDelegate::CreateStatic(&OnMoveSpeedChanged),
		ECVF_Default);

	// Toggle the gamepad-only "move stick feeds both move and aim" fallback.
	int32 GGamepadMoveStickFeedsAim = 1;

	void OnGamepadMoveStickFeedsAimChanged(IConsoleVariable* Var)
	{
		dAttackMachineSimulation::g_gamepadMoveStickFeedsAim = (Var->GetInt() != 0);
	}

	static FAutoConsoleVariableRef CVarGamepadMoveStickFeedsAim(
		TEXT("OGBrawler.GamepadMoveStickFeedsAim"),
		GGamepadMoveStickFeedsAim,
		TEXT("0 = off, 1 = on (default). When on, gamepad players with the aim stick below deadzone have their move stick feed both the movement direction and the aim direction (both resolve to the same camera-relative move-stick vector). Keyboard+mouse play is unaffected either way."),
		FConsoleVariableDelegate::CreateStatic(&OnGamepadMoveStickFeedsAimChanged),
		ECVF_Default);
}
