# DsonParser - Decision Log

The *why* behind shipped decisions: design forks and the reasoning that settled them, postmortems,
notable rejects. Cold/on-demand (off the hot-path budget) - it absorbs history so the status and
rules docs stay tight. Newest at top. When it crosses the doc ceiling, rotate as a dated
append-only log (see `Docs/DocRules.md`, "Cold-doc rotation, in detail").

## 2026-08-08 - Claude auto-memory store brought in-repo + memory-autocommit wired

**What.** Relocated the Claude per-project auto-memory store from the external harness path
(`$CLAUDE_CONFIG_DIR/projects/<slug>/memory/`) into the repo at `.claude/memory/` (57 files,
SHA256-verified copy), and wired a Stop-event `memory-autocommit` hook. Mechanism: the native Claude
Code `autoMemoryDirectory` setting (confirmed against `code.claude.com/docs/en/settings` and
`.../memory`) set to the absolute in-repo path in `.claude/settings.local.json`; the harness reads it
at session start, honored only after the workspace-trust dialog. The hook
`.claude/hooks/memory-autocommit.ps1` (ASCII, PS 5.1, fail-open) stages and commits ONLY the
`.claude/memory/` pathspec, so it never touches unrelated tree changes. Records updated:
`Docs/FrameworkSlots.md` (deviation -> wired), `Docs/Roadmap.md` (backlog -> done), `CLAUDE.md` (note).

**Why.** Version-controls the working-memory store with the repo so notes are never lost and travel
with the code - Claude Code auto-memory is machine-local and uncommitted by default. Chose the native
`autoMemoryDirectory` binding over a filesystem junction or a copy-on-Stop mirror per scope-check
discipline: the in-scope native path already delivers it, so no workaround is warranted. The binding
value must be an absolute path (the setting forbids repo-relative), so it is a per-seat local binding
in `settings.local.json` (recorded in `Docs/FrameworkSlots.md`); a clone at a different path re-sets
that one line. The autocommit hook is an optional local convenience (`Docs/MemoryConventions.md`
"Autocommit"), not a verbatim framework core, so it is authored in-tree rather than pinned to the master.

**Verification.** Staged copy SHA256-identical (57/57, 0 mismatch/0 missing/0 extra); hook pure ASCII,
no BOM; both settings JSON parse and both keys resolve. End-to-end + isolation proven by running the
hook against a dirty tree: it committed exactly the 57 store files (commit `dcece9b`), zero non-store
leaks, and left the five in-flight doc/config edits and the untracked hook script uncommitted. NOT yet
verified (user-owned, next session): the live harness cutover - that Claude Code, after the
workspace-trust dialog, reads and writes new memory under `.claude/memory/`. The external store is
retained as a backup until the user confirms the cutover.

**Rejected / adjudicated.** Directory junction from the external path to `.claude/memory/` (works and
keeps committed files machine-agnostic, but filesystem trickery + per-clone re-setup, and forgoes the
blessed native setting - not chosen). Copy-on-Stop mirror (the in-repo dir would lag the live store;
"relocate" only approximately - not chosen). Binding in the shared `.claude/settings.json` (bakes a
machine path into shared config; `settings.local.json` is the per-seat scope - chosen instead).

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

**Closed same-day.** Verification gate all-PASS: Check-DocForm 29 docs, 0 hard-cap failures, 5
adjudicated warns (the legacy-doc set, cleared by the ASCII-sweep backlog item); Check-SourceGeometry
0 COD-6 nesting failures, 34 soft COD-5 warns booked as a Roadmap known issue; Check-ReleaseTag
consistent at 2.22.0; full `/t:Rebuild` Release|x64 clean with zero warnings; both hooks verified
live in-session (doc-guard caught a real AGENTS.md budget breach; review-guard emits the checklist +
extraRules). Landed as one doc/config commit to `main` (ce12c57). The migration report went to the
user to carry the fit gaps upstream (Patterns.md core name collision, MATERIALIZE case-collision
ordering hazard, ReleaseTag `-Audit` loss, DocForm `excludeDirs` gap, cpp MATERIALIZE step-2/3
wording, the vacuous-sourceMarkers quirk). The census-noise `memory-maintenance/` scratch dir
(closed 2026-07-12 memory-migration residue) was relocated out of the tree to the user's config area
(checkpoint A2), clearing 9 of the 14 initial doc-census warns.

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
