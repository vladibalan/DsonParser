# DsonParser — Version Roadmap

## v1 — Complete (Current State)

### Summary

For a code-oriented parser map, read `docs/dson-parsing-overview.md` first.
This roadmap tracks capability status, audit history, and planned future work.

Full C++ DSON/DSF parser for DAZ Studio Genesis 9 figures, exposing all data
required to build a UE5 `USkeletalMesh` through a stable `extern "C"` API.
Verified against real Genesis 9 files (`Genesis9.dsf`, `G9.duf`, `test.dsf`)
across 4 audit passes with zero remaining gaps.

### Files
| File | Responsibility |
|---|---|
| `DsonDataTypes.h/.cpp` | Primitive type wrappers: Bool, Int, Float, String, Vector2/3, Color, IntArray, FloatArray, IndexedArray\<T\>, IndexedVector3Array |
| `DsonTypes.h/.cpp` | Domain structs + parse logic: AssetInfo, Node, NodeGeometryRef, Geometry, MaterialChannel, Material, SkinJoint, SkinBinding, Modifier, Image, UVSet, Scene, DsonDocument |
| `DsonHelpers.h` | JsonHelper static utilities (GetString, GetDouble, GetArray, GetObject, GetDoubleOrDefault) |
| `DsonParserAPI.h/.cpp` | Flat C API (`extern "C"`) with opaque `DsonDocumentHandle`; per-vertex skin cache; morph index cache |

### C API — 85 exported functions covering:

**Geometry (A)**
- Vertex positions (X/Y/Z per vertex)
- Polylist face data with variable-stride offset array
- Both leading face ints: face-group index `[0]`, material-group index `[1]`
- Vertex indices per face
- `polygon_groups` and `polygon_material_groups` name arrays
- `default_uv_set_id` per geometry

**Skeleton / Nodes (B)**
- Full `node_library`: id, name, type, parent
- Per-bone: `center_point`, `end_point`, `orientation`, `rotation_order` (default `"YXZ"`)
- Per-node transforms: `translation`, `rotation`, `scale` (X/Y/Z each)
- `general_scale` uniform scalar (default `1.0`)
- `unit_scale` from `asset_info` (default `1.0`)
- `Node::geometries` (`NodeGeometryRef`: id + url) for scene nodes

**Skin Binding (C)**
- `node_weights` primary + `local_weights` fallback
- Per-vertex influence cache (per-bone → per-vertex inversion)
- `GetVertexInfluenceCount(handle, modifierIndex, vertexIndex, maxInfluences)`
- `GetVertexBoneInfluence` — pre-cap normalized weights
- `GetVertexBoneInfluenceCapped` — renormalized over top-M influences (correct for UE5 `FSoftSkinVertex`)

**UV Sets (D)**
- UV coordinates (U/V per UV vertex)
- `polygon_vertex_indices` face-varying mapping
- Multiple UV channels

**Materials (E)**
- 8 PBR channels (channelId 0–7):
  - 0 diffuse, 1 specular, 2 roughness, 3 normal (tangent-space),
    4 opacity, 5 subsurface, 6 emission, 7 bump (grayscale height)
- Per channel: `value`, `color` (RGB), `has_color`, `image_url`, `texture_path`
- `Image::map` handles plain string, `{"url":"..."}` object, and base-layer
  URL/file from layered-image map array forms
- Post-parse image linkage pass resolves `image_url` → `texture_path`, including
  percent-decoded fragment ids
- Material `groups` array for scene material → surface zone mapping
- `geometry_id` and `uv_set_id` per material

**Morph Targets (F)**
- Sparse vertex position deltas `[vertex_idx, dx, dy, dz]`
- Normal deltas (same format)
- Morph name + `channel_label` (with `name` fallback)
- `GetMorphGeometryId` — geometry id from `parent` URL fragment
- O(1) morph access via lazy `morphIndexCache`

### Known Limitations (v1)
- **Formulas not parsed** — corrective morphs (JCMs, FHMs) and
  formula-driven morph chains are suppressed in `knownKeys` but not stored
  or exposed. Characters will import correctly but pose-driven corrective
  shapes will not fire automatically.
- **Windows only** — library validated and used on Win64. No Mac/Linux
  build configuration exists yet.
- **No weight normalization at parse time** — raw weights are stored as-is;
  normalization and capping happen at query time via `GetVertexBoneInfluenceCapped`.
  This is by design (lossless parser).

---

## v2 — Planned

### Primary Goal
Add formula parsing so that pose-driven corrective morphs (JCMs, FHMs)
and morph channel driver chains fire correctly when the character is posed
in UE5.

### Background
DAZ Studio uses a formula system to drive corrective shapes. A formula
defines a mathematical expression (typically a multiplication) that maps
an input channel value (e.g. a bone's rotation angle) to an output channel
value (e.g. a morph's strength). Genesis 9 characters ship with hundreds
of these — the `test.dsf` sample alone contains 468 formula entries on a
single morph modifier.

Example formula from `test.dsf`:
```json
{
  "output": "head:/data/.../Genesis8Female.dsf#head?center_point/x",
  "operations": [
    { "op": "push", "url": "Genesis8Female:#HD%20Bodacious%20Brow%2001?value" },
    { "op": "push", "val": 0.0002010307 },
    { "op": "mult" }
  ]
}
```
The formula is a stack-based RPN expression. Operations include:
`push` (push a constant or channel value), `mult`, `div`, `add`, `sub`,
`pow`, `spline_tcb` (tension-continuity-bias interpolation).

### Work Required

#### Parser Side (DsonParser v2)

**1. Formula struct**
```cpp
struct FormulaOperation {
    std::string op;    // "push", "mult", "div", "add", "sub", "pow", "spline_tcb"
    double val = 0.0;  // for op == "push" with a constant
    std::string url;   // for op == "push" with a channel reference
};

struct Formula {
    std::string output;                    // target channel URL
    std::vector<FormulaOperation> operations;
    std::string stage;                     // "sum" or "mult" (optional)
};
```

**2. Add `std::vector<Formula> formulas` to `Modifier` struct**
- In `DsonTypes.h`: add `formulas` field to `Modifier`
- In `Modifier::ParseFromJson` (`DsonTypes.cpp`): remove `"formulas"` from
  `knownKeys` suppression; parse the array of formula objects; for each
  formula read `output` string and `operations` array; for each operation
  read `op`, and conditionally `val` (double) or `url` (string)

**3. C API accessors**
```
// Per modifier (morph):
int         DsonDocument_GetMorphFormulaCount(handle, morphIndex)
const char* DsonDocument_GetMorphFormulaOutput(handle, morphIndex, formulaIndex)
const char* DsonDocument_GetMorphFormulaStage(handle, morphIndex, formulaIndex)

// Per formula operation:
int         DsonDocument_GetMorphFormulaOperationCount(handle, morphIndex, formulaIndex)
const char* DsonDocument_GetMorphFormulaOperationOp(handle, morphIndex, formulaIndex, opIndex)
double      DsonDocument_GetMorphFormulaOperationVal(handle, morphIndex, formulaIndex, opIndex)
const char* DsonDocument_GetMorphFormulaOperationUrl(handle, morphIndex, formulaIndex, opIndex)
```

#### UE5 Plugin Side (DsonImporter v2)

**4. Formula evaluator**
A runtime evaluator that:
- Parses channel URLs to identify source bone rotation channels and
  target morph value channels
- Evaluates the RPN stack expression for a given bone pose
- Drives `UMorphTarget` weights at runtime via the Animation Blueprint
  or a custom `UAnimInstance`

**5. Integration point**
- A custom `UAnimInstance` subclass or `AnimNotify` that evaluates all
  formula chains each frame when the skeleton pose changes
- Or: bake the corrective shapes into pose-driven `UPoseAsset` entries
  (simpler but less flexible)

### Audit Prompt for v2
When ready to begin v2, rerun the audit prompt from pass 4 with one
additional section added to Step 2:

```
G. Formulas
- Is the formulas array parsed on Modifier?
- Are FormulaOperation op, val, url fields all captured?
- Are formula output and stage fields captured?
- Is the full set of operation types handled:
  push (constant), push (url), mult, div, add, sub, pow, spline_tcb?
- Are all formula C API accessors exposed?
```

---

## Audit History

| Pass | Gaps Found | Gaps Resolved | Result |
|---|---|---|---|
| Pass 1 | 18 | 18 | Partial |
| Pass 2 | 8 | 8 | Partial |
| Pass 3 | 6 | 6 | Partial |
| Pass 4 | 0 | — | ✅ v1 Complete |
