# DsonParser Code Review Rules

Reusable ruleset for reviewing changes to this codebase. Apply these on top of a
normal correctness/quality pass. They encode the conventions and hazards specific
to DsonParser — a permissive DAZ DSON/DSF/DUF parser exposed to a UE 5.4.4 plugin
through a flat C ABI.

Read [`dson-parsing-overview.md`](dson-parsing-overview.md) first for the file map
and pipeline. Don't review `DsonParser/include/rapidjson/**` — vendored.

---

## 1. The C-ABI return-value contract (highest priority)

The public API (`DsonParserAPI.{h,cpp}`) uses **different success/failure encodings
per function family**. The same value (`0`) means opposite things across families.
This is the single biggest source of consumer bugs — review it first.

| Family | Examples | Success / found | Failure / missing |
| --- | --- | --- | --- |
| **Loaders** (`int` status) | `DsonDocument_LoadFromFile/String` | `0` | non-zero (`1`) |
| **Handle creator** (pointer) | `DsonDocument_Create` | non-null | `nullptr` |
| **Counts** (`int`) | every `*_Get*Count` | element count (≥ 0) | `0` |
| **Bool getters** (`bool`) | `*_GetVertexBoneInfluence(Capped)`, `*_Get*ChannelHasColor` | `true` | `false` |
| **String getters** (`const char*`) | `*_GetNodeId`, … | the string | `""` |
| **Numeric getters** (`double`) | `*_GetVertexX`, … | the value | `0.0` (or `1.0` for scales) |
| **Value/index accessors** (`int`) | `*_GetPolylistFaceVertex`, `*_GetUVOverrideFace`, `*_Get*DeltaVertexIndex` | the index | `-1` |

Rules:
- **R1.1 — Counts return `0` on invalid handle/index, never `-1`.** Any new or
  edited `*Count` function that returns `-1` on error is a bug. `-1` is reserved
  for value/index accessors that report "no such element."
- **R1.2 — Don't confuse a count with a value/index accessor.** `*Count` ⇒ `0`
  sentinel. An accessor returning a stored index/value ⇒ `-1` sentinel. When
  reviewing a `-1` return, confirm which family the function belongs to.
- **R1.3 — Loaders are `0 = success`.** Flag any internal logic or doc comment that
  implies non-zero/true = success for the loaders.
- **R1.4 — Keep the header contract comment accurate.** If a return convention
  changes, the orientation block in `DsonParserAPI.h` (and the `.cpp`) must still
  state it correctly: counts→`0`, bool→`false`, string→`""`, value/index→`-1`.

## 2. API changes are breaking changes (live UE consumer)

The DLL is consumed by a **separate UE 5.4.4 plugin repo** through the C ABI. Treat
the API surface as published.

- **R2.1** — Any change to a return value, sentinel, signature, or semantics of an
  `extern "C"` function is a **breaking change**. It is not a purely-internal
  refactor. Call it out explicitly in the review.
- **R2.2** — For such a change, require a **consumer handoff prompt** (see
  [`audit-prompts.md`](audit-prompts.md) style): explicit list of affected
  functions, worked before→after examples, mechanized (not judgment-based)
  instructions, and scope guards for what NOT to touch.
- **R2.3 — Polarity-inversion audit.** When reviewing consumer-facing changes,
  watch for the classic bug: treating a loader's `int` like a bool
  (`if (LoadFromFile(...)) {…}` runs on **failure**). Loaders must be compared
  explicitly to `0`.

## 3. DRY / compactness — use the established helpers

The accessor and parser layers were de-duplicated onto a small set of helpers.
Flag re-introduction of the patterns they replaced.

In `DsonParserAPI.cpp`:
- **R3.1** — Use `Doc(handle)` (null-safe) + `At(collection, index)`
  (bounds-checked, returns `nullptr`) for element access. Flag new inline
  `if (!handle) … ; if (index < 0 || index >= static_cast<int>(coll.size())) …`
  guards — that's exactly what `Doc`/`At` exist to remove.
- **R3.2** — Multi-level access nests `At` (e.g. modifier → joint → weight). Prefer
  that over re-deriving bounds inline.

In `DsonTypes.cpp`:
- **R3.3** — Leaf field extraction uses `ParseMember(json, "key", field)`. Flag new
  `if (JsonHelper::HasMember(json,"k")) { field.ParseFromJson(json["k"]); }` blocks
  — they double-hash-lookup and duplicate the helper.
- **R3.4** — Array-or-`{count,values}` unwrap uses `GetValuesArray(container,"key")`.
  Flag new inline `if (v.IsObject()) GetArray(...,"values"…) else if (v.IsArray())…`
  unwraps. (Exception: `vertices`/`polylist` legitimately also read `"count"`.)
- **R3.5** — Array-of-objects sections use
  `ParseObjectArray(container,"key",vec,unknownKeys)`. Flag hand-written
  reserve/loop/push blocks for library or scene-instance arrays.
- **R3.6** — Prefer one shared helper over N near-identical functions. New "family"
  of accessors (≥3 near-clones) should route through a helper, as the
  material-channel and node-transform families do.

## 4. Language & portability constraints

- **R4.1 — C++14 only.** The `.vcxproj` pins no standard (MSVC v143 default = C++14)
  by deliberate choice. **Flag any C++17+ feature**: structured bindings,
  `std::string_view`, `std::clamp`, `if (init; cond)` init-statements, inline
  variables, fold expressions, `std::optional`, etc. Available and fine: range-for,
  `std::accumulate`, `using` aliases, trailing return types, lamb​das,
  declaration-in-`if`-condition (`if (T* p = …)`).
- **R4.2 — Stay UE-agnostic.** The parser must not include UE headers or use UE
  types (`FString`, `TArray`, `UE_LOG`, …). Only STL + RapidJSON internally; only C
  types cross the ABI. The C ABI fully decouples the DLL's C++ standard from UE's
  (UE 5.4 is C++20) — that's expected, not a problem to "fix."
- **R4.3** — MSVC/Windows specifics are acceptable (`__declspec(dllexport)`,
  `fopen_s`, `MAX_PATH`). Don't flag them as portability issues for this target.

## 5. Dson value-wrapper gotchas

- **R5.1 — Wrapper in a ternary needs an explicit cast.** `Dson::Int/Float/String/…`
  have both a conversion operator and a converting constructor, so
  `cond ? wrapper : primitive` is ambiguous (MSVC C2445). Require
  `cond ? static_cast<int>(wrapper) : sentinel`. Plain `int`/`double` fields
  (`UVSet::vertex_count`, `Node::general_scale`) don't need it.
- **R5.2 — Unused value types are intentional scaffolding, NOT dead code.** Types in
  `DsonDataTypes.{h,cpp}` not yet referenced by the parser/API (`Bool`, `Float`,
  `Vector2`, `Color`, `ChannelType`, `ChannelValue`, `IndexedIntArray`,
  `IndexedFloatArray`) pre-stage future DSON coverage. **Do not flag them for
  removal.**

## 6. Parser conventions (permissive by design)

- **R6.1** — The parser is intentionally permissive: missing optional fields keep
  defaults, malformed array entries are skipped (not errors), unrecognized keys are
  recorded for audit (not rejected). Don't "harden" this into validation/rejection
  without an explicit request — flag such changes as semantic shifts.
- **R6.2** — Unknown keys are tracked per context via `TrackUnknownKeys` against a
  `static const std::set<std::string> knownKeys`. New parsed keys should be added to
  the relevant `knownKeys` set, or they'll show up as false-positive unknowns.
- **R6.3 — Scene vs library separation.** `scene.*` instances and `*_library`
  definitions are exposed by distinct accessors and must stay separate; don't
  collapse them.

## 7. Build & verification

- **R7.1 — Do not build.** The user runs all builds (enforced by a `permissions.deny`
  rule on `msbuild`/`dotnet build`/`devenv`). Verify changes by static review + grep,
  and report results for the user to compile.
- **R7.2** — Even comment-only or literal-only changes are handed to the user to
  build; state that explicitly.
- **R7.3** — Files > 500 lines: confirm before reading in full. The two large files
  are `DsonParserAPI.cpp` (~1.5k) and `DsonTypes.cpp` (~1.1k).

## 8. How to conduct & report the review

- **R8.1 — Enumerate, don't gesture.** For any contract/return-value finding, list
  the exact affected functions and the before/after sentinel. Don't say "some count
  functions"; name them.
- **R8.2 — Prove completeness with grep across the whole family**, not just the
  call sites that have the obvious pattern. E.g. when changing the count contract,
  grep every one of the affected functions for `< 0` / `<= -1` / `== -1` / `>= 0`,
  including call sites in sibling files — not only the ones with a visible
  `warn + clamp`.
- **R8.3 — Separate "must fix" from "dead but harmless."** A leftover `< 0` check on
  a function that can't return negative is dead code, not a bug (0 flows safely
  through loops/`Reserve`). Distinguish it from a real inverted/relied-upon check.
- **R8.4 — Call out silent behavior shifts.** Note when a contract change turns a
  former error/abort path into a "treated as empty" no-op (e.g. an invalid skin
  query now yields an unskinned mesh instead of aborting import).

## 9. Keep agent-orientation docs in sync (LLM-facing artifacts)

This repo is built to be navigated by agents **from its docs**, not by scanning
the tree. So when a code change makes an orientation artifact stale, updating
that artifact is part of the change — not a follow-up. A wrong file map or
contract table actively misdirects the next agent, which is worse than no doc.

- **R9.1 — `docs/dson-parsing-overview.md` is the source of truth; keep it in
  sync.** Adding/removing/renaming a source file, moving a responsibility between
  files, or changing the parsing pipeline or supported DSON sections requires a
  matching edit to the overview's file map / pipeline / coverage. CLAUDE.md
  defers to it, so the two must agree.
- **R9.2 — Update CLAUDE.md's "Real source surface" table** on the same file-map
  triggers as R9.1. The approximate line counts there (e.g. "~1160 lines") are
  intentionally rough — refresh them only on large size changes, not every edit.
- **R9.3 — Refresh the file's `orientation:` comment block.** If an edit changes
  a file's purpose or responsibilities, update its top-of-file orientation block
  in the same diff; agents read that block before the body.
- **R9.4 — Keep this ruleset current.** A change to the C-ABI return contract,
  the DRY helper set, or the language constraints must update the affected rule
  here (§1 table, §3 helper list, §4) — same spirit as R1.4 for the header
  comment. A stale rule mis-trains every later review.
- **R9.5 — Sync the roadmap on capability changes.** New or removed DSON coverage
  updates `DsonParser_Roadmap.md`'s capability summary / v1 limitations. A pure
  refactor that doesn't change capability needs no roadmap edit.
- **R9.6 — Make doc syncs visible.** List the documentation updates explicitly in
  the change summary so the synchronization is reviewable, not silent. (Doc-only
  edits are still handed to the user per R7.2.)

---

## Quick checklist

- [ ] Counts return `0` on error (never `-1`); value/index accessors return `-1`. (R1)
- [ ] Header/cpp orientation comments still state the return contract accurately. (R1.4)
- [ ] Any `extern "C"` change flagged as breaking + handoff prompt required. (R2)
- [ ] No loader treated as a bool; loaders compared to `0`. (R1.3, R2.3)
- [ ] Element access via `Doc`/`At`; no re-introduced inline bounds guards. (R3.1)
- [ ] Leaf parse via `ParseMember`; arrays via `GetValuesArray`/`ParseObjectArray`. (R3.3–R3.5)
- [ ] No C++17+ features; no UE headers/types in the parser. (R4.1–R4.2)
- [ ] Wrapper-in-ternary uses `static_cast`. (R5.1)
- [ ] Unused value types left in place. (R5.2)
- [ ] Permissive parsing preserved; new known keys added to `knownKeys`. (R6)
- [ ] Did not build; reported for user to compile. (R7)
- [ ] Findings enumerate exact functions; completeness grep run family-wide. (R8)
- [ ] Orientation docs synced with the code change: overview file map, CLAUDE.md table, `orientation:` blocks, this ruleset, roadmap. (R9)
