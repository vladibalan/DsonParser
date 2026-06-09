# DsonParser — Agent Guide

A C++ DLL that parses DAZ Studio DSON/DSF/DUF (JSON) assets into a typed model
and exposes it through a flat C ABI for engine importers (e.g. UE5).

## Operating model (two-agent workflow)

This repo is worked through two roles: a **Director** (takes your instructions,
reads files, writes docs/instruction/config files, writes task-files for the
Implementer, and verifies the returned change) and an **Implementer** (any LLM
coding agent the user launches on a task-file — edits source per the code-review
rules and writes back a feedback-file). The handoff is file-based via `.handoff/`;
the user launches each run by hand. At session start the user states which role
this session plays; if unstated, ask.

**Both roles:** the user handles git commits/pushes — never commit. Report build
and run results faithfully — never claim something compiled or ran unless you
actually did. If a needed file is missing from the project folder, ask the user
to upload it rather than guessing its contents.

**Builds:** the **Implementer** builds and verifies its own changes
(`msbuild DsonTest2.sln /p:Configuration=Release /p:Platform=x64`) and reports the
real result; the **Director** re-runs that build itself to verify the returned
change (it confirms the result rather than trusting a self-report). See "Build &
test" below.

**Read [`docs/agent-workflow.md`](docs/agent-workflow.md)** for the full role
definitions, the file-based handoff sequence, and the task-file / feedback-file
templates.

## Read this first (discovery shortcut)

**For any question about parsing, data model, or the C API, read
[`docs/dson-parsing-overview.md`](docs/dson-parsing-overview.md) before opening
source files.** It contains the authoritative file map, parsing pipeline,
supported DSON sections, ownership rules, and known boundaries. Most tasks need
only that doc plus one source file — you do not need to scan the tree.

Then, if you still need code: every real source file opens with an `orientation:`
comment block (purpose + responsibilities). Read that block first and only read
the full body if the task is actually scoped to that file.

Other docs:
- [`docs/agent-workflow.md`](docs/agent-workflow.md) — Director/Implementer
  roles, file-based handoff sequence, shared boundaries, task-file/feedback
  templates.
- [`DsonParser_Roadmap.md`](DsonParser_Roadmap.md) — capability summary, audit
  history, v1 limitations, planned v2 formula work.
- [`docs/versioning.md`](docs/versioning.md) — versioning & change-announcement
  policy: SemVer-with-C-ABI, the carrier contract, how upstream agents consume it.
- [`CHANGELOG.md`](CHANGELOG.md) — per-release C-ABI change log; ships beside the
  header/DLL so consumers see what changed without this repo's source.
- [`docs/audit-prompts.md`](docs/audit-prompts.md) — prompts for DSON coverage audits.
- [`docs/code-review-rules.md`](docs/code-review-rules.md) — **read before
  writing or reviewing any code in this repo** (see "Before editing source"
  below). Return-value contract, DRY helpers, C++14/UE-agnostic constraints,
  breaking-change discipline, and how to conduct the review.

## Before editing source

These rules govern *authoring*, not just review. Before you edit any file under
`DsonParser/` (or `DsonTest2/DsonTest2.cpp`):

1. **Read [`docs/code-review-rules.md`](docs/code-review-rules.md) first** if you
   have not already this session. The hazards it encodes — the per-family C-ABI
   return-value contract (R1), API changes as breaking changes (R2), the
   established DRY helpers (R3), C++14-only / UE-agnostic constraints (R4) — are
   easy to violate while writing and expensive to catch later. Write code that
   already complies, rather than fixing it on review.
2. **After each edit, self-audit the change against that doc's Quick checklist**
   and state the result. Don't gesture ("looks fine"); name the rules you
   checked against and confirm the diff satisfies them, or flag what doesn't.

This applies even to small or comment-only edits — the return-value and
breaking-change rules are most often broken by "minor" tweaks.

## Do NOT read

- `DsonParser/include/rapidjson/**` — **vendored third-party** (RapidJSON, ~35
  headers). This is the bulk of the repo's files and is never the answer to a
  task here. Don't read or modify it.
- `.handoff/**` — **agent handoff scratch** (per-task `task-<id>.md` /
  `feedback-<id>.md` and their `history/` archive). Read only the specific
  task-file you are explicitly handed; never browse `.handoff/`. See
  [`docs/agent-workflow.md`](docs/agent-workflow.md).

## Real source surface (everything else is boilerplate or vendored)

| File | Purpose |
| --- | --- |
| `DsonParser/DsonDataTypes.{h,cpp}` | Primitive JSON-backed value wrappers (`String`, `Vector3`, `FloatArray`, `IntArray`, `Url`, sparse `IndexedArray<T>`). |
| `DsonParser/DsonTypes.h` | Typed DSON model structs (assets, nodes, geometry, materials, skin, modifiers, images, UV sets, scene, root document). |
| `DsonParser/DsonTypes.cpp` | Main parser: RapidJSON → `Dson::*` model + post-parse image linkage. (~1160 lines) |
| `DsonParser/DsonHelpers.{h,cpp}` | Safe RapidJSON accessor helpers (`JsonHelper`). Declarations in `.h`, implementations in `.cpp`. |
| `DsonParser/DsonInflate.{h,cpp}` | Internal dependency-free gzip/DEFLATE inflater used by the loader; verifies CRC32 and ISIZE before JSON parsing. |
| `DsonParser/DsonParserAPI.{h,cpp}` | Flat `extern "C"` C ABI: opaque handles, parser-owned string returns, bounds-checked accessors, lazy query caches. (`.cpp` ~1810 lines) |
| `DsonParser/DsonParserVersion.h` | Canonical single-source-of-truth library version macros (`DSONPARSER_VERSION_*`); published with and included by `DsonParserAPI.h`. Backs `DsonParser_GetVersion()`. |
| `DsonTest2/DsonTest2.cpp` | Console test harness that exercises the C API. |

The published surface is `DsonParserAPI.h` (the flat C ABI). The C++ model
headers (`DsonDataTypes.h`, `DsonTypes.h`, `DsonHelpers.h`) are internal
implementation detail — consumers never include them, and the RapidJSON they
reference never reaches a consumer.

Boilerplate (rarely relevant): `pch.{h,cpp}`, `framework.h`, `dllmain.cpp`.

## Build & test

- This is a Visual Studio solution: `DsonTest2.sln` (`DsonParser` = DLL,
  `DsonTest2` = console test exe linking `DsonParser.lib`).
- Typical build: `msbuild DsonTest2.sln /p:Configuration=Release /p:Platform=x64`.
- **The Implementer builds and verifies.** After source changes, compile (and run
  the `DsonTest2` harness where useful) and report the real result — errors,
  warnings, pass/fail — in the feedback-file. Never claim something compiled or
  ran unless you actually did; if a build can't be run, say so and fall back to
  static review + grep. **The Director re-runs the build itself to verify the
  returned change** (repo as ground truth, feedback-file as advisory); the user
  still handles commits.

## Conventions

- The parser is intentionally permissive: missing optional fields keep defaults,
  malformed array entries are skipped, unrecognized keys are recorded for audit
  rather than rejected.
- C API returns parser-owned `const char*`; callers copy strings they need to
  keep. Invalid handles/indexes return empty string / 0 / false / -1.
- Keep `docs/dson-parsing-overview.md`'s file map in sync when files change — it
  is the single source of truth this guide defers to.
