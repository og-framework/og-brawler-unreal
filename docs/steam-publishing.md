<!-- SPDX-License-Identifier: BUSL-1.1 -->
# OGBrawler — Publishing to Steam

How to take OGBrawler from "no Steamworks account" to a playtest build on Steam,
and how to run playtests with it. Sections 1–5 are one-time setup (every step is
a checkbox), section 6 is day-to-day publishing, section 7 holds the one-page
guides for playtesters, and section 8 lists what this pipeline does not do.

What we ship: a Win64 **client** (Shipping) and a Win64 **dedicated server**
(Shipping, built with logging and checks kept). Both go into the game app: the
client at the root of the install folder and the server in its `Server` folder,
so every player can also host. The same server can also go into a separate
Dedicated Server tool app (section 2). Each upload chooses its destinations
(section 6.3). The game itself contains no Steam SDK code — Steam is used only
to distribute the files. Measured upload sizes of one build (debug symbols
excluded): client about **435 MB**, server about **345 MB**.

Conventions in this document:

- **⏳ WAIT** marks a step with a waiting period. Start those early.
- **Verify in the UI** marks a detail that no public Steamworks page confirms.
  Check it on the Steamworks site and fix this document if it differs.
- Every other claim links to the Valve page it comes from (checked 2026-09-26;
  the depot and launch-option quotes of sections 2 and 3 on 2026-11-04).
  Where two Valve pages disagree, both are cited.

---

## 1. Steamworks onboarding

Sign up once, as the person or company that will own the game on Steam.

- [ ] **Create or choose the Steam account that will own the partner account.**
  The whole signup runs on
  [partner.steamgames.com](https://partner.steamgames.com/). Use an account you
  control long-term, not the builder account from section 4.
- [ ] **Sign the digital agreements.** You e-sign the Non-Disclosure Agreement
  and the Steam Distribution Agreement
  ([Onboarding](https://partner.steamgames.com/doc/gettingstarted/onboarding)).
- [ ] **Enter company identification.** Give the legal name of the person or
  entity signing. It must be accurate
  ([Onboarding](https://partner.steamgames.com/doc/gettingstarted/onboarding)).
- [ ] **Enter bank information**: routing number, account number and bank
  address. Valve pays out to this account
  ([Onboarding](https://partner.steamgames.com/doc/gettingstarted/onboarding)).
- [ ] **Complete the tax interview.** It is a short questionnaire that sets your
  tax status and withholding rate
  ([Onboarding](https://partner.steamgames.com/doc/gettingstarted/onboarding)).
  **⏳ WAIT:** tax verification "may take 2-7 business days, and you may be asked
  to provide additional documentation"
  ([Onboarding](https://partner.steamgames.com/doc/gettingstarted/onboarding)).
- [ ] **Complete identity verification**
  ([Onboarding](https://partner.steamgames.com/doc/gettingstarted/onboarding)).
  How long it takes is not stated publicly. **Verify in the UI.**
- [ ] **Pay the Steam Direct fee**: $100 USD for each product
  ([Steam Direct](https://partner.steamgames.com/steamdirect)). The fee is not
  refunded. You get it back in a payout once the product has made at least
  $1,000 USD Adjusted Gross Revenue
  ([Steam Direct](https://partner.steamgames.com/steamdirect)).
  Section 2 says what one fee covers here.
  **⏳ WAIT:** the release clock starts when you pay. The
  [Onboarding](https://partner.steamgames.com/doc/gettingstarted/onboarding)
  page says **21 days** from paying to releasing. The
  [Steam Direct](https://partner.steamgames.com/steamdirect) page says
  **30 days**. Plan for 30 and **verify in the UI.**

Waiting periods that apply to a **public release**. They do not block playtest
uploads, but start them early if a release is planned:

- **⏳ WAIT:** Valve's review "takes between 1-5 days"
  ([Steam Direct](https://partner.steamgames.com/steamdirect)). Store-page review
  "typically takes 3-5 business days", so submit it "at least 7 days before you
  want it live" ([Releasing](https://partner.steamgames.com/doc/store/releasing)).
- **⏳ WAIT:** the public "Coming Soon" page must be up for at least two weeks
  before release ([Steam Direct](https://partner.steamgames.com/steamdirect),
  [Releasing](https://partner.steamgames.com/doc/store/releasing)).

**Before the fee clears:** the signup, agreements, bank, tax and identity steps
do not need the fee. You create an app with "Create new app..." after
"purchasing the Steam Direct Fee"
([Applications](https://partner.steamgames.com/doc/store/application)), so
sections 2–5 wait for the fee. Section 4's builder Steam account can be made
now; you invite it into Steamworks after section 2.

---

## 2. Create the apps

The server can go to two places, and each upload chooses where (section 6.3):

| Layout | Apps and depots (config names) | Status |
|---|---|---|
| **Bundled** | Game app OGBrawler with two depots: game-client (the client, at the root of the install folder) and game-server (the server, in the install's `Server` folder) | **Use this now.** Every player gets the server and can host from Steam's Play menu (section 7). Cost: every player also downloads the server, about 345 MB. |
| **Tool** | Additionally a Dedicated Server tool app with one depot, tool-server, holding the same server | Optional. For hosts who do not own the game. The tool can also be offered through **anonymous** steamcmd downloads, without a copy of the game ([Distributing Your Dedicated Game Server](https://partner.steamgames.com/doc/sdk/uploading/distributing_gs)). |
| **Shared depot** | The tool lists the game's server depot as a shared depot instead of having its own | **Only after the game is released.** Valve: "You currently cannot share a depot from an unreleased application." and "Shared depots are still a work-in-progress, and should only be used when the base application has been released." ([Depots](https://partner.steamgames.com/doc/store/application/depots)). |

Until a shared depot is possible, an upload that fills both game-server and
tool-server uploads the server twice, once per app (section 6.3).

- [ ] **Create the game app.** On the Steamworks landing page, under "Create A
  New Application", click "Create new app..."
  ([Applications](https://partner.steamgames.com/doc/store/application)).
  Steam gives it a unique **App ID**
  ([Applications](https://partner.steamgames.com/doc/store/application)).
  Write it down; it goes into section 5.
- [ ] **Set the app type to Game** in General Application Settings
  ([Applications](https://partner.steamgames.com/doc/store/application)).
- [ ] **Optional — create the Dedicated Server tool app** (the Tool layout).
  On the game app, open "All
  Associated Packages, DLC, Demos and Tools" and click "Create new Tool"
  ([Distributing Your Dedicated Game Server](https://partner.steamgames.com/doc/sdk/uploading/distributing_gs)).
  Write down the tool's App ID too. Without a tool, its config entries stay 0
  and you use only the profiles without tool-server (section 6.3).
  Whether a tool needs its own Steam Direct fee is not stated publicly. The fee
  is charged "for each product"
  ([Steam Direct](https://partner.steamgames.com/steamdirect)). **Verify in the
  UI.**
- [ ] **Optional — let hosts download the server without owning the game.** On
  the Associated Apps and Packages page, tick "Include this tool in Dedicated
  Server package". This adds the tool and its depots to the anonymous steamcmd
  package. It only works after the tool is marked released. If a tool you
  created yourself has no release button, contact Valve support
  ([Distributing Your Dedicated Game Server](https://partner.steamgames.com/doc/sdk/uploading/distributing_gs)).
  Skip this for private playtests.

---

## 3. Configure each app

Do this for the game app, and for the tool app if you created one. Every change
here stays hidden until the Publish step at the end.

### 3.1 Depots

- [ ] Open the app's **Depots** page. Rename the default depot (for example
  "Windows Content"), set **Language** to "[All languages]" and **OS** to
  "[All OSes]", then click "Save Changes"
  ([Uploading](https://partner.steamgames.com/doc/sdk/uploading)).
- [ ] **Game app: add a second depot** for the server, with the same Language
  and OS. All of a game's content depots "are configured on the Depots page in
  App Admin" ([Depots](https://partner.steamgames.com/doc/store/application/depots));
  the exact button name is not on the public page: **verify in the UI.** The
  game app then has one depot for the client (config name game-client) and one
  for the server (game-server). A tool app has one depot, for the server
  (tool-server).
- [ ] **Write down each Depot ID** from the Depots page, and which one is the
  client and which the server. Valve's docs do not say how depot IDs are
  numbered, so read them off the page. **Verify in the UI.**
- [ ] Make sure each depot is in a package: "Your newly defined depots will need
  to be included in a package to grant you ownership of them"
  ([Uploading](https://partner.steamgames.com/doc/sdk/uploading)). Put **both**
  game depots in the package players get: "all owned depots are mounted by
  default in the order listed"
  ([Depots](https://partner.steamgames.com/doc/store/application/depots)), so a
  player who owns both gets the client and the server.

**How the server lands in `Server`.** Our upload scripts place it: each
depot's upload script maps the local build folder to a path inside the install
folder, the way Valve's example maps a local bin folder into an executables
folder of the depot
([Uploading](https://partner.steamgames.com/doc/sdk/uploading)). Our config
gives only game-server the path `Server` (section 5), so a player's install
folder looks like this (measured in a simulated install of a real build):

```text
OGBrawlerUnrealClient.exe          game-client: the game
Engine\  OGBrawlerUnreal\          game-client: the game's files
build_info.txt                     game-client
Server\OGBrawlerUnrealServer.exe   game-server: the server
Server\Host Local Playtest.bat     game-server: the host files (section 7)
Server\Host Online Playtest.bat
Server\host_server.ps1, Server\host_server.settings.psd1, Server\run_server.bat
Server\Engine\  Server\OGBrawlerUnreal\  Server\build_info.txt
```

The tool app's depot has no path, so the tool's install folder holds the same
server files at its root.

### 3.2 Launch options

- [ ] Under **Installation → General Installation Settings**, add launch
  options. Each has the executable path, relative to the install folder, plus
  any arguments ([Uploading](https://partner.steamgames.com/doc/sdk/uploading)).
  Valve: "If your executable is in a sub-folder of the main installation folder,
  add the sub-folder name in the Executable field." and "Don't use leading
  slashes or dots."
  ([Uploading](https://partner.steamgames.com/doc/sdk/uploading)).
  The two programs are the small launchers at the top of each build, which
  start the real program inside its OGBrawlerUnreal folder (measured on a real
  Shipping package). Point Steam at the launcher, not at the inner program.
  - **Game app: three launch options**, none with arguments, the game first:

    | Executable | What it starts |
    |---|---|
    | `OGBrawlerUnrealClient.exe` | The game. Without an address it opens its join screen (section 7.3). |
    | `Server\Host Online Playtest.bat` | Hosting for players on the internet (section 7.2). |
    | `Server\Host Local Playtest.bat` | Hosting for players on this PC (section 7.1). |

    Valve's public pages do not say whether Steam offers the three as a choice
    when a player clicks Play, or whether it accepts a `.bat` as a launch
    target. Both were confirmed on the first upload (build 26519805,
    2026-11-08): Play offers the three options, and the two host options start
    the host window (section 6.8).
  - Tool app, server: executable `OGBrawlerUnrealServer.exe`, no arguments.
    Hosts normally start the server with the "Host … Playtest" files instead
    (section 7), so this option is only a fallback. Whether a tool fetched only
    through steamcmd needs a launch option at all: **verify in the UI.**

### 3.3 Redistributables

- [ ] Game app: open **Installation → Redistributables** and tick the Visual C++
  2015–2022 x64 runtime
  ([Common Redistributables](https://partner.steamgames.com/doc/features/common_redist)).
  The packaged client and server both need this runtime; the bundled server
  installs with the game, so this one setting covers both. See the prerequisites in
  [PLAYTEST_PORTABLE_README.md](../PLAYTEST_PORTABLE_README.md). The exact
  checkbox label is not on the public page: **verify in the UI.**
- [ ] Tool app (if you have one): in the same place, enable "Dedicated Server
  Redistributables"
  ([Distributing Your Dedicated Game Server](https://partner.steamgames.com/doc/sdk/uploading/distributing_gs)).
  Also tick the VC++ x64 runtime if hosts install through Steam. Our server does
  not use the Steam SDK, so whether it needs the Dedicated Server
  Redistributables: **verify in the UI.**

### 3.4 Beta branch `playtest`

- **Order (seen on the first upload, 2026-11-08):** a new app's Builds page
  offers no branch button until a build exists, and then says "Once the default
  branch has a current build set, you can create other branches." So: make the
  first upload with `-Branch ''` (it sets nothing live), set that build live on
  **default** by hand on the Builds page (safe while the game is unreleased:
  only Developer Comp accounts and key holders can install it), and only then
  create `playtest`.
- [ ] In **SteamPipe → Builds**, click "Create new app branch", name it
  playtest (no spaces) and optionally set a **password**. With a password set, users "will
  be required to put in the password before they have access to the branch,
  including its name"
  ([Branches](https://partner.steamgames.com/doc/store/application/branches)).
  Do this on the game app, and on the tool app if you have one: an upload sets
  its branch live on every app it uploads (section 6.5).
- Uploads may put a build live on playtest automatically, and ours do: the
  config's branch is `playtest` (section 5). The **default**
  branch never gets a build automatically: "you can not set the build live on
  the default branch automatically. You must do that manually in App Admin"
  ([Branches](https://partner.steamgames.com/doc/store/application/branches)).
  Our tooling also refuses it on purpose.
- Testers of an **unreleased** game also need access to the app. Request
  "Release State Override" keys, which make the content "immediately playable
  upon activation", or use the Steam Playtest feature for large tests
  ([Steam Keys](https://partner.steamgames.com/doc/features/keys)).
  **⏳ WAIT:** key requests are reviewed "on a case-by-case basis"
  ([Steam Keys](https://partner.steamgames.com/doc/features/keys)).

### 3.5 Publish

- [ ] Open the **Publish** tab and publish the changes. Depots, launch options,
  redistributables and branches do nothing until you do
  ([Uploading](https://partner.steamgames.com/doc/sdk/uploading),
  [Common Redistributables](https://partner.steamgames.com/doc/features/common_redist)).

---

## 4. Builder account

Uploads run under a **separate Steam account** that can only upload. This keeps
the owner account's credentials out of the pipeline.

- [ ] **Create a new Steam account** for uploads, for example
  `ogbrawler-builder`. You can do this before the fee clears.
- [ ] **Invite it** from the Steamworks Add/Manage Users page. Enter an email
  address; it "does not need to match any existing Steam account". The invitee
  accepts by email, then an administrator must "approve or reject the
  confirmation within 7 days"
  ([Managing Users](https://partner.steamgames.com/doc/gettingstarted/managing_users)).
  **⏳ WAIT** for the invitee to accept, then approve promptly.
- [ ] **Limit its permissions.** Create a group that holds only the OGBrawler
  apps (the game, and the tool if you have one) and the builder account. The
  default "Everyone" group covers every app
  ([Managing Users](https://partner.steamgames.com/doc/gettingstarted/managing_users)).
  Grant only:
  - **Edit App Metadata**: needed to upload depots
    ([Uploading](https://partner.steamgames.com/doc/sdk/uploading),
    [Managing Users](https://partner.steamgames.com/doc/gettingstarted/managing_users)).
  - **Publish App Changes To Steam**
    ([Uploading](https://partner.steamgames.com/doc/sdk/uploading)).

  An administrator "can only grant permissions that they have themselves"
  ([Managing Users](https://partner.steamgames.com/doc/gettingstarted/managing_users)).
- [ ] **Phone or Steam Mobile App.** An account that sets a build live on a
  **released** app needs a phone number or the Steam Mobile App attached
  ([Uploading](https://partner.steamgames.com/doc/sdk/uploading)). Attach one
  now so it does not block you later.
- [ ] **First steamcmd login and Steam Guard.** Valve documents
  `steamcmd.exe +login <account_name> <password>`, then
  `set_steam_guard_code <code>` when Steam Guard asks for the emailed code
  ([Uploading](https://partner.steamgames.com/doc/sdk/uploading)). Log in
  **without** a password on the command line and let steamcmd prompt for it,
  then check that later logins reuse the cached credentials. **Verify both on
  the first login.** Our tooling never stores the password. Installing steamcmd
  comes in section 6.

---

## 5. Fill in the Steam publish config

Copy the IDs from sections 2–4 into `tools/steam/steam-publish.psd1`. The ID
fields ship as `0`, which means "not created yet". A local run without upload
accepts 0. An upload refuses a 0 ID only for the apps and depots it uploads, so
the tool's entries can stay 0 while you have no tool and use only profiles
without tool-server (section 6.3).

<!-- lint-anchor-ignore-begin: .psd1 keys and values are not indexed by the lint (it scans .h/.cpp/.ini/.cs/.md) -->
| Where the value comes from | Config key | Notes |
|---|---|---|
| Section 2: game app's App ID | `AppId` of the app named `game` | Integer. |
| Section 3.1: game app's client depot ID | `DepotId` of the depot named `game-client` | Integer. |
| Section 3.1: game app's server depot ID | `DepotId` of the depot named `game-server` | Integer. Keep its `DepotPath = 'Server'`. |
| Section 2: tool app's App ID | `AppId` of the app named `server-tool` | Integer. Leave 0 without a tool. |
| Section 3.1: tool app's depot ID | `DepotId` of the depot named `tool-server` | Integer. Leave 0 without a tool. |
| Section 4: builder account's Steam login name | `BuilderAccount` | Login name only. The password never goes in this file. |
| Section 3.4: branch name | `Branch` | `playtest`. `default` is always rejected, because setting the default branch live is done by hand in Steamworks. |
<!-- lint-anchor-ignore-end -->

The config (schema version 2) separates **what is packaged** from **where it
goes**. Its keys, as this project uses them:

<!-- lint-anchor-ignore-begin: .psd1 keys and values are not indexed by the lint (it scans .h/.cpp/.ini/.cs/.md) -->
| Key | In our config |
|---|---|
| `SchemaVersion` | `2`. |
| `ProjectFile`, `EngineRoot`, `Platform`, `OutputRoot`, `KeepLast` | The project, the engine (`''` = the one the project is registered with), `Win64`, where builds go (`Saved\Steam\Builds`), and how many build folders to keep (3). |
| `BuilderAccount`, `Branch` | See the table above. The branch is set live on every app an upload touches. |
| `Builds` | What is packaged, each build **once** per publish however many depots use it: `client` (Client, Shipping) and `server` (Server, Shipping). Each build has its `ExpectedExecutable` (the launcher of section 3.2), its `FileExclusions` (files kept out of the upload: debug symbols, manifests, and on the server the files a running server writes) and its `ExtraFiles` (the server gets `run_server.bat`). |
| `ServerLauncher` (on the `server` build) | The host files of section 7: map and log arguments, UDP port `7777`, the log lines that mean "ready" and "player joined/left", and the window title. `LocalHint` is the controls line the host window prints (Tab adds a local player, End removes one). |
| `ClientLaunch` (in `ServerLauncher`) | Which game the host window starts, first usable entry wins: `..\OGBrawlerUnrealClient.exe` (the game next to the bundled `Server` folder), else `steam://rungameid/{AppId:game}` (through Steam, for the tool; usable once the game's App ID is filled in). |
| `Apps` → `Depots` | Where builds go. Each depot has a `Name` (the names above), a `DepotId`, the `Build` it receives and an optional `DepotPath`: the folder inside the install folder (default: the root). Only game-server has `DepotPath = 'Server'`. |
| `Profiles` | Named sets of depots for `-Profile` (section 6.3). `Default` names the profile used when a publish names none: `playtest`. A profile name with a `-` is quoted (`'playtest-tool' = ...`). |
<!-- lint-anchor-ignore-end -->

- [ ] The game's App ID and both game depot IDs, and the builder account, are
  filled in. With a tool: its App ID and depot ID too.
- [ ] Check that each build's executable matches the launch options from
  section 3.2: `OGBrawlerUnrealClient.exe` for the client,
  `OGBrawlerUnrealServer.exe` for the server. The config already holds these
  values.
- The host files in the game's `Server` folder start the game next to them, so
  they work before any ID is filled in. In the tool they start the game through
  Steam (`steam://rungameid/<game App ID>`), which the next publish writes in
  once the game's App ID is filled in. While it is still 0, the tool's host
  files say so and ask the host to start the game by hand.

---

## 6. Publishing

Publishing runs on the dev PC in PowerShell 7 with the og-tools module loaded
(the `$PROFILE` import, or `Import-Module C:\dev\og-tools\og-framework.psd1`).
Run every command from the repository root. `Get-Help Publish-OgSteamBuild -Full`
lists every parameter.

### 6.1 Once: install steamcmd

- [ ] Run `Install-OgSteamCmd`. It downloads Valve's steamcmd into
  `%LOCALAPPDATA%\og-tools\steamcmd` and lets it update itself. Running it again
  does nothing unless you add `-Force`.
- [ ] The first upload (6.5) logs the builder account in. steamcmd asks for the
  password and the Steam Guard code itself and caches the login afterwards
  (section 4). Valve notes that the cached login token lives in steamcmd's
  `config\config.vdf`; keep that folder, or you type the password and code again
  ([Uploading](https://partner.steamgames.com/doc/sdk/uploading)).

### 6.2 Every build: local check, no Steam

```powershell
Publish-OgSteamBuild -ConfigPath tools\steam\steam-publish.psd1 -NoUpload
```

- Packages the builds that the default profile, playtest, needs (section 6.3):
  the client and the server, both Shipping, each once, into
  `Saved\Steam\Builds\<label>\client\` and `Saved\Steam\Builds\<label>\server\`.
  It writes the Steam upload scripts (one per app and one per depot, in
  `<label>\_steam\`) and the host files (into the server build), and contacts no
  Steam server. IDs that are still 0 are accepted here.
- The label is the date, time and short git commit, for example
  `20260929-133933-3937eeb`. It is written into `build_info.txt` in every build,
  shown on the game's join screen and printed by the server.
- The tree must be committed. With uncommitted changes the run stops; add
  `-AllowDirty` for a local test build (its label ends in `-dirty`).
- The result lists every destination of the run (**Destinations**: app,
  depot, build, depot path and **UploadBytes**, what Steam will receive) and,
  per depot, **SizeBytes** (the whole build folder, including the debug symbols
  that are not uploaded). Measured: client 435,059,184 and server 344,603,700
  upload bytes.
- The editor must be closed while packaging; the run refuses to start otherwise.
- `-SkipPackage -BuildLabel <label>` reuses an existing build and only rewrites
  the upload scripts, `build_info.txt` and the host files (under a second).
  With `-Profile` or `-Depots` it writes them for other destinations of the
  same build (6.3).

### 6.3 Choosing destinations: profiles

One publish packages each build once, and every destination it fills gets the
same build label. Which depots it fills is chosen per run:

| Passed | Depots filled |
|---|---|
| neither option | the depots of the default profile, playtest |
| `-Profile <name>` | the depots of that profile |
| `-Depots <name>, <name>` | exactly those depots; cannot be combined with `-Profile` |

The profiles in our config:

| Profile | Depots | Use it when |
|---|---|---|
| playtest (default) | game-client, game-server | Every normal playtest upload: the game with its server in `Server`. |
| playtest-tool | game-client, game-server, tool-server | The Dedicated Server tool should get the same build too. This uploads the server **twice**, once into each app (about 345 MB each), because the tool cannot share the game's depot before release (section 2). |
| game-only | game-client | Only the client should change. Partial: needs `-AllowPartialApp` (below). |
| release | game-client, tool-server | The server should live only in the tool. Partial: needs `-AllowPartialApp` (below). |

**The partial-app guard.** If a run fills only some of an app's depots, the
publish stops before packaging and names the depots it leaves out. The reason:
Valve does not document what Steam does with a depot that an upload leaves out.
Its previous build could stay installed, for example an old server in the
game's `Server` folder whose build label no longer matches the game. The guard
also stops `-NoUpload` and `-Preview` runs, so a dry run shows it.
`-AllowPartialApp` uploads anyway:

```powershell
Publish-OgSteamBuild -ConfigPath tools\steam\steam-publish.psd1 -Profile game-only -AllowPartialApp
```

- **game-only** and **release** both fill the game app without game-server, so
  the guard stops both. Until the first real uploads show what Steam does with
  the left-out depot (6.8), release is a **deliberate partial upload**.
- Once the server lives only in the tool, the clean end state is to delete
  game-server from the game app: in Steamworks (the depot) and in the config
  (its depot entry, and the profiles that list it). Then release fills every
  depot of the game app and is no longer partial.

**More destinations from one build.** Package once, then write the upload
scripts for another profile, or upload the tool later from the same build:

```powershell
Publish-OgSteamBuild -ConfigPath tools\steam\steam-publish.psd1 -SkipPackage -BuildLabel <label> -Profile playtest-tool
Publish-OgSteamBuild -ConfigPath tools\steam\steam-publish.psd1 -SkipPackage -BuildLabel <label> -Depots tool-server
```

The second line fills the tool's only depot, so the guard does not stop it.
Add `-NoUpload` to either line for a local check. Upload scripts that an earlier
run with another profile wrote stay in the build's `_steam` folder; each app's
script lists only the depot scripts of its own run, and only those are uploaded.

### 6.4 Steam-side dry run

```powershell
Publish-OgSteamBuild -ConfigPath tools\steam\steam-publish.psd1 -Preview
```

- Needs the real IDs and builder account from section 5 for every app the run
  uploads. steamcmd logs in and makes a preview build only. Valve: "This type
  of build only outputs logs and a file manifest into the build output folder."
  ([Uploading](https://partner.steamgames.com/doc/sdk/uploading)). The logs land
  in `Saved\Steam\Builds\<label>\_steam\output\`.
- Check the manifests: no `.pdb` files, nothing from HostLogs, no
  `join_info.txt`, no `Saved` folder in a server depot, `build_info.txt`
  present in every depot, and the game app's server files under `Server`.
  Our exclusions name files by their path in the local build folder. Valve's
  example does the same (it excludes a file in its local bin folder although
  that folder is mapped to executables in the depot,
  [Uploading](https://partner.steamgames.com/doc/sdk/uploading)), but the page
  does not say it outright. The first preview (2026-11-08) confirmed it: the
  server depot held no `.pdb`, `Manifest_*.txt`, HostLogs, `join_info.txt` or
  `Saved` files under `Server`, so the exclusions match local paths. Keep the
  check as a habit.

### 6.5 Real upload to `playtest`

```powershell
Publish-OgSteamBuild -ConfigPath tools\steam\steam-publish.psd1
```

- Packages, uploads every app of the profile (playtest: the game app with both
  depots) and sets the build live on the `playtest` branch of each. The build
  then appears on the app's builds page
  ([Uploading](https://partner.steamgames.com/doc/sdk/uploading)).
- **Players and hosts need builds from the same run.** The server checks the
  client's build label; a client from one publish cannot join a server from
  another ("different build", section 7.3). The game and its bundled server
  always come from one run. Give the tool the same build: in the same run
  (playtest-tool), or later with `-SkipPackage -BuildLabel <label> -Depots
  tool-server` (6.3).
- `-Description 'Playtest 3'` sets the text shown in Steamworks (default: the
  label). `-Branch <name>` overrides the config's branch for one run.
- **The default branch** (what every owner gets) is set live **only in the
  Steamworks web UI**: pick the branch in "Set build live for branch...", then
  "Preview Change" and "Set Build Live Now"
  ([Branches](https://partner.steamgames.com/doc/store/application/branches)).
  Valve does not let scripts do it
  ([Uploading](https://partner.steamgames.com/doc/sdk/uploading)), and our tool
  refuses `default` as a branch name.

### 6.6 Good habits

- **Publish from a fresh build.** Running the server straight out of
  `Saved\Steam\Builds\<label>\` writes log and settings files into the server
  build. The config excludes those files from the upload (the HostLogs
  folder, `join_info.txt` and the two `Saved` folders), but a fresh build is the
  safer habit.
- **How the host files find the game.** In the game's `Server` folder they start
  the game next to them, with no Steam ID needed. In the tool there is no game
  next to them, so they start it through Steam, with
  `steam://rungameid/<game App ID>`; the first publish after section 5 writes
  that ID in.
- **Disk:** one build folder holds about 4.2 GB, because the debug symbols stay
  on disk. The config keeps the newest 3 build folders and deletes older ones.
  Keep the symbols of any build you hand out: they are the only way to read a
  crash from it (section 8).
- **Firewall rules on the dev PC:** each build folder gets its own Windows
  firewall rule the first time its server runs. Old ones are harmless; delete
  them in Windows Defender Firewall if the list gets long.

### 6.7 Measured timings (this dev PC)

| Run | Time |
|---|---|
| Incremental publish with `-NoUpload`, client and server | 2 min 26 s |
| Publish with `-NoUpload` after about a month of code changes (full client cook) | 17 min |
| First publish with a Shipping server (its first build compiles the engine for it) | about 21 min, of which the server about 20 min |
| `-SkipPackage` rerun, any profile | under 1 s |
| Upload | not measured yet; after the first upload SteamPipe sends only the changed parts of files ([Uploading](https://partner.steamgames.com/doc/sdk/uploading)) |

A fresh machine, or a switch to another build configuration, pays the long
first build once.

### 6.8 Check on the first real uploads

Three things no Valve page answers. The first two were checked on the first
upload (build 26519805, 2026-11-08):

- [x] **Launch-option choice.** Clicking Play on the game offers the three
  launch options of section 3.2 to choose from.
- [x] **A `.bat` as a launch target.** Host Local Playtest starts the host
  window from Steam: the server starts, "Start the game on this PC now?" starts
  the game, and it joins This PC. Steam accepts a `.bat` as a launch target.
- [ ] **What a partial upload leaves installed.** Still open; it matters only
  before the first upload with the game-only or release profile. On a
  throwaway branch (made like `playtest` in section 3.4, for example
  `partialtest`), upload playtest,
  install it on a test PC, then upload game-only:

  ```powershell
  Publish-OgSteamBuild -ConfigPath tools\steam\steam-publish.psd1 -Branch partialtest
  Publish-OgSteamBuild -ConfigPath tools\steam\steam-publish.psd1 -Profile game-only -AllowPartialApp -Branch partialtest
  ```

  Once Steam has updated the test PC, is the `Server` folder still there, and
  with which `build_info.txt` label? This decides what "leave the server out"
  means, and so whether release may stay partial (6.3).

---

## 7. Playtest guides

Three one-page guides for playtesters who are not developers. They can be copied
into a message as they are. Start with the local playtest: it proves the install
works before anyone deals with routers.

Controls used below: **Tab** adds a local player and **End** removes one
(keyboard). Each extra local player needs a gamepad: the first gamepad drives
player 1 (who also has the keyboard and mouse), the second gamepad drives
player 2, and so on.

### 7.1 Local playtest — everyone on one PC

You need: one Windows PC with OGBrawler installed and opted into the playtest
(7.3 steps 1–2). The server comes with the game, in its `Server` folder.

1. In your Steam Library, click **Play** on OGBrawler and choose **Host Local
   Playtest** from the choice Steam shows. If Steam ever starts the game
   straight away instead, close it and use the file: right-click
   OGBrawler → **Manage** → **Browse local files**, open the **Server** folder
   and double-click **Host Local Playtest** (menu names: **verify in the UI**).
   With the separate Dedicated Server tool (section 2), the same file is at the
   top of the tool's install folder; if the tool is not in your Library list,
   Steam is hiding tools: tick **Tools** in the Library's filter (**verify in
   the UI** where the filter is).
2. A console window opens.
3. The first time only, Windows asks for permission to add a firewall rule.
   Click **Yes**. If you click No, play on this PC still works.
4. Wait for **Server is ready on UDP 7777.**
5. The window asks **Start the game on this PC now? [Y/n]**. Press Enter; the
   window starts the game that is installed next to the `Server` folder. With
   the tool, it starts the game through Steam instead. If it prints **Not
   starting the game automatically: … Start it yourself.** (with the tool while
   the Steam app IDs are not filled in yet, section 5), or says it could not
   start the game, start OGBrawler from Steam yourself.
6. In the game, pick **This PC**, the top line of the list, and press **Enter**
   (or **A** on a gamepad). The first time it is already selected. After you
   have joined another server, that server's address is in the field instead:
   press **Up** (or D-pad up) until This PC is highlighted.
7. Press **Tab** to add a player; the console prints a "Player joined" line.
   **End** removes the most recently added player; player 1 always stays.
8. To stop: close the game, then press **Ctrl+C** in the console window.

Good to know:

- One PC can have at most **4** local players (an engine limit). A fifth Tab
  shows a short notice in the game and changes nothing.
- A second PC on the same network can join too, with the LAN address the console
  shows in its box.
- More than 3 players in total works and nobody is turned away, but it is above
  the tested size, so the game may run worse. Up to 8 characters have their own
  place to respawn.
- When players outside your home should join, switch to 7.2.

### 7.2 Hosting — online and on your network

You need: a Windows PC on the internet with OGBrawler installed (the server
comes with it), or with the separate **OGBrawler Dedicated Server** tool (section
2), which needs no copy of the game.

1. Click **Play** on OGBrawler and choose **Host Online Playtest**, or
   double-click **Host Online Playtest** in the game's **Server** folder or at
   the top of the tool's install folder (finding them: 7.1 step 1).
2. **Firewall:** the first time, Windows asks for permission once. Click **Yes**.
3. **Router:** the window tries to open UDP port **7777** on your router
   automatically (UPnP).
   *Port forwarding* tells your router to pass players' traffic on port 7777
   to this PC; without it the router drops that traffic. *UPnP* is a router
   feature that lets a program set up that forwarding by itself.
   - "UDP 7777 forwarded to this PC … via UPnP": done. It is removed again when
     you stop with Ctrl+C.
   - "UPnP unavailable" or "UPnP port forwarding failed": open your router's
     "port forwarding" page and forward **UDP 7777** to the address the window
     names. Your router's manual shows how.
   - "UDP 7777 is already forwarded to another PC (*address*). Stop the server
     there, or change the forwarding by hand.": another PC on your network
     already receives port 7777, usually one that hosted before. Stop the
     server on that PC (Ctrl+C there removes its forwarding), or change the
     forwarding on the router's "port forwarding" page to this PC's address,
     which the window names. Then run Host Online Playtest again.
4. **Carrier-grade NAT:** if the window prints a red **WARNING** that your
   router's internet address is not your public address, your internet provider
   shares one address between several customers, and players on the internet
   most likely cannot reach you. Options:
   - ask your provider for a public IPv4 address, or host from another
     connection;
   - rent a Windows cloud server near the players and host there;
   - for a group that trusts each other, use a virtual network such as Tailscale
     (a program everyone installs that connects your PCs as if they were on
     one home network) and have everyone join with its address;
   - players on your own network can still join with the LAN address.
5. **Share the join address.** It is shown in a box, already copied to the
   clipboard, and saved in `join_info.txt` in the same folder. Paste it to your
   players. The window also lists the LAN address (for players on your network)
   and This PC (for a game on the host PC).
6. The window asks whether to start the game on this PC. Answer **Y** if you
   play too (then pick **This PC** in the game), **n** if you only host. If it
   prints **Not starting the game automatically: … Start it yourself.** instead,
   start OGBrawler from Steam when you want to play (as in 7.1 step 5).
7. While the server runs, the window prints what happens:

   | Line | Meaning |
   |---|---|
   | Server is ready on UDP 7777. | Players can join now. |
   | [20:15:02] Player joined (2/3) | A player joined; 2 players are in, of the tested 3. |
   | Player joined (4 players - above the tested 3, expect degraded performance) | More players than tested. Nobody is refused; the game may run worse. |
   | Player left (1/3) | A player left; 1 is still in. |
   | The server stopped (exit code …). Its log: … | The server ended. If that was unexpected, send the log file to the developer. |

8. **Stop with Ctrl+C** in the window. That stops the server and removes the
   router forwarding. Closing the window with the X instead leaves the
   forwarding in place until the next run, which reuses and then removes it.

If players cannot connect although the address is right, a Windows firewall
**Block** rule may be in the way. Windows leaves one behind when someone once
clicked Cancel on its own "allow access" dialog for the server, and it overrides
our Allow rule. To fix it, open **Windows Defender Firewall → Allow an app or
feature through Windows Defender Firewall**, find the OGBrawler entries and tick
both **Private** and **Public**. If a blocked entry remains, open **Advanced
settings → Inbound Rules**, delete the OGBrawler rules marked with a Block sign,
and run Host Online Playtest again. (Menu names as in current Windows 10/11:
**verify on the PC**.)

### 7.3 Playing

You need: a Windows PC with Steam, the playtest password from the developer, and
the join address from the host.

1. Install OGBrawler from Steam (your key or invite comes from the developer).
2. **Opt into the playtest:** in your Steam Library, right-click OGBrawler →
   **Properties** → **Game Versions & Betas**. Enter the password, then choose
   **playtest**
   ([Branches](https://partner.steamgames.com/doc/store/application/branches);
   the exact button names: **verify in the UI**). Steam downloads the playtest
   build.
3. Click **Play**. If Steam asks which to start, pick the game itself, not one
   of the Host options. The join screen opens.
4. **Paste the address** (Ctrl+V) or type it, for example `203.0.113.7:7777`.
   Leaving out `:7777` is fine. Or pick a recent address from the list (Up/Down,
   or the gamepad D-pad). **This PC** means a server on your own PC.
5. Press **Enter** (or **A**) to join. The screen shows "Connecting to …".
   Press **Escape** (or **B**) to cancel a join that takes too long.

If joining fails, the screen shows one of these:

| Message | What to do |
|---|---|
| **Can't reach server** *address*. Check the address, and that the server is running and reachable. | Check the address with the host, and that the host's window shows "Server is ready". If it still fails, the host's router or firewall blocks it (7.2 steps 3–4 and the firewall note). The message appears after about 20 seconds; Escape (or B) cancels sooner. |
| **Different build.** This game is *label*; the server runs a different build. | Your game and the host's server come from different publishes. Both update through Steam; players can download a new build "within a couple of minutes" of it going live ([Uploading](https://partner.steamgames.com/doc/sdk/uploading)). Let Steam finish updating OGBrawler (restarting Steam makes it check), and ask the host to update the Dedicated Server tool the same way. |
| **Connection lost.** The server stopped responding. | The server stopped or the connection dropped. Ask the host whether the server still runs, then join again. |
| **Server refused:** *reason* | The server turned you away and says why. Tell the host the reason. |
| **Could not join.** *details* | Anything else. Try again; if it repeats, send the details to the developer. |

The join screen shows **Build:** followed by the build label. Include it when you
report a problem. Everyone in a session, and the host's server, needs the same
build.

For advanced players: a Steam launch option holding only the address (for
example `203.0.113.7:7777`) makes the game join that server straight away,
without the join screen. Where Steam lets you set launch options for a game:
**verify in the UI**.

---

## 8. Deferred — not in this pipeline

- **Steam inside the game.** No SteamAPI start-up or ownership check, no
  OnlineSubsystemSteam, no Steam networking. With those, hosts would not need
  port forwarding and friends could invite each other through Steam. Today
  players join by address.
- **LAN discovery.** The join screen does not search the local network for
  servers; players type or paste the LAN address. Not planned for now.
- **Store page and Valve review.** Only the private `playtest` branch is used.
  The waits before a public release are in section 1.
- **Crash reporting and symbol upload.** Nothing is sent to Steam. The debug
  symbols (`.pdb`) stay on the dev PC in `Saved\Steam\Builds\<label>\`, for the
  newest 3 builds only (section 6.6).
- **The 9th character.** A server has respawn places for 8 characters. A 9th
  simultaneous character still has no place and keeps dying on respawn.
