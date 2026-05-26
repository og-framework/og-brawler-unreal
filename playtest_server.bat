:: SPDX-License-Identifier: BUSL-1.1
@echo off
rem Launches the OGBrawler dedicated server on this PC.
rem
rem Usage:
rem   playtest_server.bat            - listens on default port 7777
rem   playtest_server.bat 7780       - listens on port 7780
rem
rem Notes:
rem   - First launch triggers a Windows Firewall prompt. Allow inbound on Private networks.
rem   - Uses UnrealEditor.exe with -server (works without building the Server target).
rem     To use a packaged build instead, build target OGBrawlerUnrealServer and replace
rem     the launch line below with:
rem       "%~dp0Binaries\Win64\OGBrawlerUnrealServer.exe" %MAP% -log -port=%PORT%
rem   - If your PC has multiple NICs (VPN, Hyper-V vSwitch, etc.) and the wrong one binds,
rem     add: -MULTIHOME=<your.lan.ip>

setlocal
set ENGINE_DIR=C:\dev\UnrealEngine
set PROJECT=%~dp0OGBrawlerUnreal.uproject
set MAP=/Game/ThirdPerson/Maps/ThirdPersonMap
set PORT=%1
if "%PORT%"=="" set PORT=7777

set EDITOR_EXE=%ENGINE_DIR%\Engine\Binaries\Win64\UnrealEditor.exe

echo.
echo === OGBrawler dedicated server ===
echo Map:  %MAP%
echo Port: %PORT%
echo.

"%EDITOR_EXE%" "%PROJECT%" %MAP% -server -log -port=%PORT% -nullrhi -nosound -unattended
endlocal
