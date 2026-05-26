:: SPDX-License-Identifier: BUSL-1.1
@echo off
rem Set Android SDK env vars explicitly.
rem Git Bash / Claude Code does not inherit Windows user env vars set after shell launch.
rem Build.bat uses FD-9 file locking that silences stdout from bash; invoke UBT directly instead.
set NDKROOT=C:\Users\olle\AppData\Local\Android\Sdk\ndk\27.2.12479018
set JAVA_HOME=C:\Program Files\Android\Android Studio\jbr
set ANDROID_HOME=C:\Users\olle\AppData\Local\Android\Sdk
dotnet "C:\dev\UnrealEngine\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" OGBrawlerUnrealClient Android Development "-Project=C:\dev\og-brawler-unreal\OGBrawlerUnreal.uproject"
exit /B %ERRORLEVEL%
