---
name: ue-engine-source-location
description: "DsonHost's UE engine is a source build at D:/UE_5.4 — readable on disk for engine-API diagnostics"
metadata: 
  node_type: memory
  type: reference
  originSessionId: aa167fb7-51f5-425e-856a-a4a6f58b0738
---

The DsonHost UE project (`D:/Unreal Projects/DsonHost`) is bound to a **source build of UE 5.4 at `D:/UE_5.4`** (registered by GUID in `HKCU\Software\Epic Games\Unreal Engine\Builds`, not under `C:/Program Files/Epic Games`). Launcher builds also exist at `G:\UE_5.1`, `G:\UE_5.3`, `G:\UE_5.4`.

Because the engine source is on disk, confirm UE-API behavior by reading it directly (e.g. `D:/UE_5.4/Engine/Source/Runtime/...`) instead of guessing. Example that paid off: `FTiffImageWrapper` (`ImageWrapper/Private/Formats/TiffImageWrapper.cpp`) only emits **BGRA8** for 8-bit images, so a hand-rolled `IImageWrapper` decode must request `ERGBFormat::BGRA`, not RGBA, or TIFF inputs fail while jpg/png still work — that's what broke the DsonToUnreal IrayUber bump-bake on DAZ's LZW TIFF normals. Importer side is UE 5.4.4 / C++20; cf. [[ue-consumer-cpp14-constraint]] (the DsonParser side).
