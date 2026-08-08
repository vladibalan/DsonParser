---
name: upstream-status-stale-confirm-with-user
description: "From the Artisan seat, upstream importer task/version/ship status is opaque and fast-stale — confirm current state with the user before asserting it or building docs on it"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 9cac6796-d4ea-404f-bee4-58f2c72ac3d3
---

When working the DsonArtisan (Artisan) seat, treat upstream DsonToUnreal task / version / ship status
as **opaque and fast-stale** — do NOT assert it from MEMORY.md index hooks or point-in-time memory
notes, and don't build docs or decisions on it without confirming the current state with the user.

**Why:** 2026-06-15 I described the JCM importer task as "design-review pending" (stale index line),
then "shipped v1.8.0" (stale file body), then learned it had been reverted and was being reworked —
two doc course-corrections in one session, costing several round-trips. The importer is opaque from
this seat ([[dsonartisan-upstream-opaque]], [[parser-changes-route-through-user]]); the user is both
the authority on its real state and the only cross-repo channel.

**How to apply:** before citing upstream status, read the FULL memory file (not just the MEMORY.md
hook) AND ask the user to confirm current upstream state. Keep Artisan docs to carrier-level facts
(a version is usable / not usable) + what/why asks — never importer internals (runtime counts,
mechanisms, the importer's own decisions/versions). If two of my notes disagree on upstream state,
that disagreement is itself the signal to stop and confirm.
