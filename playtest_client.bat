:: SPDX-License-Identifier: BUSL-1.1
@echo off
rem Launches an OGBrawler client and connects it to a server.
rem
rem Usage:
rem   playtest_client.bat                     - connects to 127.0.0.1:7777 (same PC)
rem   playtest_client.bat 192.168.1.42        - connects to that IP on port 7777
rem   playtest_client.bat 192.168.1.42 7780   - custom port
rem
rem Tip: find the server PC's IP with `ipconfig` (use the IPv4 Address on the LAN adapter).
rem
rem Notes:
rem   - Uses UnrealEditor.exe -game (works without building the Game/Client target).
rem     To use a packaged build instead, build OGBrawlerUnreal (or OGBrawlerUnrealClient)
rem     and replace the launch line below with:
rem       "%~dp0Binaries\Win64\OGBrawlerUnreal.exe" %SERVER%:%PORT% -game -windowed -ResX=1280 -ResY=720
rem   - Asset/version mismatch causes silent connection failures. Both PCs must run the
rem     same build (same git revision + content).

setlocal
set ENGINE_DIR=C:\dev\UnrealEngine
set PROJECT=%~dp0OGBrawlerUnreal.uproject
set SERVER=%1
set PORT=%2
if "%SERVER%"=="" set SERVER=127.0.0.1
if "%PORT%"=="" set PORT=7777

set EDITOR_EXE=%ENGINE_DIR%\Engine\Binaries\Win64\UnrealEditor.exe

echo.
echo === OGBrawler client ===
echo Connecting to %SERVER%:%PORT%
echo.

"%EDITOR_EXE%" "%PROJECT%" %SERVER%:%PORT% -game -log -windowed -ResX=1280 -ResY=720
endlocal
