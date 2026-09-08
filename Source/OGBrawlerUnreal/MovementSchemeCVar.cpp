// SPDX-License-Identifier: BUSL-1.1

#include "HAL/IConsoleManager.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/ConfigCacheIni.h"                 // GConfig - the stale-ini sweep reads the raw section
#include "PhysicsEngine/PhysicsSettings.h"       // UPhysicsSettings::DefaultGravityZ
#include "OGBrawler/DAttackMachineSimulationRuntimeTweakables.h"
// ⭐ [movement-sim task 16] For LogOGBrawler, FMovementStaticDataCVars and the
// ASimulationManagerUImpl::readMovementStaticDataCVars declaration this TU DEFINES. The read
// lives here, next to the variables it reads; see that declaration's comment for why.
#include "SimulationManagerUImpl.h"

#include <atomic>

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

	//////////////////////////////////////////////////////////////////////////////////////////
	// ⭐⭐ THE FOUR ONE-TIME MOVEMENT VARIABLES — [movement-sim task 16, USER RULING #3].
	//
	// ⛔ ONE-TIME MEANS ONE-TIME. These four are read EXACTLY ONCE, in
	// `ASimulationManagerUImpl::readMovementStaticDataCVars()` below, when the manager builds its
	// `StaticData`. Nothing reads them per tick, and nothing may: they are AUTHORED DATA that
	// every peer must agree on for the whole session, and a value that moved mid-session would
	// make a resimulated tick disagree with the tick it replays — a divergence with no wire
	// symptom, because `StaticData` never travels on the wire.
	//
	// ⭐ SO THE SINKS DO NOT APPLY ANYTHING. Each one exists to tell the operator that the
	// change they just made is being IGNORED, which is the whole reason a change that lands
	// after the read is not silent. Before the read they say nothing — an ini applied at startup
	// is the SUPPORTED way to set these, and warning about it would train people to ignore the
	// warning that matters. `GMovementCVarsConsumed` is the latch that separates the two.
	std::atomic<bool> GMovementCVarsConsumed{ false };

	void WarnIfAfterTheOneTimeRead(const TCHAR* Name)
	{
		if (GMovementCVarsConsumed.load(std::memory_order_relaxed))
		{
			UE_LOG(LogOGBrawler, Warning,
				TEXT("[Movement.cvar] '%s' changed AFTER the one-time read - THE CHANGE IS IGNORED. ")
				TEXT("These four are read once, when the simulation manager constructs its StaticData ")
				TEXT("(user ruling #3). Set it from an ini before the session starts, or restart PIE."),
				Name);
		}
	}

	// Character walk speed, cm/s. ⭐ [movement-sim task 16] LIVE AGAIN, as a ONE-TIME read: the
	// sim-side global it used to drive is deleted and this value now reaches
	// `brawlerMovementSimulation::StaticData::maxWalkSpeed` through the manager's constructor.
	// (It was INERT between task 15, which retired the CharacterMovementComponent, and here.)
	float GMoveSpeed = 100.f;

	void OnMoveSpeedChanged(IConsoleVariable*) { WarnIfAfterTheOneTimeRead(TEXT("OGBrawler.MoveSpeed")); }

	static FAutoConsoleVariableRef CVarMoveSpeed(
		TEXT("OGBrawler.MoveSpeed"),
		GMoveSpeed,
		TEXT("Character walk speed in UE units (cm) per second, default 100. READ ONCE when the simulation manager constructs its StaticData; mid-session changes are ignored (a warning is logged). Feeds brawlerMovementSimulation::StaticData::maxWalkSpeed."),
		FConsoleVariableDelegate::CreateStatic(&OnMoveSpeedChanged),
		ECVF_Default);

	// Which movement law turns the stick into a surface-relative 2D velocity.
	// ⭐ DEFAULT 0 = ContinuousAccelBrake, per USER RULING #10 (cleared 2026-09-04).
	int32 GMovementModel =
		static_cast<int32>(brawlerMovementSimulation::MovementModel::ContinuousAccelBrake);

	void OnMovementModelChanged(IConsoleVariable*) { WarnIfAfterTheOneTimeRead(TEXT("OGBrawler.MovementModel")); }

	static FAutoConsoleVariableRef CVarMovementModel(
		TEXT("OGBrawler.MovementModel"),
		GMovementModel,
		TEXT("0 = ContinuousAccelBrake (default, user ruling #10), 1 = Cadence. READ ONCE when the simulation manager constructs its StaticData; mid-session changes are ignored (a warning is logged). An out-of-range value is refused loudly and the default is used."),
		FConsoleVariableDelegate::CreateStatic(&OnMovementModelChanged),
		ECVF_Default);

	// ⚠ SIM TICKS, NOT FRAMES, NOT SECONDS. 20 ticks = 1/3 s at the 60 Hz sim clock, whose
	// authority is `Config/DefaultEngine.ini`'s AsyncFixedTimeStepSize — a render frame has
	// nothing to do with it, and the help text says so because "period" invites the other reading.
	int32 GStepPeriodTicks = 20;

	void OnStepPeriodTicksChanged(IConsoleVariable*) { WarnIfAfterTheOneTimeRead(TEXT("OGBrawler.StepPeriodTicks")); }

	static FAutoConsoleVariableRef CVarStepPeriodTicks(
		TEXT("OGBrawler.StepPeriodTicks"),
		GStepPeriodTicks,
		TEXT("Cadence model only: how many SIMULATION ticks a committed step direction is held. Default 20 = 1/3 s at the 60 Hz sim clock (these are sim ticks, not render frames). READ ONCE when the simulation manager constructs its StaticData; mid-session changes are ignored. Values below 1 are refused loudly and clamped to 1."),
		FConsoleVariableDelegate::CreateStatic(&OnStepPeriodTicksChanged),
		ECVF_Default);

	// ⭐ THE SENTINEL IS THE POINT: the specified default for this one is "= MoveSpeed", and a
	// hard-coded 100 would only be equal to it until somebody set MoveSpeed. A NEGATIVE value
	// means "follow OGBrawler.MoveSpeed", so `OGBrawler.MoveSpeed=200` moves both models'
	// speeds, which is what an operator setting a walk speed means. Set it explicitly to
	// divorce the two.
	float GStepSpeed = -1.f;

	void OnStepSpeedChanged(IConsoleVariable*) { WarnIfAfterTheOneTimeRead(TEXT("OGBrawler.StepSpeed")); }

	static FAutoConsoleVariableRef CVarStepSpeed(
		TEXT("OGBrawler.StepSpeed"),
		GStepSpeed,
		TEXT("Cadence model only: the constant speed in cm/s held between direction commits. NEGATIVE (the default) means 'follow OGBrawler.MoveSpeed'. READ ONCE when the simulation manager constructs its StaticData; mid-session changes are ignored."),
		FConsoleVariableDelegate::CreateStatic(&OnStepSpeedChanged),
		ECVF_Default);

	//////////////////////////////////////////////////////////////////////////////////////////
	// ⭐⭐ TOMBSTONES FOR REFUSED NAMES — [movement-sim task 16], OBLIGATION ROUTED FROM TASK 56.
	//
	// ⛔ AN UNREGISTERED CVAR NAME IS NOT AN ERROR IN UE, IT IS A DEFERRED DUMMY.
	// `UE::ConfigUtilities::OnSetCVarFromIniEntry` (Core/Private/Misc/ConfigUtilities.cpp, 5.6)
	// creates a hidden `ECVF_Unregistered | ECVF_CreatedFromIni` placeholder for any ini key it
	// cannot resolve, so a module registering later still picks the value up. That is a good
	// feature and it is exactly what makes a RETIRED name dangerous: `OGBrawler.MaxSnapSpeed=400`
	// left in an ini produces one `LogConfig` Log line and nothing else, forever, and the
	// operator believes the servo is clamped.
	//
	// ⭐ SO THERE ARE TWO INSTRUMENTS, AND THE SWEEP IS THE PRIMARY ONE.
	//   1. `sweepRefusedNames()` below reads the raw `[ConsoleVariables]` section out of GConfig
	//      at the one-time read. It does not go through the console at all, so it cannot be
	//      defeated by registration order — and registration order IS a live hazard here: the
	//      ini transfer happens inside `RegisterConsoleVariable`, BEFORE
	//      `FAutoConsoleVariable`'s constructor gets to install the sink, so an ini-set value
	//      may never reach the sink.
	//   2. These tombstones catch the console/mid-session vector, and give the name a `Help`
	//      entry so `OGBrawler.MaxSnapSpeed ?` answers instead of shrugging. Their VALUE is
	//      also inspected by the sweep, which closes the registration-order hole for good.
	//
	// ⚠ STRING-TYPED WITH AN EMPTY DEFAULT ON PURPOSE. Any assignment whatsoever is then a
	// change, so there is no "set it to exactly the sentinel and slip through" hole that a
	// numeric tombstone would have.
	void OnRefusedCVarSet(IConsoleVariable* Var)
	{
		const FString Name = IConsoleManager::Get().FindConsoleObjectName(Var);
		const FString Value = Var != nullptr ? Var->GetString() : FString();
		if (Value.IsEmpty())
			return;  // cleared back to the default; nothing was tuned

		// The bare name, without the `OGBrawler.` prefix, is the key into the sim-side table.
		FString Bare = Name;
		Bare.RemoveFromStart(TEXT("OGBrawler."));
		const char* Reason = dAttackMachineSimulation::refusedVariableReason(TCHAR_TO_UTF8(*Bare));

		UE_LOG(LogOGBrawler, Error,
			TEXT("[Movement.cvar] REFUSED '%s' = '%s' - %hs"),
			*Name, *Value, Reason != nullptr ? Reason : "this name is not a tunable constant.");
	}

	// ⛔ ONE STATIC PER NAME, WITH A LITERAL NAME, AND THE TABLE IS WHAT KEEPS THEM HONEST.
	// A console object needs a name that outlives it, so these cannot be generated from the
	// table in a loop without leaking strings. Instead `sweepRefusedNames()` FENCES the pair:
	// every table entry flagged `refusedOnCVarPath` must resolve to a registered variable, and
	// a `checkf` fires at the one-time read if somebody adds an entry and forgets its tombstone.
	static FAutoConsoleVariable CVarRetiredMaxSnapSpeed(
		TEXT("OGBrawler.MaxSnapSpeed"), TEXT(""),
		TEXT("RETIRED (user ruling #28, movement-sim task 56). Setting this does nothing and logs an error. See the refused-name table in DAttackMachineSimulationRuntimeTweakables.cpp."),
		FConsoleVariableDelegate::CreateStatic(&OnRefusedCVarSet),
		ECVF_Default);

	static FAutoConsoleVariable CVarRefusedHoverFrequency(
		TEXT("OGBrawler.HoverFrequency"), TEXT(""),
		TEXT("NOT TUNABLE. Setting this does nothing and logs an error: the hover gains' valid region is two-dimensional and is checked, coupled, where they are authored. See the refused-name table in DAttackMachineSimulationRuntimeTweakables.cpp."),
		FConsoleVariableDelegate::CreateStatic(&OnRefusedCVarSet),
		ECVF_Default);

	static FAutoConsoleVariable CVarRefusedHoverDampingRatio(
		TEXT("OGBrawler.HoverDampingRatio"), TEXT(""),
		TEXT("NOT TUNABLE. Setting this does nothing and logs an error: zeta's valid region is coupled to hoverFrequency, so a per-variable range check on it is wrong. See the refused-name table in DAttackMachineSimulationRuntimeTweakables.cpp."),
		FConsoleVariableDelegate::CreateStatic(&OnRefusedCVarSet),
		ECVF_Default);

	// Walks the refused table and reports, LOUDLY, every way a refused name is set today.
	// Returns the number of refusals, so the caller can say "none" out loud too — an
	// instrument that only ever speaks on failure cannot be distinguished from a dead one.
	int32 sweepRefusedNames()
	{
		int32 Refusals = 0;
		for (const dAttackMachineSimulation::RefusedVariable* Entry =
					dAttackMachineSimulation::refusedVariablesBegin();
			 Entry != dAttackMachineSimulation::refusedVariablesEnd(); ++Entry)
		{
			if (!Entry->refusedOnCVarPath)
				continue;   // dead on the named-pipe path only; the cvar of that name is live

			const FString Name = FString(TEXT("OGBrawler.")) + FString(UTF8_TO_TCHAR(Entry->name));

			// (1) THE RAW INI. Independent of the console entirely.
			FString IniValue;
			if (GConfig != nullptr &&
				GConfig->GetString(TEXT("ConsoleVariables"), *Name, IniValue, GEngineIni))
			{
				++Refusals;
				UE_LOG(LogOGBrawler, Error,
					TEXT("[Movement.cvar] STALE INI: [ConsoleVariables] %s=%s in %s names a constant that is not tunable. %hs"),
					*Name, *IniValue, *GEngineIni, Entry->reason);
			}

			// (2) THE REGISTERED TOMBSTONE - both a fence on this file and the catch-all for a
			// value that arrived by any route (command line, -dpcvars, an ini transfer that
			// beat the sink's installation).
			IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(*Name);
			checkf(Var != nullptr,
				TEXT("MovementSchemeCVar: refused name '%s' has no tombstone console variable. ")
				TEXT("Add a static FAutoConsoleVariable for it beside the others in this file - ")
				TEXT("without one the name is silently swallowed as a deferred ini dummy."), *Name);
			const FString Current = Var->GetString();
			if (!Current.IsEmpty())
			{
				++Refusals;
				UE_LOG(LogOGBrawler, Error,
					TEXT("[Movement.cvar] REFUSED '%s' = '%s' - %hs"),
					*Name, *Current, Entry->reason);
			}
		}
		return Refusals;
	}

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

//////////////////////////////////////////////////////////////////////////////////////////////
// ⭐⭐ THE ONE-TIME READ ITSELF — [movement-sim task 16, USER RULING #3].
//
// Declared on ASimulationManagerUImpl, defined HERE so it loads the four file-static variables
// the console writes directly. Called from exactly one place: `m_movementStaticDataCVars`'s
// member initializer, which runs immediately before `m_staticData` is constructed from it.
//
// ⚠ IT RUNS ONCE PER MANAGER, NOT ONCE PER PROCESS, AND A LISTEN SERVER HAS TWO MANAGERS.
// That is still "read once when the manager constructs its StaticData": both read the same
// values in the same frame, so both peers agree, which is the property ruling #3 is about. The
// latch is armed by whichever runs first and the refusal sweep is reported once, because the
// second manager's identical report would read as a second fault.
FMovementStaticDataCVars
ASimulationManagerUImpl::readMovementStaticDataCVars()
{
	const bool bFirstRead = !GMovementCVarsConsumed.exchange(true, std::memory_order_relaxed);

	const int32 Refusals = bFirstRead ? sweepRefusedNames() : 0;

	// ---- MovementModel. Out of range is a LOUD refusal, never a silent clamp: an operator who
	// typed 2 meant something, and quietly running model 0 is how a tuning session wastes an
	// hour. Ruling #10's default is what we fall back to.
	brawlerMovementSimulation::MovementModel Model =
		brawlerMovementSimulation::MovementModel::ContinuousAccelBrake;
	if (GMovementModel == static_cast<int32>(brawlerMovementSimulation::MovementModel::Cadence))
	{
		Model = brawlerMovementSimulation::MovementModel::Cadence;
	}
	else if (GMovementModel != static_cast<int32>(brawlerMovementSimulation::MovementModel::ContinuousAccelBrake))
	{
		UE_LOG(LogOGBrawler, Error,
			TEXT("[Movement.cvar] REFUSED OGBrawler.MovementModel=%d - only 0 (ContinuousAccelBrake) ")
			TEXT("and 1 (Cadence) exist. Falling back to 0."), GMovementModel);
	}

	// ---- MoveSpeed. Negative is meaningless (the model clamps a magnitude), so it is refused
	// rather than folded to zero, which would look like a frozen character with no explanation.
	float MaxWalkSpeed = GMoveSpeed;
	if (!(MaxWalkSpeed >= 0.f))
	{
		UE_LOG(LogOGBrawler, Error,
			TEXT("[Movement.cvar] REFUSED OGBrawler.MoveSpeed=%f - a walk speed is a magnitude. Falling back to 100."),
			MaxWalkSpeed);
		MaxWalkSpeed = 100.f;
	}

	// ---- StepPeriodTicks. Zero would make Cadence commit a direction every tick, which is not
	// the model; it is also the value a `% period` would divide by if the law ever moved to one.
	int32 StepPeriod = GStepPeriodTicks;
	if (StepPeriod < 1)
	{
		UE_LOG(LogOGBrawler, Error,
			TEXT("[Movement.cvar] REFUSED OGBrawler.StepPeriodTicks=%d - a step must last at least one sim tick. Clamping to 1."),
			StepPeriod);
		StepPeriod = 1;
	}

	// ---- StepSpeed. Negative IS the documented sentinel: follow MoveSpeed.
	const float StepSpeed = GStepSpeed < 0.f ? MaxWalkSpeed : GStepSpeed;

	// ---- Gravity. ⭐ `UPhysicsSettings::Get()->DefaultGravityZ`, NOT `GetWorld()->GetGravityZ()`,
	// AND THE REASON IS WHEN THIS RUNS. This is a member initializer of an AActor: it runs during
	// construction, which for the class default object happens at module load with no world in
	// existence at all, and `GetWorld()` is a member function of an object that is not finished
	// being built. `UPhysicsSettings` is a config-backed developer-settings CDO — available from
	// engine init onwards and independent of any world.
	// ⚠ THAT IS ALSO WHY THE `checkf` IN BeginPlay IS NOT A TAUTOLOGY. The value captured here is
	// the project DEFAULT; a level is allowed to override gravity in its WorldSettings, and
	// `GetWorld()->GetGravityZ()` in BeginPlay is the first moment that override is observable.
	// The sim's own gravity law would then disagree with everything else falling in that level.
	float Gravity = -980.f;
	if (const UPhysicsSettings* PhysicsSettings = UPhysicsSettings::Get())
	{
		Gravity = PhysicsSettings->DefaultGravityZ;
	}
	else
	{
		UE_LOG(LogOGBrawler, Error,
			TEXT("[Movement.cvar] UPhysicsSettings unavailable at StaticData construction; using the authored -980."));
	}

	// ⭐ WARNING, NOT Log OR Display, AND THAT IS THIS TREE'S CONVENTION FOR A ONCE-PER-SESSION
	// COMPOSITION-ROOT BANNER — `[StateRotation] session K` and `[ResimGate] session policy`, the
	// two lines this one sits beside at startup, are both `Warning` for the same reason. It is
	// also the only verbosity that survives a default `LogOGBrawler` setting AND reaches the
	// dedicated server's stdout, where `Log` does not: a banner nobody can read is not a banner.
	// It fires ONCE per manager, so it is not a volume class of any kind.
	// ⚠ THE SWEEP'S RESULT IS REPORTED EVEN WHEN IT IS ZERO. An instrument that only ever speaks
	// on failure is indistinguishable from one that is not wired up, and this one has to survive
	// being trusted for months without firing.
	const FString SweepReport = bFirstRead
		? FString::Printf(TEXT("  refusedNameSweep=%d refusal(s) over %d known-dead name(s)"),
			Refusals,
			static_cast<int32>(dAttackMachineSimulation::refusedVariablesEnd()
							 - dAttackMachineSimulation::refusedVariablesBegin()))
		: FString(TEXT("  (second manager on this process; the sweep ran on the first)"));

	UE_LOG(LogOGBrawler, Warning,
		TEXT("[Movement.cvar] ONE-TIME READ: model=%d maxWalkSpeed=%.3f stepPeriodTicks=%d stepSpeed=%.3f gravity=%.3f%s"),
		static_cast<int32>(Model), MaxWalkSpeed, StepPeriod, StepSpeed, Gravity, *SweepReport);

	return FMovementStaticDataCVars{
		Model, MaxWalkSpeed, static_cast<uint32_t>(StepPeriod), StepSpeed, Gravity };
}
