---
name: director-parser-verify-via-pinvoke
description: DsonParser Director can runtime-verify C-ABI accessors headlessly via a PowerShell Add-Type P/Invoke harness that dynamic-loads DsonParser.dll and asserts against an on-disk proof asset — not only build + code-review
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 7401096a-bafa-4f45-9cc2-20962c508900
  modified: 2026-08-03T03:26:14.895Z
---

For a DsonParser accessor FR, Director verification is **not limited to build + code-review**.
When a proof asset is on disk, write a scratchpad PowerShell harness that `Add-Type`s a C#
`[DllImport(..., CallingConvention.Cdecl)]` wrapper over the new exports (plus `DsonDocument_Create`
/ `LoadFromFile` / `GetGeometryCount` / `Destroy` / `DsonParser_GetLastError`), loads the asset
through the built `x64\Release\DsonParser.dll`, and asserts the parsed values against known oracle
numbers. No source edit, no `.lib` link — it dynamic-loads, matching the consumer's real model
[[dsonparser-consumer-dynamic-load-only]].

**Why:** this delivers true runtime *correctness* of the parser accessors headlessly, well beyond
"compiles clean." Confirmed 2026-08-03 on the 2.20.0 `GetPolyline*` FR: an 18-check harness against
`DsonTest2/TestFiles/EponaWavyHair.json` (213 MB plain JSON) pinned count/segment_count, the
polylist-vs-polyline strand discriminator, offset-table partitioning, R1 sentinels, and the
exhaustive-partition identity — all through the DLL.

**How to apply:** rebuild first (a live harness process holds the DLL open → `LNK1104` on a later
rebuild); run the harness in a fresh shell (each PowerShell call is a new process, so `Add-Type`
never collides and the DLL handle releases on exit). Marshal `const char*` returns as `IntPtr` +
`PtrToStringAnsi`. If no proof asset is available, fall back to build + review and say so
[[no-silent-fails]]. Parser-seat complement to [[build-and-read-prefs]]; distinct from
[[director-runtime-verify-via-user-import]], which is the *DsonToUnreal* seat's UE-editor-import
limit and still holds there (a UE import is not a parser accessor read).
