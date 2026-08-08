# Code Review Rules - the source rulebook (framework core, code layer)

A **verbatim framework core**: domain-free, byte-identical across consumers. The code layer's source
review rulebook - the numbered rules a source author self-audits against and the Director's review gate
checks the finished diff against (`AgentWorkflow.md`). It is the **lead section** of a cross-layer
rulebook: a consumer's assembled rulebook is this `COD-*` section followed by the `CPP-*` / `UE-*`
sections of whatever layers its stack adds (assembly by concatenation under stable ids, DecisionLog
D8/D10). Rules carry only **abstract illustration** (D9); a language's or engine's concrete traps live
in that layer's Reference starter, never inline here.

**Rule ids are stable (D8).** Framework code rules are `COD-1..99`; a project's own rules extend at
`COD-100+`. A **retired rule keeps its number as a stub** so downstream citations and the rule count
stay stable - never renumber. **COD-10** (upstream pin-token) and **COD-11** (versioning) are the
Director-side rules carried by the upstream and versioning modules; they join the rulebook when their
module is included. Reviewing? Read **COD-9** (conduct) first - it is the method; COD-1..8 are the
substance it applies.

## COD-1 - Do not duplicate logic; shared helpers have a home

Before adding a helper, check whether it already exists; a helper that gains a second caller has a
named shared home, listed in the reuse-seam index (code-layer skeleton) - add it there the moment it
does. **Two altitudes, two phases.** Reuse binds at helper altitude and at design time: the Director
scopes a task to *extend* the nearest existing seam, and reinventing a named seam is a violation even
when no helper is literally duplicated (catch it at design time, not on review - by review the fresh
design is already written). **Split by evidence scope.** Write time (the Implementer) is the local tier
only - reuse the named homes, keep the diff duplication-free, and report any unavoidable copy in the
feedback, never silently; unifying look-alikes beyond the diff is never a write-time action. Cross-file
DRY is the Director's **wave-triggered** decomposition sweep (never scheduled, never per-task), which
maintains a **keep-separate ledger** - a positive record of ruled non-duplicates a later pass must not
re-flag. A **correctness-critical** transform (one that must stay identical across call sites) is never
re-inlined.

## COD-2 - Compact, but never drop functionality

The goal is the smallest code that keeps every behavior. Remove dead parameters threaded through call
chains, generated/annotation declarations with no consumer, and debug scaffolding left in the hot path;
declare locals at point of use. **Confirm nothing is load-bearing before deleting** - if in doubt, ask
rather than delete. The author-side dual: a deliberately retained quirk workaround carries a one-line
**WHY** at the site, so a later compaction pass sees it is load-bearing instead of removing it. Size
and nesting are their own rules (COD-5, COD-6).

## COD-3 - Match the codebase's idiom

Mirror the patterns already in the file you are editing rather than importing a foreign style. Names
are unique, descriptive, and greppable - one search finds every touchpoint of a thing; a file is named
for its primary unit (one primary unit per file; small file-private helpers may colocate). Keep the
build **warning-clean** - a new warning is a finding, not noise. The language-specific idiom - cast
forms, compile-time branches, include/import hygiene, explicit-over-deduced types, one-log-statement-
per-line - is the `CPP-*` half of this rule at the language layer.

## COD-4 - Preserve the failure and breaking-change contracts

- Builders and entry points **return a failure sentinel** (null / false / empty) on failure and log
  enough context (identity, path, index) to reproduce; keep that contract - do not swallow context when
  consolidating logs.
- A **public interface** states its contract at the declaration: responsibility and lifecycle in a
  short comment, plus whatever binds callers that the signature does not say - ownership, call
  order/phase, null/failure semantics. Comment the WHY on any deliberate quirk workaround.
- A **public-signature change is a breaking change** for every consumer - call it out explicitly, never
  slip it into a "minor" edit. A breaking change to a **published** surface also drives a mechanized
  consumer-handoff prompt (the versioning module), so no downstream consumer discovers the break by
  compile error.
- **Permissive ingest**, where the project ingests external/serialized data: skip a malformed unit with
  a warning rather than hard-failing the whole run, and keep faithful pass-through. (n/a where nothing
  external is ingested - state it n/a, keep the number.)

## COD-5 - Source geometry: soft ceilings, split-not-grow

Small units keep review and agent context cheap - the doc-economy philosophy applied to source. A
ceiling crossing is a **signal to factor out a unit**, never a reason to drop functionality (COD-2):

- **Functions:** signal at ~60 lines, split obligation at ~100. A genuinely linear sequence may stay
  whole **with a stated reason** (the escape is a one-line note, not silence).
- **Files:** signal at ~800 lines, split obligation at ~1,000.
- One responsibility per unit; prefer composition over deep inheritance chains.
- **Never delete a comment or a diagnostic to fit a budget.** They are load-bearing (COD-2 record,
  COD-4 log-context) and splitting a unit does not reduce total context - the comments travel with the
  code. Split it, extract a **named** helper so the name carries what the comment carried, or state why
  the size is right; never shrink the record. (Contrast COD-6: nesting is a *hard* cap.)

## COD-6 - Control-flow nesting: a hard cap at 4

Deep nesting hides the reviewable path even in a short unit - a 25-line body nested five deep reads
worse than a flat 300-line spine. Unlike COD-5's soft ceilings this is a **hard cap with no
stated-reason escape** (the contrast is deliberate):

- **Max control-flow nesting depth is 4.** Depth counts nested control blocks
  (`if`/`for`/`while`/`switch`/`else`/`do`/`try`/`catch`) inside a unit body: the body is level 0, the
  first control block is level 1, so depth 4 is four nested blocks. A lambda/closure body starts a fresh
  count; non-control braces (namespaces, scopes, initializer lists) do not count.
- **Depth >= 5 must be refactored** - hoist the inner block to a helper, invert with an early-return
  guard clause, or dispatch through a table. Never state a reason and keep it.

## COD-7 - Logic lives in text; carriers are thin; no hidden dispatch

This is the verbatim-carrier principle (DecisionLog D4) in rule form:

- **Logic lives in text, not in a shipped binary/data carrier.** A shipped asset is a thin, logic-free
  carrier whose behavior contract is declared in text. (The binary-asset specifics are the `UE-*` half
  at the unreal layer.)
- **No string-based *code* dispatch** - invoking a function by a runtime name string is grep-invisible
  and rename-broken. String-keyed *data* addressing (records by name/id) is fine and often the point.
- **No new hand-rolled singletons or mutable statics** - dependencies arrive as parameters or members;
  a sanctioned engine/framework accessor is not a new singleton.

## COD-8 - Tests are source

Test code obeys every rule here, like any other source (no separate rule set, no separate seat). The
charter:

- **Refutation only.** A green suite asserts only that ratified behavior did not move - never that the
  behavior is *right* (a self-consistent check certifies its own wrong assumption). Correctness is
  settled by the real, user-driven observation, not a boolean a test computed.
- **Spec-literal assertions.** Every assertion compares against a **task-file spec literal**, never a
  value read back from the code under test (no vacuous capture-then-assert); **non-vacuity** is
  asserted.
- **Loud skip.** A test that cannot run without an absent fixture or local config **skips loudly by
  name**, never a silent green.
- **One run per change**, with a **stale-binary refusal** and a **report stamp**. A red suite blocks
  like a build failure, and an expectation is never hand-edited to force green. (The runner mechanics,
  freshness stamp, and exit-code taxonomy are the test-runner tool core.)

## COD-9 - Review conduct

How to run a review - generation-independent method, binding whenever the diff is read as a whole (the
Director's gate, or any explicit review pass):

- **Lead with findings, ordered by severity, each with a `file:line`** - do not summarize the code
  first.
- **Enumerate, do not gesture** - name every instance and prove completeness by grepping the family
  (e.g. every call site of a changed signature), not a spot check.
- **Separate must-fix from dead-but-harmless**, and label out-of-focus findings (correctness,
  efficiency) as such rather than dropping them.
- **Call out silent behavior shifts** - a change that moves behavior without saying so is itself the
  finding.
- **Propose fixes in batches by risk** - pure deletions first, mechanical consolidation next,
  structural rewrites last - so the maintainer can build and sign off between batches.

## Quick checklist (state results after each change, naming the rules checked)

COD-1..9 are the source author's self-audit and the reviewer's gate. The same-change doc/status sync is
the text layer's (TXT-7/8/9), authored by the Director at the merge (`AgentWorkflow.md`).

- [ ] COD-1: no duplicated helper; the nearest seam extended over a fresh design; correctness-critical
      transforms not re-inlined; any unavoidable copy reported, nothing unified beyond the diff unbidden.
- [ ] COD-2: no dead params, no consumer-less declarations, no debug scaffolding in the hot path;
      nothing load-bearing removed; retained quirks carry their one-line WHY.
- [ ] COD-3: idiom mirrors the file; unique greppable names; file named for its primary unit; build
      stays warning-clean.
- [ ] COD-4: failure-sentinel + reproduce-context contract intact; public interfaces state their
      contract; breaking changes flagged (published surface -> consumer-handoff prompt); permissive
      ingest preserved (or n/a).
- [ ] COD-5: no function past ~100 lines and no file past ~1,000 without a stated reason; no comment or
      diagnostic deleted to fit a budget.
- [ ] COD-6: no unit nests control flow past depth 4 (hard cap, no stated-reason exception).
- [ ] COD-7: no logic in shipped carriers; no string-based code dispatch; no new hand-rolled singletons.
- [ ] COD-8: tests obey the rulebook; assertions compare spec literals; non-vacuity asserted; absent
      fixtures skip loudly; the suite ran once with a fresh-binary stamp.
- [ ] COD-9: findings led with severity + `file:line`, enumerated across the family, must-fix separated,
      silent behavior shifts called out.

# C++ Review Rules - the cpp rulebook section (framework core, cpp layer)

A **verbatim framework core**: domain-free (C++-generic, no project specifics), byte-identical across consumers.
The cpp layer's slice of the cross-layer source rulebook - the `CPP-*` rules a C++ source author self-audits
against and the review gate checks the diff against (`AgentWorkflow.md`, code layer). It **concatenates after
the `COD-*` section** under stable ids (assembly by concatenation, DecisionLog D8/D10): a stack of text+code+cpp
assembles COD-1..11 then CPP-1..2 (then the optional CPP-3..4 native-binding module if the project adopts it).
These rules carry only **abstract C++ illustration** (D9); a project's concrete traps - its toolchain, its
vendored libraries, its ABI - live in the cpp Reference starter, never inline here.

**Rule ids are stable (D8).** Framework cpp rules are `CPP-1..99`; a project's own extend at `CPP-100+`; a
retired rule keeps its number as a stub. **CPP-1..2 are the always-on language rules** carried by this file.
**CPP-3** (native-binding single source) and **CPP-4** (external-resource RAII) are **optional** rules carried
by the native-binding module (a separate cpp core) - like COD-10/COD-11 live in their modules, they join the
assembled rulebook only where the project has a native ABI, and stay an **n/a-stub** (number retained) where it
does not. CPP-1 is new at this layer; **CPP-2 is the C++ half of COD-3** (match the idiom). A rule's
engine-specific edge forward-points to the `UE-*` layer.

## CPP-1 - Pin the language standard; every construct exists at the pin

The project pins one **C++ language standard**, and every construct in the source must exist at that standard -
a newer-standard feature is a defect even when the local toolchain would compile it.

- **Pin explicitly, in the build.** State the standard in the build itself (the compiler / project standard
  setting), not by relying on a toolchain's *default* - a default silently changes when the toolchain moves,
  un-pinning the code with no diff. The pinned value is the one project-local slot this rule carries; the rule
  text is standard-agnostic.
- **No newer-standard constructs.** A feature from a standard above the pin is a review finding even if it
  builds locally; the review rule is the **backstop** to the build pin, never the sole mechanism. (Keep a short
  local list of the common over-the-line features for the pinned standard - a Reference seed, not this core.)
- **Platform / toolchain specifics for the pinned target are acceptable** - a construct guaranteed by the
  target platform or compiler is not a portability defect to "fix"; state the target so a reviewer does not
  flag it.
- **A C ABI decouples this module's standard from a consumer's.** Where the module exposes a C ABI, its own C++
  standard is independent of any consumer's - a standard mismatch across the ABI boundary is expected, not a
  bug. Only C-safe constructs cross the boundary (the CPP-3 module's concern).
- The **engine/toolchain-version** sharpening - a two-sided supported *range* (the floor binds API choice; the
  ceiling is the half that gets forgotten) and "supported = actually run and verified on that version, not
  merely compiled" - is the **UE-1** half at the engine layer; a plain-C++ project pins one standard and stops
  here.

## CPP-2 - Match the C++ idiom (the C++ half of COD-3)

COD-3 says mirror the file's existing idiom, keep names unique and greppable, keep the build warning-clean. Its
language-specific half at C++:

- **Casts:** prefer a **checked, explicit** cast (`static_cast`; `reinterpret_cast` / `const_cast` only with
  cause) over a C-style cast, and disambiguate an ambiguous implicit conversion with an explicit cast rather
  than leaving the compiler to choose.
- **Include hygiene / include-what-you-use.** A translation unit includes what it directly uses and does not
  lean on transitive or unity-build includes to supply a name. A **header is self-contained** - it compiles on
  its own, includes (or forward-declares) its own dependencies, and pulls no more than it needs. A source file
  **includes its own header first** (after any forced prefix / precompiled header), so the header's
  self-containedness is exercised on every build.
- **`explicit` one-argument constructors by default.** A one-arg constructor is `explicit` unless an implicit
  conversion is a deliberate design choice - and a deliberate implicit-conversion type carries a one-line WHY
  at the declaration (the COD-2 / COD-4 quirk-WHY discipline), because it trades a real hazard (ambiguous
  overload / ternary resolution) for its ergonomics.
- **Overloads share semantics.** An overload set differs only by parameter type, never by behavior - same call,
  same meaning. Prefer compile-time branching (where the pinned standard provides it, CPP-1) over a runtime
  type switch when the choice is known at compile time.
- **Prefer explicit types where deduction hides intent** - deduce to avoid repetition, name the type where the
  reader needs it to follow the code.
- **One log statement per line** - one event, one line, so a log stays greppable and a diff to it stays legible
  (n/a where the unit has no logging path; state it n/a).
- **Compiler warnings are findings.** A new warning is a defect, not noise - resolve it or record why it is
  intentional. A warning-clean build is the COD-3 floor; at the language level the compiler is the first
  reviewer.

## Checklist additions (fold into the COD-9 review checklist)

- [ ] CPP-1: no construct newer than the pinned standard; the standard is pinned in the build, not by default;
      target-platform specifics not flagged as portability defects; C-ABI standard-decoupling respected.
- [ ] CPP-2: checked / explicit casts over C-casts; headers self-contained + own-header-first + IWYU; one-arg
      constructors explicit (or a documented implicit-conversion exception); overloads share semantics; one log
      per line; build warning-clean.

## Project-local rules (COD-100+) - DsonParser

Project-local extensions to the assembled rulebook (framework D8: a project's own rules extend at
COD-100+ under stable ids, never renumbered). Wording approved by the user 2026-08-08.

## COD-100 - Unused Dson value types are intentional scaffolding

Types in `DsonDataTypes.{h,cpp}` not yet referenced by the parser or API (`Bool`, `Float`, `Vector2`,
`Color`, `ChannelType`, `ChannelValue`, `IndexedIntArray`, `IndexedFloatArray`) pre-stage future DSON
coverage. Do not flag or remove them as dead code (COD-2's confirm-before-delete, settled in advance).

## COD-101 - Faithful, non-interpretive parsing; scene and library never merge

The parser reports each DSON section as the file states it. `scene.*` instances and `*_library`
definitions stay separate accessors (never collapsed), and no section's data may overwrite, fill, or
override another's - not even when the target field is empty or defaulted. Keyframe application,
formula evaluation, override resolution, and instance-onto-definition collapsing are consumer
decisions computed from the faithful inputs. Sole sanctioned exception: intra-material
`image_url -> texture_path` resolution against `image_library`. Any new cross-section merge is a
semantic shift requiring explicit user sign-off.

## COD-102 - Conform to the vendored framework; argue every divergence

This repo runs on the vendored LLMfrmw stack (pin record: `Docs/Reference.md` "Framework pin record");
conformance is the default, not a preference. Adopt the stock cores, doc tiers, tooling, and
procedures as they ship, and take core updates by re-syncing from the pin - never by hand-forking a
vendored core. A divergence - a skipped slot, a local procedure, a non-stock tool or layout - is
allowed only when it is strongly argued: a justification naming the concrete constraint and its
trade-off, recorded as a deviation in `Docs/FrameworkSlots.md`, with a `Docs/DecisionLog.md` entry
where the call is non-obvious, and carried to the master as a fit gap where it exposes a framework
limitation rather than silently forked. "It was the least-effort path", "easier", or "the framework
made it awkward" is never the justification; an unargued or convenience-only deviation is a defect
this rule fails. Conformity and discipline are the standard.
