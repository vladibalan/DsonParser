---
name: director-runtime-verify-via-user-import
description: "DsonToUnreal Director builds via Build.bat but can't run DAZ imports headlessly; runtime verification of import output falls to the user's editor import → merge-then-measure cadence"
metadata: 
  node_type: memory
  type: project
  originSessionId: 115c8b31-54d8-4dc8-82d2-991248eec14f
---

The DsonToUnreal Director can compile the host target (`Build.bat DsonHostEditor Win64
Development …`, "up to date" = success) but **cannot run an actual DAZ import headlessly** —
imports are UE Editor actions and Tooling.md exposes no commandlet/headless path. So
runtime verification of import *behavior* — e.g. the recipe `[recipe-shape]` diagnostic
counts, emitted assets on disk, asset contents — **falls to the user running an import (Nancy)
in the editor**.

**Why:** the build verifies compilation/ABI/UHT only, not what the importer produces at run time.

**How to apply:** for import-output features, use a **merge-then-measure** cadence — land the
instrumented change (always-on diagnostic counts so the gap is visible, not silent), the user
imports & pastes the numbers, then refine from evidence. Don't promise runtime verification the
Director can't deliver; scope the diagnostic so the user's single import is decisive. Confirmed
across recipe-emission slices 2–3 (2026-06-11): slice 2's dial join shipped instrumented-but-empty,
the Nancy `[recipe-shape]` line + an on-disk Composites check pinned the cause, slice 3 fixed it.
Related: [[compliance-run-definition]], [[no-silent-fails]], [[ask-for-files-before-diagnostics]].

**Generalized 2026-07-11** ([[editor-checks-are-users-job]]): this isn't just the
Director's own limitation — the Director must not write a task-file asking the
*Implementer* to do an editor/GUI check either. All editor verification, full stop,
stays with the user.
