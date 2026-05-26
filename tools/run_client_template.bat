:: SPDX-License-Identifier: BUSL-1.1
@echo off
rem Runs the packaged OGBrawler client and connects to a server. Designed to live
rem in the root of a portable archive folder.
rem
rem Usage:
rem   run_client.bat                     - connects to 127.0.0.1:7777 (same PC)
rem   run_client.bat 192.168.1.42        - connects to that IP on port 7777
rem   run_client.bat 192.168.1.42 7780   - custom port
rem
rem Locates OGBrawlerUnrealClient.exe by searching beneath this script's folder,
rem so the script tolerates UE archive layout variations.
rem
rem Asset/version mismatch causes silent connection failures. The client and the
rem server must be cooked from the same git revision + content.

setlocal enabledelayedexpansion

set SERVER=%1
set PORT=%2
if "%SERVER%"=="" set SERVER=127.0.0.1
if "%PORT%"=="" set PORT=7777

set EXE=
for /r "%~dp0" %%F in (OGBrawlerUnrealClient.exe) do (
    if exist "%%F" if not defined EXE set EXE=%%F
)

if not defined EXE (
    echo.
    echo === ERROR: OGBrawlerUnrealClient.exe not found ===
    echo Searched beneath: %~dp0
    echo.
    echo Make sure run_client.bat is at the root of the cooked WinClient archive.
    echo.
    pause
    exit /b 1
)

echo.
echo === OGBrawler client (packaged) ===
echo Exe:    %EXE%
echo Server: %SERVER%:%PORT%
echo.

"%EXE%" %SERVER%:%PORT% -windowed -ResX=1280 -ResY=720 -log

endlocal
