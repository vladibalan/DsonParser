# DsonParser - Roadmap

The single source of truth for **open status**: the current phase, what is deferred, and the
backlog. Status only - the *why* lives in `Docs/DecisionLog.md`, durable facts in `Docs/Reference.md`,
capability detail in `Docs/Capabilities.md`, per-release history in `CHANGELOG.md`. Keep it current
**in the same change that moves the status**.

## Current phase

The parser is stable at 2.22.0 (flat C ABI; consumer: the DsonToUnreal UE 5.4.4 plugin). The
2026-08-08 LLMfrmw framework migration (v0.2.0) and its follow-on doc settling are complete (see
Done below); governance runs on the vendored cores (`Docs/Rulebook.md`, `Docs/AgentWorkflow.md`,
`Docs/Versioning.md`), the doc tiers are settled with `Docs/Capabilities.md` owning capability
detail, and the close-outs are in `Docs/DecisionLog.md`. Framework conformance is under reframe
(2026-08-08): the owner is moving the framework's closed-catalog consumer model toward a selective one
with the framework Agent, so the COD-102 conformance discipline is paused and the doc-elimination
objective is on hold (see Deferred; why: `Docs/DecisionLog.md`). The v2 formula epoch's parser side is
complete (catalog: `Docs/Capabilities.md`); the remaining formula work is consumer-side (the UE
importer).

## Done

- 2026-08-08 - Established COD-102 (framework-conformance discipline: conform to the vendored LLMfrmw
  stack by default, argue every divergence) and audited the `Docs/FrameworkSlots.md` deviation ledger
  against it - every deviation clears the bar; the two gaps found are closed (the `ReleaseTag -Audit`
  trade-off now argued in the ledger; the C++14 pin below). Dispositions and why: `Docs/DecisionLog.md`.
- 2026-08-08 - Pinned the C++ standard to C++14 via a root `Directory.Build.props`
  (`<LanguageStandard>stdcpp14</LanguageStandard>`), covering all three projects and configs from one
  home instead of the v143 default (CPP-1). Verified: full `/t:Rebuild` Release|x64 emits `/std:c++14`
  for all three projects, 0 warnings. Closes the COD-102 audit's CPP-1 gap - dispositions and why:
  `Docs/DecisionLog.md`.
- 2026-08-08 - Retired `DsonParser_Roadmap.md`: promoted its v1 inventory + the shipped formula
  accessors into the framework `Docs/Capabilities.md` catalog slot, reframed `dson-parsing-overview.md`
  as the parse-behavior map, and dropped the roadmap's `CHANGELOG.md`-duplicated history and
  fully-shipped v2 plan. Dispositions and why: `Docs/DecisionLog.md`.
- 2026-08-08 - Legacy-doc ASCII sweep (`CHANGELOG.md`, `DsonParser_Roadmap.md`,
  `Docs/dson-parsing-overview.md` -> 0 non-ASCII) + root `CHANGELOG.md` rotation: sealed the v1.x
  epoch (1.0.0-1.6.0) into `Docs/CHANGELOG/1.x.md`, 465-char line wrapped; root 58.9 KB under the
  64 KB ceiling, doc census 0 warns - dispositions and why: `Docs/DecisionLog.md`.
- 2026-08-08 - Claude auto-memory store brought in-repo (`.claude/memory/`) via the
  `autoMemoryDirectory` binding, plus the `memory-autocommit` Stop-hook wired - dispositions and
  why: `Docs/DecisionLog.md`.
- 2026-08-08 - LLMfrmw framework migration (text+code+cpp) - dispositions and why:
  `Docs/DecisionLog.md`.

## Deferred

- Framework-conformance enforcement (COD-102) and the "convert + eliminate non-framework docs" objective
  are paused pending the closed-catalog -> consumer-selective reframe the owner is taking to the framework
  Agent (why: `Docs/DecisionLog.md`; a sitrep was prepared for that conversation). COD-102 stays as written,
  unenforced and unrevised, until the reframe lands; its final shape depends on the outcome. The one decided
  disposition - `Docs/dson-parsing-overview.md` convert+delete - is in the Backlog and is not blocked by this.
  [2026-08-08]

## Known issues

- Source-geometry census (2026-08-08): 34 soft COD-5 warns, COD-6 nesting clean. Oversize:
  `DsonParserAPI.cpp` 3154 ln, `DsonTest2/DsonTest2.cpp` 2925 ln, `DsonTypes.cpp` 1885 ln,
  `DsonParserAPI.h` 1003 ln (ceiling 1000); long-function warns concentrate in the harness (16,
  per-section drivers), `DsonTypes.cpp` (6), `DsonInflate.cpp` (3), `DsonLoadTest.cpp` (1).
  Split-vs-rule adjudicated per file when next worked; the in-flight `DsonTypes.cpp` helper
  splits continue that direction.

## Backlog

- Sweep the stale `docs/versioning.md` comment references in source to `Docs/Versioning.md`
  (grep `docs/versioning.md`: `DsonParserAPI.h`, `DsonParserVersion.h`; comment-only, needs an
  Implementer task). [added 2026-08-08]
- Vendor the cpp-layer `Patterns.md` core at the next framework sync, once the master resolves its
  filename collision with the text-layer core (fit gap carried back). [added 2026-08-08]
- Convert `Docs/dson-parsing-overview.md` into framework slots, then delete it (owner runs it in a
  separate session): component/pipeline map -> `Docs/Architecture.md`; durable parse-behavior facts +
  C-ABI return/sentinel contracts -> `Docs/Reference.md`; enumerated catalog already in
  `Docs/Capabilities.md` (kit artifact map: overview -> `Architecture.md` + cpp `orientation:` headers).
  Reverses the 2026-08-08 keep-as-parse-behavior-map call (`Docs/DecisionLog.md`, which flagged a full
  de-interleave as regression risk since accessor citations are woven into behavior prose) - preserve those
  citations in the targets and record the reversal in `Docs/DecisionLog.md` when the work lands.
  [added 2026-08-08]
