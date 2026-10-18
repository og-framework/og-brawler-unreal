<!-- SPDX-License-Identifier: MPL-2.0 -->
# `UEPresentControls.h` and `.cpp` — rationale

The Unreal side of og-simulation's `PresentControls`: the six `og.Render.*` console variables and the one function that
fills the presenter's per-frame controls from them. The source holds the licence, a docs pointer and code; this file
carries the why. It has no prohibition (`UEPresentControls-guards.md` has no live entry).

**If this file and the source disagree, the source is authoritative and this file is stale.**

---

## 1. What it is

`UEPresentControls::fromConsoleVariables(parkSnap)` returns a `PresentControls` (og-simulation's
`TickDriverPresenter.h`) for the presenter's `present_GameThread`. The class has no instance (its constructor is
deleted): it names the one function, and is exported (`OGSIMULATIONUNREAL_API`) because the game's module calls it.

| field | from | conversion |
|---|---|---|
| `delaySteps` | `og.Render.InterpolationDelaySteps` (default 2) | none |
| `newestOnly` | `og.Render.NewestSnapshot` (0) | true when the value is not 0 |
| `correctionSeconds` | `og.Render.ErrorCorrectionSeconds` (0.3) | none |
| `maxCorrection` | `og.Render.ErrorCorrectionMaxCm` (250) | centimetres to og-simulation lengths, `UERenderTargetTable::lengthFromCentimetres` |
| `desyncSeconds` | `og.Render.ErrorCorrectionDesyncSeconds` (0.6) | none |
| `traceFrames` | `og.Render.TraceFrames` (0), not in Shipping | true when the value is not 0; in Shipping the variable does not exist and the field keeps its default, false |
| `parkSnap` | the caller's argument | none |
| `lengthReportScale` | the constant 1.0 | none |

What each control does is the presenter's (`TickDriverPresenter-rationale.md` in og-simulation's docs); the brawler's
reading of them is `TickDriverBackendUImpl-rationale.md` §5 and §7.

## 2. When it is read

Every frame, on the game thread: the brawler's `TickDriverBackendUImpl::beforePresent_GameThread` calls it as an
argument of `present_GameThread`. Each variable is read with `GetValueOnGameThread`, in the field order of the table,
so a console change applies to the next frame. Until og-tickdriver task 15c the same body was the brawler's
renderPresentControls, in TickDriverBackendUImpl.cpp (a historical name: the host's `.cpp`, deleted by og-tickdriver task 15e), with the same reads in the same order and on the same thread.

## 3. The park snap is the caller's

The park snap is a distance: a body whose two bracketing poses lie farther apart than it is snapped instead of blended
(the interpolation's `snapDistanceCm`). It belongs to the game, whose bodies jump that far: the brawler's pooled
projectiles, parked and unparked far below the arena (its binding's `BrawlerJoltBindingUImpl::kParkSnap`,
`BrawlerJoltBindingUImpl-rationale.md` §4). So it is a parameter, not a field this function leaves at its default
(infinity, never snap): a caller cannot forget it, because the call does not compile without it. `lengthReportScale` is the constant 1.0 (design of og-tickdriver, K9): the `…Cm=` fields of the
presenter's lines print og-simulation lengths times this scale, which is their value in centimetres while the
simulation runs in centimetres.

## 4. Units

`og.Render.ErrorCorrectionMaxCm` is a centimetre value. It goes through `UERenderTargetTable::lengthFromCentimetres`,
which divides by `UERenderTargetTable::kCentimetresPerLength` (1.0 today), so the presenter receives the same number.
The other five variables are counts, seconds or switches, and need no conversion.

## 5. The help texts

Names, defaults, flags and help texts are the ones the variables had in the brawler's game module, with one
exception. `og.Render.ErrorCorrectionMaxCm`'s last sentence named a Chaos variable with no verb, as if the
variable were Chaos's; task 15c reworded it in meaning only, so that it says what the comparison is. Old:

> A correction of a body by this many centimetres or more is not blended: the body snaps to the corrected pose.
> Chaos's p.RenderInterp.MaximumErrorCorrectionBeforeSnapping (250 there and here).

New:

> A correction of a body by this many centimetres or more is not blended: the body snaps to the corrected pose. The
> same threshold as Chaos's p.RenderInterp.MaximumErrorCorrectionBeforeSnapping (250 there and here).

Both start with "Jolt configuration only.". The task named only this text. `og.Render.ErrorCorrectionDesyncSeconds`'s
help text ends in the same verbless shape ("Chaos's p.RenderInterp.MaximumErrorCorrectionDesyncTimeBeforeSnapping (0.6
there and here).") and is unchanged; two others compare in a full sentence ("2 matches Chaos's
p.AsyncInterpolationMultiplier", "as Chaos converts p.RenderInterp.ErrorCorrectionDuration"), and the newest-snapshot and
trace texts name no Chaos variable.

## 6. Both physics configurations

The module is compiled in both configurations and the files have no `#if` arm, so the six variables register at module
load in both. In the Chaos configuration nothing calls `fromConsoleVariables`, and the variables are never read. Until
og-tickdriver task 15c they were in a whole-file Jolt arm of the game module, and the Chaos configuration did not have
them (`UETickDriverHost-rationale.md` §4 says the same of the frame's four).

## 7. Module placement

The header includes og-simulation's `TickDriverPresenter.h` for the `PresentControls` struct. The `.cpp` builds the
struct and instantiates nothing of the presenter, so nothing here touches og-simulation's tick-driver thread context
(`UETickDriverHost-rationale.md` §7, `TickDriverThreadContext-rationale.md` §7 in og-simulation's docs). Task 15c
checked it on the Editor objects: this file's object names no `tickDriverThread` symbol, scope, driver or world host.

## 8. Provenance

Written by og-tickdriver task 15c (2026-10-18). The six console variables and the fill moved here from the brawler's
TickDriverBackendUImpl.cpp (a historical name: the host's `.cpp`, deleted by og-tickdriver task 15e), where og-tickdriver task 9 had made the fill a `PresentControls` and tasks 15a and 15b had
moved the park snap into the brawler's binding and made the fill a template over it.
