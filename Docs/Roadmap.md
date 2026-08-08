# DsonParser - Roadmap

The single source of truth for **open status**: the current phase, what is deferred, and the
backlog. Status only - the *why* lives in `Docs/DecisionLog.md`, durable facts in
`Docs/Reference.md`, capability detail and epoch history in `DsonParser_Roadmap.md`. Keep it
current **in the same change that moves the status**.

## Current phase

The parser is stable at 2.22.0 (flat C ABI; consumer: the DsonToUnreal UE 5.4.4 plugin).
2026-08-08: the repo migrated onto the LLMfrmw text+code+cpp framework stack (v0.2.0) - migration
complete, verification gate green; governance now runs on the vendored cores (`Docs/Rulebook.md`,
`Docs/AgentWorkflow.md`, `Docs/Versioning.md`); the migration entry (and its close-out) is in
`Docs/DecisionLog.md`. The v2 formula epoch continues per `DsonParser_Roadmap.md`.

## Done

- 2026-08-08 - Claude auto-memory store brought in-repo (`.claude/memory/`) via the
  `autoMemoryDirectory` binding, plus the `memory-autocommit` Stop-hook wired - dispositions and
  why: `Docs/DecisionLog.md`.
- 2026-08-08 - LLMfrmw framework migration (text+code+cpp) - dispositions and why:
  `Docs/DecisionLog.md`.

## Deferred

- Optional restructure of `DsonParser_Roadmap.md` into the Capabilities/DecisionLog tiers (ruled
  out of the migration's scope; revisit on demand).

## Known issues

- Standing doc-census warns, cleanup-pending (ruled 2026-08-08): `CHANGELOG.md` 897 ln / 68.5 KB
  (over the 64 KB universal ceiling; one 465-char line; 447 non-ASCII chars),
  `DsonParser_Roadmap.md` (239 non-ASCII), `Docs/dson-parsing-overview.md` (178 non-ASCII).
  Cleared by the ASCII-sweep backlog item and its follow-up ceiling re-measure.
- Source-geometry census (2026-08-08): 34 soft COD-5 warns, COD-6 nesting clean. Oversize:
  `DsonParserAPI.cpp` 3154 ln, `DsonTest2/DsonTest2.cpp` 2925 ln, `DsonTypes.cpp` 1885 ln,
  `DsonParserAPI.h` 1003 ln (ceiling 1000); long-function warns concentrate in the harness (16,
  per-section drivers), `DsonTypes.cpp` (6), `DsonInflate.cpp` (3), `DsonLoadTest.cpp` (1).
  Split-vs-rule adjudicated per file when next worked; the in-flight `DsonTypes.cpp` helper
  splits continue that direction.

## Backlog

- ASCII cleanup sweep across legacy/domain docs (reverses the earlier deliberate Unicode sweep,
  `CHANGELOG.md` typography included); afterwards re-measure `CHANGELOG.md` against the 64 KB
  ceiling and rule rotate-vs-linear. [added 2026-08-08]
- Sweep the stale `docs/versioning.md` comment references in source to `Docs/Versioning.md`
  (grep `docs/versioning.md`: `DsonParserAPI.h`, `DsonParserVersion.h`; comment-only, needs an
  Implementer task). [added 2026-08-08]
- Vendor the cpp-layer `Patterns.md` core at the next framework sync, once the master resolves its
  filename collision with the text-layer core (fit gap carried back). [added 2026-08-08]
- Pin `/std:c++14` explicitly in the three `.vcxproj` files (CPP-1: pin in the build, not by the
  v143 default; build-config change - verify with a full rebuild). [added 2026-08-08]
