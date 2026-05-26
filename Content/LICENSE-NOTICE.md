<!-- SPDX-License-Identifier: MPL-2.0 -->
# Content Licensing

The `Content/` directory contains both project-specific assets and Epic Games-provided template assets. The licensing breakdown is:

## Project-specific assets
- Custom maps, blueprints, materials authored for this project.
- Located primarily under `Content/LevelPrototyping/`, `Content/__ExternalActors__/`, `Content/__ExternalObjects__/`, and any custom subdirectories.

## Unreal Engine EULA (Epic Games template content)
- **Mannequin character assets** (`Content/Characters/Mannequins/`, `Content/Characters/Mannequin_UE4/`) — Manny and Quinn meshes, animations, rigs, textures, and materials provided as part of UE5's default project templates.
- **Third-Person template content** (`Content/ThirdPerson/`) — maps, blueprints, and supporting assets from UE5's Third-Person project template.
- Use of these assets is governed by the [Unreal Engine End User License Agreement](https://www.unrealengine.com/eula).

## What was deliberately excluded
- `StarterContent/` (Epic's "Starter Content" pack — 613 MB) is NOT bundled. Source available via UE Editor's "Add Feature or Content Pack" if needed.

Source code in this repository (`Source/`, `Plugins/<submodules>/Source/`, `docs/`, build scripts) is mixed-licensed (BUSL-1.1 / MPL-2.0) — independent of the bundled content. See [`LICENSING.md`](../LICENSING.md) at the repository root for the full breakdown.
