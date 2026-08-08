# DsonParser Architecture

The code layout and per-component responsibilities - the map an agent routes from before touching
source. Kept in sync with the code **in the same change** (the review gate authors this at merge).
Cold/on-demand.

## Components

- **DsonParser** (DLL) - the parser + typed model + flat C ABI; `DsonParserAPI.h` is the published
  surface (single export header, CPP-3). Internally STL + vendored RapidJSON only; UE-agnostic;
  effective C++14 (v143 default; explicit pin is a `Docs/Roadmap.md` backlog item). Version macros:
  `DsonParser/DsonParserVersion.h` (`Docs/Versioning.md`).
- **DsonTest2** (console exe) - test harness exercising the C API; load-time-links `DsonParser.lib`.
- **DsonLoadTest** (console exe) - standalone dynamic-load regression test (`LoadLibrary` /
  `GetProcAddress`, no `.lib` link, worker threads started before the load) verifying the per-thread
  last-error contract under the consumer's real load model.

Build unit: `DsonTest2.sln`, verified config Release|x64 - mechanics in `Docs/Tooling.md`.

## File map

**`Docs/dson-parsing-overview.md` owns the authoritative per-file map** (file table, parsing
pipeline, supported DSON sections, ownership rules, known boundaries) - this doc points at it
rather than restating it (one tier per doc). Every real source file also opens with an
`orientation:` comment block; read that block before the body. Boilerplate (`pch.*`, `framework.h`,
`dllmain.cpp`) and the sealed vendored tree `DsonParser/include/rapidjson/**` (`VENDORED.txt`) are
outside the real source surface.

## Detail

n/a - the component split above plus the overview's file map cover the tree at its current size; no
`ArchitectureDetail` tier is warranted.
