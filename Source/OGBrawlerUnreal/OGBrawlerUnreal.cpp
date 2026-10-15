// SPDX-License-Identifier: BUSL-1.1
// docs/OGBrawlerUnreal-rationale.md

#include "OGBrawlerUnreal.h"
#include "OGBrawlerUnreal/OGBuildIdentityUImpl.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include <OGBrawler/ConsoleCommandsNamedPipeServer.h>

#if PLATFORM_WINDOWS
#include <thread>
#endif

#include "OGSimulation/CompilerControl.h"
OGSIM_OPTIMIZE_OFF

DEFINE_LOG_CATEGORY_STATIC(LogOGStartup, Log, All);

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/WindowsHWrapper.h"

namespace
{
constexpr PROCESS_POWER_THROTTLING_STATE kHonourTimerResolution{
    PROCESS_POWER_THROTTLING_CURRENT_VERSION,
    PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION,
    0};
static_assert(kHonourTimerResolution.ControlMask == PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION,
              "ControlMask must name IGNORE_TIMER_RESOLUTION only: any other bit (EXECUTION_SPEED) changes EcoQoS, not the timer");
static_assert(kHonourTimerResolution.StateMask == 0,
              "StateMask must stay 0 (honour): a set IGNORE_TIMER_RESOLUTION bit asks Windows to IGNORE the 1 ms timer request (31 ms DS frames)");

using ProcessInformationFn = BOOL(WINAPI*)(HANDLE, PROCESS_INFORMATION_CLASS, LPVOID, DWORD);

BOOL callProcessInformation(const char* name, PROCESS_POWER_THROTTLING_STATE& state, DWORD& error)
{
    const FARPROC proc = ::GetProcAddress(::GetModuleHandleW(L"kernel32.dll"), name);
    const ProcessInformationFn fn = reinterpret_cast<ProcessInformationFn>(reinterpret_cast<void*>(proc));
    const BOOL ok = fn != nullptr && fn(::GetCurrentProcess(), ProcessPowerThrottling, &state, sizeof(state));
    error = ok ? 0 : ::GetLastError();
    return ok;
}

void honourTimerResolution()
{
    const bool skipped = FParse::Param(FCommandLine::Get(), TEXT("OGNoTimerHonour"));
    BOOL set = FALSE;
    DWORD setError = 0;
    if (!skipped)
    {
        PROCESS_POWER_THROTTLING_STATE state = kHonourTimerResolution;
        set = callProcessInformation("SetProcessInformation", state, setError);
    }

    PROCESS_POWER_THROTTLING_STATE readback{PROCESS_POWER_THROTTLING_CURRENT_VERSION, 0, 0};
    DWORD readError = 0;
    const BOOL read = callProcessInformation("GetProcessInformation", readback, readError);

    UE_LOG(LogOGStartup, Display,
           TEXT("[Startup.Timer] honour=%d set=%d err=%u readback=%d control=0x%x state=0x%x readErr=%u"),
           skipped ? 0 : 1, set ? 1 : 0, setError, read ? 1 : 0, readback.ControlMask, readback.StateMask, readError);
}
} // namespace

#include "Windows/HideWindowsPlatformTypes.h"
#endif

class FOGBrawlerUEModule : public FDefaultGameModuleImpl
{
public:
    virtual void StartupModule() override
    {
#if PLATFORM_WINDOWS
        honourTimerResolution();
#endif

        buildIdentityUImpl::registerNetworkVersionHook();

#if PLATFORM_WINDOWS
        std::thread(DAttackPipeServer::NamedPipeServer).detach();
#endif
    }

    virtual void ShutdownModule() override
    {
    }
};

IMPLEMENT_PRIMARY_GAME_MODULE(FOGBrawlerUEModule, OGBrawlerUnreal, "OGBrawlerUnreal");

OGSIM_OPTIMIZE_ON
