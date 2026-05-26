:: SPDX-License-Identifier: BUSL-1.1
@echo off
rem Runs the packaged OGBrawler dedicated server. Designed to live in the root of
rem a portable archive folder (Saved\Archive\WinServer\ on the dev PC, or wherever
rem the archive was copied on the host PC).
rem
rem Usage:
rem   run_server.bat            - listens on port 7777, default map
rem   run_server.bat 7780       - listens on port 7780
rem
rem Locates OGBrawlerUnrealServer.exe by searching beneath this script's folder,
rem so the script tolerates UE archive layout variations across versions.
rem
rem First launch may trigger a Windows Firewall prompt — allow inbound on Private.

setlocal enabledelayedexpansion

set MAP=/Game/ThirdPerson/Maps/ThirdPersonMap
set PORT=%1
if "%PORT%"=="" set PORT=7777

set EXE=
for /r "%~dp0" %%F in (OGBrawlerUnrealServer.exe) do (
    if exist "%%F" if not defined EXE set EXE=%%F
)

if not defined EXE (
    echo.
    echo === ERROR: OGBrawlerUnrealServer.exe not found ===
    echo Searched beneath: %~dp0
    echo.
    echo Make sure run_server.bat is at the root of the cooked WinServer archive.
    echo.
    pause
    exit /b 1
)

echo.
echo === OGBrawler dedicated server (packaged) ===
echo Exe:  %EXE%
echo Map:  %MAP%
echo Port: %PORT%
echo.
echo LAN IPv4 addresses (pass one of these to connecting clients):
for /f "tokens=1,*" %%I in ('powershell -NoProfile -Command "Get-NetIPAddress -AddressFamily IPv4 ^| Where-Object { $_.IPAddress -notmatch '^(127\.|169\.254\.)' -and $_.PrefixOrigin -ne 'WellKnown' } ^| Sort-Object InterfaceMetric ^| ForEach-Object { $_.IPAddress + ' ' + $_.InterfaceAlias }"') do (
    echo   %%I  %%J
)
echo.
echo Skip VPN, Hyper-V, vEthernet, and Tailscale entries — pick the one on the
echo physical Wi-Fi/Ethernet adapter the clients can reach.
echo.

"%EXE%" %MAP% -log -port=%PORT%

endlocal
