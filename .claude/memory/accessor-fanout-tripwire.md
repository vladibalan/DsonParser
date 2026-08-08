---
name: accessor-fanout-tripwire
description: the DsonParser accessor fan-out tripwire is prompt-only by choice and user-run periodically; the prompt itself is now self-sufficient
metadata: 
  node_type: memory
  type: project
  originSessionId: d4a95460-d464-432c-8db4-d2b5b82184dd
---

The "Accessor Fan-Out (C ABI Maintenance Tripwire)" audit in
`docs/audit-prompts.md` is an early-warning tripwire the user wants **run
periodically**, watching whether the flat C ABI's accessor fan-out is outgrowing
the typed model. As of 2026-07-16 the prompt is **self-sufficient** — method,
corrected baseline anchors, exclusion list, arity-vs-data-depth rule, sanctioned
cases, verdict, and remedy discriminator all live there. Read it there; do not
duplicate it here, and do not trust any figure quoted outside it.

**Prompt-only by user choice — do not write a script for it.** Run it as an
LLM/compliance-style audit; the user does not want a script to maintain. This
matters more now that the prompt carries exact regexes and makes scripting look
easy: that is a reproduction recipe for the agent to follow, not a spec to
automate.

**Run every metric, every run — not just the one the release seems to touch.**
2.1.0 pushed an accessor to 4 indices; that run checked only the mirror metric,
recorded "no metric impact," and the tripwire sat tripped and unnoticed for a
month until the 2026-07-16 run. The per-release note is not a substitute for the
full sweep.

Last run 2026-07-16 @ 2.17.0: **GREEN on all five** (the metric-1 anchors in the
prompt encode that result). Distinct from the broader [[compliance-run-definition]]
(rules audit). See [[reconcile-fr-framing-against-codebase]] when an FR motivates a
run.
