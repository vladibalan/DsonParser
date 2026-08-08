---
name: gate-process-changes-on-recurrence
description: "User institutionalizes process/workflow fixes only after recurrence, not from a single incident"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: d81db576-08af-4b16-81a9-e4ec5edb58e7
---

When a one-off incident suggests a process/template/workflow change (e.g. tweaking the task-file template after one Implementer run stopped before docs/build), surface the option but do NOT codify it into standing docs/templates yet — the user prefers to wait and see if it recurs first.

**Why:** said "I'll see if this happens again" when declining a Definition-of-Done task-template change after a single truncated Implementer run (2026-06-11); consistent with the recurrence-gated threshold in [[accessor-fanout-tripwire]] ("recurrence not magnitude").

**How to apply:** propose the fix as an option with its rationale, but default to "watch for recurrence" unless the user opts in. One data point isn't a pattern; don't over-engineer process from it.
