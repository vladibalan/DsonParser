# DsonParser — Agent Guide (Codex / other agents)

The full guide lives in [`CLAUDE.md`](CLAUDE.md) — **read it first.** The
essentials, repeated here so they're never missed:

1. **Discovery shortcut:** for any parsing / data-model / C-API question, read
   [`docs/dson-parsing-overview.md`](docs/dson-parsing-overview.md) before
   opening source files. It is the authoritative map (file table, pipeline,
   supported sections, boundaries). Most tasks need that doc + one source file.
2. **Never read `DsonParser/include/rapidjson/**`** — vendored third-party
   (~35 headers, the bulk of the repo). Not the answer to any task here.
3. Each source file opens with an `orientation:` comment block. Read that block
   before reading the full body.
4. Real source surface is small: `DsonDataTypes`, `DsonTypes`, `DsonHelpers`,
   `DsonParserAPI` (in `DsonParser/`) and `DsonTest2/DsonTest2.cpp`. Everything
   else is boilerplate or vendored. See `CLAUDE.md` for the per-file table.
5. **Implementers build and verify** (`msbuild DsonTest2.sln /p:Configuration=Release
   /p:Platform=x64`) and report the real result; the Director defers builds, and the
   user handles git commits. Report build/run results faithfully — never claim a clean
   build you didn't run.
