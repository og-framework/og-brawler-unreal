<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `BrawlerSessionTraitsUImpl.h` — guards

Prohibitions with a tagged site in `Source/OGBrawlerUnreal/BrawlerSessionTraitsUImpl.h`. Ids are opaque, stable, and
retired rather than reused. The reasoning lives in `BrawlerSessionTraitsUImpl-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.**

No live entry: the header is a type list, a wrong member is a compile error in the template that reads it, and since
task 34 a wrong row of the core-system map fails its `static_assert` (rationale §4).

---

## §R — Retired ids

None.
