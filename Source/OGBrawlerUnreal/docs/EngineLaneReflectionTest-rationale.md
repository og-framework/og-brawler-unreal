<!-- SPDX-License-Identifier: BUSL-1.1 -->
# The engine-lane reflection dump — rationale

`Source/OGBrawlerUnreal/EngineLaneReflectionTest.cpp` is a dev automation test,
`OGBrawler.Replication.EngineLaneMembers` (`EditorContext | ProductFilter`, compiled only under
`WITH_DEV_AUTOMATION_TESTS`). It prints, as automation info lines, every piece of engine replication that
the game's reflected classes declare. It changes nothing and asserts one thing (§4). Its pattern is the
older `OGBrawler.Replication.CharacterReplicatedComponents` in `BrawlerReplicatedComponentsTest.cpp`.

---

## 1. Why a reflection dump

The repository's engine-lane fence finds engine replication by reading C++ source. Two kinds of
replication have no source text to read:

- a Blueprint's replicated variables and its RPC events, which live in `.uasset` files;
- replication a class gets from an engine base class: the game's pawn has no replication marker of its own
  for the class itself, and its default object replicates because the engine's `APawn` constructor sets
  `bReplicates`.

The reflection system sees both. This test asks it, and the fence's reflection arm checks the printed
lines against the fence's manifest in both directions. The dependency runs one way: the lint reads the
test's output, and nothing here reads the lint or its manifest.

## 2. What it prints

1. **Blueprints first.** It runs a synchronous asset-registry scan of `/Game`, collects every asset whose
   class is `UBlueprint` or a subclass (animation Blueprints included), keeps those whose package is under
   `/Game`, and loads each one. A Blueprint's generated class exists in memory only after its asset is
   loaded, so without this step an unloaded Blueprint is invisible to the class iteration below. Each loaded
   Blueprint prints `[EngineLane.Blueprint] <package> <generated class>`. A Blueprint that has no generated
   class after loading is an error, because a class that cannot be seen cannot be checked.
2. **Then every class in scope.** It iterates all loaded `UClass` objects and keeps those whose outermost
   package is `/Script/OGBrawlerUnreal`, `/Script/OGSimulationUnreal` or `/Script/OGSimulationJoltUnreal`,
   or lies under `/Game`. It skips the Blueprint compiler's `SKEL_` and `REINST_` classes. For each class it
   prints one `[EngineLane.Reflected] <Kind> <Class> <Member> <CPPType>` line per item:
   - `Property` for each property the class itself declares (`ExcludeSuper`) with `CPF_Net`;
   - `Rpc` for each function the class itself declares with `FUNC_Net`;
   - `ReplicatedClass <Class> * -` when the class's default object replicates: `AActor::GetIsReplicated()`
     for an actor, `UActorComponent::GetIsReplicated()` for a component.

   The lines are sorted, so two runs of one build print the same text.
3. **A summary,** `[EngineLane.Summary] blueprints=<n> reflected=<n>`.

Two things the dump does not see, both measured on the first real run:

- **A class that turns replication on at run time.** The `ReplicatedClass` line reads the default object.
  The engine makes a player controller replicate with `SetReplicates(true)` when it spawns one, not in a
  constructor, so `AOGBrawlerPlayerController` has no `ReplicatedClass` line. Its `Rpc` line still names it.
- **A level Blueprint of a map that is not loaded.** A level script class lives in its map's package, not in
  a Blueprint asset, so only the maps the editor has loaded are seen. Headless, that is the editor's
  startup map, whose level script actor (`AThirdPersonMap_C`) replicates through the engine's
  level script actor base class.

## 3. How the names and types are spelled

The fence's manifest spells keys the way C++ source does, so the dump does too:

- **Class:** the C++ prefix plus the class name (`GetPrefixCPP()` then `GetName()`): `AOGBrawlerUECharacter`,
  `USimulatableNetSyncComponent`. A Blueprint's generated class gets the same treatment, so the
  ThirdPerson character Blueprint prints as `ABP_ThirdPersonCharacter_C`.
- **Property type:** the property's `GetCPPType` with its extended text appended, so a `TArray` property prints
  `TArray<…>` rather than a bare `TArray`.
- **Rpc type:** the parameter types joined with `, ` (`-` when there are none). The return value is not a
  parameter. Reflection keeps a parameter's `const` and `&` as flags, not as text, so the test adds them back:
  `const ` for `CPF_ConstParm`, and `&` for `CPF_ReferenceParm` or `CPF_OutParm`. A `const FStruct&`
  parameter carries both flags in its generated code, so it prints as `const FStruct&`, the same text the
  source declares.

## 4. The one assertion

The test fails unless some reflected line names `AOGBrawlerUECharacter`. The pawn replicates and declares
replicated properties, so a run that saw the game module always prints it. The assertion catches a run whose
package filter or class iteration has stopped seeing the game module: without it, such a run would print
fewer lines and still pass, and the lint would then report missing rows rather than a broken dump. Removing
`/Script/OGBrawlerUnreal` from the package list makes it fail.

## 5. Template content

`/Game` holds engine template content (StarterContent, ThirdPerson, the mannequin animation Blueprints). Its
Blueprints are loaded and checked like any other. Any replication they report needs a manifest row like any
other class; whether that content stays in the project is the user's decision, not this test's.

## 6. Running it

The test needs an editor. Headless, after the Editor Development build:

```
UnrealEditor-Cmd.exe "<project>.uproject" "-ExecCmds=Automation RunTests OGBrawler.Replication.EngineLaneMembers;Quit" -unattended -nullrhi -log=EngineLane.log
```

The log lands in the project's `Saved/Logs/`.
