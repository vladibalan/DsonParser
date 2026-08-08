---
name: parser-refactor-behavioral-diff-gate
description: Gate behavior-preserving parser refactors on a before/after harness-stdout diff PLUS a P/Invoke probe for surfaces the harness misses; run probes in separate processes.
metadata: 
  node_type: memory
  type: reference
  originSessionId: eebe71e5-6616-4146-a58c-2cf062d4126b
  modified: 2026-08-07T04:42:50.947Z
---

To verify a behavior-preserving DsonParser refactor (e.g. splitting a `ParseFromJson` into helpers), the Director gates it on a byte-for-byte before/after diff of the observable C-ABI output — not just a clean build.

1. **Harness golden.** Run `x64/Release/DsonTest2.exe` over every `DsonTest2/TestFiles/` asset (pass each as `argv[1]`) plus the no-arg battery and the 8 `--*-regression` modes; capture stdout (feed empty stdin to release the final `cin.get()` — see [[feedback-harness-hang-on-exit]]). Output is deterministic (indexed accessors, sorted unknown-key trails). Capture from clean `main` before handoff, re-capture after, require an identical SHA manifest.
2. **P/Invoke probe for the gap.** The harness does NOT dump every surface: **`polyline_list` has zero coverage**, and vertices/polylist appear only as *counts* in the smoke dump (whose default target `Genesis3.duf` has 0 geometries). Probe those accessors (`GetPolyline*`, `GetPolylistFace*`, graft/rigidity/geometry-channel) via `Add-Type` C# P/Invoke against `DsonParser.dll`, fold values into a per-geometry FNV hash, diff before-DLL vs after-DLL. Note there is NO per-vertex-value C-ABI accessor, so vertex *values* aren't observable (count suffices).

**Gotcha:** a P/Invoke call pins `DsonParser.dll` in that PowerShell process, so a same-process `msbuild` relink then fails `LNK1104`. Build the before-DLL by `git stash` of the change → rebuild → probe, then `git stash pop` → rebuild → probe, running each probe in a **separate** process.

Builds on [[director-parser-verify-via-pinvoke]]; complements [[no-asset-specific-test-oracles]]. Reusable capture/probe scripts were built in scratchpad for the 2026-08 Geometry/Node/Modifier ParseFromJson refactor; promote to `tools/` if this recurs.
