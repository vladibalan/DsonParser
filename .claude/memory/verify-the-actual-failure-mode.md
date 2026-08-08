---
name: verify-the-actual-failure-mode
description: "Don't claim verified/confirmed off a proxy metric — the evidence must exercise the actual failure mode"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 7fdca49a-cc80-4dc9-80ef-ed7daff660eb
---

I marked DsonArtisan sub-task #2 (dialed-shape completeness on the derived mesh) "confirmed clean"
from `skippedCompanion=0` — an adjacent proxy that only proves no *formula-bound companion* morph
was dropped. The user then ground-truthed the real failure mode (base **directly-dialed leaf**
morphs `body_bs_Navel_HD3` / `head_bs_MouthRealism_HD3`, dialed 1.0, sat at weight 0) and my
"confirmed" was wrong.

**Why:** a "verified/confirmed" claim resting on a proxy rather than the actual failure mode is a
silent fail — it ships a false done-state the user has to catch. This user repeatedly corrects
Director over-claims; faithful verification matters to them.

**How to apply:** before writing "verified" / "confirmed", name the exact failure mode the claim
covers and confirm the evidence exercises THAT, not a neighbouring signal. If a check covers only
part, say so explicitly ("X verified; Y still unverified"). Builds on [[no-silent-fails]] +
[[ground-truth-before-fixing]]; sibling of [[scope-check-before-escalating-forks]] (both are
Director self-corrections the user drove, 2026-06-17/18).
