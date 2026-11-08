# SPDX-License-Identifier: BUSL-1.1
# docs/steam-publishing.md
@{
    SchemaVersion  = 2
    ProjectFile    = '..\..\OGBrawlerUnreal.uproject'
    EngineRoot     = ''
    Platform       = 'Win64'
    BuilderAccount = 'ogbrawlerbuilder'
    Branch         = 'playtest'
    OutputRoot     = '..\..\Saved\Steam\Builds'
    KeepLast       = 3
    Builds = @(
        @{
            Name               = 'client'
            TargetType         = 'Client'
            Configuration      = 'Shipping'
            ExpectedExecutable = 'OGBrawlerUnrealClient.exe'
            FileExclusions     = @('*.pdb', 'Manifest_*.txt')
            ExtraFiles         = @()
        }
        @{
            Name               = 'server'
            TargetType         = 'Server'
            Configuration      = 'Shipping'
            ExpectedExecutable = 'OGBrawlerUnrealServer.exe'
            FileExclusions     = @('*.pdb', 'Manifest_*.txt', 'HostLogs\*', 'join_info.txt', 'Engine\Saved\*', 'OGBrawlerUnreal\Saved\*')
            ExtraFiles         = @(
                @{ Source = '..\run_server_template.bat'; Destination = 'run_server.bat' }
            )
            ServerLauncher     = @{
                ServerArguments  = '/Game/ThirdPerson/Maps/ThirdPersonMap -ini:Engine:[Core.Log]:LogOGNet=Warning -ini:Engine:[Core.Log]:LogOGBrawler=Warning -ini:Engine:[Core.Log]:LogOGSimTick=Warning'
                Port             = 7777
                JoinLinePattern  = 'OGBrawlerSession: (?:(?<joined>joined)|(?<left>left)) players=(?<players>\d+) tested=(?<tested>\d+)(?: local=(?<local>\d+))?'
                ReadyLinePattern = 'OGBrawlerSession: listening port=(?<port>\d+)'
                Title            = 'OGBrawler dedicated server'
                ClientLaunch     = @('..\OGBrawlerUnrealClient.exe', 'steam://rungameid/{AppId:game}')
                LocalHint        = 'After joining: Tab adds a local player, End removes one'
            }
        }
    )
    Apps = @(
        @{
            Name   = 'game'
            AppId  = 5563780
            Depots = @(
                @{ Name = 'game-client'; DepotId = 5563781; Build = 'client' }
                @{ Name = 'game-server'; DepotId = 5563782; Build = 'server'; DepotPath = 'Server' }
            )
        }
        @{
            Name   = 'server-tool'
            AppId  = 0
            Depots = @(
                @{ Name = 'tool-server'; DepotId = 0; Build = 'server' }
            )
        }
    )
    Profiles = @{
        Default         = 'playtest'
        'playtest'      = @('game-client', 'game-server')
        'playtest-tool' = @('game-client', 'game-server', 'tool-server')
        'game-only'     = @('game-client')
        'release'       = @('game-client', 'tool-server')
    }
}
