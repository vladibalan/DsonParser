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
| `DsonParser/DsonParserAPI.h/.cpp` | Flat `extern "C"` API for DLL consumers. Owns opaque handles, parser-owned string returns, bounds-checked accessors, and lazy query caches. |
| `DsonParser_Roadmap.md` | Current capability summary, audit history, known v1 limitations, and planned v2 formula parsing work. |

The published surface is `DsonParserAPI.h` (the flat C ABI). The C++ model
headers (`DsonDataTypes.h`, `DsonTypes.h`, `DsonHelpers.h`) are internal
implementation detail — consumers never include them, and the RapidJSON they
reference never reaches a consumer.

## Parsing Pipeline

1. Callers create an opaque document handle with `DsonDocument_Create`.
2. `DsonDocument_LoadFromFile` or `DsonDocument_LoadFromString` parses JSON
   syntax with RapidJSON.
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

## Supported Top-Level Sections

`asset_info`
: Parsed into `AssetInfo`, including id, type, contributor metadata, revision,
  modified date, and `unit_scale`.

`scene`
: Parsed separately from libraries. Scene arrays contain placed instances and
  references to library entries. The parser currently reads scene nodes,
  modifiers, materials, and UV sets. Presentation, animations, current camera,
  and extra are recognized but not stored as typed fields.

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
: Parsed into `Modifier`. Captures morph deltas, normal deltas, channel
  metadata, and skin binding payloads. Formula keys are recognized but not yet
  stored or evaluated.

`image_library`
: Parsed into `Image`. Captures id, name, URL, map file/path, and map size.

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
`image_library` is parsed by matching a channel image reference against image id,
image URL, or map file.

Scene materials and library materials use the same channel representation, but
scene materials may include instance-level surface groups or channel values.

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

Morph APIs expose only modifiers where `type == "morph"`. The public
`morphIndex` is therefore a filtered index and is not the same as the raw
`modifier_library` index.

Morph position deltas and normal deltas are sparse. Each delta stores a source
vertex index and XYZ offset. Consumers should apply deltas only to listed
vertices.

`DsonDocument_GetMorphGeometryId` extracts the geometry id fragment from the
morph modifier's parent URL.

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

The v1 parser deliberately does not handle:

- Formula parsing or formula evaluation.
- Pose-driven corrective morph chains.
- Recursive loading of external referenced DSF/DUF assets.
- Full DSON semantic validation.
- Cross-platform build validation beyond the current Windows DLL setup.

See `DsonParser_Roadmap.md` for the planned formula work and audit history.
