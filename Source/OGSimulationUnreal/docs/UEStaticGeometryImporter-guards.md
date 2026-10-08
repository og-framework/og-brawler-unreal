<!-- SPDX-License-Identifier: MPL-2.0 -->
# `UEStaticGeometryImporter.h` and `.cpp` — guards

Every prohibition that governs a line of `UEStaticGeometryImporter.cpp`. Each entry has an **opaque,
stable id**. In the source a single line `// ⛔G-nn  docs/UEStaticGeometryImporter-guards.md` sits
directly above the statement where the forbidden edit would be typed.

**If this file and the source disagree, the source is authoritative and this file is stale.** Fix
this file; do not soften the source to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time check, moves to
§R and its number is spent forever. A retired id may be **named** in prose; it may never again
appear as a `⛔G-nn` tag.

⛔ **Nothing in this file is a rationale.** Orientation, derivations and the evidence behind each
rule live in `UEStaticGeometryImporter-rationale.md`. A guard is a prohibition plus the consequence
of ignoring it.

**Why these four are guards.** Each forbids an edit that compiles, passes the importer's own repeat
check (two imports in one process produce one checksum) and changes nothing a single-process run
can see. Three of them break only *between* peers or only against Chaos, which is exactly what the
in-process check cannot observe.

---

## G-01 — the key's actor path has its PIE prefix removed; never key on anything process-local

**Site:** `const FString actorPath = UWorld::RemovePIEPrefix(actor->GetPathName());` in
`UEStaticGeometryImporter::importWorld`.

**The prohibition.** Do not drop the `RemovePIEPrefix` call, and do not replace the actor path with
a value that differs between processes or between PIE instances: an object's unique id, a pointer,
an editor-only label, or the position in the actor iteration.

**The consequence.** In a PIE session each instance N loads its world into a package named with the
prefix UEDPIE_N_, so the raw path name of the same
floor actor differs on every peer, and so does every `stableKey`. A physics backend adds statics in
key order (`StaticGeometry-rationale.md` §3), so the peers would build their static worlds in
different orders: the arena-import divergence risk the design calls R4.

**What breaks if it moves.** Nothing at runtime outside PIE: a `-game` standalone and a `-server`
process have no prefix, so they keep agreeing. The defect appears only in multi-instance PIE, which
is where the equivalence playtest runs.

## G-02 — a body imports EITHER its simple elements OR its triangle meshes, chosen as Chaos chooses

**Site:** `const bool useSimpleGeometry = traceFlag != CTF_UseComplexAsSimple || bodySetup.TriMeshGeometries.Num() == 0;`
in `importBody`.

**The prohibition.** Do not import a body's triangle meshes when its resolved trace flag is anything
other than complex-as-simple, and do not import both the simple elements and the triangle meshes of
one body.

**The consequence.** Chaos creates the triangle-mesh shapes of a body (unless the flag is simple-as-complex and the body
has simple elements), but its collision filter drops them from simulation unless the shape's trace
flag is complex-as-simple; with complex-as-simple and at least one triangle mesh it creates no simple
shapes at all. A default-flag mesh imported as a triangle mesh gives the Jolt world contacts
against render-detail geometry that Chaos never collides with; importing both doubles every contact.
Either way the capsule-versus-world response differs from today's, which the M1 equivalence forbids.

**What breaks if it moves.** The selection reads the body setup's flag after the `CTF_UseDefault`
resolution above it; moving it above that resolution makes every default-flag body follow the
unresolved value.

## G-03 — the output is sorted by (`stableKey`, content hash) before the checksum and the return

**Site:** the `std::sort(order.begin(), order.end(), …)` statement in
`UEStaticGeometryImporter::importWorld`.

**The prohibition.** Do not delete this sort, do not drop its content-hash tie-break, and do not sort
by anything that is not identical on every peer.

**The consequence.** Shapes are gathered in actor-iteration order, which is the order of the level's
actor array in this process. Two imports in one process iterate in the same order, so the repeat
check still reports `identical=yes` without the sort; but two peers are not guaranteed the same
order, and the description is the input a backend adds statics from. The tie-break keeps the order
total even if two keys collide, so the result never depends on the input order.

**What breaks if it moves.** The checksum is computed over the sorted description; moving the sort
below the checksum makes the checksum describe the unsorted order.

## G-04 — `blockingCategories` is filled only for an element whose collision includes physics

**Site:** `descriptor.blockingCategories = CollisionEnabledHasPhysics(collision) ? component.blockingCategories : CollisionCategories{};`
in `WorldImport::emit`.

**The prohibition.** Do not fill `blockingCategories` from the collision responses for an element
whose combined collision setting has no physics (QueryOnly, ProbeOnly, QueryAndProbe), and do not
read the component's setting alone in place of the combined `collision`.

**The consequence.** Chaos gives such a shape no simulation contacts at all, whatever its responses.
With a non-empty set the Jolt world would make the capsule collide with it: a query-only blocking
volume would become a wall.

**What breaks if it moves.** `collision` is the intersection of the component's and the element's
settings; an element can switch physics off while its component has it on.

---

## R. Retired ids

None.
