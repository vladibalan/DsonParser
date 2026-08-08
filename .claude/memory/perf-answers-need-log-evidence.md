---
name: perf-answers-need-log-evidence
description: "For DsonArtisan perf/behavior questions, read the DsonArtisanHost.log timestamps or the code BEFORE answering — plausible-sounding explanations asserted without evidence have been wrong repeatedly."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: c10b1e4f-fc25-4913-9fc0-961c41014a02
---

The user repeatedly ("you are making wrong assumptions. Again.") caught me asserting
performance/behavior explanations from plausibility instead of evidence — e.g. I blamed a slow bake on
"Bottom + Wrap + fabric fit" when there was **no wrap** on that character, and a wrap-bearing bake was
actually *faster* (<5s). Guessing the mechanism of an observed runtime behavior is the same failure mode
as guessing the cause of a bug.

**Why:** guessing burns trust and sends us chasing non-problems; the user explicitly values evidence
("you can read the logs") and dislikes assumption-dressed-as-fact.

**How to apply:** for any "why is this slow / why did it build N times / what does the bake actually do"
question, FIRST read `DsonArtisanHost/Saved/Logs/DsonArtisanHost.log` (and/or backups
`DsonArtisanHost-backup-<stamp>.log`) and/or the code, THEN answer with the numbers. UE log lines carry
`[YYYY.MM.DD-HH.MM.SS:mmm]` timestamps → elapsed time per op; grep `[wearable-bake]`,
`[rigid-follow-reload]`, `Building Skeletal Mesh ... [Xs]`, `Built Skeletal Mesh [Xs]`. Evidence from
2026-07-03 (Jewel Bikini bake): I stopped at **two** partial answers before the truth — first "fabric
fit" (wrong: no wrap on that character), then "the 11.25s Bottom build is the cost" (**also incomplete**).
The user kept pushing ("~10 rebuilds, there aren't 10 components") and the log settled it: the real
dominant cost was the body shape-morph strip rebuilding the 27k-vert mesh **44×** (~77s) via the engine's
per-target `RemoveMorphTargets`. The **gem** bake+reload was <0.5s (not the cost, one reseat/reload for the
whole `Gem_Drop` mesh, not per-rivet). FIXED (commit `0cdaa42`) by batching the strip to one rebuild
(`StripMorphTargetsBatched`; `Docs/Reference.md`, `Docs/DecisionLog.md` 2026-07-03). **Doubled lesson:
don't stop at the first plausible cost the log shows — count the rebuilds and find what actually
dominates.** Related: [[verify-the-actual-failure-mode]],
[[ground-truth-before-fixing]], [[dsonartisan-host-runtime-log]], [[no-silent-fails]].
