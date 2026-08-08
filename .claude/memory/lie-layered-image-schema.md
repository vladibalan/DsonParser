---
name: lie-layered-image-schema
description: "DAZ LIE (layered image) map-array schema + what the parser keeps vs drops — needed for any per-layer material work"
metadata: 
  node_type: memory
  type: project
  originSessionId: f7aed16b-23bb-4d2f-93e1-c75a5f460b64
---

DAZ Layered Image Editor (LIE) data lives in each `image_library` entry's `map`
array. The parser **retains every layer's `url` + `label` + full per-layer compositing
metadata on `Image::layers`** — so the per-layer stack is fully recoverable from the
repo (compositing fields added in 1.4.0; path+label since 1.3.0/baseline). Schema confirmed by inspecting
`D:/Daz_content/People/Genesis 9/Characters/HID Nancy 9.duf` (2026-06-06) and the
Genesis 9 eyes MAT preset (2026-06-09).

- **`map` is ALWAYS an array**, even for plain single textures (wrapped in a
  1-element array). So "map is an array" is NOT a LIE test. A genuine multi-layer
  LIE has **≥ 2 elements** (element[0] = base, [1..N-1] = overlays). Sample
  distribution (HID Nancy): 27 images × 1 layer, 2 images × 4 layers.
- **Per-layer element keys:** `url` (content-relative path) and `label` (human
  name) are the import-relevant ones. There is **no `file` key** — so a per-layer
  "image url" and "resolved path" are the same string. Per-layer compositing keys
  (now parsed & exposed, **1.4.0**): `operation` (blend_source_over/_multiply/…),
  `transparency` (opacity, 1=opaque), `active`, `invert`, `color`, `rotation`,
  `x/yoffset`, `x/yscale`, `x/ymirror`.
- **A no-`url` (color-only) base layer is SKIPPED on parse**, so `Image::layers` can
  be shorter than the raw `map`: G9 "Eye Color-3" map = [Base(no url) / Sclera / Iris]
  → **2** stored layers (Sclera, Iris). Good for a source-over composite (textured
  layers only), but stored count ≠ raw map length.
- **Flat-vs-LIE base-path collision:** a flat image and a LIE can share the same
  `map[0].url`. Channels reference a LIE by `#id`, a flat file by bare path. ⇒
  attach/match by **identity (id/url)**, never by `map_file` path, or a path
  reference wrongly inherits the LIE's overlays.
- **Verification anchor (HID Nancy 9):** diffuse → 4-layer LIE; Fingernails /
  Mouth Cavity diffuse → plain. Mouth Cavity references flat head_base.jpg by path.

Exposed two ways (both additive; parser stays faithful — exposes raw, performs no
compositing): `GetSceneMaterialChannelLayer{Count,TexturePath,Label}` per
scene-material-channel (Count = `≥2 ? N : 0`, attaches only on identity match), and
**1.3.0** `GetImageLayer{Count,TexturePath,Label}` per image index (Count = faithful
stored size: 1 plain / N LIE / 0 none) for an image referenced OUTSIDE an inline
channel — e.g. a `scene.animations` LIE binding like the G9 eyes. **1.4.0** adds the 14
per-layer compositing accessors to BOTH families —
`…Layer{BlendMode,Opacity,Active,Invert,ColorR,ColorG,ColorB,Rotation,ScaleX,ScaleY,OffsetX,OffsetY,MirrorX,MirrorY}`
(R1 sentinels ""/false/0.0, scales 1.0; Opacity = raw `transparency`). No-`url` base
layers stay excluded, so their compositing fields remain unreachable. Nancy's head
diffuse/SSS reach this only via the per-IMAGE family (channels ref by path, not `#id`).
See [[agnostic-plugin-architecture]] and [[dson-animations-keyed-material-overrides]].
