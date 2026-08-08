---
name: parser-changes-route-through-user
description: Plugin Director never edits the DsonParser repo directly; route parser changes as what/why requests through the user to the Parser Director
metadata: 
  node_type: memory
  type: feedback
  originSessionId: a0c1d81f-db51-441b-ab4c-928a0733d3d6
---

The DsonToUnreal plugin Director must NOT author DsonParser-repo artifacts (its CHANGELOG, `.handoff` task-files, or source). Roles do not cross repos. A plugin-side need for a parser change is a request routed THROUGH THE USER to the Parser Director, stating **what** (the DSON data / importer behaviour / which symbols) and **why** — never the C-ABI **how** (version bump, CHANGELOG wording, line edits, internal fields). Governing doc: plugin `Docs/AgentWorkflow.md` "Requesting parser features (cross-repo)".

**Why:** the parser is consumer-blind upstream and must be driven by its own Director from its own governance (versioning.md / code-review-rules). If the consumer's Director authors parser releases, the boundary erodes and plugin-side assumptions leak into parser decisions.

**How to apply:** when a task needs a parser change, produce a tight what/why request for the user to relay, and do only the plugin-side consequence yourself (re-vendor via `Tools/Sync-Parser.ps1`, X-macro binding, rebuild, plugin docs). Generalizes [[dsonartisan-upstream-opaque]] one level up (DsonToUnreal → DsonParser); see [[dsontounreal-plugin-git-repo]]. Incident 2026-06-13: on cleanup-backlog item 1 (remove the dead `GetUVPolygonVertexIndex*` exports) I edited the parser CHANGELOG and wrote a parser task-file directly; reverted both and reissued as a request.
