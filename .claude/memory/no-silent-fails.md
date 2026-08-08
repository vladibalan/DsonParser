---
name: no-silent-fails
description: "User dislikes silent failures; surface blockers, gaps, uncertainty, and partial results explicitly"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 5d075102-8438-47a0-8d38-08eeb3815719
---

The user explicitly dislikes silent failures. When a task can't be fully done, a
needed input is missing, an instruction is ambiguous, an assumption had to be
made, or a result is only partial, surface it plainly and up front — don't paper
over the gap, guess quietly, or present a partial/unverified result as complete.

**Why:** They value faithful reporting over a clean-looking answer that's quietly
wrong; a flagged gap they can act on is more useful than false confidence.

**How to apply:** State what worked, what didn't, what was skipped and why, and
what you're unsure of. Codified for the Director role in `docs/agent-workflow.md`.
Related work preferences: [[build-and-read-prefs]].
