:: SPDX-License-Identifier: BUSL-1.1
@echo off
rem Installs the most recently cooked APK + OBB to a connected Android device.
rem
rem Usage:
rem   playtest_android_install.bat
rem
rem Notes:
rem   - Phone must be plugged in via USB with USB debugging enabled and authorized.
rem   - Verifies adb sees the phone before invoking UE's generated install script.
rem   - The actual work is done by Saved\Archive\Android\Install_*.bat which UE
rem     generated during cook — uninstall, install APK, push OBB via UnrealAndroidFileTool,
rem     grant permissions.

setlocal
set ARCHIVE_DIR=%~dp0Saved\Archive\Android
set INSTALL_BAT=%ARCHIVE_DIR%\Install_OGBrawlerUnrealClient-arm64.bat
set ADB=%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe

if not exist "%INSTALL_BAT%" (
    echo.
    echo === ERROR: cooked install script not found ===
    echo Expected: %INSTALL_BAT%
    echo Run playtest_android_cook.bat first.
    echo.
    pause
    exit /b 1
)

if not exist "%ADB%" (
    echo.
    echo === ERROR: adb not found at %ADB% ===
    echo Install Android Studio / SDK platform-tools, or edit ADB path in this script.
    echo.
    pause
    exit /b 1
)

rem Confirm exactly one device is attached.
set DEVICE_COUNT=0
for /f "skip=1 tokens=1" %%I in ('"%ADB%" devices') do (
    if not "%%I"=="" set /a DEVICE_COUNT+=1
)

if %DEVICE_COUNT%==0 (
    echo.
    echo === ERROR: no device attached ===
    echo Plug in the phone via USB. Make sure USB debugging is enabled and that
    echo the phone has approved this PC. Run "%ADB% devices" to verify.
    echo.
    pause
    exit /b 1
)

if %DEVICE_COUNT% GTR 1 (
    echo.
    echo === WARNING: multiple devices attached ===
    "%ADB%" devices
    echo.
    echo The install script will use the first one. Disconnect others if that's wrong.
    echo.
)

echo.
echo === OGBrawler Android install ===
echo APK:    %ARCHIVE_DIR%\OGBrawlerUnrealClient-arm64.apk
echo Device: (see above)
echo.
echo This will uninstall any previous install and push the latest APK + OBB.
echo Expect 30-60 seconds.
echo.

pushd "%ARCHIVE_DIR%"
call "%INSTALL_BAT%"
set INSTALL_RC=%ERRORLEVEL%
popd

echo.
if %INSTALL_RC%==0 (
    echo === Install SUCCEEDED ===
    echo Now run playtest_android_play.bat to launch + connect to a server.
) else (
    echo === Install FAILED with exit code %INSTALL_RC% ===
)
echo.
endlocal
exit /b %INSTALL_RC%
