---
name: ask-for-files-before-diagnostics
description: "When needed data lives in a file, ask the user to upload it before proposing a build/diagnostic workaround"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: bedf278e-7dce-4be0-b398-009811470eeb
---

When a task or answer needs a fact that lives in a file I don't have (DAZ `.duf`/`.dsf` assets, logs, headers), ask the user to upload the file and read it directly — *before* proposing a diagnostic build, a dump, or any workaround that makes the user compile/run code to surface data the file already contains.

**Why:** The user can hand over source assets directly; a build round-trip is slower and is the user's own manual step (they run all builds — see [[build-and-read-prefs]]). I had defaulted to proposing a diagnostic build to learn Genesis 9 scene structure when uploading the `.duf` would have answered it immediately. The user flagged this and asked to harden the workflow rule.

**How to apply:** A diagnostic/build is the fallback only for data no static file holds (runtime/engine behavior). For inspecting an asset, ask for the file first. Hardened in DsonToUnreal `Docs/AgentWorkflow.md` "Shared boundaries" → Missing inputs. Generalizes [[no-silent-fails]].

**Reinforced 2026-07-10 (auto-follow arc):** the rule covers PROXY substitution too — when the exact
file is gzip-blocked ([[dson-gzip-decompress-blocked]]), do NOT analyze a readable *sibling* document
(e.g. the "Bottom And Wrap" DUF standing in for the loaded "Bottom"-only DUF) and present its facts as
the loaded asset's; ask the user for a decompressed copy of the ACTUAL file. User: "Don't make
assumptions, ask me for files if you need them." A proxy's facts must be labeled as the proxy's, with
the gap named, until the real file or a user check closes it. Also: a decompressed extension-less
sibling's NAME is not proof of its content — verify `asset_info.id` before attributing it.
