:: SPDX-License-Identifier: BUSL-1.1
@echo off
rem Host-PC variant: launches the OGBrawler app on a phone plugged into THIS PC
rem and points it at this PC's LAN IP (which is also where run_server.bat runs).
rem Uses bundled adb so no Android SDK install is required on the host PC.
rem
rem Usage:
rem   play_android.bat                     - auto-detects this PC's LAN IP
rem   play_android.bat 192.168.1.42        - connect to a specific IP
rem   play_android.bat 192.168.1.42 7780   - custom port
rem
rem Notes:
rem   - Run install_phone.bat first to push the APK + OBB.
rem   - run_server.bat must already be running on this PC.
rem   - Phone must be on the same Wi-Fi as this PC (or routable to it).
rem   - The phone can be unplugged after this script returns; the app stays
rem     running on the phone and remains connected to the server.

setlocal enabledelayedexpansion
set ADB=%~dp0platform-tools\adb.exe
set PACKAGE=com.yourcompany.testyo
set ACTIVITY=%PACKAGE%/com.epicgames.unreal.GameActivity

set SERVER=%~1
set PORT=%~2
if "%PORT%"=="" set PORT=7777

if "%SERVER%"=="" (
    echo.
    echo Detecting LAN IPv4 addresses...
    for /f "tokens=1,*" %%I in ('powershell -NoProfile -Command "Get-NetIPAddress -AddressFamily IPv4 ^| Where-Object { $_.IPAddress -notmatch '^(127\.|169\.254\.)' -and $_.PrefixOrigin -ne 'WellKnown' } ^| Sort-Object InterfaceMetric ^| ForEach-Object { $_.IPAddress + ' ' + $_.InterfaceAlias }"') do (
        if not defined SERVER (
            set SERVER=%%I
            echo   %%I  %%J  [picked]
        ) else (
            echo   %%I  %%J
        )
    )
    echo.
    echo If [picked] is the wrong adapter ^(VPN, Hyper-V vSwitch, phone hotspot
    echo on the wrong subnet, etc.^), pass the right IP explicitly:
    echo   play_android.bat 192.168.1.42
    echo.
)

if "%SERVER%"=="" (
    echo.
    echo === ERROR: could not auto-detect a LAN IP ===
    echo Pass it explicitly: play_android.bat 192.168.1.42
    echo.
    pause
    exit /b 1
)

if not exist "%ADB%" (
    echo.
    echo === ERROR: bundled adb not found at %ADB% ===
    echo Re-cook on the dev PC, or copy adb.exe + AdbWinApi.dll +
    echo AdbWinUsbApi.dll into a platform-tools folder next to this script.
    echo.
    pause
    exit /b 1
)

"%ADB%" get-state >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo === ERROR: no device attached or unauthorized ===
    echo Plug in the phone, accept the USB debugging prompt, and try again.
    echo.
    pause
    exit /b 1
)

echo.
echo === OGBrawler Android playtest ===
echo Server: %SERVER%:%PORT%
echo.
echo Force-stopping any existing instance...
"%ADB%" shell am force-stop %PACKAGE%

echo Launching client and pointing at server...
"%ADB%" shell am start -n "%ACTIVITY%" --es "cmdline" "%SERVER%:%PORT%"

if %ERRORLEVEL%==0 (
    echo.
    echo Launch OK. Phone should travel into the server's level shortly.
    echo You can unplug the phone now if you like.
) else (
    echo.
    echo === Launch FAILED ===
)
echo.
endlocal
