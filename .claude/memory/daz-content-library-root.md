---
name: daz-content-library-root
description: DAZ source assets for DsonToUnreal imports resolve under multiple plain-JSON content roots on this machine — D:/Daz_content/ and D:/daz_fast/Gen8F-3rparty/ (third-party G9 anatomy, e.g. Golden Palace); directly readable, no decompress
metadata: 
  node_type: memory
  type: reference
  originSessionId: be16e2b5-4b5c-4ad1-b9a7-9bb1466b27c0
---

The DsonToUnreal importer resolves DAZ content-relative paths under **`D:/Daz_content/`**
on this machine (e.g. `data/Daz 3D/Genesis 9/...` → `D:/Daz_content/data/Daz 3D/Genesis 9/...`).
The import log reports 12 content roots, but G9 base + the HID characters (Nancy) resolve here.
**Third-party G9 anatomy** (Golden Palace geograft + geoshells, Breastacular, etc.) resolves under
**`D:/daz_fast/Gen8F-3rparty/`** — e.g. `People/Genesis 9/Anatomy/Golden Palace/2a-Golden Palace Smart_Vanilla.duf`
and `data/Meipex/GoldenPalace_Genitalia_G9/...`. These are **already-decompressed plain JSON** too
(magic byte `7B`=`{`; `1F8B`=gzip). So the "gzip decompress is AV-blocked" caveat in
[[dson-gzip-decompress-blocked]] does NOT apply here — the machine's content roots are decompressed
on disk; **check the magic byte and read the file directly before assuming a block or asking for an upload.**

**How to find it again / read source assets:** the importer log
`D:/Unreal Projects/DsonHost/Saved/Logs/DsonHost.log` records resolved absolute paths,
e.g. `DsonValidator: companion resolved — ... matPreset='D:/Daz_content/...'` and
`GeomDSF: D:/Daz_content/...`. Enabling `bDumpMaterialDiagnostics` adds a
`=== DSON Material Diagnostic ===` dump per material (channel id/type, `value=`,
`imageUrl=`/`texturePath=`, `[import] ... OK`) — the ground-truth record of what the
importer actually saw.

`.duf`/`.dsf` here are **plain JSON** (not gzipped), so readable directly. A curated
test-preset subset is also bundled at `E:/Work/Code/DsonTest2/DsonTest2/TestFiles/`
(`G9.duf`, `Laura9.duf`, `HID_Nancy_9.duf`, `Genesis3.duf`, `Victoria7HD.duf`) — but
those are top-level character/figure presets only; G9 *companion* MAT presets
(eyes/mouth/eyelashes/tear) live under `D:/Daz_content/` and are not in the repo.

Useful when the Director needs to trace resolved material channels from a real source
asset rather than guessing. Related: [[ask-for-files-before-diagnostics]],
[[ue-engine-source-location]].
