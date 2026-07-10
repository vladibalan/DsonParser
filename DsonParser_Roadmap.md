# DsonParser — Version Roadmap

## v1 — Complete (Current State)

### Summary

For a code-oriented parser map, read `docs/dson-parsing-overview.md` first.
This roadmap tracks capability status, audit history, and planned future work.

> **Versioning note.** The milestones below ("v1", "v2") are **capability
> epochs, not library versions.** The library's SemVer version is independent:
> additive capability ships as MINOR bumps within `1.x`, and only a breaking
> C-ABI change yields `2.0.0`. Per-release C-ABI change history lives in
> [`CHANGELOG.md`](CHANGELOG.md); the policy is in
> [`docs/versioning.md`](docs/versioning.md).

Full C++ DSON/DSF parser for DAZ Studio Genesis 9 figures, exposing all data
required to build a UE5 `USkeletalMesh` through a stable `extern "C"` API.
Verified against real Genesis 9 files (`Genesis9.dsf`, `G9.duf`, `test.dsf`)
across 4 audit passes with zero remaining gaps.

### Files
| File | Responsibility |
|---|---|
| `DsonDataTypes.h/.cpp` | Primitive type wrappers: Bool, Int, Float, String, Vector2/3, Color, IntArray, FloatArray, IndexedArray\<T\>, IndexedVector3Array |
| `DsonTypes.h/.cpp` | Domain structs + parse logic: AssetInfo, Node, NodeGeometryRef, Geometry, MaterialChannel, ImageLayer, Material, SkinJoint, SkinBinding, FormulaOperation, Formula, Modifier, Image, UVOverride, UVSet, Scene, DsonDocument |
| `DsonHelpers.h/.cpp` | `JsonHelper` safe-accessor utilities (`GetString`/`GetDouble`/`GetInt`/`GetBool`/`GetArray`/`GetObject` + `*OrDefault`, `HasMember`) |
| `DsonInflate.h/.cpp` | Internal dependency-free gzip/DEFLATE inflater run before JSON parsing; verifies gzip CRC32 + ISIZE (accepts a blank all-zero trailer on a clean inflate — DAZ-compat) |
| `DsonParserAPI.h/.cpp` | Flat C API (`extern "C"`) with opaque `DsonDocumentHandle`; per-vertex skin cache; morph index cache |

### C API — ~180 exported functions (v1 baseline; post-v1 additions below) covering:

**Geometry (A)**
- Vertex positions (X/Y/Z per vertex)
- Polylist face data with variable-stride offset array
- Both leading face ints: face-group index `[0]`, material-group index `[1]`
- Vertex indices per face
- `polygon_groups` and `polygon_material_groups` name arrays
- `default_uv_set_id` per geometry
- Source-order geometry `material_uvs` assignments through
  `GetGeometryMaterialUVAssignment{Count,MaterialGroup,UVSetName}` — verbatim
  per-surface material-group → UV-set names, with resolution left to the importer
  (2.12.0)
- Geograft signal: `GetGeometryIsGraft` — `true` for a populated `graft` (non-empty
  `vertex_pairs`); empty `"graft": {}` → `false` (1.5.0)
- Geograft weld correspondence: `GetGeometryGraft{VertexPair*,HiddenPoly*,BaseVertexCount,
  BasePolyCount}` — raw `vertex_pairs` (`[graft-local, base-figure]`), `hidden_polys`, and
  the declared base vertex/poly counts, file-local (2.9.0)
- Geometry rigidity: `GetGeometryHasRigidity` plus
  `GetGeometryRigidity{Weight*,Group*}` — complete raw `geometry.rigidity`
  sparse weights and groups, geometry-local and unevaluated (2.10.0)

**Skeleton / Nodes (B)**
- Full `node_library`: id, name, type, parent
- Per-bone: `center_point`, `end_point`, `orientation`, `rotation_order` (default `"YXZ"`)
- Per-node transforms: `translation`, `rotation`, `scale` (X/Y/Z each)
- `general_scale` uniform scalar (default `1.0`)
- `unit_scale` from `asset_info` (default `1.0`)
- `Node::geometries` (`NodeGeometryRef`: id + url) for scene nodes
- Per-node `presentation` content type + label: `GetNodePresentation{Type,Label}`
  (DAZ "Content Type" / display name; `""` if absent) (1.5.0)
- Per-node authored conform-target base figure: `GetNodePresentationPreferredBase` —
  `presentation.preferred_base` string, e.g. `"/Genesis 8/Female"` (the authored
  base-figure a follower/geograft targets; names the conform-target body, not the
  product's styling, so a Female-named product may declare a Male target); `""` if
  absent. Faithful passthrough (R6.4); no content-path resolution or catalog inference
  (2.15.0)
- Scene-instance parent and complete local translation/rotation/scale,
  `general_scale`, and `rotation_order` through `GetSceneNode*` (2.4.0)
- Raw authored scene-instance `center_point` and `orientation` XYZ values with
  X/Y/Z presence masks, plus `inherits_scale` value/presence (2.5.0). These are
  faithful `scene.nodes` reads; resolving and merging a referenced library
  definition remains importer-side.
- Authored scene-instance translation/rotation/scale XYZ presence masks plus
  `general_scale` and `rotation_order` presence (2.6.0), completing exact sparse
  transform override merging without default-value heuristics.
- Per-scene-node geometry-shell `material_uvs` assignments through
  `GetSceneNodeShellMaterialUVAssignment{Count,MaterialGroup,UVSetName}` — valid
  `[material-group, uv-set-name]` rows from exact `studio/node/shell` extras,
  retained verbatim and kept separate from geometry-library assignments, with
  external UV-set resolution left to the importer (2.13.0).

**Skin Binding (C)**
- `node_weights` primary + `local_weights` fallback
- Per-vertex influence cache (per-bone → per-vertex inversion)
- `GetVertexInfluenceCount(handle, modifierIndex, vertexIndex, maxInfluences)`
- `GetVertexBoneInfluence` — pre-cap normalized weights
- `GetVertexBoneInfluenceCapped` — renormalized over top-M influences (correct for UE5 `FSoftSkinVertex`)

**UV Sets (D)**
- UV coordinates (U/V per UV vertex)
- `polygon_vertex_indices` face-varying mapping, exposed as sparse `[face, corner, uv_index]` UV overrides (`GetUVOverride*`)
- Multiple UV channels
- Authored `id` / `name` / `label` per set (`GetUVSetId` / `GetUVSetName` / `GetUVSetLabel` — `label` = the DAZ display name, verbatim; since 2.14.0)

**Materials (E)**
- Source-order channels keyed by raw DAZ channel id — **no fixed engine-slot
  layout**. `GetMaterialChannelCount` is the parsed channel count and
  `GetMaterialChannelId` returns the DAZ id string (e.g. "diffuse",
  "Metallic Weight", "Normal Map"); the top-level `diffuse` block plus every
  `extra[].studio_material_channels.channels[]` entry are kept in file order, and
  consumers iterate or search by id
- Per channel: `value`, `color` (RGB), `has_color`, `image_url`, `texture_path`
- `Image::map` handles plain string, `{"url":"..."}` object, and layered-image
  (LIE) map arrays — all layers retained on `Image::layers` (url + label), with
  `map_file` kept as the base layer for back-compatible single-texture resolution
- Per scene-material-channel LIE layer surface (additive):
  `GetSceneMaterialChannelLayer{Count,TexturePath,Label}` — `Count` is `0` for a
  plain channel, `N ≥ 2` for a layered one; layer `0` == the channel's base
  `TexturePath`. Layers attach only on an identity (id/url) image match, never on a
  shared base-path match. Per-layer compositing metadata — blend `operation`, opacity,
  active/invert, color tint, and the 2D transform — is exposed on **both** layer
  surfaces via the matching `…Layer{BlendMode,Opacity,Active,Invert,ColorR,ColorG,ColorB,Rotation,ScaleX,ScaleY,OffsetX,OffsetY,MirrorX,MirrorY}`
  accessors (1.4.0; raw values, no compositing performed).
- Per-`image_library`-index LIE layer surface (additive, 1.3.0):
  `GetImageLayer{Count,TexturePath,Label}` — the same parsed `Image::layers`, addressed
  by the `GetImageId` index space, for an image referenced outside an inline channel
  (e.g. a `scene.animations` LIE binding such as the Genesis 9 eyes). `Count` is the
  faithful stack size (`1` plain / `N` LIE / `0` none), differing from the per-channel
  `0`-for-plain.
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

**Modifiers — non-morph (G)**
- Geometry-shell **push modifier** (`studio/modifier/push`, the "Mesh Offset" of a
  Geometry Shell wearable): identity + effective "Offset Distance" channel value,
  both read from the modifier's nested `extra[]` (not the modifier top level) —
  `GetModifierIsPush` / `GetModifierPushOffset` (offset prefers `current_value` →
  `value`, returned raw in the channel's cm; `0.0` is both the sentinel and a
  legitimate value, so gate on `IsPush`). Faithful/unevaluated; `extra[]` is
  otherwise unmodeled for modifiers (2.7.0)
- Modifier target: `GetModifierParent` returns each raw `modifier_library` item's
  complete authored `parent` URL verbatim, including its fragment. This lets a
  consumer associate multiple push modifiers with their individual geometry-shell
  targets; fragment matching and URL resolution remain consumer work (2.11.0).

### Known Limitations (v1)
- **Formulas not evaluated** — as shipped in v1, formula keys were suppressed in
  `knownKeys` and neither stored nor exposed. **Resolved at the parser level in
  v2** (formulas are now stored and exposed, including array-valued `val` operands
  such as `spline_tcb` TCB knots, surfaced raw via `...ValArray{Count,Element}` as
  of 2.1.0; see v2 Parser Side below). Evaluation and baking remain importer-side,
  so pose-driven correctives still require the importer integration before they fire
  automatically.
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

### Formula use cases — two distinct consumers (verified Jun 2026)

The v1 importer integration surfaced that formulas drive **two** different
features. The parser-side work below (store + expose per-morph formulas) serves
**both**; only the consumer differs.

**(a) Pose-driven correctives — JCM/FHM (runtime).**
Driver = a bone rotation/transform channel; output targets a morph `?value` or a
bone rigging property (e.g. `#head?center_point/x`). These must be evaluated each
frame from the live pose. This is the case the "UE5 Plugin Side" section below
(runtime evaluator / AnimInstance) addresses.

**(b) Character / control morphs (import-time) — NEW finding.**
A character is a **control morph** with a `channel` dial (0..1, e.g. label
"Laura") and **no `morph`/`deltas` of its own**. Its `formulas` drive *other
morphs'* `?value` channels. Those children are frequently **also** pure controls
(formulas, no deltas) — so the graph is a **multi-level tree** that bottoms out at
delta-bearing leaf morphs. The shape "Laura" is the weighted sum of those leaves
at the dialed value.

Worked example (Genesis 9, files under
`…/data/Daz 3D/Genesis 9/Base/Morphs/Daz 3D/Base Characters 9/`):

```
Laura for Genesis 9.duf  → scene.modifiers (3 entries):
  • body_bs_Navel_HD3   cv=1   → delta morph (72 deltas)     [imports today]
  • SkinBinding                → Genesis9.dsf#SkinBinding (not a morph)
  • Laura_figure_ctrl_Character cv=1 → CONTROL: 0 deltas, 2 formulas
        ├─ output …Laura_head_bs_Head.dsf#Laura_head_bs_Head?value  (× dial)
        │     └─ Laura_head_bs_Head.dsf : 0 deltas, 44 formulas → leaf morphs (deltas)
        └─ output …Laura_body_bs_body.dsf#Laura_body_bs_body?value  (× dial)
              └─ Laura_body_bs_body.dsf : 0 deltas, 9 formulas → leaf morphs (deltas)
```

Key structural facts (so this need not be re-derived from the DSON):
- A morph modifier has **no `type` field**; delta morphs carry a nested `morph`
  object (`morph.deltas` / `morph.normal_deltas`, values `[idx,dx,dy,dz]`),
  control morphs carry `formulas` + `channel` and **no** `morph` block.
- Formula `output` URL form: `Scheme:/path/File.dsf#ModifierId?property`
  (character case: `property == value`). A `push` url form referencing the
  current file is `Scheme:#ModifierId?value`.
- The `.duf` lists **only the top control** (with `channel.current_value` = the
  dial); the driven children appear **only** inside formulas, in **separate
  `.dsf` files**.

**Parser scope for (b):** the parser stays single-document and does NOT do
recursive external loading or evaluation. It only needs to **store and expose the
per-morph formulas** (the API below). Following `output` URLs to the child
`.dsf` files, recursing the tree, evaluating the RPN, and composing/baking the
result is **importer** work (the importer already loads external morph files).

### Work Required

> **Status (Jun 2026):** Parser Side (items 1–3) is **implemented** — formulas
> are parsed, stored on `Modifier`, and exposed through the C ABI. The exposed
> API differs from the original item-3 sketch (see item 3 for the as-built
> surface and rationale). UE5 Plugin Side (items 4–6) is still pending.
>
> **Next increment (item 3b, agreed Jun 2026):** planning the case-(b) importer
> consumer showed the formula *tree* is traversable but not *evaluable* with the
> current surface — the parser exposes no channel dial value, no clamp range, and
> no morph identity to map a formula `output` fragment to its leaf morph. Item 3b
> adds those stored-field accessors. It keeps the parser single-document and
> non-evaluating (every accessor is a pure read of an already-or-newly-stored
> field); the channel numeric fields are **not** parsed today and need a parse
> extension on `Modifier`.

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
- In `Modifier::ParseFromJson` (`DsonTypes.cpp`): **keep** `"formulas"` in
  `knownKeys` (a parsed key belongs there, or it shows up as a false-positive
  unknown) and parse the array of formula objects; for each formula read
  `output` string, `stage` string, and `operations` array; for each operation
  read `op`, and conditionally `val` (double) or `url` (string). `Formula` and
  `FormulaOperation` each get a `ParseFromJson` so they reuse `ParseObjectArray`,
  and each tracks unknown keys so unmodeled op fields surface in diagnostics
  instead of being dropped. Note: `spline_tcb` knots are NOT unknown keys — they
  ride on the known `val` key as a JSON array; prior to 2.1.0 the parser silently
  collapsed each knot array to `0.0`. As of 2.1.0, array-valued formula operands
  are retained verbatim and exposed via `...FormulaOperationValArray{Count,Element}`
  on both formula families (see CHANGELOG 2.1.0).

**3. C API accessors (as built)**

The original sketch keyed these on `morphIndex` (the filtered morph list). That
was changed during implementation: formulas frequently live on **control
modifiers that have no `morph`/`deltas` block**, so they are not in the filtered
morph list at all (case (b) above). Keying on `morphIndex` would have left every
character control morph unreachable. Instead the accessors are exposed on **both
modifier index spaces**, which together reach every stored formula (case (a)
JCMs included, via their raw library index):

```
// Per modifier, by raw modifier_library index:
int         DsonDocument_GetModifierFormulaCount(handle, modifierIndex)
const char* DsonDocument_GetModifierFormulaOutput(handle, modifierIndex, formulaIndex)
const char* DsonDocument_GetModifierFormulaStage(handle, modifierIndex, formulaIndex)
int         DsonDocument_GetModifierFormulaOperationCount(handle, modifierIndex, formulaIndex)
const char* DsonDocument_GetModifierFormulaOperationOp(handle, modifierIndex, formulaIndex, opIndex)
double      DsonDocument_GetModifierFormulaOperationVal(handle, modifierIndex, formulaIndex, opIndex)
const char* DsonDocument_GetModifierFormulaOperationUrl(handle, modifierIndex, formulaIndex, opIndex)

// Same seven, by scene.modifiers index (the .duf control-morph top node):
int         DsonDocument_GetSceneModifierFormulaCount(handle, sceneModifierIndex)
const char* DsonDocument_GetSceneModifierFormulaOutput(handle, sceneModifierIndex, formulaIndex)
const char* DsonDocument_GetSceneModifierFormulaStage(handle, sceneModifierIndex, formulaIndex)
int         DsonDocument_GetSceneModifierFormulaOperationCount(handle, sceneModifierIndex, formulaIndex)
const char* DsonDocument_GetSceneModifierFormulaOperationOp(handle, sceneModifierIndex, formulaIndex, opIndex)
double      DsonDocument_GetSceneModifierFormulaOperationVal(handle, sceneModifierIndex, formulaIndex, opIndex)
const char* DsonDocument_GetSceneModifierFormulaOperationUrl(handle, sceneModifierIndex, formulaIndex, opIndex)
```

**3b. Channel-value + identity accessors (formula-evaluation increment)**

Items 1–3 let a consumer *walk* the formula tree. Evaluating it correctly needs
three more reads the current surface does not provide. All are stored-field
reads; the parser still neither evaluates RPN nor follows URLs.

Parse extension (one place, serves both index spaces — `modifier_library` and
`scene.modifiers` share the `Modifier` struct): `Modifier::ParseFromJson` today
reads only the channel object's `id` and `label`. Extend it to also store the
channel's numeric state — the dial value (`current_value`, falling back to
`value`, else `0.0`) and, for clamp-correct evaluation, `min` / `max` /
`clamped`. (The `channel` sub-object is not audited for unknown keys, so no
`knownKeys` change is needed.)

Required (correctness blockers):
```
double      DsonDocument_GetModifierChannelValue(handle, modifierIndex)        // current_value → value → 0.0
double      DsonDocument_GetSceneModifierChannelValue(handle, sceneModifierIndex)
const char* DsonDocument_GetMorphId(handle, morphIndex)                        // Modifier::id; already stored, just exposed
```
- `GetMorphId` maps a formula `output` `#fragment` (e.g. `…#Laura_head_bs_Head?value`)
  to its delta-bearing leaf morph. `id` is already parsed; `GetMorphName`/`GetMorphLabel`
  return name/label, not `id`, so a distinct accessor is needed.

Recommended (clamp-correct evaluation; importer stays permissive if absent):
```
double DsonDocument_GetModifierChannelMin(handle, modifierIndex)              // min,  default 0.0
double DsonDocument_GetModifierChannelMax(handle, modifierIndex)              // max,  default 1.0
bool   DsonDocument_GetModifierChannelClamped(handle, modifierIndex)          // clamped, default false
double DsonDocument_GetSceneModifierChannelMin(handle, sceneModifierIndex)
double DsonDocument_GetSceneModifierChannelMax(handle, sceneModifierIndex)
bool   DsonDocument_GetSceneModifierChannelClamped(handle, sceneModifierIndex)
```

Confirmed already correct (no change): `Get{Modifier,SceneModifier}FormulaOutput`
returns the full output URL verbatim including the `?property` suffix; operation
`op` strings are the literal DAZ tokens; for a `push` the discriminator is a
non-empty `Url` (a constant `push` leaves `Url` empty and carries `Val`).

#### UE5 Plugin Side (DsonImporter v2)

**4. Formula evaluator**
A runtime evaluator that:
- Parses channel URLs to identify source bone rotation channels and
  target morph value channels
- Evaluates the RPN stack expression for a given bone pose
- Drives `UMorphTarget` weights at runtime via the Animation Blueprint
  or a custom `UAnimInstance`

**5. Integration point (case (a), runtime)**
- A custom `UAnimInstance` subclass or `AnimNotify` that evaluates all
  formula chains each frame when the skeleton pose changes
- Or: bake the corrective shapes into pose-driven `UPoseAsset` entries
  (simpler but less flexible)

**6. Character / control morphs (case (b), import-time) — separate consumer**
- This is NOT the runtime evaluator. At import the plugin must follow each
  control's formula `output` URLs to the child `.dsf` files, recurse the tree to
  the delta leaves, evaluate the RPN seeded by the scene modifier's
  `current_value`, and compose the result (bake into base vertices and/or emit a
  combined morph target). It reuses the existing external-morph loader.
- The plugin-side design for this lives in the importer repo:
  `Plugins/DsonToUnreal/Docs/FormulaMorphsV2.md`. It depends only on the
  per-morph formula API (items 1–3 above).

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

## Recently completed (post-v1)

### Scene animations full-keyframe surface (count + per-key time + numeric value) — ✅ implemented (Jul 2026)
The additive sibling of the 1.2.0 key-0 surface (library version **2.16.0**, 3
**additive** accessors). The 1.2.0 family exposes each `scene.animations`
channel's verbatim `url`, its first key's `ValueKind`, and the first key's typed
value — exactly right for its motivating use (material initialization data;
single-frame pose presets). 2.16.0 extends that surface with the shape and
numeric-value data an animated preset actually carries.

Motivation (originating consumer FR, 2026-07-10): a real DAZ animated pose
preset — `GEA G8F Getting Up Tired From Seat.duf`, 6 s at 30 fps, 1820
scene.animations channels (1700 bone + 120 dial), 544 multi-key channels —
still declares `asset_info.type = "preset_pose"`, indistinguishable from a
static pose except by per-channel key count. Through 1.2.0 the DsonToUnreal
importer imported such a file as a silent frame-0 pose and could not even warn.

The parser now retains every authored key on `SceneAnimation`:
- `SceneAnimation::key_times` — one `double` per authored `[t, v]` row, all
  value kinds (DAZ times are always numeric).
- `SceneAnimation::key_values` — one `double` per row, populated **only** when
  `kind == KindNumber`; empty otherwise. Non-numeric multi-key channels are not
  in evidence.
- `SceneAnimation::number` / `boolean` / `str` / `color` (the 1.2.0 fields) are
  unchanged — still populated from `keys[0][1]`, still returned byte-identical by
  the 1.2.0 kind-typed accessors.

Exposed:
- `DsonDocument_GetSceneAnimationKeyCount` — `key_times.size()`; `0` on invalid.
- `DsonDocument_GetSceneAnimationKeyTime(handle, animIndex, keyIndex)` — key
  time in seconds at DSON-native double precision, populated for all kinds;
  `0.0` on invalid, but `0.0` is also legitimate (first key at `t=0`), so
  gate on `KeyCount`.
- `DsonDocument_GetSceneAnimationKeyFloat(handle, animIndex, keyIndex)` — the
  numeric value at DSON-native double precision when `ValueKind == 1` (number);
  `0.0` on invalid or non-numeric channel.

Faithful raw passthrough (**R6.4**): times return verbatim (no snapping, no fps
inference — the file carries no fps/playrange metadata; that is consumer-side
interpretation), no interpolation-hint exposure (zero 3-element keys in
evidence), no keyframe application onto `scene.materials`. Sentinels follow the
R1 family contract: count → `0`, numeric → `0.0`. `knownKeys` unchanged
(`keys` already known). Purely additive; the 1.2.0 accessors keep their exact
behavior.

**Consumer note (additive, non-breaking):** the three new functions are
available to the UE plugin; existing calls are unaffected. `KeyCount` alone
enables the loud "animation dropped" warning; `KeyTime` + `KeyFloat` back a
full multi-frame `UAnimSequence` import.

### Gzip blank (all-zero) trailer acceptance — DAZ-compat loader — ✅ implemented (Jun 2026)
A gzip-wrapped DSON member whose 8-byte trailer (CRC32 + ISIZE) is all-zero now loads instead of
being rejected with `gzip CRC32 mismatch`, provided the DEFLATE stream itself inflated cleanly
(library version **2.2.3**, **PATCH**, `DsonParserAPI.h` byte-identical — no new/changed symbol). A
real shipped DAZ product (3D Universe "Pose Architect P1", G8F) writes **52/53** of its `.dsf`
members with a zeroed trailer; DAZ Studio loads them because it does not validate the trailer, so the
strict check was stricter than DAZ and silently dropped legitimately-shipped, DAZ-loadable assets
from a catalog.

- **Design (clean-inflate gate).** The internal inflater (`DsonInflate.cpp` `InflateDeflate`) only
  returns success on a final-block DEFLATE termination, so a clean inflate already proves the payload
  is whole independent of the trailer. `TryGunzip` therefore accepts a blank trailer and skips the
  CRC32/ISIZE comparison; **both** trailer fields must be zero to skip (a coincidentally-zero CRC with
  a non-zero ISIZE still takes the strict path). Genuine truncation/corruption still fails **inside**
  inflate, before the trailer is read, and any present, non-zero, mismatched trailer is still fully
  enforced.
- **Faithful to DAZ, not lax.** This RELAXES a rejection in the permissive-by-design direction (R6.1),
  matching DAZ Studio's own load behavior rather than inventing acceptance.

Verified by an independent Director build (Release|x64, clean, zero warnings) and the `DsonTest2` gzip
harness — all five `GZIP LOAD TESTS` PASS: the new `blank footer acceptance` and `blank footer +
truncated DEFLATE rejection`, alongside the unchanged `happy path`, `CRC rejection` (a present, wrong
trailer is still rejected), and `body corruption rejection`.

### Channel value faithfulness — bool coercion (2.2.1) + type-mismatch audit (2.2.2) — ✅ implemented (Jun 2026)
**2.2.1 (bool coercion).** A DSON channel value of `type:"bool"` now reads numerically as `1.0`/`0.0`
in the modifier and material channel-value accessors (it was silently dropped to the `0.0` default by
the `IsNumber()`-gated read) — `DsonParserAPI.h` byte-identical, no signature change. This is the
parser-side fix for the G8/G8.1 `JCMs On` base-joint-corrective master gate reading `0.0`: that bool
gate mult-gates ~80 base correctives, so at `0.0` it zeroed every one (committed `6beebbc`).

**2.2.2 (type-mismatch audit).** The systemic sibling (library version **2.2.2**, **no new accessor**
— the existing unknown-key trail is reused). 2.2.1 made a *bool* channel value faithful but left the
broader CLASS open: a **recognized** channel value present but of a type the numeric read cannot
represent (after 2.2.1: a string, object, or non-color array) was still dropped to the `0.0` default
and recorded **nowhere** — the same blind spot that hid the "JCMs On" gate. The parser now **records**
that event (it stays permissive — never throws; the numeric still falls back to its default) as a
decorated, self-describing entry in the existing per-context audit trail, distinguishable from a
genuine unknown key: `value [channel "<id>": type=<jsontype>, used default]`.

- Recorded at the two channel-value parse sites — `Modifier::ParseFromJson` (feeds
  `GetModifierChannelValue` / `GetSceneModifierChannelValue`) and `ParseMaterialChannel` (feeds
  `GetMaterialChannelValue` / `GetSceneMaterialChannelValue`).
- Surfaced through the **existing** C ABI — `DsonDocument_GetUnknownKey` / `…GetUnknownKeyCount` — so
  `DsonParserAPI.h` stays byte-identical (**PATCH**, no `@since`). **Report-only:** no coercion is
  invented beyond the number/bool rule (there is no faithful numeric for those types).

Trade-off (accepted when choosing trail-reuse over a structured accessor, per the accessor-fan-out
tripwire): the trail is a deduped `std::set` per context, so identical mismatches across sibling
channels in one context collapse to one entry. Verified by an independent Director build (Release|x64,
clean) and the `DsonTest2` harness: a string-valued modifier channel and a string-valued material
channel each emit the decorated entry while their numeric reads stay `0.0`; a numeric channel
(`0.75`) and a bool channel (`true` → `1.0`) emit **no** entry (no false positives); a real
`Genesis_9_Mouth_MAT.duf` load surfaces its DAZ `"Tags"` string channel as designed; and the 2.2.1
assertions stay green.

**Consumer note (behavioral, non-breaking):** no signature change; consumers already reading the
unknown-key trail will now also see decorated channel type-mismatch entries — filter on the
`used default` marker if a pure unknown-key list is wanted.

### Modifier control-inventory metadata — group / region / icon — ✅ implemented (Jun 2026)
The additive sibling of the 1.5.0 catalog work, for an Importer/Artisan building a UI over a
figure's sliderable controls (library version **2.2.0**, 3 **additive** accessors). A
modifier's DAZ Parameter-Settings "Path" (`group`) and "Region" (`region`) — both
**modifier-level** keys — and its per-control thumbnail (`presentation.icon_large`) were
parsed-as-known but dropped; they are now stored on `Modifier`
(`group` / `region` / `presentation_icon`) and exposed verbatim:

- `DsonDocument_GetModifierGroup` — modifier-level `group` (Parameter-Settings "Path", e.g.
  `/Pose Controls/Head/Expressions`).
- `DsonDocument_GetModifierRegion` — modifier-level `region` (e.g. `Head`).
- `DsonDocument_GetModifierPresentationIcon` — `presentation.icon_large` thumbnail path,
  returned **verbatim** (percent-encoded as stored; the consumer resolves/loads it).

Faithful raw passthrough (**R6.4**): no decode, normalization, or hierarchy inference. String
sentinel `""` when the field is absent or the index is invalid, matching
`GetModifierPresentation{Type,Label}` (R1). `group`/`region` were already in the modifier
`knownKeys` set, so the unknown-key audit is unchanged. Verified by an independent Director
build (Release|x64, clean) and the `DsonTest2` harness: `body_bs_NipplesFeminine_HD3.dsf`
modifier[0] (`group="/Feminine"`, `region="Chest"`, icon = the verbatim
`…/body_bs_NipplesFeminine_HD3.png`) and `BaseJointCorrectives.dsf` modifier[0] `"JCMs On"`
(`group="/General/Misc"`, `region` absent → `""`, `icon_large` present-but-empty → `""`).

**Consumer note (additive, non-breaking):** the three new functions are available to the UE
plugin; existing calls are unaffected.

### Asset-catalog metadata — presentation + geograft — ✅ implemented (Jun 2026)
For an Importer building a faithful library catalog of installed `.duf`/`.dsf` assets from
declared data only, three declared facts are now exposed (library version **1.5.0**, 5
**additive** accessors). `presentation.{type,label}` is parsed onto `Node` and `Modifier`
(`presentation_type` / `presentation_label`) and a geograft flag onto `Geometry`
(`is_graft`); the three keys joined their `knownKeys` sets (clearing prior unknown-key
noise):

- `DsonDocument_GetNodePresentationType` / `…Label` — node_library item `presentation.type`
  (DAZ "Content Type", e.g. `"Follower"`, `"Wardrobe/Clothing"`) and display label.
- `DsonDocument_GetModifierPresentationType` / `…Label` — modifier_library item
  `presentation.type` (e.g. `"Modifier/Shape"`) and label.
- `DsonDocument_GetGeometryIsGraft` — `true` only for a **populated** graft (non-empty
  `vertex_pairs`); an empty `"graft": {}` (base figures, G9 Eyes/Eyelashes) is `false`.

Faithful per-item exposure (**R6.4**): the parser does no classification, no document-level
content-type resolution, and no cross-section merge — the consumer maps `presentation.type`
to its catalog taxonomy and selects the asset's defining item. String sentinels `""`, bool
sentinel `false` (R1). Verified by an independent Director build (Release|x64, clean) and a
standalone consumer: TestFiles `test.dsf` (modifier `"Modifier/Shape"`), `Genesis9.json`
(base geom `is_graft=false`, node[0] `"Actor"`), and an external geograft
`Genesis9FemaleGenitalia.dsf` (`is_graft=true`, 84 `vertex_pairs`; node[0] `"Follower"`).

**Consumer note (additive, non-breaking):** the five new functions are available to the UE
plugin; existing calls are unaffected.

### Geograft weld correspondence (`graft.vertex_pairs` / `hidden_polys`) — ✅ implemented (Jul 2026)
The geograft signal (1.5.0) reports *whether* a geometry is a graft; the composed-figure
assembler in DsonToUnreal also needs the *weld correspondence* to merge the graft into the
body. Library version **2.9.0** (7 **additive** accessors) exposes the raw `graft` block
arrays per geometry, in the file's own DSON index space — the importer owns the
DSON→import-point remap and the weld; the parser does neither (**R6.4**):

- `DsonDocument_GetGeometryGraftVertexPairCount` / `…GraftVertexPairGraftVertex` /
  `…GraftVertexPairBaseVertex` — the boundary weld pairs, both members, as
  `[graft-local vertex, base-figure vertex]`. The count is the parsed `values` length,
  authoritative over DAZ's declared `vertex_pairs.count` (they can disagree).
- `DsonDocument_GetGeometryGraftHiddenPolyCount` / `…GraftHiddenPoly` — the base-figure
  polygon indices the graft hides on weld; empty for an additive graft.
- `DsonDocument_GetGeometryGraftBaseVertexCount` / `…GraftBasePolyCount` — the graft
  block's declared `vertex_count` / `poly_count`, the base-figure resolution the base-side
  pair indices are expressed in.

Count family → `0` on invalid; the vertex/poly value accessors → `-1` (index 0 is
legitimate, mirroring the rigid-follow reference-vertex family). Proof assets, values read
directly from the DSON and asserted by the `DsonTest2` harness: `Genesis9FemaleGenitalia.dsf`
(82 weld pairs — DAZ declares 84 — 180 hidden polys, declared base 25182/25156) and the
additive `BaseShortsGeoGraft_318.dsf` (106 weld pairs, 0 hidden polys, declared base
8256/8130). Verified by an independent Director build (Release|x64, clean — 0 warnings) and
the `DsonTest2` harness (all geograft pair/hidden/base/sentinel checks PASS).

**Consumer note (additive, non-breaking):** the seven new functions are available to the UE
plugin; existing calls are unaffected.

### Geometry rigidity (`geometry.rigidity`) — ✅ implemented (Jul 2026)
Library version **2.10.0** adds 17 geometry-indexed C-ABI accessors for the complete
authored rigidity block. `DsonDocument_GetGeometryHasRigidity` is an explicit presence
discriminator, including for an authored empty object. `...RigidityWeight*` exposes valid
parsed sparse `[vertexIndex, weight]` rows, while `...RigidityGroup*` exposes source-order
group id, rotation mode, per-axis scale modes, reference/mask vertex arrays, reference,
transform-node strings, and the boolean authored under DAZ's misspelled
`use_tranform_bones_for_scale` key.

All values are faithful raw passthrough (R6.4): vertex indices remain in the owning
geometry's index space, node ids/names remain as-authored, and the parser performs no
remap, weld, node resolution, scale derivation, or rigidity evaluation. Counts return
`0` on invalid input; vertex-index accessors return `-1`; strings return `""`, weight
`0.0`, and bools `false`. A deterministic in-memory regression covers authored-empty vs
absent presence, malformed-row skipping, fractional weights, multiple groups, every
field and sentinel, non-`none` rotation, and mixed `primary`/`secondary`/`none` scale
modes. The installed Genesis 9 Male Genitalia proof additionally passes with 1,354
weights and the complete `Gens` group; no installed rotation-using graft was found in
the targeted search, so that variant is verified synthetically.

**Consumer note (additive, non-breaking):** the 17 new functions are available to the UE
plugin; existing calls are unaffected.

### Per-layer LIE compositing metadata (`Image::layers`) — ✅ implemented (Jun 2026)
DAZ Layered Image (LIE) `map` elements carry per-layer compositing instructions — blend
`operation`, `transparency` (opacity), `active`/`invert` flags, `color` tint, and a 2D
transform (`rotation`, `xscale`/`yscale`, `xoffset`/`yoffset`, `xmirror`/`ymirror`).
Through 1.3.0 only each layer's `url`/`label` was retained; these are now parsed
faithfully onto `Dson::ImageLayer` (raw values, DAZ-semantic defaults) and exposed by
**28 additive** accessors (library version **1.4.0**) — 14 shared suffixes across both
the per-`image_library`-index surface (`DsonDocument_GetImageLayer*`) and the
per-scene-material-channel surface (`DsonDocument_GetSceneMaterialChannelLayer*`):
`{BlendMode, Opacity, Active, Invert, ColorR, ColorG, ColorB, Rotation, ScaleX, ScaleY,
OffsetX, OffsetY, MirrorX, MirrorY}`. Sentinels follow the R1 family contract (string
`""`, bool `false`, numeric `0.0`, scales `1.0`); `Opacity` is the raw `transparency`
(1 = opaque). The parser performs no compositing — a downstream consumer re-composites
from the raw layers (R6.4). A color-only base layer with no `url` stays excluded from
`Image::layers`, unchanged from 1.3.0. Verified against `TestFiles/HID_Nancy_9.duf` (G9
HID Nancy head diffuse + SSS Color, 4-layer stacks) and a crafted inline-`#id`
per-channel snippet in `DsonTest2.cpp`.

**Consumer note (additive, non-breaking):** the 28 new functions are available to the
UE plugin; existing calls are unaffected.

### Scene animations key-0 channel surface (`scene.animations`) — ✅ implemented (Jun 2026)
DAZ `preset_hierarchical_material` presets park real material-channel values and
`image_file` paths in `scene.animations` (`{url, keys}`, key 0 = initialization data)
while leaving `scene.materials` channels as bare placeholders — the root cause of
Genesis 9 mouth/teeth importing untextured/"metallic". `scene.animations` is now parsed
in `Scene::ParseFromJson` onto `Scene::animations` (`SceneAnimation`: verbatim `url` +
the first key's typed value) and exposed by nine **additive** accessors (library
version **1.2.0**):

- `DsonDocument_GetSceneAnimationCount` — `0` on invalid handle / no animations.
- `DsonDocument_GetSceneAnimationUrl` — verbatim DSON property pointer.
- `DsonDocument_GetSceneAnimationValueKind` — `0` null / `1` number / `2` bool /
  `3` string / `4` color (`-1` invalid).
- `DsonDocument_GetSceneAnimationFloat` / `…Bool` / `…String` / `…ColorR/G/B`.

Faithful by design (**R6.4**): the parser does **not** apply the keyframes onto
`scene.materials`, resolve the pointer, or resolve the `image_file` string against
`image_library` — the consumer reads both sections and decides the override. Every
entry is exposed (no filter); v1 reads the first key only, so `image_modification`/
tiling and multi-key are recognized in the data but not modeled. Data shapes verified
against `TestFiles/Genesis_9_Mouth_MAT.duf` (Mouth/Teeth: `diffuse/image_file` =
`Genesis9_Mouth_D_1001.jpg`, `Diffuse Roughness` = 0.3, `Translucency Weight` = 0.8);
verified by an independent Director build (Release|x64, clean) + a
`DsonTest2 Genesis_9_Mouth_MAT.duf` harness run — the three anchors and the NO-MERGE
check all pass.

**Consumer note (additive, non-breaking):** existing calls are unaffected; the UE
plugin opts into the new accessors and performs the key-0 override itself — it now owns
that interpretation (R6.4).

### Scene post-load addon manifest (`scene.extra` Character Addon Loader) — ✅ implemented (Jun 2026)
The DAZ "Character Addon Loader" `PostLoadAddons` block in `scene.extra` lists
companion conforming figures (Genesis 9 eyes / mouth / eyelashes / tear / a
character-dependent eyebrows figure) that a `character` preset instances but does
**not** list in `scene.nodes`. It is now parsed in `Scene::ParseFromJson` onto
`Scene::post_load_addons` (`ScenePostLoadAddon`: `slot` / `asset_name` /
`asset_file` / `mat_preset`) and exposed by five **additive** accessors (library
version **1.1.0**):

- `DsonDocument_GetScenePostLoadAddonCount` — `0` on invalid handle / no manifest.
- `DsonDocument_GetScenePostLoadAddonSlot` — DAZ slot key, e.g.
  `Follower/Attachment/Head/Face/Eyes`.
- `DsonDocument_GetScenePostLoadAddonAssetName`
- `DsonDocument_GetScenePostLoadAddonAssetFile` — content-relative loader `.duf`.
- `DsonDocument_GetScenePostLoadAddonMatPreset` — content-relative MAT preset
  `.duf`; `""` when the slot has none.

The index is a flat, document-ordered walk across every `PostLoadAddons` map in
`scene.extra`. The slot map is iterated by member (not `ParseObjectArray`, which is
for arrays); a slot is kept only when it has a non-empty `AssetFile`, so the count
is meaningful. Parser stays permissive (any missing hop skips that slot, never
errors) and single-document — resolving the paths and loading the referenced `.duf`
files (`preset_hierarchical_material` MAT presets / figure presets) remains importer
work. The per-addon `SelectAddon` flag is **not** exposed — uniformly `false` across
the samples, it is a UI hint, not a load gate. Manifest data verified against
`TestFiles/{G9,HID_Nancy_9,Laura9}.duf` (5 / 4 / 5 addon slots); accessor behavior
pending a build + harness run.

### Scene post-load scripts (`scene.extra` DAZ Script references) — ✅ implemented (Jun 2026)
A `scene.extra` entry of type `scene_post_load_script` names a DAZ Script
(`.dse`/`.dsa`) that DAZ Studio runs at load — runtime work (e.g. the Genesis 9
base "Character Addon Loader" that assigns the card-eyebrow textures, or a "Remove
Duplicate Eyebrows" cleanup) that a static import cannot replicate and, before this,
could not even detect. It is now parsed in `Scene::ParseFromJson` onto
`Scene::post_load_scripts` (`ScenePostLoadScript`: `type` / `name` / `script`) and
exposed by four **additive** accessors (library version **2.3.0**):

- `DsonDocument_GetScenePostLoadScriptCount` — `0` on invalid handle / none.
- `DsonDocument_GetScenePostLoadScriptName` — the entry `name`.
- `DsonDocument_GetScenePostLoadScriptType` — the entry `type`, e.g.
  `scene_post_load_script`.
- `DsonDocument_GetScenePostLoadScriptFile` — the content-relative `.dse`/`.dsa`
  path; `""` when the entry names no script.

The index is a flat, document-ordered walk across every `scene.extra` entry that
carries a `script` string; the gate is the presence of the `script` reference (not
the `type` literal), and `type` is surfaced verbatim so a consumer can narrow to
`scene_post_load_script` itself. Faithful passthrough — the parser neither resolves,
loads, nor **executes** the script (R6.4; consistent with the no-recursive-load
boundary), so a consumer warns (no-silent-fails) that the script's runtime effects
are not captured. The capture runs independently of the `PostLoadAddons` walk, so an
entry carrying both (the Genesis 9 base does) appears in both families. Evidence:
`People/Genesis 9/Genesis 9.duf` (Character Addon Loader `.dse`) and
`data/Daz 3D/Genesis 9/Base/Tools/Script Loads/Eyebrows/Card Eyebrows.duf`
(Remove Duplicate Eyebrows `.dse`), per the originating importer FR (2026-06-22).

**Status (2026-06-22):** built clean Release|x64 and Director-verified (compile +
link of the four exports; `code-review-rules` pass clean). No in-repo DSON fixture
carries a `scene_post_load_script` entry, so the runtime accessor path is exercised
only against the consumer's real DAZ content — the same coverage gap as the sibling
addon family above. Originating importer FR (`scene.extra` DAZ-script visibility);
the importer-side warning consumes this after vendoring 2.3.0.

### Image `map_size` (pixel dimensions) — ✅ implemented (Jun 2026)
`map_size` (a `[width, height]` int array, e.g. `[ 4096, 4096 ]`, verified against
`TestFiles/HID_Nancy_9.duf`) is now parsed in `Image::ParseFromJson` into
`Image::map_width` / `map_height`, replacing the former silent no-op. It is exposed
over the C ABI by three **additive** accessors alongside the existing `GetImageCount`:

- `DsonDocument_GetImageId(handle, imageIndex)` — string getter, `""` on invalid index.
- `DsonDocument_GetImageMapWidth(handle, imageIndex)` — numeric getter, `0` on invalid
  index or absent `map_size`.
- `DsonDocument_GetImageMapHeight(handle, imageIndex)` — same contract.

Parser stays permissive (any non-`[w,h]` shape leaves the `0` defaults); `map_size`
was already in `Image`'s `knownKeys`, so no knownKeys change. (At that time the DAZ LIE
`map` per-layer compositing fields were unmodeled — only each layer's `url`/`label` was
retained on `Image::layers`; the full per-layer compositing set was added later in
1.4.0, see Recently completed.) Covered by a harness check
in `DsonTest2.cpp` (`HID_Nancy_9.duf`, expects 4096×4096 plus the `0` out-of-range
sentinel).

**Consumer note (additive, non-breaking):** the three new functions are available to
the UE plugin; existing calls are unaffected.

---

## Audit History

| Pass | Gaps Found | Gaps Resolved | Result |
|---|---|---|---|
| Pass 1 | 18 | 18 | Partial |
| Pass 2 | 8 | 8 | Partial |
| Pass 3 | 6 | 6 | Partial |
| Pass 4 | 0 | — | ✅ v1 Complete |
