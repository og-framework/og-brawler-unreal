# SPDX-License-Identifier: MPL-2.0
# tools/lint/README.md "Module tier lint"
@{
    SourceParents   = @('Source/*', 'Plugins/OGSimulation/Source/*', 'Plugins/OGBrawler/Source/*')
    ExcludeSegments = @('.*', 'Binaries', 'Intermediate', 'Saved', 'DerivedDataCache', 'Content', 'build*', 'Build*', 'ThirdParty',
                        'glm', 'docs', 'tests', 'Tests', 'tools')
    Extensions      = @('.h', '.hpp', '.inl', '.cpp', '.c')

    EngineHeaders   = @('CoreMinimal.h', 'CoreTypes.h', 'CoreGlobals.h', 'CoreUObject.h', 'EngineMinimal.h', '*.generated.h',
                        'Algo/*', 'Async/*', 'Blueprint/*', 'Camera/*', 'Chaos/*', 'Components/*', 'Containers/*', 'Core/*',
                        'Delegates/*', 'Engine/*', 'Features/*', 'Framework/*', 'GameFramework/*', 'GenericPlatform/*', 'HAL/*',
                        'Input/*', 'Internationalization/*', 'Iris/*', 'Kismet/*', 'Logging/*', 'Materials/*', 'Math/*',
                        'Misc/*', 'Modules/*', 'Net/*', 'Physics/*', 'PhysicsEngine/*', 'PhysicsProxy/*',
                        'ProfilingDebugging/*', 'Runtime/*', 'Serialization/*', 'Slate/*', 'Stats/*', 'String/*',
                        'Subsystems/*', 'Tasks/*', 'Templates/*', 'Trace/*', 'UObject/*', 'Widgets/*', 'Windows/*',
                        'CanvasItem.h', 'CanvasTypes.h', 'DrawDebugHelpers.h', 'EngineUtils.h', 'EnhancedInputComponent.h',
                        'EnhancedInputSubsystems.h', 'InputAction.h', 'InputActionValue.h', 'InputMappingContext.h',
                        'IPAddress.h', 'PacketHandler.h', 'PBDRigidsSolver.h', 'RewindData.h', 'Sockets.h',
                        'SocketSubsystem.h', 'TimerManager.h', 'UnrealClient.h')

    Nodes = @{
        OGNet                  = @{ Tier = 0; EngineFree = $true
                                    Roots = @('**/Source/OGNet', '**/og-net/OGNet')
                                    ModuleBoilerplate = @('OGNetModule.cpp')
                                    MayNotInclude = @('OGSimulation') }
        OGSimulation           = @{ Tier = 0; EngineFree = $true; Group = 'og-simulation'
                                    Roots = @('**/Source/OGSimulation', '**/og-simulation/OGSimulation')
                                    ModuleBoilerplate = @('OGSimulation.cpp')
                                    MayNotInclude = @('OGNet') }
        OGTickDriver           = @{ Tier = 1; EngineFree = $true; Group = 'og-simulation'
                                    Roots = @('**/Source/OGTickDriver', '**/og-tickdriver/OGTickDriver')
                                    ModuleBoilerplate = @('OGTickDriverModule.cpp')
                                    MayNotInclude = @('OGNet') }
        OGSimulationJolt       = @{ Tier = 2; EngineFree = $true
                                    Roots = @('**/Source/OGSimulationJolt', '**/og-simulation-jolt/OGSimulationJolt')
                                    ModuleBoilerplate = @('OGSimulationJolt.cpp')
                                    MayNotInclude = @('OGNet', 'OGSimNet', 'OGBrawler') }
        OGSimNet               = @{ Tier = 2; EngineFree = $true
                                    Roots = @('**/Source/OGSimulationNet', '**/og-sim-net/OGSimNet')
                                    ModuleBoilerplate = @('OGSimulationNetModule.cpp')
                                    MayNotInclude = @('OGTickDriver', 'OGSimulationJolt', 'OGBrawler') }
        OGBrawler              = @{ Tier = 2; EngineFree = $true
                                    Roots = @('**/Source/OGBrawler', '**/og-brawler/OGBrawler')
                                    ModuleBoilerplate = @('OGBrawler.cpp')
                                    MayNotInclude = @('OGNet', 'OGTickDriver', 'OGSimulationJolt', 'OGSimNet') }
        OGSimSession           = @{ Tier = 3; EngineFree = $true
                                    Roots = @('**/Source/OGSimulationSession', '**/og-sim-session/OGSimSession')
                                    ModuleBoilerplate = @('OGSimulationSessionModule.cpp')
                                    MayNotInclude = @('OGNet', 'OGSimulationJolt', 'OGBrawler', 'OGNetUnreal', 'DVolume',
                                                      'ProceduralMountainSide', 'OGSimulationUnreal', 'OGSimulationJoltUnreal',
                                                      'OGBrawlerUnreal') }
        OGNetUnreal            = @{ Tier = 4; EngineFree = $false; Roots = @('**/Source/OGNetUnreal') }
        DVolume                = @{ Tier = 4; EngineFree = $false; Roots = @('**/Source/dVolume') }
        ProceduralMountainSide = @{ Tier = 4; EngineFree = $false; Roots = @('**/Source/ProceduralMountainSide') }
        OGSimulationUnreal     = @{ Tier = 5; EngineFree = $false; Roots = @('**/Source/OGSimulationUnreal')
                                    MayNotInclude = @('OGNet') }
        OGSimulationJoltUnreal = @{ Tier = 6; EngineFree = $false; Roots = @('**/Source/OGSimulationJoltUnreal') }
        OGBrawlerUnreal        = @{ Tier = 7; EngineFree = $false; Roots = @('**/Source/OGBrawlerUnreal')
                                    MayNotInclude = @('OGTickDriver') }
    }

    Unlinted = @{
        '**/Source/glm'               = 'the glm third-party library wrapped as a UE module'
        '**/Source/OGSimulationTests' = 'test module: its cases include every tier'
        '**/Source/OGBrawlerTests'    = 'test module: its cases include every tier'
        '**/Source/OGNetTests'        = 'test module: its cases include every tier'
    }

    Passengers = @{ Of = @('OGSimulation'); Riders = @('OGSimNet', 'OGSimSession', 'OGSimulationUnreal') }

    Watch = @(
        @{ From = 'OGSimSession'; To = 'OGSimNet' }
    )

    ReachClasses = @(
        @{ Class = 'UESimulationSession'; DeclaredIn = @('UESimulationSession.h'); Aliases = @('Session', 'SessionT') }
    )
}
