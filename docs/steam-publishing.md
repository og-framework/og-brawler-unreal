<!-- SPDX-License-Identifier: BUSL-1.1 -->
# OGBrawler — Publishing to Steam

How to take OGBrawler from "no Steamworks account" to "IDs filled in and ready to
upload". Work through it top to bottom; every step is a checkbox.

What we ship: a Win64 **client** (Shipping) and a Win64 **dedicated server**
(Development). The game itself contains no Steam SDK code — Steam is used only
to distribute the files.

Conventions in this document:

- **⏳ WAIT** marks a step with a waiting period. Start those early.
- **Verify in the UI** marks a detail that no public Steamworks page confirms.
  Check it on the Steamworks site and fix this document if it differs.
- Every other claim links to the Valve page it comes from (checked 2026-09-26).
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

Recommended layout: **two apps**.

| App | Type | Depot | What it holds |
|---|---|---|---|
| OGBrawler | Game | client-win64 | The Shipping client that players install |
| OGBrawler Dedicated Server | Tool (linked to the game) | server-win64 | The Development dedicated server that hosts run |

Why two apps: players who install the game never download the server. The
server can also be offered to hosts through **anonymous** steamcmd downloads,
without a copy of the game
([Distributing Your Dedicated Game Server](https://partner.steamgames.com/doc/sdk/uploading/distributing_gs)).

- [ ] **Create the game app.** On the Steamworks landing page, under "Create A
  New Application", click "Create new app..."
  ([Applications](https://partner.steamgames.com/doc/store/application)).
  Steam gives it a unique **App ID**
  ([Applications](https://partner.steamgames.com/doc/store/application)).
  Write it down; it goes into section 5.
- [ ] **Set the app type to Game** in General Application Settings
  ([Applications](https://partner.steamgames.com/doc/store/application)).
- [ ] **Create the Dedicated Server tool app.** On the game app, open "All
  Associated Packages, DLC, Demos and Tools" and click "Create new Tool"
  ([Distributing Your Dedicated Game Server](https://partner.steamgames.com/doc/sdk/uploading/distributing_gs)).
  Write down the tool's App ID too.
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

**Alternative: one app, two depots.** You can put the server depot inside the
game app instead, but then every player downloads the server unless you split
the depots into separate packages. In section 5 this means a single app entry
holding both depots (**verify in the UI** how to stop the server depot from
installing for players).

---

## 3. Configure each app

Do this for **both** apps. Every change here stays hidden until the Publish step
at the end.

### 3.1 Depots

- [ ] Open the app's **Depots** page. Rename the default depot (for example
  "Windows Content"), set **Language** to "[All languages]" and **OS** to
  "[All OSes]", then click "Save Changes"
  ([Uploading](https://partner.steamgames.com/doc/sdk/uploading)).
  The game app gets one depot for the client, and the tool app gets one depot
  for the server.
- [ ] **Write down each Depot ID** from the Depots page. Valve's docs do not say
  how depot IDs are numbered, so read them off the page. **Verify in the UI.**
- [ ] Make sure each depot is in a package: "Your newly defined depots will need
  to be included in a package to grant you ownership of them"
  ([Uploading](https://partner.steamgames.com/doc/sdk/uploading)).

### 3.2 Launch option

- [ ] Under **Installation → General Installation Settings**, add a launch
  option with the executable path, relative to the depot root, plus any
  arguments ([Uploading](https://partner.steamgames.com/doc/sdk/uploading)).
  - Game app, client: executable path **filled in after T8** (the first
    Shipping package). The expected value is `OGBrawlerUnrealClient.exe`, since
    the depot root is the packaged `<archive>\WindowsClient` folder.
  - Tool app, server: executable path **filled in after T8**. The expected
    value is `OGBrawlerUnrealServer.exe`. Whether a tool fetched only through
    steamcmd needs a launch option at all: **verify in the UI.**

### 3.3 Redistributables

- [ ] Game app: open **Installation → Redistributables** and tick the Visual C++
  2015–2022 x64 runtime
  ([Common Redistributables](https://partner.steamgames.com/doc/features/common_redist)).
  The packaged client needs this runtime. See the prerequisites in
  [PLAYTEST_PORTABLE_README.md](../PLAYTEST_PORTABLE_README.md). The exact
  checkbox label is not on the public page: **verify in the UI.**
- [ ] Tool app: in the same place, enable "Dedicated Server Redistributables"
  ([Distributing Your Dedicated Game Server](https://partner.steamgames.com/doc/sdk/uploading/distributing_gs)).
  Also tick the VC++ x64 runtime if hosts install through Steam. Our server does
  not use the Steam SDK, so whether it needs the Dedicated Server
  Redistributables: **verify in the UI.**

### 3.4 Beta branch `playtest`

- [ ] In **SteamPipe → Builds**, click "Create new app branch", name it
  playtest (no spaces) and set a **password**. With a password set, users "will
  be required to put in the password before they have access to the branch,
  including its name"
  ([Branches](https://partner.steamgames.com/doc/store/application/branches)).
  Do this on both apps.
- Uploads may put a build live on playtest automatically. The **default**
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
- [ ] **Limit its permissions.** Create a group that holds only the two OGBrawler
  apps and the builder account. The default "Everyone" group covers every app
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
accepts 0, but any real upload with a 0 ID is refused.

<!-- lint-anchor-ignore-begin: .psd1 keys and values are not indexed by the lint (it scans .h/.cpp/.ini/.cs/.md) -->
| Where the value comes from | Config key | Notes |
|---|---|---|
| Section 2: game app's App ID | `AppId` of the app named `client` | Integer. |
| Section 3.1: game app's depot ID | `DepotId` of the depot named `client-win64` | Integer. |
| Section 2: tool app's App ID | `AppId` of the app named `server` | Integer. |
| Section 3.1: tool app's depot ID | `DepotId` of the depot named `server-win64` | Integer. |
| Section 4: builder account's Steam login name | `BuilderAccount` | Login name only. The password never goes in this file. |
| Section 3.4: branch name | `Branch` | `playtest`. `default` is always rejected, because setting the default branch live is done by hand in Steamworks. |
<!-- lint-anchor-ignore-end -->

- [ ] All four IDs and the builder account are filled in.
- [ ] If you used the one-app alternative from section 2: move the server depot
  entry into the client app's depot list and delete the server app entry.
- [ ] Check that each depot's executable matches the launch option from section
  3.2 (**filled in after T8**).

<!-- Sections 6–8 are added by task 7. -->
