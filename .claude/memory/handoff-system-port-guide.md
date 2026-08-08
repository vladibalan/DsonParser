---
name: handoff-system-port-guide
description: Portable port-guide for replicating the Director/Implementer handoff system in other projects; keep in sync with agent-workflow.md
metadata: 
  node_type: memory
  type: project
  originSessionId: ff2bf2c9-37bb-414c-b446-66c0cbd59592
---

`docs/handoff-system-port-guide.md` is the canonical portable spec for installing
this repo's Director/Implementer file-based handoff system in *another* project.
It abstracts all DsonParser specifics into a "§2 adaptation slots" table
(`BUILD_VERIFY_CMD`, `SOURCE_SURFACE`, `REVIEW_RULES_DOC`, `WHOLE_CHANGE_CHECKS`,
UTC `TIMESTAMP_CMD`, `AGENT_GUIDE`), with §3 a copy-ready genericized
`agent-workflow.md` (4-backtick outer fence so the target Director can paste it).

**Why:** user wants to propagate the two-role workflow to other repos, handing the
target project's Director a self-contained guide.

**How to apply:** when the handoff system changes (roles, `<id>` convention, flow,
the uniform review gate, two-tier reporting, history rules in
`docs/agent-workflow.md`), update this port-guide too so the portable copy doesn't
drift. Encodes the Implementer-builds / Director-re-verifies model from
[[build-and-read-prefs]].
