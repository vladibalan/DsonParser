---
name: parser-opaque-vendor-only-via-procedure
description: "DsonToUnreal seat — keep DsonParser opaque; cross only to vendor on request, via Tools/Sync-Parser.ps1, verify via plugin build."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: f2652872-1cf8-4793-bb56-97d60beb2f19
---

From the DsonToUnreal Director seat the DsonParser repo is OPAQUE: don't read its source, run its test harness (DsonTest2.exe), or inspect its git — even to verify a shipped fix. The only sanctioned boundary crossing is vendoring a new parser version, and only when the user asks.

**Why:** the agnostic Parser→Importer→Designer split depends on each seat not reaching upstream; the parser's own testing is the Parser Director's job. Running the parser's harness and presenting it as this project's verification conflates the layers (the user corrected this 2026-06-21).

**How to apply:** on a user-requested vendor, run `Tools/Sync-Parser.ps1 -ParserRepo <root>` (Docs/Tooling.md §"Sync the vendored parser") — one-way pull of the 4-file bundle (DsonParserAPI.h / DsonParserVersion.h / CHANGELOG.md / DsonParser.dll, DLL from `x64\Release`); its compat gate refuses a downgrade and warns on a MAJOR bump; it never stages or commits. Verify via the PLUGIN build (`DsonParserAbiCheck.cpp` ABI check on `DsonHostEditor`), NOT the parser's tests; then commit branch-per-task. Take upstream behavior as attested by the Parser Director's shipped+tested release. New exports also need an X-macro row in `DsonParserFunctions.h` (R2) before they bind. Related: [[parser-changes-route-through-user]], [[dsonparser-consumer-dynamic-load-only]].
