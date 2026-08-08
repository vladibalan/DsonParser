---
name: handoff-design-read-via-file
description: All Director↔Implementer exchanges go through files — including the pre-code design read; never make the user hand-relay console text
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 115c8b31-54d8-4dc8-82d2-991248eec14f
---

When a task-file sets **Feedback requested = YES** (a design read *before* coding), the Director
must instruct the Implementer to **write the design read to `.handoff/feedback-<id>.md`** (e.g.
`Status: design-review`) and stop — the Director reviews it **from disk** and relays go/changes; the
same file is overwritten with the final report after coding. Do **not** leave the channel
unspecified: if only `Report:` names the feedback-file (read as the *post-implementation* report)
while `Feedback requested:` just says "design read before heavy coding," a reasonable Implementer
surfaces the design read in its **chat output**, and the user is forced to copy/paste it back.

**Why:** the file-based handoff exists precisely so the user does **not** hand-relay walls of console
text — it's fidgety and error-prone (user, 2026-06-11). A chat-only design read defeats the whole
model. Applies to both repos (DsonToUnreal + DsonParser) — same two-role file handoff.

**How to apply:** in every task-file, pin the design-read channel to the feedback-file. **Now codified
in `Docs/AgentWorkflow.md`** (2026-06-13): the task-file `Feedback requested` line defines the flow
and the feedback `Status` enum includes `design-review`; the Roadmap "Cleanup backlog" item that
tracked this is now closed. Related: [[handoff-system-port-guide]],
[[director-runtime-verify-via-user-import]], [[no-silent-fails]].
