:: SPDX-License-Identifier: BUSL-1.1
@echo off
rem Host-PC variant of the Android installer. Uses adb bundled into this archive
rem at platform-tools\adb.exe, so the host PC does not need an Android SDK install.
rem
rem Wraps UE's generated Install_OGBrawlerUnrealClient-arm64.bat by setting
rem ANDROID_HOME to the archive root before calling it (the UE script reads
rem ANDROID_HOME and falls back to the cook PC's hardcoded SDK path otherwise).
rem
rem Usage:
rem   install_phone.bat
rem
rem Notes:
rem   - Plug the phone into THIS PC via USB; enable USB debugging; approve the
rem     RSA prompt on the phone.
rem   - Will uninstall any previous install of com.yourcompany.testyo.

setlocal

set ANDROID_HOME=%~dp0
if "%ANDROID_HOME:~-1%"=="\" set ANDROID_HOME=%ANDROID_HOME:~0,-1%

if not exist "%ANDROID_HOME%\platform-tools\adb.exe" (
    echo.
    echo === ERROR: bundled adb not found ===
    echo Expected: %ANDROID_HOME%\platform-tools\adb.exe
    echo The cook script should have placed adb here. Re-cook on the dev PC,
    echo or copy a platform-tools folder ^(adb.exe + AdbWinApi.dll +
    echo AdbWinUsbApi.dll^) into the archive next to this script.
    echo.
    pause
    exit /b 1
)

if not exist "%~dp0Install_OGBrawlerUnrealClient-arm64.bat" (
    echo.
    echo === ERROR: UE install script not found ===
    echo Expected: %~dp0Install_OGBrawlerUnrealClient-arm64.bat
    echo.
    pause
    exit /b 1
)

set ADB="%ANDROID_HOME%\platform-tools\adb.exe"
set DEVICE_COUNT=0
for /f "skip=1 tokens=1" %%I in ('%ADB% devices') do (
    if not "%%I"=="" set /a DEVICE_COUNT+=1
)

if %DEVICE_COUNT%==0 (
    echo.
    echo === ERROR: no device attached ===
    echo Plug in the phone, enable USB debugging, and approve the RSA prompt.
    echo Run "%ADB% devices" to verify.
    echo.
    pause
    exit /b 1
)

if %DEVICE_COUNT% GTR 1 (
    echo.
    echo === WARNING: multiple devices attached ===
    %ADB% devices
    echo Disconnect the others if that's not what you want.
    echo.
)

echo.
echo === OGBrawler Android install (host PC) ===
echo Using bundled adb at %ANDROID_HOME%\platform-tools\adb.exe
echo This will uninstall any previous install and push the APK + OBB.
echo.

pushd "%~dp0"
call Install_OGBrawlerUnrealClient-arm64.bat
set RC=%ERRORLEVEL%
popd

echo.
if %RC%==0 (
    echo === Install SUCCEEDED ===
    echo Now run play_android.bat to launch + connect to the local server.
) else (
    echo === Install FAILED with exit code %RC% ===
)
echo.
endlocal
exit /b %RC%
