# DsonParser Agent Guide

Canonical, tool-neutral entry for **any** LLM agent working this repo (Claude Code auto-loads
`CLAUDE.md`, which defers here; other agents start here). DsonParser is a C++ DLL that parses DAZ
Studio DSON/DSF/DUF (JSON) assets into a typed model and exposes it through a flat C ABI for engine
importers (a separate UE 5.4.4 plugin consumes it).

## Session pickup

1. Read `Docs/Intent.md` (direction) and `Docs/Roadmap.md` (open status; `Docs/Principles.md`
   governs both). Capabilities: `Docs/Capabilities.md`; *why*: `Docs/DecisionLog.md`; facts: `Docs/Reference.md`.
2. For any parsing / data-model / C-API question, read `Docs/dson-parsing-overview.md` **before**
   opening source - it is the authoritative file map, pipeline, and coverage doc. Most tasks need
   only it plus one source file; every real source file also opens with an `orientation:` block.
3. Before any source edit: `Docs/Rulebook.md` (COD-* + CPP-* + the COD-100+ project rules) and
   `Docs/NativeBinding.md` (CPP-3/4, the C-ABI contract rules). Before any doc edit:
   `Docs/DocRules.md` + `Docs/Disciplines.md`.

## Operating model (two roles)

Source work runs the code layer's two-role model (`Docs/AgentWorkflow.md`): a **Director**
(coordination, docs/config, git, verification - no source edits) and an **Implementer** (any coding
agent the user launches - source edits only, no git, no doc edits). The user declares the role at
session start; if undeclared, ask. The handoff is file-based via `.handoff/` (gitignored): read
ONLY the one task-file you are handed - never browse `.handoff/`. Task/feedback forms:
`Docs/HandoffTemplates.md`; seat tiering: `Docs/Staffing.md`.

## Hard rules (binding for every agent)

- No silent fails: surface blockers, gaps, uncertainty, and partial results explicitly; never claim
  a build or run you did not do. Ask the user for a missing file rather than guessing its contents
  (`Docs/Disciplines.md`).
- Git is the Director's; the Implementer never runs it; the user pushes. Doc/config commits go
  straight to `main`; source lands via a `task/<id>` branch squash-merge (`Docs/Tooling.md`).
- **The user decides when a version bump ships.** Surface changes stage under `## [Unreleased]` in
  `CHANGELOG.md`; release finalization is user-triggered (`Docs/Versioning.md`, COD-11).
- Vendored/consumed upstreams are opaque - reach them only by a formal request through the user
  (`Docs/Upstream.md`).

## Do NOT read

- `DsonParser/include/rapidjson/**` - vendored third-party (RapidJSON 1.1.0, sealed; in-tree
  marker `VENDORED.txt`). Never project source and never the answer to a task here.
- `.handoff/**` - agent handoff scratch; only the specific task-file you are explicitly handed.

## Build & verify

`Docs/Tooling.md` owns the mechanics: msbuild via vswhere (NOT on PATH), Release|x64, `/t:Rebuild`
for the from-scratch check; close running test exes first (they pin the DLL - LNK1104).

## Enforcement differs by harness

Claude Code fires `.claude/hooks/doc-guard.ps1` + `review-guard.ps1` automatically on edits (they
read `Tools/DocForm.config.json` / `Tools/ReviewGuard.config.json`). **Other harnesses get no hook
backstop**: self-audit against `Docs/DocRules.md` / `Docs/Rulebook.md`, then run the censuses:

```
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Check-DocForm.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Check-SourceGeometry.ps1
```
