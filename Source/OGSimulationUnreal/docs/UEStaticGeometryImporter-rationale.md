<!-- SPDX-License-Identifier: MPL-2.0 -->
# `UEStaticGeometryImporter.h` and `.cpp` — rationale

The importer walks a `UWorld` once and turns its static collision into the engine-independent
`StaticWorldDescription` of `StaticGeometry.h`, so that a physics backend outside Unreal can build
the same static world Chaos builds today. The source holds the licence, a docs pointer, code and
four guard tags; this file carries the why. Prohibitions live in `UEStaticGeometryImporter-guards.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.**

---

## 1. Placement and callers

The importer is host code: it reads Unreal types and lives in the MPL-2.0 OGSimulationUnreal
module. It names no game category and no physics backend. Its output type is og-simulation core
(`StaticGeometry.h`), which it uses read-only.

**Callers today:** only the console command `og.sim.ImportStaticGeometry` in the same `.cpp` (§9).
Wiring the importer into the Jolt host, with the game's category table, is a later task; nothing in
the game path calls it yet.

## 2. The category table is an input

`CollisionCategories` bits are consumer-local ids, not engine channels (`QueryGeometry.h`). This
module cannot know a game's ids, so the importer takes the table as `UEStaticCategoryChannel`
pairs, one category to one `ECollisionChannel`, in the same shape as the table the brawler hands
the Chaos query adapter (`SimulationManagerUImpl.cpp` :: `emplaceBrawlerQueryAdapter`).

`addMapping` rejects (with an Error line) a category of 32 or more, a channel outside the enum, and
a pair that repeats a category or a channel already mapped. A one-to-one table is what makes the
channel → category lookup (`m_channelToCategory`) well defined.

## 3. Which components are imported

The walk visits every actor of the world (`TActorIterator`) and each of its primitive components.

| component | handling | why |
|---|---|---|
| not registered, or collision NoCollision | ignored, not counted | it has no body in Chaos either |
| landscape heightfield collision | **Unsupported**, counted, Warning line | no height-field shape in M1 (follow-on 16). Detected by class name up the class chain, so the module needs no Landscape dependency |
| simulating physics | skipped, counted | a dynamic body, not a static |
| mobility Movable | skipped, counted | it can move at runtime; a static is built once |
| skinned mesh | **Unsupported**, counted, Warning line | collision comes from a physics asset, not one body setup |
| no body setup | skipped, counted | nothing to convert |
| instanced static mesh | imported **per instance**, each with its own world transform | Chaos gives each instance its own body |
| any other primitive with a body setup | imported | static meshes, and also box/sphere/capsule components and brushes, whose body setups hold simple elements |

Each skip is logged at Verbose with the actor path, the component name and its class. On
ThirdPersonMap the skips were the brawler capsule (movable), its camera sphere (simulating), the
gameplay-debugger rendering component (movable, standalone only) and a text render component
(movable, dedicated server only).

## 4. `categories` and `blockingCategories`

* **`categories`** is the category mapped to the component's **object type**. A component whose
  object type is not in the table has no category; its elements are counted as
  `elementsUnmappedObjectType` and **not emitted**. Under the M1 rule (Chaos's AND) such a shape
  collides with nothing and no query can find it, because queries search mapped categories only,
  so leaving it out changes nothing and keeps the backend's body count down.
* **`blockingCategories`** has the bit of every mapped channel the component's response to which is
  **Block**. A BlockAll static therefore collides with every mapped category. The brawler capsule's
  channel is declared with a default response of Block in `DefaultEngine.ini`, so every profile that
  does not name it, BlockAll included, blocks the capsule, and the capsule lists the world category:
  capsule-versus-world collides under the AND rule, as it does in Chaos.
* The set is filled only when the element's collision includes physics (G-04). The element's
  setting is the intersection of the component's and the element's own, as Chaos combines them.
  Elements that collide physically but are not queryable (PhysicsOnly) are emitted and counted as
  `elementsPhysicsOnly`: the descriptor has no "not queryable" flag, so a backend's queries would
  find them although Chaos's would not. None exist on ThirdPersonMap.

Observed on ThirdPersonMap with the brawler's table: every imported shape has `categories=0x10`
(world, category 4) and `blockingCategories=0x3f` (all six mapped categories).

## 5. Shapes: the importer mirrors how Chaos builds the body

Each body is converted the way the engine's Chaos geometry builder (ChaosInterface::CreateGeometry,
in the engine's PhysicsEngine/Experimental/ChaosInterfaceUtils.cpp) converts it, so the description
holds the geometry Chaos collides with, not a re-derivation of it.

* **Simple or complex (G-02).** The trace flag is read from the body setup, with `CTF_UseDefault`
  resolved through the project physics settings. A complex-as-simple body with at least one
  triangle mesh imports its triangle meshes only; every other body imports its simple elements only.
* **Scale.** The body is built with the component's world scale and an identity relative
  transform; an almost-zero scale becomes 0.1 on every axis, as the engine's body setup does.
  Spheres, boxes and (tapered) capsules go through the engine's own per-element scaling
  (`GetFinalScaled`), the same call Chaos makes.
* **Box** → `StaticBox`: half of each scaled edge length; the element's centre and rotation become
  the shape's local transform.
* **Sphere** → `StaticSphere`: the scaled radius at the scaled centre.
* **Capsule** → `StaticCapsuleZ`: the engine's capsule element stores the **cylinder** length, and
  `totalHalfHeight` runs from the centre to a hemisphere tip (`StaticGeometry-rationale.md` §2.1),
  so totalHalfHeight = scaled length / 2 + scaled radius. The element's rotation puts its axis on
  local Z, which is the axis of `StaticCapsuleZ`.
* **Convex** → `StaticConvexHull`: the vertices of the element's cooked Chaos convex, each scaled
  per axis by the component scale (a zero component replaced by a small positive value, as Chaos
  does). These are the vertices of the hull Chaos collides with. An element without a cooked convex
  gets no Chaos shape either; it is counted as `convexWithoutChaosMesh`.
* **Triangle mesh** → `StaticTriangleMesh`: the vertices and triangles of the body setup's cooked
  Chaos triangle meshes, scaled per axis. With an odd number of negative scale components the
  winding of every triangle is reversed, so a mirrored mesh keeps its outward faces.
* Every radius, half-extent and half-length is clamped to at least the engine's small number
  (1e-4 cm), as Chaos clamps them.
* **Tapered capsule** → `StaticCapsuleZ`: Chaos builds a tapered capsule as a straight capsule
  whose radius is the mean of the two scaled end radii, and the importer does the same.

## 6. Non-uniform scale on spheres and capsules

A sphere or capsule under non-uniform scale cannot stay a sphere or capsule, and the Jolt shapes
cannot express the ellipsoid. The importer takes **the engine's own approximation**, because that is
what Chaos collides with today:

* sphere: radius × the smallest absolute scale component
* capsule and tapered capsule: radius × max(|X|, |Y|), clamped to the scaled half-length; total
  length × |Z|

Each such element is counted in `nonUniformSphereOrCapsule`, and the first one per component logs a
Warning naming the component and its scale. Boxes, convexes and triangle meshes take non-uniform
scale exactly.

## 7. Transforms and `stableKey`

**Transforms.** The body's frame is the component's world transform without its scale; the scale
has already gone into the shape (§5, §6). Each shape's `localToWorld` is its element-local
transform (centre and rotation) followed by that body frame, so it carries no scale.

**`stableKey`** (`UEStaticGeometryImporter.cpp` :: `stableKeyFor`) is a 64-bit FNV-1a hash of:
1. the actor's path name with any PIE prefix removed (G-01), as UTF-8, then a zero byte;
2. the component's name, as UTF-8, then a zero byte;
3. the instance index (0 except for instanced meshes), 4 bytes little-endian;
4. the element index, 4 bytes little-endian.

The element index is the element's position in the engine's order: spheres, boxes, capsules,
tapered capsules, convexes, then the triangle meshes after all simple elements (the order Chaos
creates the shapes in). The index depends
on the asset only, never on which elements a filter dropped, so a key does not change when another
element of the same body is skipped.

The key is the same on every peer that runs the same binary on the same map. On ThirdPersonMap the
actors are one-file-per-actor actors whose names carry a unique actor id
(PersistentLevel.StaticMeshActor_UAID_…), and all of them were in the persistent level at import
time.

## 8. Sorting, duplicates and the checksum

The shapes are sorted by `stableKey`, ties broken by a hash of the whole descriptor (G-03), so the
order is total and independent of iteration order. Equal neighbouring keys are counted in
`duplicateKeys` and logged as an Error: they defeat the per-shape key contract even though the
order stays deterministic.

The checksum (`UEStaticGeometryImporter::checksum`) is FNV-1a 64 over the shape count and then, per
descriptor in sorted order: the key, the shape's alternative index, every shape field (each float by
its bit pattern, each vector length and index), the 16 floats of `localToWorld`, both category
sets, friction and restitution. All multi-byte values are fed little-endian, so the value does not
depend on the host's byte order. Two descriptions with one checksum are, short of a hash collision,
bit-identical in key order.

Friction and restitution are the component's simple physical material (the project default on
ThirdPersonMap: 0.7 and 0.3). Per-face materials of triangle meshes are not carried; the descriptor
has one material per shape.

## 9. Logging and the console command

Category `LogOGStaticImport`, default verbosity Log. Each import logs:

```
[StaticImport] <label> shapes=N box=.. sphere=.. capsule=.. convex=.. trimesh=.. components=.. instances=.. checksum=0x.. seconds=..
[StaticImport] <label> skipped: movable=.. simulating=.. noBodySetup=.. elemCollisionOff=.. elemUnmappedObjectType=..; elemPhysicsOnly=.. convexWithoutChaosMesh=.. nonUniformSphereOrCapsule=..
[StaticImport] <label> Unsupported: landscape=.. skinned=.. components not imported      (Warning, only when non-zero)
```

The per-type names come from a compile-time table over the `StaticShape` alternatives, so a new
alternative fails to compile until it has a name; the checksum's visitor fails the same way until
it hashes the new shape. Verbose adds one line per emitted shape (key, type, actor and component,
category sets, position, material) and one per skipped component.

`og.sim.ImportStaticGeometry [runs] [category:channel ...]` imports the current world `runs` times
(default 2, at most 16), logs each report, then logs
`[StaticImport] repeat runs=N identical=yes|NO checksum=0x.. world=.. mapping=..`, plus an Error line
when the runs differ. Channels are written as enum names with or without the ECC_ prefix. With no
table the command maps category N to channel N for all 32 channels, which is enough to count
shapes; the brawler's table, from a command line:

```
-ExecCmds="og.sim.ImportStaticGeometry 2 0:GameTraceChannel2 1:GameTraceChannel3 2:GameTraceChannel4 3:GameTraceChannel5 4:WorldStatic 5:GameTraceChannel6"
```

## 10. Evidence (2026-10-06)

ThirdPersonMap, `-game -nullrhi`: two standalone processes and one dedicated server (`-server`),
each importing twice with the brawler's table. Every import of every process logged
`shapes=10 box=0 sphere=0 capsule=0 convex=9 trimesh=1 components=7` and
`checksum=0xc96a2ef6fa952d5f`, with `identical=yes`. The identity table gave `0x1eec478d6b539757`
(same shapes, different category sets). A rebuilt binary (tapered capsules added) gave the same two
checksums in a fresh standalone and a fresh dedicated server; the command rejected a bad channel
name with its Error line. The imports ran at frame 0, right after the map loaded. No ensure, assert
or fatal line in any log.

## 11. Limits and follow-ons

* Landscape and skinned meshes are counted, not imported (follow-on 16 adds height fields).
* Movable components that do not simulate (kinematic geometry) are skipped; they are not statics.
* Only actors loaded when the import runs are seen: a level that streams geometry in later needs
  the import to run after that streaming, or again.
* The PIE path (G-01) is reasoned from the engine's PIE naming, not yet run: the evidence runs are
  `-game` and `-server` processes, which carry no PIE prefix.
* There is no low-level test: no low-level test target links this module, and every input is an
  engine object. Coverage is the runtime check of §10 and the compile-time checks of §9.

## 12. Guards

G-01 to G-04, in `UEStaticGeometryImporter-guards.md`.
