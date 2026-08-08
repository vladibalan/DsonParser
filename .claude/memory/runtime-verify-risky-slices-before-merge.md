---
name: runtime-verify-risky-slices-before-merge
description: Runtime-verify risky UE slices (skeleton/render/skinning) on the branch before merging; sequence lower-risk visible wins before the riskiest slice
metadata: 
  node_type: memory
  type: feedback
  originSessionId: b5ddb2fe-e994-4f4d-a6aa-4556cf685dcf
---

For any DsonArtisan slice that mutates the runtime **skeleton, render data, or skinning**, the USER
runtime-verifies on the task branch **before** the Director merges to `main` — do **not** merge
build-verified-but-runtime-pending (the Director can't run the editor, so build-verify alone let three
geograft owned-skeleton bugs land on main in a row: a superset-validation bug → fix → a skinning crash on
its first real run). And **sequence the lower-risk, user-visible win before the riskiest slice** — pulling
the riskiest geograft slice (owned skeleton / render data) *ahead* of the visible interior-shape-follow
slice (Slice 4), for "verifies better on a posing graft" convenience, was the root misjudgment.

**Why:** the user's 2026-07-06 correction — "we are backtracking every win; optimize for stability and
correctness, not speed."

**How to apply:** gate risky-slice merges on the user's runtime verification; order slices
lowest-risk-visible-win first; compose-time / morph-only changes with strong self-checks may still merge on
build-verify. Complements [[stability-first-plans-before-handoff]] (vet the plan first) and
[[verify-the-actual-failure-mode]] (verify against the real failure, not a proxy).
