<!-- SPDX-License-Identifier: BUSL-1.1 -->
# OGBrawler Portable Playtest

Goal: a teammate's PC hosts the server **and** runs a local PC client, while a
phone joins as the third player over LAN. The dev PC is not in the loop during
the playtest.

```
        +-------------------+
        |   HOST PC         |
        |  +-------------+  |          USB cable (~5 sec, then unplug)
        |  | run_server  |  |        +----------------+
        |  | run_client  |--+--Wi-Fi-+ Phone (Android) |
        |  +-------------+  |        +----------------+
        +-------------------+
```

## TL;DR

**Dev PC** (one-time per build):
```powershell
.\playtest_winserver_cook.bat
.\playtest_winclient_cook.bat
.\playtest_android_cook.bat
```
Then zip the three archive folders. **Don't use `Compress-Archive`** — it has a
2 GB limit and will fail with `Stream was too long.` on the WinServer/WinClient
archives. Use the bundled `tar` instead:
```powershell
tar -a -c -f WinServer.zip -C .\Saved\Archive\WinServer .
tar -a -c -f WinClient.zip -C .\Saved\Archive\WinClient .
tar -a -c -f Android.zip   -C .\Saved\Archive\Android   .
```
`tar` ships with Windows 10 1803+ and Windows 11 and produces standard `.zip`
files any recipient can open in Explorer.

**Host PC** (one-time setup): install
[VC++ 2015–2022 x64 Redist](https://aka.ms/vs/17/release/vc_redist.x64.exe).
Allow `run_server.exe` through Windows Firewall (UDP 7777 inbound, Private) on
first launch.

**Host PC** (each playtest): unzip the three archives anywhere, then:
```
1. Double-click run_server.bat                 (in WinServer\)
2. Double-click run_client.bat 127.0.0.1       (in WinClient\)
3. Plug phone in via USB, USB-debug enabled
4. Double-click install_phone.bat              (in Android\, first time only)
5. Double-click play_android.bat               (in Android\)
6. Unplug phone, walk away — app stays connected
```

That's the full loop.

---

## Build (on the dev PC, with UE source build at `C:\dev\UnrealEngine`)

### Quick — wrapped scripts

```powershell
.\playtest_winserver_cook.bat       # → Saved\Archive\WinServer\ (~200-400 MB)
.\playtest_winclient_cook.bat       # → Saved\Archive\WinClient\ (~300-500 MB)
.\playtest_android_cook.bat         # → Saved\Archive\Android\   (~165 MB + bundled adb)
```

Each wrapper:
- Aborts if `UnrealEditor.exe` is running (Live Coding holds the binary lock).
- Logs to `Saved\Logs\uat_cook_*_<timestamp>.log`.
- On success, drops the matching run/install scripts into the archive.

The Android cook also bundles `adb.exe` + the two `AdbWin*.dll` files from
`%LOCALAPPDATA%\Android\Sdk\platform-tools\` into `Archive\Android\platform-tools\`
so the host PC needs no Android SDK install.

Wall clock: 10–15 min warm cache, 30–60 min cold.

### Raw UAT — PowerShell one-liners

If you want to drive UAT directly:

**Win64 dedicated server:**
```powershell
& "C:\dev\UnrealEngine\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="C:\dev\og-brawler-unreal\OGBrawlerUnreal.uproject" -server -serverconfig=Development -serverplatform=Win64 -noclient -cook -stage -package -pak -build -archive -archivedirectory="C:\dev\og-brawler-unreal\Saved\Archive\WinServer" -unattended -nop4 -utf8output
```

`-noclient` is important — without it UAT also builds the Game target, which currently fails on duplicate Catch2 symbols when both test modules are linked. The `TargetDenyList: ["Server", "Client", "Game"]` in the test uplugins is the matching belt-and-braces fix.

**Win64 client:**
```powershell
& "C:\dev\UnrealEngine\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="C:\dev\og-brawler-unreal\OGBrawlerUnreal.uproject" -client -clientconfig=Development -targetplatform=Win64 -cook -stage -package -pak -build -archive -archivedirectory="C:\dev\og-brawler-unreal\Saved\Archive\WinClient" -unattended -nop4 -utf8output
```

**Android client (arm64):**
```powershell
& "C:\dev\UnrealEngine\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="C:\dev\og-brawler-unreal\OGBrawlerUnreal.uproject" -client -clientconfig=Development -targetplatform=Android -cook -stage -package -pak -build -archive -archivedirectory="C:\dev\og-brawler-unreal\Saved\Archive\Android" -unattended -nop4 -utf8output
```

The raw one-liners do **not** bundle adb or copy run-script templates — that's
the wrappers' job. If you cook with raw UAT you'll need to run the bundling
steps manually, or just use the wrappers.

### Run packaged builds locally on the dev PC (smoke test)

After cooking, the same scripts that ship to the host PC also work on the dev PC:
```powershell
.\Saved\Archive\WinServer\run_server.bat
.\Saved\Archive\WinClient\run_client.bat 127.0.0.1
```

Use this to verify a cook before zipping.

### Dev-side Android install/launch (unchanged, plug phone into dev PC)

```powershell
.\playtest_android_install.bat                      # uses dev PC's SDK adb
.\playtest_android_play.bat                         # auto-detects dev PC's IP
.\playtest_android_play.bat 192.168.1.42            # or specify a server IP
```

These remain at repo root for fast iteration during development. The host-PC
counterparts (`install_phone.bat`, `play_android.bat`) live inside the archive
and use the bundled adb.

---

## Move to the host PC

Zip and copy. Three archives:
- `WinServer.zip` — required (the server)
- `WinClient.zip` — required if you want a PC client on the host
- `Android.zip` — required if a phone is joining and the host PC will launch it

Unzip each anywhere on the host PC. Layout matters only within each archive
folder; the three folders can sit anywhere relative to each other.

---

## Run on the host PC

### One-time setup
1. Install [VC++ 2015–2022 x64 Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe).
   Most modern Win10/11 already has it.
2. **Open the firewall for UDP 7777.** Two ways:

   a. **GUI:** the first launch of `run_server.bat` triggers a Windows Firewall
   prompt — click "Allow access" and tick **Private networks** (and **Public**
   too if you'll ever play over a phone hotspot — see below). If you missed the
   prompt: Control Panel → Windows Defender Firewall → Allow an app …, find
   `OGBrawlerUnrealServer.exe`.

   b. **PowerShell as Administrator** (no GUI prompt, also covers the phone-hotspot
   case where Windows often classifies the new network as Public):
   ```powershell
   New-NetFirewallRule -DisplayName "OGBrawler UDP 7777" -Direction Inbound -Protocol UDP -LocalPort 7777 -Action Allow -Profile Any
   ```
   Verify it stuck:
   ```powershell
   Get-NetFirewallRule -DisplayName "OGBrawler UDP 7777"
   ```
   To remove later: `Remove-NetFirewallRule -DisplayName "OGBrawler UDP 7777"`.

### Each playtest
1. Run `run_server.bat` (or `run_server.bat 7780` for a custom port). Server
   binds UDP 7777 and waits.
2. Run `run_client.bat 127.0.0.1` to start a local PC client. It connects over
   loopback — the host's network card isn't involved.
3. Find your LAN IP for the phone:
   ```powershell
   Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.IPAddress -notmatch '^(127\.|169\.254\.)' -and $_.PrefixOrigin -ne 'WellKnown' } | Select-Object IPAddress,InterfaceAlias
   ```
   Pick the entry on your physical LAN/Wi-Fi adapter (skip VPNs, vSwitches).
4. Plug the phone into the host PC via USB. Enable USB debugging and accept the
   RSA prompt the first time the phone sees this PC.
5. **First time on this phone:** double-click `install_phone.bat` (in the Android
   archive). 30–60 sec to push APK + OBB.
6. Double-click `play_android.bat`. Phone launches, connects to the host's LAN IP.
7. Unplug the phone. App keeps running and stays connected.

If the phone needs to rejoin after closing the app, plug back in and re-run
`play_android.bat` — there's no in-game IP entry, the launch intent is the only
way to seed the server URL.

### No Wi-Fi available — use the phone's hotspot

Works at a coffee shop with no usable Wi-Fi, on the road, anywhere with no LAN.
Doesn't need an active cellular data plan — the phone's hotspot is just a local
Wi-Fi network it broadcasts.

1. **Phone:** Settings → Network → Mobile Hotspot. Turn it on. Note the SSID
   and password.
2. **Host PC:** connect to the phone's Wi-Fi like any other network. Windows
   will probably classify it as **Public** — that's why the firewall rule above
   uses `-Profile Any` (Private + Public + Domain).
3. **Host PC:** find the IP that the phone's hotspot DHCP'd to you. Run
   `play_android.bat` once with no args to print all candidate IPs and the
   adapter each is on. Look for the entry on a wireless adapter with an address
   like `192.168.43.X` or `192.168.137.X` (varies by phone vendor).
4. **Run server:** `run_server.bat` (binds all interfaces, including the new
   hotspot one).
5. **Run PC client:** `run_client.bat 127.0.0.1` (loopback as usual).
6. **Run phone client:** explicitly pass the host PC's hotspot-subnet IP — the
   auto-pick may grab a stale Ethernet adapter that's no longer carrying real
   traffic.
   ```
   play_android.bat 192.168.43.179
   ```

That's it. UDP flows over the phone's hotspot Wi-Fi the same as any LAN. Latency
is generally fine; bandwidth is limited but a small playtest doesn't push it.

#### If `run_server.bat` won't accept the phone's connection
Run server with explicit bind to the hotspot interface:
```
run_server.bat 7777
```
…then if the phone still can't reach it, edit `run_server.bat` to add
`-MULTIHOME=<host-PC-hotspot-IP>` to the exe arguments. UE binds to all
interfaces by default, but multi-NIC boxes occasionally route weirdly.

#### Alternatives if phone hotspot doesn't work
- **Windows Mobile Hotspot** (PC hosts the AP, phone joins): Settings → Network
  & Internet → Mobile Hotspot. Sometimes refuses to start without an internet
  connection to share — phone hotspot is less fiddly.
- **USB tethering** (Settings → Network → USB tethering on the phone): creates
  a bridged subnet over USB. UDP works fine. Caveat: many phones grey out the
  toggle unless cellular data is active.

What does *not* work:
- `adb reverse` — TCP only, UE replication is UDP.
- Bluetooth tethering — too laggy for action games.

---

## Host PC dependencies — full list

| Dep | Server | PC client | Android launch | Notes |
|---|---|---|---|---|
| Windows 10/11 x64 | Required | Required | Required | x86 not supported. |
| VC++ 2015–2022 x64 Redist | Required | Required | — | https://aka.ms/vs/17/release/vc_redist.x64.exe |
| GPU + drivers (D3D12-capable) | Not needed | Required | — | Server is headless. |
| Android SDK / platform-tools | — | — | Bundled | adb is shipped inside Android archive. |
| .NET / Java / DirectX runtime | — | — | — | UE bundles all transitive DLLs in `Engine\Binaries\ThirdParty\`. |
| Firewall: UDP `<port>` inbound | Required | — | — | Default 7777, Private network. |
| Same LAN/Wi-Fi as players | Required | Required | Required (for phone) | Or a VPN like Tailscale/Hamachi. |
| USB cable + phone with USB debugging | — | — | Required (briefly) | Only at launch time; can unplug after. |

### Things that will *not* work
- Mixing builds from different git revisions — silent disconnect or crash.
  Re-cook all three sides whenever assets or replicated code change.
- Phone joining an Internet-routable host without port forwarding or a VPN.
  This setup is LAN-only by design.
- Closing the server window without closing the clients first — clients will
  hang on a broken connection.
- Paths with non-ASCII characters in some UE versions — keep the unzip target
  simple (e.g. `C:\Playtest\WinServer\`).

---

## File map

```
playtest_winserver_cook.bat        # cook Win64 server (dev PC)
playtest_winclient_cook.bat        # cook Win64 client (dev PC)
playtest_android_cook.bat          # cook Android arm64 + bundle host-PC kit
playtest_android_install.bat       # dev-side phone install (plug into dev PC)
playtest_android_play.bat          # dev-side phone launch
playtest_server.bat                # legacy editor-based server (dev only)
playtest_client.bat                # legacy editor-based client (dev only)

tools\run_server_template.bat        # → Saved\Archive\WinServer\run_server.bat
tools\run_client_template.bat        # → Saved\Archive\WinClient\run_client.bat
tools\install_phone_template.bat     # → Saved\Archive\Android\install_phone.bat
tools\play_android_template.bat      # → Saved\Archive\Android\play_android.bat

Saved\Archive\WinServer\           # ship as WinServer.zip
  run_server.bat                   # host runs this
  WindowsServer\...                # cooked content + binaries

Saved\Archive\WinClient\           # ship as WinClient.zip
  run_client.bat                   # host runs this
  Windows\...                      # cooked content + binaries

Saved\Archive\Android\             # ship as Android.zip
  install_phone.bat                # host runs this once
  play_android.bat                 # host runs this each session
  Install_OGBrawlerUnrealClient-arm64.bat  # UE-generated, called by install_phone.bat
  Uninstall_OGBrawlerUnrealClient-arm64.bat
  SymbolizeCrashDump_*.bat
  OGBrawlerUnrealClient-arm64.apk
  main.1.com.yourcompany.testyo.obb
  platform-tools\
    adb.exe
    AdbWinApi.dll
    AdbWinUsbApi.dll
  win-x64\
    UnrealAndroidFileTool.exe      # used to push the OBB
  OGBrawlerUnreal_Symbols_v1\
```
