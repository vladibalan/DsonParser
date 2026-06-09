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
| `DsonInflate.h/.cpp` | Internal dependency-free gzip/DEFLATE inflater run before JSON parsing; verifies gzip CRC32 + ISIZE |
| `DsonParserAPI.h/.cpp` | Flat C API (`extern "C"`) with opaque `DsonDocumentHandle`; per-vertex skin cache; morph index cache |

### C API — ~180 exported functions covering:

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
  shared base-path match. Per-layer compositing metadata (operation/opacity/color/
  transforms/active) deferred to a future consumer.
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

### Known Limitations (v1)
- **Formulas not evaluated** — as shipped in v1, formula keys were suppressed in
  `knownKeys` and neither stored nor exposed. **Resolved at the parser level in
  v2** (formulas are now stored and exposed; see v2 Parser Side below). Evaluation
  and baking remain importer-side, so pose-driven correctives still require the
  importer integration before they fire automatically.
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
  and each tracks unknown keys so unmodeled op fields (e.g. `spline_tcb` knots)
  surface in diagnostics instead of being dropped.

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
was already in `Image`'s `knownKeys`, so no knownKeys change. The DAZ LIE `map`
compositing layers (blend operation/offsets) remain intentionally unmodeled — only
each layer's `url`/`label` is retained on `Image::layers`. Covered by a harness check
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
