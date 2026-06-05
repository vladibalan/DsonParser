# DsonParser — Agent Guide

A C++ DLL that parses DAZ Studio DSON/DSF/DUF (JSON) assets into a typed model
and exposes it through a flat C ABI for engine importers (e.g. UE5).

## Read this first (discovery shortcut)

**For any question about parsing, data model, or the C API, read
[`docs/dson-parsing-overview.md`](docs/dson-parsing-overview.md) before opening
source files.** It contains the authoritative file map, parsing pipeline,
supported DSON sections, ownership rules, and known boundaries. Most tasks need
only that doc plus one source file — you do not need to scan the tree.

Then, if you still need code: every source file opens with an `orientation:`
comment block (purpose + responsibilities). Read that block first and only read
the full body if the task is actually scoped to that file.

Other docs:
- [`DsonParser_Roadmap.md`](DsonParser_Roadmap.md) — capability summary, audit
  history, v1 limitations, planned v2 formula work.
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

## Real source surface (everything else is boilerplate or vendored)

| File | Purpose |
| --- | --- |
| `DsonParser/DsonDataTypes.{h,cpp}` | Primitive JSON-backed value wrappers (`String`, `Vector3`, `FloatArray`, `IntArray`, `Url`, sparse `IndexedArray<T>`). |
| `DsonParser/DsonTypes.h` | Typed DSON model structs (assets, nodes, geometry, materials, skin, modifiers, images, UV sets, scene, root document). |
| `DsonParser/DsonTypes.cpp` | Main parser: RapidJSON → `Dson::*` model + post-parse image linkage. (~900 lines) |
| `DsonParser/DsonHelpers.{h,cpp}` | Safe RapidJSON accessor helpers (`JsonHelper`). Declarations in `.h`, implementations in `.cpp`. |
| `DsonParser/DsonParserAPI.{h,cpp}` | Flat `extern "C"` C ABI: opaque handles, parser-owned string returns, bounds-checked accessors, lazy query caches. (`.cpp` ~1520 lines) |
| `DsonTest2/DsonTest2.cpp` | Console test harness that exercises the C API. |

Boilerplate (rarely relevant): `pch.{h,cpp}`, `framework.h`, `dllmain.cpp`.

## Build & test

- This is a Visual Studio solution: `DsonTest2.sln` (`DsonParser` = DLL,
  `DsonTest2` = console test exe linking `DsonParser.lib`).
- Typical build: `msbuild DsonTest2.sln /p:Configuration=Release /p:Platform=x64`.
- **The user runs builds.** Don't assume you've built or run anything unless you
  actually did — report build/run results faithfully, or ask the user to build.

## Conventions

- The parser is intentionally permissive: missing optional fields keep defaults,
  malformed array entries are skipped, unrecognized keys are recorded for audit
  rather than rejected.
- C API returns parser-owned `const char*`; callers copy strings they need to
  keep. Invalid handles/indexes return empty string / 0 / false / -1.
- Keep `docs/dson-parsing-overview.md`'s file map in sync when files change — it
  is the single source of truth this guide defers to.
