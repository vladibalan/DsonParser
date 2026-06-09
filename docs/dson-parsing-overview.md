# DSON Parsing Overview

This document is the first stop for audits and LLM-assisted questions about how
this library parses DAZ Studio DSON/DSF/DUF data. It summarizes the code paths,
data ownership, supported sections, and known boundaries so readers do not need
to start by reading the full implementation files.

## File Map

| File | Responsibility |
| --- | --- |
| `DsonParser/DsonDataTypes.h/.cpp` | Primitive JSON-backed wrappers such as `String`, `Vector3`, `FloatArray`, `IntArray`, `Url`, and sparse `IndexedArray<T>` types. |
| `DsonParser/DsonTypes.h` | Typed DSON model: asset metadata, nodes, geometry, materials, skin bindings, modifiers, images, UV sets, scene instances, and root document. |
| `DsonParser/DsonTypes.cpp` | Main parser implementation. Converts RapidJSON objects into the `Dson::*` model and performs limited post-parse image reference linkage. |
| `DsonParser/DsonHelpers.h/.cpp` | Safe RapidJSON helper API (`JsonHelper`) used by the parser. Declarations in `.h`, implementations in `.cpp`. |
| `DsonParser/DsonInflate.h/.cpp` | Internal, dependency-free gzip/DEFLATE support used by the loader before JSON parsing. Verifies gzip CRC32 and ISIZE. |
| `DsonParser/DsonParserAPI.h/.cpp` | Flat `extern "C"` API for DLL consumers. Owns opaque handles, parser-owned string returns, bounds-checked accessors, and lazy query caches. |
| `DsonParser/DsonParserVersion.h` | Canonical library version macros (`DSONPARSER_VERSION_*`); published with and included by `DsonParserAPI.h`. Single source of truth for `DsonParser_GetVersion()` and the `CHANGELOG.md` baseline. |
| `DsonParser_Roadmap.md` | Current capability summary, audit history, known v1 limitations, and planned v2 formula parsing work. |

The published surface is `DsonParserAPI.h` (the flat C ABI). The C++ model
headers (`DsonDataTypes.h`, `DsonTypes.h`, `DsonHelpers.h`) are internal
implementation detail — consumers never include them, and the RapidJSON they
reference never reaches a consumer.

## Parsing Pipeline

1. Callers create an opaque document handle with `DsonDocument_Create`.
2. `DsonDocument_LoadFromFile`, `DsonDocument_LoadFromString`, or
   `DsonDocument_LoadFromBuffer` parses JSON syntax with RapidJSON.
3. `DsonDocument::ParseFromJson` dispatches recognized top-level DSON sections
   to their domain parsers.
4. Each domain parser fills typed C++ structs and records unrecognized keys in
   a per-context unknown-key map.
5. A post-parse pass resolves material channel image references against
   `image_library` entries and fills `MaterialChannel::texture_path`.
6. Consumers query the result through the C API. Some expensive views, such as
   morph filtering and per-vertex skin influence inversion, are built lazily.

The parser is intentionally permissive. Missing optional fields keep defaults,
malformed entries inside arrays are usually skipped, and broad semantic
validation is left to the importer or audit layer.

The parser is also **faithful and non-interpretive**: it never overwrites or
fills one section's data from another (e.g. it does not apply `scene.animations`
onto `scene.materials`, even when a channel is empty). Merging overrides,
evaluating formulas, and collapsing instances onto definitions are left to the
consumer, which reads the faithful sections and decides. The only resolution it
performs is intra-material image linkage (`image_url → texture_path`). This is
rule R6.4 in [`code-review-rules.md`](code-review-rules.md).

### Compressed Input (gzip)

DAZ files may keep `.duf`/`.dsf` extensions while storing a single gzip stream
around the JSON document. The loader detects gzip by magic bytes (`1F 8B`) in
`DsonDocument::LoadFromBuffer`; gzip inputs are inflated by the internal
`DsonInflate` module before RapidJSON parsing, while plain JSON buffers parse
unchanged. The gzip trailer CRC32 and ISIZE are mandatory checks, so corrupt or
mis-decoded data fails with a gzip-specific error rather than surfacing later as
a misleading JSON parse error.

`DsonDocument_LoadFromBuffer` is the public C ABI entry point for callers that
already hold bytes in memory, including gzip bytes that may contain NULs.
`DsonDocument_LoadFromFile` reads the whole file and delegates to the same
length-aware path. `DsonDocument_LoadFromString` remains for null-terminated
plain JSON strings and is not suitable for binary gzip data.

Boundaries: gzip support is single-member gzip only. ZIP archives (`PK` files),
central directories, and concatenated multi-member gzip streams are outside the
current loader scope.

## Supported Top-Level Sections

`asset_info`
: Parsed into `AssetInfo`, including id, type, contributor metadata, revision,
  modified date, and `unit_scale`.

`scene`
: Parsed separately from libraries. Scene arrays contain placed instances and
  references to library entries. The parser currently reads scene nodes,
  modifiers, materials, and UV sets, plus the post-load addon manifest in
  `scene.extra` (the DAZ "Character Addon Loader" `PostLoadAddons`; see the Scene
  Post-Load Addons section below), and the `scene.animations` keyframe channels
  (see the Scene Animations section below — stored faithfully, never applied onto
  `scene.materials`). Presentation and current camera are recognized but not stored
  as typed fields.

`node_library`
: Parsed into library `Node` definitions. Captures id, name, label, type,
  parent, URL, translation, rotation, scale, general scale, center/end points,
  orientation, and rotation order.

`geometry_library`
: Parsed into `Geometry`. Captures vertex positions, polygon/polylist data,
  face offsets, polygon groups, material groups, vertex/polygon counts, and
  default UV-set reference.

`material_library`
: Parsed into `Material`. Captures source-order material channels, shader type
  from `extra[]`, surface groups, geometry reference, UV-set reference, scalar
  values, colors, raw image URLs, and resolved texture paths.

`modifier_library`
: Parsed into `Modifier`. Captures morph deltas, normal deltas, stored channel
  dial metadata/value bounds, skin binding payloads, and formulas. Formula `output`, `stage`, and
  the source-order RPN `operations` (`op` plus `val`/`url`) are stored and
  exposed; the parser does not evaluate them or follow their channel references.

`image_library`
: Parsed into `Image`. Captures id, name, URL, map file/path, and the `map_size` pixel dimensions
  (`map_width` / `map_height`, exposed via `DsonDocument_GetImageMapWidth` /
  `GetImageMapHeight`). For
  DAZ layered-image (LIE) `map` arrays it retains every layer (url + label) on
  `Image::layers`, not just the base; `map_file` stays the base layer (`map[0]`)
  so existing single-texture resolution is unchanged. Those `Image::layers` are
  exposed by image index via
  `DsonDocument_GetImageLayer{Count,TexturePath,Label}` (see Layered Image (LIE)
  Channels below).

`uv_set_library`
: Parsed into `UVSet`. Captures UV coordinates, vertex count, and sparse
  polygon-vertex UV overrides.

## Scene vs Library Data

DSON separates reusable definitions from scene instances. For example,
`node_library` contains node definitions, while `scene.nodes` contains placed
instances that reference definitions with URL fields and may carry instance
labels or geometry references.

The C API keeps these separate:

- `DsonDocument_GetNode*` reads `node_library`.
- `DsonDocument_GetSceneNode*` reads `scene.nodes`.
- `DsonDocument_GetMaterial*` reads `material_library`.
- `DsonDocument_GetSceneMaterial*` reads `scene.materials`.

This distinction matters when an importer needs the base asset definition versus
the configured scene instance.

### Scene Post-Load Addons

A DAZ "Character Addon Loader" entry in `scene.extra` lists companion conforming
figures — Genesis 9 eyes, mouth, eyelashes, tear, and a character-dependent
eyebrows figure — that a `character` preset pulls in but does **not** list in
`scene.nodes`. The parser models this manifest
(`scene.extra[].settings.PostLoadAddons`) on `Scene::post_load_addons` and exposes
it as a flat, document-ordered list:

- `DsonDocument_GetScenePostLoadAddonCount`
- `DsonDocument_GetScenePostLoadAddonSlot` — the DAZ slot key, e.g.
  `Follower/Attachment/Head/Face/Eyes`.
- `DsonDocument_GetScenePostLoadAddonAssetName`
- `DsonDocument_GetScenePostLoadAddonAssetFile` — content-relative loader `.duf`.
- `DsonDocument_GetScenePostLoadAddonMatPreset` — content-relative MAT preset
  `.duf`, or `""` when the slot has none.

The index flattens slots across every `PostLoadAddons` map in `scene.extra`, in
document order; a slot is kept only if it has a non-empty `AssetFile`. The parser
surfaces the referenced paths only — resolving them against content roots and
loading the referenced `.duf` files remains an importer responsibility (consistent
with the no-recursive-load boundary below). The per-addon `SelectAddon` flag is
intentionally not exposed: observed uniformly `false` across sample characters, it
is a UI hint, not a load gate.

### Scene Animations

DAZ `preset_hierarchical_material` presets (e.g. a Genesis 9 companion MAT preset)
often declare `scene.materials` channels as bare `{id,type}` placeholders and park
the real channel values and `image_file` paths in `scene.animations` as `{url, keys}`
keyframes, where **key 0 is initialization data** (not runtime animation). The parser
stores each entry faithfully on `Scene::animations` — the verbatim `url` property
pointer plus the first key's typed value — and exposes it:

- `DsonDocument_GetSceneAnimationCount`
- `DsonDocument_GetSceneAnimationUrl` — the raw DSON pointer, e.g.
  `Genesis9Mouth#materials/Mouth:?diffuse/image_file`.
- `DsonDocument_GetSceneAnimationValueKind` — first-key value kind:
  `0` null · `1` number · `2` bool · `3` string · `4` color (`-1` invalid).
- `DsonDocument_GetSceneAnimationFloat` / `…Bool` / `…String` / `…ColorR` /
  `…ColorG` / `…ColorB` — the value, read per kind.

Per **R6.4** the parser does **not** apply these onto `scene.materials`: it does not
resolve the pointer, match the target channel, or fill an empty channel, and it leaves
the `image_file` string as the verbatim DSON path (not resolved against
`image_library`). The consumer reads both sections and decides whether key 0 overrides
its material. Every entry is exposed (including `image_modification`/tiling rows);
v1 reads the first key only, so multi-key keyframes are not modeled.

## Geometry And Faces

Geometry vertices are stored as a flat XYZ array in `Geometry::vertices`.
DSON commonly represents vertices as `{ "count": N, "values": [[x,y,z], ...] }`,
and the parser also accepts legacy flat arrays where practical.

Polylist faces are flattened into `Geometry::polylist.values`, while
`polylist_face_offsets` records where each face begins. Each face preserves the
DSON leading group/material indices before the vertex indices. This lets callers
reconstruct variable-length faces and recover both polygon group and material
group assignments.

## UV Sets

UV coordinates are stored as flat U/V pairs. DAZ DSF files commonly encode
`polygon_vertex_indices` as sparse triplets:

```text
[face, corner, uv_index]
```

The implied default mapping is identity: `uv_index == vertex_index`. The sparse
triplets describe only face corners whose UV index differs from that default.
The C API exposes these sparse overrides so consumers can expand them into their
own per-corner UV buffers.

## Materials And Images

Material channels are preserved in source order as pairs:

```text
DAZ channel id -> MaterialChannel
```

The parser does not collapse channels into a fixed engine shader layout. It
captures raw DAZ channel ids and types, scalar values, RGB colors, raw image
references, and resolved texture paths. Image resolution happens after
`image_library` is parsed by matching a channel image reference against image id
(including percent-decoded fragment ids), image URL, or map file.

Scene materials and library materials use the same channel representation, but
scene materials may include instance-level surface groups or channel values.

### Layered Image (LIE) Channels

A DAZ Layered Image Editor channel stores its layers as a `map` array on the
`image_library` entry: element `[0]` is the base, elements `1..N-1` are overlays
(makeup, brows, etc.). DAZ wraps even plain single textures in a one-element `map`
array, so a true layered channel is one with **two or more** layers. The parser
retains all layers on `Image::layers` and, during the post-parse linkage, copies
them onto the matched material channel only when the channel references the image
by **identity** (id/url) and the image has **≥ 2** layers — a bare-path reference
resolves to the flat base only. The C API exposes the per-channel layer surface
for scene materials:

- `DsonDocument_GetSceneMaterialChannelLayerCount` — number of LIE layers; `0` for a
  plain single-texture channel, `N ≥ 2` for a layered one.
- `DsonDocument_GetSceneMaterialChannelLayerTexturePath` — layer path; layer `0`
  equals the channel's existing `TexturePath`.
- `DsonDocument_GetSceneMaterialChannelLayerLabel` — the LIE layer label.

The same layer stack is also addressable **by image index** (the `GetImageId` index
space), for an image referenced from outside an inline material channel — e.g. a
`scene.animations` `diffuse/image` binding to a base-figure LIE such as the Genesis 9
eyes, which no channel references inline:

- `DsonDocument_GetImageLayerCount` — textured-layer count of `Image::layers`. Unlike
  the per-channel count, this is the **faithful stack size**: `1` for a plain single
  texture, `N` for a LIE, `0` for a non-array or absent `map`. A color-only base layer
  (a `map` element with no `url`) is not counted, so the Genesis 9 "Eye Color-3" stack
  (Base[no url] / Sclera / Iris) reports `2`.
- `DsonDocument_GetImageLayerTexturePath` / `…Label` — per-layer path/label by
  `(imageIndex, layerIdx)`; layer `0` is the first textured map element.

Resolving the `scene.animations` `"#fragment"` reference to an image index
(percent-decode, then match against `GetImageId`) and compositing the layers remain
consumer responsibilities.

Per-layer compositing metadata (blend operation, opacity, color tint, transforms,
active flag) is present in the DSON but intentionally not modeled in this surface;
it is left for a future compositing consumer.

## Skin Binding

DSON skin data is parsed in its native joint-to-vertex layout. Each `SkinJoint`
stores the referenced node plus parallel arrays of vertex indices and weights.

The raw skin API exposes that parsed layout directly:

- `DsonDocument_GetSkinJointCount`
- `DsonDocument_GetSkinJointNodeId`
- `DsonDocument_GetSkinJointWeightCount`
- `DsonDocument_GetSkinJointWeightVertexIndex`
- `DsonDocument_GetSkinJointWeight`

For engine import, `DsonParserAPI.cpp` lazily builds a vertex-to-bone cache for
one skin modifier at a time. The processed influence API sorts influences by
descending weight and normalizes them per vertex. The capped influence API then
renormalizes only the top `maxInfluences` entries, which is useful for fixed-size
engine vertex formats.

## Morph Targets

Morph APIs expose modifiers with morph data. Real DAZ morph modifiers are
identified by a nested `morph` payload, while legacy flat files may use
`type == "morph"`. The public `morphIndex` is therefore a filtered index and is
not the same as the raw `modifier_library` index.

Morph position deltas and normal deltas are sparse. Each delta stores a source
vertex index and XYZ offset. Consumers should apply deltas only to listed
vertices.

`DsonDocument_GetMorphId` returns the modifier id in this filtered morph index
space. `DsonDocument_GetMorphGeometryId` extracts the geometry id fragment from
the morph modifier's parent URL.

## Formulas

DAZ formulas drive corrective and control morphs. The parser stores each
modifier's `formulas` array without evaluating it: per `Formula` it keeps the
`output` channel URL, the optional `stage` (`"sum"`/`"mult"`), and a vector of
`FormulaOperation` (`op`, plus `val` for a constant `push` or `url` for a
channel-reference `push`). Operation fields beyond `op`/`val`/`url` are not
modeled yet but are recorded in the unknown-key diagnostics rather than dropped.

Formulas attach to ordinary modifiers, not to the filtered morph list, because
the formula-bearing modifier is often a control with no `morph`/`deltas` block
(so it is not a "morph"). The C API therefore exposes formulas on both modifier
index spaces, not on `morphIndex`:

- `DsonDocument_GetModifierFormula*` — raw `modifier_library` index (JCM/FHM
  correctives and control-morph children in their own `.dsf` files).
- `DsonDocument_GetSceneModifierFormula*` — `scene.modifiers` index (the
  character control-morph top node carried inline in a `.duf`).

Each family exposes formula count/output/stage and per-operation
count/op/val/url, plus stored channel value/min/max/clamped accessors for the
modifier dial state. Evaluating the RPN, resolving `output`/`url` references, and
recursively loading the referenced `.dsf` files remain importer responsibilities.

## Unknown-Key Diagnostics

Each parser function has a known-key set for its current supported schema.
Unrecognized keys are recorded by context, such as `document`,
`geometry_library`, `material_library`, or `scene`.

The C API exposes this audit trail through:

- `DsonDocument_GetContextCount`
- `DsonDocument_GetContextName`
- `DsonDocument_GetUnknownKeyCount`
- `DsonDocument_GetUnknownKey`

This is intended to make future DSON coverage audits easier: a clean unknown-key
report means the current parser recognized every key in the tested files, not
that every recognized key is necessarily stored or semantically implemented.

## API Ownership And Return Conventions

The C API returns parser-owned `const char*` pointers. Callers should copy any
string they need to keep after clearing or destroying the document, and should
not assume scratch-string results survive unrelated API calls.

Invalid handles, invalid indexes, or missing data generally return empty strings,
zero, `false`, or `-1` depending on the existing function family. The API is
bounds-checked and does not throw exceptions across the C boundary.

## Current Boundaries

The parser deliberately does not handle:

- Formula evaluation, RPN solving, or following formula channel references
  (formulas are parsed and stored, but never evaluated).
- Pose-driven corrective morph chains.
- Recursive loading of external referenced DSF/DUF assets.
- Full DSON semantic validation.
- ZIP archive parsing and concatenated multi-member gzip streams.
- Cross-platform build validation beyond the current Windows DLL setup.

See `DsonParser_Roadmap.md` for the planned formula work and audit history.
