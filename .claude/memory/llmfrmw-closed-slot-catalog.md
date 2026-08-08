---
name: llmfrmw-closed-slot-catalog
description: LLMfrmw docs = cores + skeletons per layer; in DsonTest2 only dson-parsing-overview.md is a genuine non-slot.
metadata: 
  node_type: memory
  type: reference
  originSessionId: 0431d528-329a-474d-9ee6-e94d665c7c62
  modified: 2026-08-08T06:37:22.602Z
---

LLMfrmw (v0.2.0, commit 229775d) defines each doc slot as either `layers/<layer>/cores/*` (verbatim,
domain-free) or `layers/<layer>/skeletons/*` (materialized with project content), across text/code/cpp.
Code skeletons include Architecture, AuditGuide, Capabilities, Contract, HandoffTemplates, ReusePatterns,
Tooling; text skeletons include AGENTS, CLAUDE, Intent, Principles, Roadmap, DecisionLog, Reference. So in
DsonTest2 almost every `Docs/*.md` is a filled slot; the ONLY genuine non-slot is
`Docs/dson-parsing-overview.md` (CalibrationLog.md is most likely a Staffing-core module artifact, not
bespoke).

The authoritative catalog is NOT in this repo - `Docs/FrameworkSlots.md` records only FILLED slots. It lives
in the master (read-only local clone at E:\Work\Code\LLMfrmw): `layers/**` + `Docs/Catalog/DsonTest2Kit.md`.
Read at the pin via `git show 229775d:<path>`; never checkout (the master's working tree is dirty).

**Why:** classifying docs by their in-repo self-description over-counts "non-framework" docs (a careful read
got 6; the truth is 1) because the slot universe isn't visible in-repo.

**How to apply:** classify against the master catalog at the pin, not by guesswork. See
[[llmfrmw-consumer-interaction-reframe]] and [[parser-opaque-vendor-only-via-procedure]].
