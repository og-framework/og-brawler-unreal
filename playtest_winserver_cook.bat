:: SPDX-License-Identifier: BUSL-1.1
@echo off
rem Cooks and packages the Win64 dedicated server via UAT BuildCookRun.
rem
rem Usage:
rem   playtest_winserver_cook.bat
rem
rem Notes:
rem   - Wall-clock: ~10 min warm cache, 30-60 min cold.
rem   - Output: Saved\Archive\WinServer\ (self-contained, no UE install needed to run).
rem   - Cook log: Saved\Logs\uat_cook_winserver_<timestamp>.log
rem   - Editor must be CLOSED (Live Coding holds the binary lock and UAT cannot
rem     rebuild while it is active).
rem   - On success, copies tools\run_server_template.bat into the archive root as
rem     run_server.bat so the run script ships with the zip.
rem   - To deliver to a host PC: zip the entire archive folder, copy, unzip,
rem     install VC++ redist, allow firewall, run run_server.bat. See
rem     PLAYTEST_PORTABLE_README.md.

setlocal
set ENGINE_DIR=C:\dev\UnrealEngine
set PROJECT=%~dp0OGBrawlerUnreal.uproject
set RUNUAT=%ENGINE_DIR%\Engine\Build\BatchFiles\RunUAT.bat
set ARCHIVE_DIR=%~dp0Saved\Archive\WinServer
set RUN_TEMPLATE=%~dp0tools\run_server_template.bat

rem Live Coding precondition check.
tasklist /FI "IMAGENAME eq UnrealEditor.exe" 2>nul | find /I "UnrealEditor.exe" >nul
if %ERRORLEVEL%==0 (
    echo.
    echo === ERROR: UnrealEditor.exe is running ===
    echo.
    echo UAT cannot rebuild the Editor target while Live Coding holds the binary
    echo lock. Close the editor and any running game windows, then re-run this
    echo script.
    echo.
    pause
    exit /b 1
)

rem Build a unique log filename so attempts don't overwrite each other.
for /f "tokens=2 delims==" %%I in ('wmic os get localdatetime /value ^| find "="') do set DT=%%I
set TIMESTAMP=%DT:~0,8%_%DT:~8,6%
set LOG=%~dp0Saved\Logs\uat_cook_winserver_%TIMESTAMP%.log

if not exist "%~dp0Saved\Logs" mkdir "%~dp0Saved\Logs"

echo.
echo === OGBrawler Win64 server cook ===
echo Project:    %PROJECT%
echo Archive to: %ARCHIVE_DIR%
echo Log:        %LOG%
echo.
echo Running UAT BuildCookRun. This may take 10-60 minutes.
echo.

call "%RUNUAT%" BuildCookRun ^
    -project="%PROJECT%" ^
    -server -serverconfig=Development ^
    -serverplatform=Win64 ^
    -noclient ^
    -cook -stage -package -pak ^
    -build ^
    -archive -archivedirectory="%ARCHIVE_DIR%" ^
    -unattended -nop4 -utf8output > "%LOG%" 2>&1

set UAT_RC=%ERRORLEVEL%

echo.
if %UAT_RC% NEQ 0 (
    echo === Cook FAILED with exit code %UAT_RC% ===
    echo See log: %LOG%
    echo.
    endlocal
    exit /b %UAT_RC%
)

echo === Cook SUCCEEDED ===

if exist "%RUN_TEMPLATE%" (
    copy /Y "%RUN_TEMPLATE%" "%ARCHIVE_DIR%\run_server.bat" >nul
    echo Wrote run_server.bat into archive root.
) else (
    echo WARNING: run template not found at %RUN_TEMPLATE%
    echo Skipping run_server.bat generation.
)

echo.
echo Archive: %ARCHIVE_DIR%
echo Next: zip the archive folder and copy to the host PC, then run run_server.bat.
echo.
endlocal
exit /b 0
