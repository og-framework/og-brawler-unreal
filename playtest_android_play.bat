:: SPDX-License-Identifier: BUSL-1.1
@echo off
rem Launches the installed OGBrawler client on the phone and auto-connects to a server.
rem
rem Usage:
rem   playtest_android_play.bat                     - auto-detects this PC's LAN IP
rem   playtest_android_play.bat 192.168.1.42        - connect to specific server IP
rem   playtest_android_play.bat 192.168.1.42 7780   - custom port
rem
rem Notes:
rem   - The phone must already have the APK installed (run playtest_android_install.bat).
rem   - The phone must be on the same Wi-Fi network as the server PC.
rem   - The server must be running first (playtest_server.bat on the host PC).
rem   - Auto-detection picks the first non-loopback IPv4 address from ipconfig.
rem     If your PC has a VPN, Hyper-V vSwitch, or multiple NICs, it may pick the
rem     wrong one — pass the IP explicitly in that case.
rem   - UE's GameActivity reads the cmdline extra and treats it as the open URL.

setlocal enabledelayedexpansion
set ADB=%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe
set PACKAGE=com.yourcompany.testyo
set ACTIVITY=%PACKAGE%/com.epicgames.unreal.GameActivity

set SERVER=%~1
set PORT=%~2
if "%PORT%"=="" set PORT=7777

rem Auto-detect LAN IP if no server arg given. Prints all candidate IPs with
rem their interface name so you can spot a wrong-adapter pick.
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
    echo   playtest_android_play.bat 192.168.1.42
    echo.
)

if "%SERVER%"=="" (
    echo.
    echo === ERROR: could not auto-detect a LAN IP ===
    echo Pass it explicitly: playtest_android_play.bat 192.168.1.42
    echo.
    pause
    exit /b 1
)

if not exist "%ADB%" (
    echo.
    echo === ERROR: adb not found at %ADB% ===
    echo.
    pause
    exit /b 1
)

rem Sanity check device.
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
    echo If you see "could not connect" on the phone, verify:
    echo   - playtest_server.bat is running on the PC
    echo   - Windows Firewall allows UDP 7777 inbound
    echo   - Phone and PC are on the same Wi-Fi network
) else (
    echo.
    echo === Launch FAILED ===
)
echo.
endlocal
