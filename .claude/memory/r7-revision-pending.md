---
name: r7-revision-pending
description: DsonToUnreal R7 (permissive/never-abort) slated for revision; layered-import H1 hard-fail is a sanctioned R7 exception meanwhile
metadata: 
  node_type: memory
  type: project
  originSessionId: 0e29b258-8d80-4a0f-92d8-719876d9a5ef
---

DsonToUnreal **R7** (CodeReviewRules — post-gate import steps are permissive and
**never abort the character import**) is slated for revision. User intent stated
2026-06-14 ("we need to revise some rules later, R7 specifically"); no date/scope
fixed yet, so treat the revision as pending/unscheduled.

**Why:** the layered-figure-import S3 lean-delta work introduced the "H1" case — when
`FigureId` is set but the shared parent figure can't be built/resolved, the character
delta has no skeleton to bind and no parent morph set to diff against, so "continue
permissively" is no longer meaningful. The user directed a **HARD FAIL** there (abort
the character import, mirror the `M_DazDefault` gate) rather than the R7-default
legacy-fallback.

**How to apply:** the H1 hard-fail is a **deliberate, user-approved exception to R7**,
not a violation — do NOT "fix" it back to permissive, and don't let a compliance run
[[compliance-run-definition]] flag it (the plugin axis checks R1–R12). Only the Director
edits R7/CodeReviewRules, and only when the revision is actually taken — not as a side
effect of S3. Related: [[layered-figure-import-rework]].
