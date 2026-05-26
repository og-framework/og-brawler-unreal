:: SPDX-License-Identifier: BUSL-1.1
@echo off
rem Cooks and packages the Android client APK + OBB via UAT BuildCookRun.
rem
rem Usage:
rem   playtest_android_cook.bat
rem
rem Notes:
rem   - Wall-clock: ~10 min warm cache, 30-60 min cold. Subsequent runs reuse caches.
rem   - Output: Saved\Archive\Android\OGBrawlerUnrealClient-arm64.apk + main.1.<pkg>.obb
rem   - Cook log: Saved\Logs\uat_cook_android_<timestamp>.log
rem   - The Editor must be CLOSED before running — Live Coding holds a lock on the
rem     Editor binary and UAT cannot rebuild while it is active. This script checks
rem     and stops with a clear error if it finds UnrealEditor.exe running.
rem   - Uses -client -targetplatform=Android (not -platform=Android) because the
rem     project has two TargetType.Game declarations and UAT can't disambiguate.

setlocal
set ENGINE_DIR=C:\dev\UnrealEngine
set PROJECT=%~dp0OGBrawlerUnreal.uproject
set RUNUAT=%ENGINE_DIR%\Engine\Build\BatchFiles\RunUAT.bat
set ARCHIVE_DIR=%~dp0Saved\Archive\Android

rem Live Coding precondition check.
tasklist /FI "IMAGENAME eq UnrealEditor.exe" 2>nul | find /I "UnrealEditor.exe" >nul
if %ERRORLEVEL%==0 (
    echo.
    echo === ERROR: UnrealEditor.exe is running ===
    echo.
    echo UAT cannot rebuild the Editor target while Live Coding holds the binary
    echo lock. Close the editor and any running game windows, then re-run this
    echo script. Auto-killing the editor would destroy unsaved work, so this
    echo script refuses to do that.
    echo.
    pause
    exit /b 1
)

rem Build a unique log filename so attempts don't overwrite each other.
for /f "tokens=2 delims==" %%I in ('wmic os get localdatetime /value ^| find "="') do set DT=%%I
set TIMESTAMP=%DT:~0,8%_%DT:~8,6%
set LOG=%~dp0Saved\Logs\uat_cook_android_%TIMESTAMP%.log

if not exist "%~dp0Saved\Logs" mkdir "%~dp0Saved\Logs"

echo.
echo === OGBrawler Android cook ===
echo Project:    %PROJECT%
echo Archive to: %ARCHIVE_DIR%
echo Log:        %LOG%
echo.
echo Running UAT BuildCookRun. This may take 10-60 minutes.
echo.

call "%RUNUAT%" BuildCookRun ^
    -project="%PROJECT%" ^
    -client ^
    -clientconfig=Development ^
    -targetplatform=Android ^
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
echo APK: %ARCHIVE_DIR%\OGBrawlerUnrealClient-arm64.apk
echo.
echo Bundling host-PC kit into archive...

rem Bundle adb (just adb.exe + the two AdbWin DLLs, ~5 MB) into the archive
rem so the Android folder is self-contained for use on a host PC without an
rem Android SDK install.
set SRC_PT=%LOCALAPPDATA%\Android\Sdk\platform-tools
set DST_PT=%ARCHIVE_DIR%\platform-tools

if exist "%SRC_PT%\adb.exe" (
    if not exist "%DST_PT%" mkdir "%DST_PT%"
    copy /Y "%SRC_PT%\adb.exe"          "%DST_PT%\" >nul
    copy /Y "%SRC_PT%\AdbWinApi.dll"    "%DST_PT%\" >nul
    copy /Y "%SRC_PT%\AdbWinUsbApi.dll" "%DST_PT%\" >nul
    echo   Bundled adb to %DST_PT%
) else (
    echo   WARNING: %SRC_PT%\adb.exe not found.
    echo   Archive will not be self-contained on a host PC without Android SDK.
)

rem Copy host-PC variants of the install/play scripts into the archive root.
set INSTALL_TEMPLATE=%~dp0tools\install_phone_template.bat
set PLAY_TEMPLATE=%~dp0tools\play_android_template.bat

if exist "%INSTALL_TEMPLATE%" (
    copy /Y "%INSTALL_TEMPLATE%" "%ARCHIVE_DIR%\install_phone.bat" >nul
    echo   Wrote install_phone.bat
) else (
    echo   WARNING: install template not found at %INSTALL_TEMPLATE%
)

if exist "%PLAY_TEMPLATE%" (
    copy /Y "%PLAY_TEMPLATE%" "%ARCHIVE_DIR%\play_android.bat" >nul
    echo   Wrote play_android.bat
) else (
    echo   WARNING: play template not found at %PLAY_TEMPLATE%
)

echo.
echo Archive: %ARCHIVE_DIR%
echo Dev-side install (phone plugged into this PC): playtest_android_install.bat
echo Host-PC kit: zip the archive folder and ship; on host PC run install_phone.bat then play_android.bat.
echo.
endlocal
exit /b 0
