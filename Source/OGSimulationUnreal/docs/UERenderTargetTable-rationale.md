<!-- SPDX-License-Identifier: MPL-2.0 -->
# `UERenderTargetTable.h` and `.cpp` — rationale

The table is the Unreal render sink of og-simulation's `TickDriverPresenter`: it maps a simulation id to the actor
the presenter moves, applies a root pose to that actor, and reads back where the actor's root is. The source holds
the licence, a docs pointer, code, one guard tag and its `checkf` messages; this file carries the why. The one
prohibition lives in `UERenderTargetTable-guards.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.**

---

## 1. Placement and callers

The table is host code: it reads Unreal types and lives in the MPL-2.0 OGSimulationUnreal module. It names no game
type and no physics backend. It models og-simulation's `TickDriverRenderSink` concept (`TickDriverConcepts.h`):
`applyRootPose`, `readAnchor` and `name`. The presenter calls those three from `present_GameThread`, on the game
thread; the game calls `registerTarget` and `unregisterTarget`, also on the game thread. Nothing else touches it, so
it holds no lock and no atomic, and it reads none of og-simulation's tick-driver thread masks.

**Callers today:** the tick-driver host `TickDriverBackendUImpl` (this module's header template since og-tickdriver
task 15e; the brawler's Jolt host, instantiated in OGBrawlerUnreal) owns one table as
`m_renderTargets`. It registers each character on its Ready path, next to the presenter's own `onRegistered`, and
unregisters it next to the presenter's `onUnregistered` (`TickDriverBackendUImpl-rationale.md` §5).

## 2. What it holds

One entry per registered id: a weak pointer to the actor and the actor's name in UTF-8, taken at registration. An
entry whose actor is gone, or was never there, is a target the presenter skips: `applyRootPose` writes nothing and
returns empty, and `readAnchor` returns empty. That is the contract the presenter relies on (its rationale §5): a
target whose engine object is gone is the one whose anchor read is empty. The engine's weak pointer reads a
destroyed actor as gone, as the engine-hosted render path's weak pointer did.

The presenter keeps its own id-to-slot table, and it iterates that one; the table here is looked up by id only, so
its container order never reaches a line.

## 3. Registration

`registerTarget` takes the id, the actor, the expected root component and the role, and:
1. stores the entry (an empty actor stores an entry too, so the presenter's target count and this table agree);
2. in a non-shipping build, `checkf`s that the actor's root component is `expectedRoot`, the component the game
   names as its render root. The apply moves the actor, so the root is what moves, and the checks read the root's
   translation; the engine-hosted render path read the brawler's capsule instead. The brawler's capsule is its root
   by construction (`OGBrawlerUECharacter.cpp`, `RootComponent = GetCapsuleComponent();`), and this check turns a
   later change of the root into a failure at the first registration, not a silent change of what the checks read.
   The brawler passes `GetCapsuleComponent()`;
3. switches the root's physics simulation off (`SetSimulatePhysics(false)`), so the engine's own physics never
   moves the actor between two applies (`OGBrawlerUECharacter-rationale.md` §1.2 for the brawler's capsule);
4. logs one `[SimHost.RenderTarget]` line: the role, the id, whether the root still simulates physics, and the
   names of every primitive component of the actor that still does (for the brawler, the cosmetic camera-axis
   sphere).

**The line's text is frozen.** Its field names (`capsuleSimulatesPhysics`, `simulatingComponents`) are the ones the
brawler's host printed before the table existed, kept byte for byte so a session before the move and one after it
print the same line; for another game the first field is its root's state.

**The log category is `LogOGSimHost`.** The line goes to it at `Log`, through the engine's UE_LOG. The category is
declared, exported, in this module's `OGSimulationUnreal.h` and defined in `OGSimulationUnreal.cpp`, under the name
the brawler's host lines have always used, so `Config/DefaultEngine.ini`'s `LogOGSimHost=Warning` still governs it.
The line appears exactly when the old UE_LOG line on LogOGSimHost at Log did, under the same category name and with
the same text.

*History.* Between og-tickdriver tasks 9 and 11 the category was declared in the game module, which this module
cannot name, so the owner passed it to the constructor as an engine log-category reference and the table logged
through the engine's category-reference macro (UE_LOG_REF), which applies the same verbosity test as UE_LOG: the
category's runtime suppression, against its compile-time verbosity. Task 11 moved the category into this module, and
the table took the plain UE_LOG and lost its constructor parameter.

## 4. The doors

* **`applyRootPose(id, position, rotation)`** converts the position to centimetres, takes the given rotation or,
  when there is none, keeps the actor's own quaternion, and moves the actor with no sweep and the engine's teleport
  (`⛔G-01`). It returns the actor's location after the move, read back from the engine, in og-simulation lengths:
  where the engine actually put it, which the presenter records as the applied position. The engine does not move a
  component whose new location equals the old one within its no-move threshold, so this is not always the pose
  passed in (`TickDriverPresenter-rationale.md` §8).
* **`readAnchor(id)`** is the root component's world translation, in og-simulation lengths. It is the position the
  presenter's held and applied checks compare, and the `from=` of its first-apply line. For an actor it equals the
  actor's location (the engine defines the actor's location as its root component's).
* **`name(id)`** is the actor's name at registration, or an empty string. No moved line prints it.

**Both reads return `std::optional<glm::dvec3>`:** the actor's double-precision location, never rounded to float.
The engine stores positions in double. A float would round the anchor to about 6e-5 cm at 1,000 cm, above the held
check's 1e-6 cm tolerance, so the MOVED count and the applied check's `exact` count would change.

## 5. Units

`kCentimetresPerLength` is the number of engine centimetres in one og-simulation length: 1.0 today, because the
simulation runs in centimetres. Every position crossing the table is converted with it, positions in by
multiplication and positions out by division, so a metre simulation changes this one constant.
`lengthFromCentimetres` converts a centimetre value the other way, for centimetre console variables
(`og.Render.ErrorCorrectionMaxCm`, which this module's `UEPresentControls.cpp` converts since og-tickdriver task 15c).
With the factor 1.0 every conversion returns its input exactly.

## 6. Provenance

The table is the engine half of the brawler Jolt host's render apply and render-target registration, moved out of
the Jolt host (JoltBackendUImpl, its historical name; `TickDriverBackendUImpl` since og-tickdriver task 15b) by og-tickdriver task 9; the engine-free half went to og-simulation's `TickDriverPresenter`. What
moved here:
- the registration's `SetSimulatePhysics(false)` and its `[SimHost.RenderTarget]` line, with the same format;
- the apply's teleport and its rotation rule, with `TickDriverBackendUImpl-guards.md` G-04, retired there in favour of this
  file's G-01;
- the reads of the actor's location before and after the move, and of the capsule's translation, now one root read.

What changed, and why each change keeps the behaviour:
- the weak pointer is to an `AActor`, not to the brawler's character class: the table needs only the actor;
- the physics switch and the log read the actor's root instead of the brawler's capsule: the registration's
  non-shipping `checkf` asserts they are the same component;
- the host's category was passed in rather than named until og-tickdriver task 11, which moved it into this module (§3);
- the length conversions are identities (§5).
