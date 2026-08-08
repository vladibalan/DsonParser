# DsonParser - Decision Log

The *why* behind shipped decisions: design forks and the reasoning that settled them, postmortems,
notable rejects. Cold/on-demand (off the hot-path budget) - it absorbs history so the status and
rules docs stay tight. Newest at top. When it crosses the doc ceiling, rotate as a dated
append-only log (see `Docs/DocRules.md`, "Cold-doc rotation, in detail").

## 2026-08-08 - LLMfrmw framework migration (text+code+cpp, v0.2.0)

**What.** Migrated the repo onto the LLMfrmw framework stack at v0.2.0 (commit
`229775d32721f4e55976b21ddf282d75118374d1`, `https://github.com/vladibalan/LLMfrmw.git`): vendored
the text/code/cpp cores byte-identical into `Docs/` + `.claude/hooks/` + `Tools/`; assembled
`Docs/Rulebook.md` (COD-1..9 + CPP-1..2 + project-local COD-100/101); replaced the four
hand-authored governance docs (`code-review-rules`, `agent-workflow`, `versioning`,
`audit-prompts`) with cores + filled skeletons; inverted the entry pair (AGENTS.md canonical,
CLAUDE.md adapter); renamed `docs/` -> `Docs/`, `tools/` -> `Tools/` to the framework's canonical
casing; wired doc-guard + review-guard into `.claude/settings.json`; swapped `Check-ReleaseTag.ps1`
to the config-driven core; sealed the vendored RapidJSON tree (`VENDORED.txt`). Modules: versioning
LIVE (downstream UE consumer), native-binding LIVE (flat C ABI), upstream LIVE (RapidJSON 1.1.0),
testing dormant (no automated suite - COD-8 stays JIT).

**Why.** DsonTest2 is the G0 lineage repo the code+cpp stack was harvested from, so the cores fit
near-verbatim (rule crosswalk: the master's `Docs/Catalog/DsonTest2Kit.md`). One shared governance
home beats hand-maintained per-repo copies; a pinned version sync replaces doc drift. Conducted per
the master's `layers/MigrationGuide.md` with the user holding every checkpoint.

**Rejected / adjudicated.** Archive-over-delete for superseded docs (git history suffices;
deleted). Lowercase `docs/`/`tools/` landing paths (user ruling: framework conventions win - dirs
renamed instead). Include-guard hook (off - plain C++, `owningHeaders` empty, adds nothing here).
`Contract.md` (skipped - `DsonParserAPI.h` is the single-export-header contract; family table in
`Docs/Reference.md`). `.codex/` hook mirror (off). The repo release-tag script's `-Audit` mode
(lost in the core swap; carried to the master as a fit gap). cpp `Patterns.md` core (deferred -
filename collision with the text-layer core; backlog + fit gap). Immediate ASCII conversion of
legacy docs (deferred to a backlog cleanup sweep - user ruling). The framework `Roadmap.md`
skeleton was initially skipped, then adopted on the user's ruling (the first non-capability status
item needed a home).
