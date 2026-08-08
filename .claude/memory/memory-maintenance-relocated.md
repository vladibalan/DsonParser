---
name: memory-maintenance-relocated
description: memory-maintenance/ moved out of the DsonTest2 repo to D:/ClaudeConfig/memory-maintenance/ (2026-08-08); salvaged-from-orphan-stores.md still awaits plugin-seat adoption
metadata: 
  node_type: memory
  type: project
  originSessionId: a0d3a71c-eaf6-4d12-9d4d-6cc67e1c5a13
  modified: 2026-08-08T03:07:52.735Z
---

The `memory-maintenance/` folder (the 2026-07-12 memory-store migration's working docs) was moved
out of the DsonTest2 repo tree to `D:\ClaudeConfig\memory-maintenance\` on 2026-08-08 (LLMfrmw
migration checkpoint A2 - it was polluting the new doc census and was never repo material).

**Why:** four of its five files are closed-arc residue of the completed 2026-07-12 migration; the
folder only lived in the repo because Desktop sessions used to root there, which per-repo rooting
ended ([[claude-desktop-session-rooting]]).

**How to apply:** the live artifact is `salvaged-from-orphan-stores.md` - five durable UE-API/DAZ
facts awaiting adoption by the DsonToUnreal / DsonArtisan Directors (promote to each plugin's
`Docs/` or memory store, their call). Raise it when working those seats. The old in-repo path and
its `.gitignore` entry are gone.
