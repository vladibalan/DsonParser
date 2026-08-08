---
name: llmfrmw-consumer-interaction-reframe
description: LLMfrmw consumer model being reframed closed->selective; COD-102 + doc-elimination paused pending framework Agent.
metadata: 
  node_type: memory
  type: project
  originSessionId: 0431d528-329a-474d-9ee6-e94d665c7c62
  modified: 2026-08-08T06:37:15.869Z
---

On 2026-08-08 the owner named the LLMfrmw "closed catalog" consumer model (materialize the full slot set;
record every skip or project-local doc as an argued deviation) as the root cause behind repeated Director
misreads of the "convert + eliminate all non-framework docs" objective. The owner is taking a reframe to the
framework Agent: consumers should SELECT what they need (a) at adoption and (b) as they evolve; non-selection
is normal, not a deviation. A situation report was written for that conversation (scratchpad:
LLMfrmw-sitrep-consumer-selection.md) - it proposes a small in-repo selection manifest instead of vendoring
the whole catalog (owner explicitly does NOT want the catalog vendored in-repo).

**Why:** COD-102 and the "eliminate non-framework docs" objective encode the framework's closed/conform
default, which inverts the owner's actual intent - so acting on them as written repeats the churn.

**How to apply:** Do NOT rewrite COD-102 or start mass doc elimination until the reframe lands. The only
booked doc-elimination is `dson-parsing-overview.md` convert+delete (Docs/Roadmap.md backlog). See
[[llmfrmw-closed-slot-catalog]] and [[compliance-run-definition]].
