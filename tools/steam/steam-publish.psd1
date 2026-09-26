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
                    Configuration      = 'Development'
                    ExpectedExecutable = 'OGBrawlerUnrealServer.exe'
                    FileExclusions     = @('*.pdb', 'Manifest_*.txt')
                    ExtraFiles         = @(
                        @{ Source = '..\run_server_template.bat'; Destination = 'run_server.bat' }
                    )
                }
            )
        }
    )
}
