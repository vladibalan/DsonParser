---
name: compliance-run-definition
description: "what the user means by a \"compliance run\" and how the Director executes it (differs by repo: DsonParser COD/CPP rulebook + P1-P4/msbuild since 2026-08-08, DsonToUnreal R1–R12+P1–P5, DsonArtisan R1–R13+P1–P6 — both plugins UE-host build)"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: d7bee488-180d-4cc0-a6cf-0b470165abf9
  modified: 2026-08-08T03:07:46.909Z
---

A "compliance run" = a Director audit of the **current repo state** against **all
rules defined in the documentation**. Since the 2026-08-08 LLMfrmw migration that
means `Docs/Rulebook.md` (COD-1..9 + CPP-1..2 + project-local COD-100/101) +
`Docs/NativeBinding.md` (CPP-3/4) + `Docs/Principles.md` (P1-P4), conducted per
`Docs/AuditGuide.md`. It is **not** a DSON coverage audit (that's the AuditGuide's
prompt library). Pre-migration runs cited `docs/code-review-rules.md` R1-R10 —
those ids are retired; the crosswalk is in the migration DecisionLog entry.

**Why:** "compliance" is not a documented term (grep only hits vendored RapidJSON
license headers); the user confirmed (2026-06-09) it means "compliance with the
rules defined in the documentation," and proceeded on that reading.

**How to apply (post-migration ids):** grep-check the mechanical invariants — the
per-family return contract (every `*Count`→`0`, value/index→`-1`, loaders
`0`=success; table in `Docs/Reference.md`, rule CPP-3), COD-1 `Doc`/`At` +
`ParseMember` helpers (`Docs/ReusePatterns.md`), CPP-1 no C++17 / no UE types,
COD-100 scaffolding types present, `knownKeys` currency (ReusePatterns row),
COD-11 version↔CHANGELOG↔`@since` carriers — then confirm docs in sync (same-change
sync: overview file map, AGENTS.md routing, orientation blocks, `Docs/Roadmap.md`),
and run both censuses (`Tools/Check-DocForm.ps1`, `Tools/Check-SourceGeometry.ps1`).
Finish with the Director's **own** verification rebuild (`msbuild DsonTest2.sln
/t:Rebuild /p:Configuration=Release /p:Platform=x64`) — Rebuild, not incremental,
so it actually recompiles. Report enumerated per COD-9. See [[build-and-read-prefs]].

**Plugin repo (DsonToUnreal) differs** (confirmed 2026-06-09): audit against
`Docs/CodeReviewRules.md` **R1–R12** (R12 = consumer-versioning gate, added 2026-06-10) *and* `Docs/Principles.md` **P1–P5**, conducted
per `Docs/AuditGuide.md` (report shape: findings→evidence→open-Qs→next-step). The
verification build is the **UE host editor target** (`Build.bat DsonHostEditor Win64
Development -Project=…`, close the UE Editor first — `Docs/Tooling.md`), **not**
msbuild. Doc-only Director fixes commit **straight to `main`** (no branch;
`Tooling.md`). First run 2026-06-09 found only doc-sync/settings drift (R8 arch-doc lag,
R11 settings-mirror, R10 Tooling budget); code rules R1–R7 were clean. See
[[dsontounreal-plugin-git-repo]].

**DsonArtisan seat (updated 2026-07-11):** audit against `Docs/CodeReviewRules.md`
**R1–R13** (R13 = own-version carrier gate, added 2026-07-11 with the v1.0.0
versioning adoption — see [[artisan-versioning-and-framework-kit]]) *and*
`Docs/Principles.md` **P1–P6**, per `Docs/AuditGuide.md`. Verification build =
`Build.bat DsonArtisanHostEditor Win64 Development -Project="D:\Unreal
Projects\DsonArtisanHost\DsonArtisanHost.uproject"`. (The 2026-07-11 R10 overages
were fixed by the same-day curation — all hot-path docs within budget since.)

**Split-run precedent (2026-07-11):** the user may ask for the docs run (R8–R13+P,
inline) and the codebase run (R1–R7+P) separately. Codebase-run method that worked:
4 parallel full-file cluster auditors (each given a rule digest + a
sanctioned-pattern list to suppress false positives on documented decisions) +
Director mechanical greps + **Director source-verification of every medium finding
before reporting** (15+ spot checks, 100% confirm rate). Findings then stage into
risk-ordered fix batches per CodeReviewRules "How to conduct the review" (deletions
→ mechanical consolidation → structural), one task-file + one release per batch;
unresolved scaffolding questions go to the Roadmap backlog as "open rulings", never
deleted unilaterally. Executed 3 batches 2026-07-11: v1.0.1 PATCH (failure-path/
diagnostics), v1.0.2 PATCH (mechanical consolidation), v1.1.0 **MINOR** (seam
unification). **Version-classification trap:** a "pure cleanup/refactor" batch reads
as PATCH by intuition, but if it moves a shared helper into a `Public/` header of the
runtime module (the only way two modules can share it) that is a **consumer-surface
addition → MINOR** (R13/Versioning.md). Batch 3's cross-module URL-parse promotion
forced 1.1.0, not 1.0.3. Structural (batch-3-class) seam unification also gets a user
**runtime-verify on the branch before merge**, unlike the PATCH batches. Full arc ran
6 releases: v1.0.1/1.0.2 PATCH, v1.1.0 MINOR, v1.1.1 (rulings 1+3, PATCH), v1.1.2
(ruling 2, PATCH); all 4 open scaffolding rulings then resolved (2 shipped deletes,
1 = no-code "leave as intended", recorded in DecisionLog so it doesn't resurface).
**Deleting a code symbol → sweep the DOCS too, not just Source:** grep the removed
name across `Docs/` and triage — struct-layout/field-list mentions in orientation docs
(Architecture/ArchitectureDetail/design-doc code blocks) are stale-orientation R8 fixes;
dated DecisionLog entries + historical "no X change" scope statements STAY (append-only
history, accurate when written). **Removing a never-written `UPROPERTY` from a persisted
`UCLASS`/`UDataAsset` is serialization-safe** — zero writers means every saved asset holds
it at default, UE drops the unresolved property tag on load: no `CurrentSchemaVersion`
bump, no `PostLoad` migration, no CoreRedirect (redirects are for renames, not dead-field
removals). Corroborate by building: a widely-included header recompiles all dependent TUs,
which is stronger than a text grep for confirming zero live references. **False-positive trap:** a same-file identifier grep is
NOT an include-usage oracle — a `TSoftObjectPtr<T>` member's `LoadSynchronous()`
upcast needs T's full definition with T appearing nowhere in the TU text; the build
is the authority (hit 2026-07-11, include-removal finding reverted by the
Implementer's load-bearing clause).
