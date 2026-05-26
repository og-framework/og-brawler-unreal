// SPDX-License-Identifier: BUSL-1.1

#include "OGBrawlerUnreal.h"
#include "Modules/ModuleManager.h"
#include <OGBrawler/ConsoleCommandsNamedPipeServer.h>

#if PLATFORM_WINDOWS
#include <thread>
#endif

#pragma optimize( "", off )

class FOGBrawlerUEModule : public FDefaultGameModuleImpl
{
public:
    virtual void StartupModule() override
    {
#if PLATFORM_WINDOWS
        // Start the named pipe server in a detached thread
        std::thread(DAttackPipeServer::NamedPipeServer).detach();
#endif
    }

    virtual void ShutdownModule() override
    {
    }
};

IMPLEMENT_PRIMARY_GAME_MODULE(FOGBrawlerUEModule, OGBrawlerUnreal, "OGBrawlerUnreal");

#pragma optimize( "", on )
