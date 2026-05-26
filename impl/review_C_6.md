# Review — C6: Update README and docs for mixed-license posture

**Verdict: APPROVED**

## Commit verification
- Single commit `dd44d1f` local-only (ahead of `origin/main`).
- Files changed: `README.md`, `Content/LICENSE-NOTICE.md`.

## Acceptance criteria check

| # | Criterion | Result |
|---|---|---|
| 1 | README accurately states mixed-license posture and links to LICENSING.md | PASS — intro note and License section both name BUSL-1.1/MPL-2.0 and link to `LICENSING.md`. |
| 2 | Project structure table includes `OGSimulationUnreal/` with MPL marker | PASS — `Source/OGSimulationUnreal/  UE simulation adapters module  [MPL-2.0]` present. |
| 3 | No docs still claim project is uniformly MPL | PASS — grep for unqualified MPL-as-whole-project patterns returns no matches in README or docs/. |
| 4 | `Content/LICENSE-NOTICE.md` references the project split | PASS — final paragraph references "mixed-licensed (BUSL-1.1 / MPL-2.0)" and links to LICENSING.md. |
| 5 | README license section concise (3-5 lines, no duplicate of LICENSING.md subtree table) | PASS — License section is 4 lines; no subtree table duplicated. |
| 6 | Existing non-license README content preserved | PASS — building, targets, project structure, workflow sections all intact. |
| 7 | Single C6 commit, local only | PASS. |

## Notes
- MPL appearances in `docs/` are SPDX headers on the doc files themselves — correct, not unqualified project-wide claims.
- `ProceduralMountainSide` correctly annotated `[MPL-2.0 → BUSL-1.1 pending C8]` in the structure table — consistent with outstanding C8 task.
