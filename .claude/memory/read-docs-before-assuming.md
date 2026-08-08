---
name: read-docs-before-assuming
description: "Read the project's own docs before proposing a next step or asking the user; don't assume."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: e2402a62-924a-454f-a1f5-38ae855d8d67
---

User correction (2026-06-27, DsonArtisan seat): I repeatedly jumped to assumptions and asked the
user questions whose answers were already in the documentation (e.g. authoring=rest / posing=on the
baked-shipped figure, and wearable delivery = a separate per-pairing cooked skeletal mesh with the
rig — all in `Intent.md` shape-vs-pose + tier model, `ComposeDesign.md` §2 authoring-vs-bake,
`WearableFitDesign.md` §"Stage-1 packaging").

**Why:** this repo has a deep, well-maintained doc tier (Intent / ComposeDesign / WearableFitDesign /
Reference / DecisionLog). The answer to "how should this work" is usually already written. Asking or
assuming wastes the user's time and erodes trust.

**How to apply:** before proposing a new step, scoping a slice, or asking the user a design question,
**grep + read the relevant `Docs/*.md` first** and answer from them. Only ask when the docs genuinely
don't cover it. Cite what the doc says. This is the DsonArtisan instance of the broader
[[ground-truth-before-fixing]] discipline — for design questions the ground truth is the docs.

**Now codified (2026-06-27):** this is no longer just a preference — it is a standing Director duty,
"Ground before you answer", in DsonArtisan `Docs/AgentWorkflow.md` (Director role section, commit
f5ac7ec). It gates answering/proposing/asking/raising a fork-or-FR on confirming the answer isn't
already written, requires grounding each claim in a named `doc`/`file:line` or marking it inference,
and names an unfounded-confident answer a silent fail. Treat it as a rule, not a nicety.
