---
name: stability-first-plans-before-handoff
description: "DsonArtisan Director — user gates Implementer handoffs on a vetted, stability-first plan; convince first, don't optimize for speed."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: b5ddb2fe-e994-4f4d-a6aa-4556cf685dcf
---

In the DsonArtisan Director seat, the user will NOT approve launching any Implementer task-file until convinced the plan is solid — grounded, stability-first, and verified against real code/assets, not a plausible sketch. They have been burned by Directors prioritizing speed over long-term stability + project feature impact.

**Now codified (2026-07-06):** this is a hard governing rule — `Docs/Principles.md` **P6** (stability and correctness outrank speed) + the mandatory task-file **Robustness & scope** field and the review-gate/reporting hooks in `Docs/AgentWorkflow.md` (one-line pointer in `AGENTS.md`). The rule + anti-patterns ("MVP"/"demo"/"harden it later" are not valid default framings; deferral must be stated and justified as safe) now live in the repo; this memory holds the *how-to-apply* behavior behind P6, not a duplicate of it.

**Why:** speed-first plans look fine in a demo but rot the project — e.g. a geograft weld that renumbers vertices and silently corrupts the morph pipeline, or a bone added to the shared base skeleton that pollutes every character. The user catches these and rejects; the fix is to not propose them.

**How to apply:** Before any handoff — (1) verify load-bearing claims first-hand (read the actual plugin code / DAZ DSF / UE engine source), (2) surface grounding gaps explicitly and close them first, (3) decompose into de-risking slices that build to the full feature (not a demo to ship-and-stop), (4) re-present for scrutiny and expect several reject-refine cycles — treat each as improving the plan, not friction. Don't reach for AskUserQuestion to pick "which slice first" (reads as a speed move); go deep on architecture + risks instead. Related: [[ground-truth-before-fixing]], [[decisions-grounded-in-principles]], [[verify-the-actual-failure-mode]], [[no-silent-fails]].
