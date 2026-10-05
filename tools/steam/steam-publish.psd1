# SPDX-License-Identifier: BUSL-1.1
# docs/steam-publishing.md
@{
    SchemaVersion  = 1
    ProjectFile    = '..\..\OGBrawlerUnreal.uproject'
    EngineRoot     = ''
    Platform       = 'Win64'
    BuilderAccount = ''
    Branch         = 'playtest'
    OutputRoot     = '..\..\Saved\Steam\Builds'
    KeepLast       = 3
    Apps = @(
        @{
            Name   = 'client'
            AppId  = 0
            Depots = @(
                @{
                    Name               = 'client-win64'
                    DepotId            = 0
                    TargetType         = 'Client'
                    Configuration      = 'Shipping'
                    ExpectedExecutable = 'OGBrawlerUnrealClient.exe'
                    FileExclusions     = @('*.pdb', 'Manifest_*.txt')
                    ExtraFiles         = @()
                }
            )
        }
        @{
            Name   = 'server'
            AppId  = 0
            Depots = @(
                @{
                    Name               = 'server-win64'
                    DepotId            = 0
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
                        ClientLaunch     = 'steam://rungameid/{AppId:client}'
                        LocalHint        = 'After joining: Tab adds a local player, End removes one'
                    }
                }
            )
        }
    )
}
