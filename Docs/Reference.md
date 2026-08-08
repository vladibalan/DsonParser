# DsonParser - Reference

Durable facts, hard-won lessons, and recurring gotchas - the things that stay true across phases
and that a future session should not have to rediscover. Cold/on-demand. Status lives in
`Docs/Roadmap.md`; rationale in `Docs/DecisionLog.md`. A **living fact store**: facts are titled
and updated in place; topic-split at the ceiling (`Docs/DocRules.md`).

## Framework pin record (LLMfrmw)

Vendored from `https://github.com/vladibalan/LLMfrmw.git` at tag v0.2.0, commit
`229775d32721f4e55976b21ddf282d75118374d1`. Adaptation record: `Docs/FrameworkSlots.md`.

    text layer materialized from LLMfrmw v0.2.0 (229775d) on 2026-08-08.
    code layer materialized from LLMfrmw v0.2.0 (229775d) on 2026-08-08.
    cpp layer materialized from LLMfrmw v0.2.0 (229775d) on 2026-08-08.

## Vendored upstream pin: RapidJSON (the COD-10 canonical token)

The one canonical pin literal for the vendored parser dependency: **RapidJSON 1.1.0**
(`Tencent/rapidjson`, MIT), under `DsonParser/include/rapidjson/` (sealed; in-tree marker
`VENDORED.txt`; license text: `THIRD_PARTY_NOTICES.md`). Every other mention says "the pinned
version" and points here; a re-pin bumps this token and records the adoption in the same change
(`Docs/Upstream.md`, COD-10).

## C-ABI return-value contract (per function family)

The published API (`DsonParserAPI.h` - the authoritative statement, CPP-3) encodes success/failure
differently **per function family**; the same value means opposite things across families. The
single biggest source of consumer bugs - confirm the family before judging a sentinel:

| Family | Examples | Success / found | Failure / missing |
| --- | --- | --- | --- |
| Loaders (int status) | `DsonDocument_LoadFromFile/String/Buffer` | `0` | non-zero (`1`) |
| Handle creator (pointer) | `DsonDocument_Create` | non-null | `nullptr` |
| Counts (int) | every `*_Get*Count` | count (>= 0) | `0` |
| Bool getters | `*_GetVertexBoneInfluence(Capped)`, `*_Get*ChannelHasColor` | `true` | `false` |
| String getters (const char*) | `*_GetNodeId`, ... | the string | `""` |
| Numeric getters (double) | `*_GetVertexX`, ... | the value | `0.0` (`1.0` for scales) |
| Value/index accessors (int) | `*_GetPolylistFaceVertex`, `*_Get*DeltaVertexIndex` | the index | `-1` |

- Counts return `0` on invalid handle/index, **never `-1`** (`-1` belongs to the value/index
  family: "no such element").
- Loaders are `0 = success` - the polarity trap: `if (LoadFromFile(...))` runs on **failure**;
  compare explicitly to `0`.
- Keep the header/cpp contract comments accurate when a convention changes (COD-4).
- Callers copy `const char*` returns before the next ABI call (CPP-4: parser-owned, transient).

## Version carriers (the project's COD-11 binding)

The consumer is an LLM agent in a separate repo that sees only shipped artifacts - never this
repo's git log or internal docs like `Docs/Capabilities.md`. Four carriers announce every change (policy:
`Docs/Versioning.md`):

| Carrier | Answers |
| --- | --- |
| Version macros in `DsonParser/DsonParserVersion.h` (included by `DsonParserAPI.h`) | which version, at compile time |
| `DsonParser_GetVersion()` (C ABI) | which version, at runtime |
| `@since x.y.z` tags + header banner in `DsonParserAPI.h` | which symbols are new + the headline |
| `CHANGELOG.md` (ships beside header/DLL) | what changed each release |

- On a release three points move together - the macros, the CHANGELOG top heading, the `vX.Y.Z`
  tag (close-gate: `Tools/Check-ReleaseTag.ps1`). New exports get `@since <version>`; the header
  banner's "what's new" line refreshes.
- The SemVer boundary is binary (ABI) compatibility of the flat C API. Comment/whitespace-only
  header edits: no bump, no CHANGELOG entry.
- **Capability epochs are not SemVer**: `Docs/Capabilities.md`'s "v1"/"v2" label capability eras;
  the v2 formula work shipped as additive MINOR bumps within 1.x. A MAJOR comes only from a
  breaking ABI change.
- Baseline `1.0.0` labeled the first versioned tree (~180 exports); pre-versioning history is not
  retro-numbered.
- Naming: `DsonParser_GetVersion()` returns the library version; `DsonDocument_GetFileVersion()`
  returns the parsed asset's `file_version` field - do not confuse them.

## Effective C++ standard: C++14 by toolset default

The `.vcxproj` files pin no standard; MSVC v143's default (C++14) is the effective pin - a toolset
bump silently re-pins the code, so treat one as a review trigger. The explicit `/std:c++14` pin is
a `Docs/Roadmap.md` backlog item (CPP-1). No C++17+ constructs (structured bindings,
`string_view`, `clamp`, if-init, `optional`, fold expressions, inline variables); fine: range-for,
`using` aliases, trailing returns, lambdas, declaration-in-`if`. MSVC/Windows specifics are
acceptable for this target (`__declspec(dllexport)`, `fopen_s`, `MAX_PATH`); UE-agnostic always -
no UE headers/types; the C ABI decouples the DLL's standard from UE's C++20 (expected, not a bug).

## Dson value wrappers: a mixed ternary needs an explicit cast (MSVC C2445)

`Dson::Int/Float/String/...` carry both a conversion operator and a converting constructor, so
`cond ? wrapper : primitive` is ambiguous - write `cond ? static_cast<int>(wrapper) : sentinel`.
Plain `int`/`double` model fields (`UVSet::vertex_count`, `Node::general_scale`) need no cast. The
unused scaffolding value types themselves are load-bearing by decree (COD-100).

## DsonTest2 harness: piped-EOF exit is 255, not a failure

The harness blocks on `cin.get()` at exit; under piped/redirected stdin the EOF path exits 255.
Read the PASS/FAIL lines, not the exit code. A still-running harness also pins `DsonParser.dll`
(build gotchas: `Docs/Tooling.md`).

## Harness verification blind spots (proof assets)

Known gaps when diffing harness stdout as a behavioral oracle: the harness exercises
`polyline_list` at zero coverage, and `Genesis3.duf` loads zero geometries - a behavior-preserving
refactor can pass a stdout diff while touching those paths. Pair the diff with a targeted
PowerShell P/Invoke probe of the affected accessors, in a separate process (a probe holding the
DLL open breaks the next rebuild - LNK1104).

## DAZ gzip: an all-zero trailer is accepted

DAZ-packaged `.dsf`/`.duf` files can carry a blank all-zero gzip trailer (CRC32 + ISIZE); the
internal inflater (`DsonInflate`) accepts it on a clean inflate - DAZ-compat, tolerated since
2.2.3. Do not "fix" it into a hard failure (COD-4 permissive ingest).

## Windows PowerShell 5.1 environment hazards

- Double quotes inside a `git commit -m` message break PS 5.1 quoting - reword quote-free or use
  an ASCII `-F` temp file (`-F -` piping adds a BOM; avoid it).
- `.ps1` files must be pure ASCII: PS 5.1 reads BOM-less UTF-8 as cp1252 (mojibake on non-ASCII).
- Prefer absolute paths where paths carry spaces; build tooling lives in `Docs/Tooling.md`.
