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
| `DsonParser/DsonInflate.h/.cpp` | Internal, dependency-free gzip/DEFLATE support used by the loader before JSON parsing. Verifies gzip CRC32 and ISIZE (a blank all-zero trailer is accepted on a clean inflate — DAZ-compat). |
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
unchanged. The gzip trailer CRC32 and ISIZE are enforced when present, so corrupt
or mis-decoded data fails with a gzip-specific error rather than surfacing later
as a misleading JSON parse error. One DAZ-compatibility exception: a **blank
(all-zero) trailer** is accepted when the DEFLATE stream itself inflated cleanly
— a real shipped DAZ product writes its `.dsf` members with a zeroed trailer and
DAZ Studio loads them (it does not validate the trailer), so the loader trusts the
clean inflate (the inflater only succeeds on a final-block DEFLATE termination)
rather than dropping the asset. Genuine truncation or corruption still fails inside
inflate, and any present, non-zero, mismatched trailer is still rejected. Both
trailer fields must be zero to take the blank-trailer path.

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
  including their raw authored transform/joint overrides and component presence,
  and each node's `studio/node/shell` extra `material_uvs` assignments,
  modifiers, materials, and UV sets, plus the post-load addon manifest in
  `scene.extra` (the DAZ "Character Addon Loader" `PostLoadAddons`; see the Scene
  Post-Load Addons section below), the `scene.extra` `scene_post_load_script`
  references (DAZ Scripts the static import does not execute; see the Scene
  Post-Load Scripts section below), and the `scene.animations` keyframe channels
  (see the Scene Animations section below — stored faithfully, never applied onto
  `scene.materials`). Presentation and current camera are recognized but not stored
  as typed fields.

`node_library`
: Parsed into library `Node` definitions. Captures id, name, label, type,
  parent, URL, translation, rotation, scale, general scale, center/end points,
  orientation, rotation order, and the item's `presentation` content type +
  label (`DsonDocument_GetNodePresentationType`/`…Label`; see Asset Catalog
  Metadata below).

  A node's `translation[]` / `rotation[]` / `scale[]` are additionally retained as
  **authored transform channels** — per channel the verbatim `id`, `label`, `min`,
  `max`, `clamped`, and a field-presence mask — exposed by
  `DsonDocument_GetNode{Translation,Rotation,Scale}Channel{Count,Id,Label,Min,Max,Clamped,FieldPresenceMask}`
  (since 2.18.0). These sit beside the existing
  `GetNode{Translation,Rotation,Scale}{X,Y,Z}` value accessors, which are unchanged
  and remain the only read of a channel's *value* — the channel family deliberately
  does not re-expose it. **Presence is the contract:** DAZ authors `clamped` only
  sometimes and never as `false` (Genesis 9 base: 800 authored `true`, 451 absent —
  and scale never authors it at all), while `min`/`max` are always authored but
  legitimately `0..0` on a locked joint. So `Min`/`Max`/`Clamped` are meaningful only
  when the matching `DSONPARSER_CHANNEL_FIELD_{MIN,MAX,CLAMPED}` bit is set — `0.0`
  and `false` are each both the invalid sentinel *and* a legitimate reading; query
  the mask first. Faithful passthrough (R6.4): channels stay in source order, and
  nothing is clamped, resolved onto engine axes, or filtered — a locked or unbounded
  channel is reported as authored, because classifying a range as a "real" constraint
  is the consumer's judgment. Read from `node_library`: the shared `Node` parse
  co-populates `scene.nodes` instances, but no scene-family accessor is published. A
  plain `[x,y,z]` numeric transform array authors no channels (`Count` → 0). Note the
  per-channel keys (`min`/`max`/`clamped`/`label`, and the unmodeled
  `name`/`type`/`step_size`/`visible`) live inside array elements, which
  `TrackUnknownKeys` does not walk — they never appear in the unknown-key trail, so a
  clean audit report says nothing about them either way.

  For a **rigid-follow** node — a DAZ rigid-follow gem/attachment
  whose `extra[]` carries a `studio/node/rigid_follow` entry with an inline
  `rigidity_group` (a fixed reference-vertex patch on a followed mesh it rides
  rigidly) — the group's raw reference-vertex indices, `rotation_mode`, and
  per-axis `scale_modes` are exposed via `DsonDocument_GetNodeHasRigidFollow` /
  `…GetNodeRigidFollowRotationMode` / `…GetNodeRigidFollowScaleMode{Count,}` /
  `…GetNodeRigidFollowReferenceVertex{Count,}`. Read from the **`node_library`**
  definition — the `scene.nodes` instance carries only a bare
  `studio/node/rigid_follow` marker with no group — and gated on that marker
  (twin of the modifier push read below): faithful/unevaluated (R6.4), raw
  passthrough, no follow reconstruction or cross-section merge. `HasRigidFollow`
  is the presence discriminator (an empty group still reports `true`, so absence
  stays distinguishable from an empty vertex list); bound-check the `…Count`
  accessors, and note `…ReferenceVertex` returns `-1` on invalid because vertex
  index 0 is legitimate (mirrors `GetSkinJointWeightVertexIndex`). Since 2.8.0.

`geometry_library`
: Parsed into `Geometry`. Captures vertex positions, polygon/polylist data,
  face offsets, polygon groups, material groups, vertex/polygon counts,
  default UV-set reference, source-order `material_uvs` assignments from each
  material-group name to its authored UV-set name, a geograft signal — whether the geometry
  declares a populated `graft` block (`DsonDocument_GetGeometryIsGraft`) — and,
  for a graft, the raw geograft weld correspondence (`vertex_pairs` /
  `hidden_polys` / declared base counts; see Asset Catalog Metadata below), plus
  the complete authored `rigidity` block: sparse vertex weights and source-order
  groups with rotation/scale modes, reference/mask vertices, reference and
  transform-node strings, and the authored transform-bones-for-scale flag.

`material_library`
: Parsed into `Material`. Captures source-order material channels, shader type
  from `extra[]`, surface groups, geometry reference, UV-set reference, scalar
  values, colors, raw image URLs, and resolved texture paths.

`modifier_library`
: Parsed into `Modifier`. Captures morph deltas, normal deltas, stored channel
  dial metadata/value bounds, skin binding payloads, formulas, the item's
  authored parent URL (`DsonDocument_GetModifierParent`),
  `presentation` content type + label + icon
  (`DsonDocument_GetModifierPresentationType`/`…Label`/`…Icon`), and the
  modifier-level `group`/`region` control tags (`DsonDocument_GetModifierGroup`/
  `…Region`; see Asset Catalog Metadata below). Formula `output`, `stage`, and
  the source-order RPN `operations` (`op` plus scalar `val`/`url`, or array
  `val_array` for spline_tcb knots) are stored and exposed; the parser does not
  evaluate them or follow their channel references. For a geometry-shell **push
  modifier** (`studio/modifier/push`, the "Mesh Offset" of a Geometry Shell
  wearable), the push identity and its "Offset Distance" channel value — both
  nested in `extra[]`, not at the modifier top level — are exposed via
  `DsonDocument_GetModifierIsPush` / `…GetModifierPushOffset` (the offset prefers
  `current_value` over `value`, returned raw in the channel's cm; `0.0` is both the
  sentinel and a legitimate value, so gate on `IsPush`). `GetModifierParent`
  addresses that same raw modifier index and returns the parent URL verbatim, so
  a consumer can match each push to its target geometry by fragment; the parser
  does not resolve or join the reference (R6.4). A modifier's `extra[]` is otherwise
  unmodeled.

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
: Parsed into `UVSet`. Captures id, name, label, UV coordinates, vertex count,
  and sparse polygon-vertex UV overrides. The authored `name` and `label` (the
  DAZ display name, e.g. `"Base Multi UDIM"`) are exposed verbatim via
  `DsonDocument_GetUVSetName` / `…GetUVSetLabel`, `""` when absent (since
  2.14.0).

## Asset Catalog Metadata (presentation + geograft)

For building a library catalog of installed assets — and a UI over a figure's controls —
several declared facts are exposed as faithful single-file reads: the parser reports what the
opened file declares and does no classification, folder inference, or document-level
resolution (R6.4); the consumer maps and selects.

- **Content type + display label** come from a library item's `presentation` block
  (`presentation.type` is the DAZ "Content Type"; `presentation.label` the display name).
  They are exposed **per item**, not as one document-level value, because `presentation`
  legitimately appears on many items and which one represents "the asset" is the consumer's
  call (it already knows `asset_info.type`):
  - `DsonDocument_GetNodePresentationType` / `…Label` — for figures, clothing, hair, props
    (e.g. `"Follower"`, `"Wardrobe/Clothing"`).
  - `DsonDocument_GetModifierPresentationType` / `…Label` — for shapes/morphs
    (e.g. `"Modifier/Shape"`).
  - A preset (`.duf`) with no `presentation` returns `""` — the consumer treats `""` as
    "unknown".
- **Node conform-target base figure** — a follower/attachment `node_library` item's
  `presentation` block may also carry a `preferred_base` string naming the base figure
  the item conforms to (e.g. `"/Genesis 8/Female"`). Exposed via
  `DsonDocument_GetNodePresentationPreferredBase` on the same node index space as the
  presentation type/label above. Read from `node_library` only; modifiers do not carry it.
  Use case: a standalone geograft importer disambiguates which base a graft targets — the
  wearable DUF itself carries no identity (its `conform_target` is the
  `name://@selection:` placeholder) and node/geometry names lie (the official G8F
  genitalia is internally `Genesis3FemaleGenitalia`); the 2.9.0 declared graft base
  vertex/poly counts split G8 Female (16556/16368) from G8 Male (16384/16196) but cannot
  split same-topology bases, so this authored string is the decisive signal. The string
  names the CONFORM-TARGET body, not the product's styling — a Female-named product may
  declare a Male target (`Genesis8FemaleGenitalia.dsf` → `"/Genesis 8/Male"`). Raw
  passthrough (R6.4): the parser does no content-path resolution, catalog inference, or
  cross-section merge; `""` when the field is absent or the handle/index is invalid.
  Since 2.15.0.
- **Geograft signal** — `DsonDocument_GetGeometryIsGraft` returns `true` only when the
  geometry declares a **populated** `graft` (a non-empty `vertex_pairs`). DAZ writes an
  empty `"graft": {}` on non-graft meshes (base figures, and Genesis 9 Eyes — which uses
  `rigidity` — and Eyelashes), so key-presence alone is not the signal; an empty graft
  reports `false`.
- **Geograft weld correspondence** — when the geometry is a graft, the `graft` block's
  raw weld arrays are exposed per geometry in the file's own DSON index space (the
  importer owns the DSON→import-point remap and the weld; the parser does neither):
  - `vertex_pairs` — the boundary weld pairs, **both** members as `[graft-local vertex,
    base-figure vertex]` — via `DsonDocument_GetGeometryGraftVertexPairCount` /
    `…GraftVertexPairGraftVertex` / `…GraftVertexPairBaseVertex`.
  - `hidden_polys` — the base-figure polygon indices the graft hides on weld (empty for
    an additive graft) — via `…GetGeometryGraftHiddenPolyCount` / `…GraftHiddenPoly`.
  - the graft block's declared `vertex_count` / `poly_count` — the base-figure resolution
    the base-side pair indices are expressed in — via `…GetGeometryGraftBaseVertexCount` /
    `…GraftBasePolyCount`.
  The **pair count is the parsed `values` length, not DAZ's declared
  `vertex_pairs.count`** (they can disagree: `Genesis9FemaleGenitalia.dsf` declares 84 but
  ships 82 rows), consistent with how `is_graft` keys off the values size. Count family →
  `0` on invalid; the vertex/poly accessors → `-1` (index 0 is legitimate). Faithful
  passthrough (R6.4): no remap, weld, reorder, or cross-section merge. Since 2.9.0.
- **Geometry rigidity** — `DsonDocument_GetGeometryHasRigidity` distinguishes an
  authored `geometry.rigidity` object (including an empty one) from absence. The
  `DsonDocument_GetGeometryRigidityWeight*` family exposes valid parsed sparse
  `[vertexIndex, weight]` rows; the `...RigidityGroup*` family exposes every group
  field: `id`, `rotation_mode`, per-axis `scale_modes`, `reference_vertices`,
  `mask_vertices`, `reference`, `transform_nodes`, and DAZ's literally misspelled
  `use_tranform_bones_for_scale` boolean. Vertex arrays remain in the owning
  geometry's index space and node references remain as-authored; there is no
  remap, weld, reference resolution, transform/scale derivation, or evaluation
  (R6.4). Counts return `0` on invalid input; vertex-index accessors return `-1`;
  strings return `""`, weight `0.0`, and bools `false`. Since 2.10.0.
- **Control inventory (modifier `group`/`region`/icon)** — for a UI over a figure's
  sliderable controls. A modifier carries its DAZ Parameter-Settings "Path" as a
  modifier-level `group` (`DsonDocument_GetModifierGroup`, e.g.
  `/Pose Controls/Head/Expressions`) and "Region" as a modifier-level `region`
  (`DsonDocument_GetModifierRegion`, e.g. `Head`); the per-control thumbnail is
  `presentation.icon_large` (`DsonDocument_GetModifierPresentationIcon`, returned
  **verbatim** — percent-encoded as stored, the consumer resolves/loads it). All three are
  raw passthrough, `""` when the field is absent or the index is invalid. Nesting note:
  `group`/`region` are siblings of `channel`/`presentation`; the icon sits inside
  `presentation` next to `type`/`label`.

These read `Node`/`Modifier`/`Geometry` fields directly; there is no cross-section merge.

## Scene vs Library Data

DSON separates reusable definitions from scene instances. For example,
`node_library` contains node definitions, while `scene.nodes` contains placed
instances that reference definitions with URL fields and may carry instance
labels or geometry references.

The C API keeps these separate:

- `DsonDocument_GetNode*` reads `node_library`.
- `DsonDocument_GetSceneNode*` reads `scene.nodes`, including each instance's
  verbatim parent pointer, its fit-parent pointer (a fitted figure root's
  `conform_target`, the DAZ "Fit To" target URL — `GetSceneNodeConformTarget`,
  2.17.0), and local translation/rotation/scale, general scale, rotation order,
  raw `center_point` / `orientation`, and raw `inherits_scale`. `parent` and
  `conform_target` are distinct: a fitted figure root carries `conform_target`
  and has no `parent`; its child bones carry `parent` and no `conform_target`.
  Both are faithful raw string reads (R6.4).
- `DsonDocument_GetMaterial*` reads `material_library`.
- `DsonDocument_GetSceneMaterial*` reads `scene.materials`.
- `DsonDocument_GetSceneModifier*` reads `scene.modifiers`, including each
  entry's authored `parent` URL naming the target node (e.g. `"#Genesis9-1"`
  or `"#Genesis9_JewelBikini._Bottom"`) — `GetSceneModifierParent` (2.17.0),
  the sibling of the 2.11.0 library-family `GetModifierParent`. The channel
  value family exposes both the numeric read (`GetSceneModifierChannelValue`)
  and a **value-kind discriminator + string getter** for channels whose
  `current_value` (or `value` fallback) is a string — `"type":"file"` DAZ
  script / template loaders. `GetSceneModifierChannelValueKind` returns
  `0` null/absent · `1` number · `2` bool · `3` string; `-1` invalid — the
  same numeric legend as `GetSceneAnimationValueKind` (a color kind is
  defined but not applicable to modifier channels, which are scalar).
  `GetSceneModifierChannelValueString` returns the raw string when kind
  is `3`; `""` otherwise. The existing double
  `GetSceneModifierChannelValue` is unchanged — it still returns `0.0` for
  a string (and `1.0`/`0.0` for a bool per 2.2.1) — so consumers only
  reading the numeric side see byte-identical behavior; a string still
  trips the `TrackChannelTypeMismatch` audit entry (2.2.2) because the
  numeric read still defaults. Since 2.17.0.

A geometry shell may author its per-surface UV selection on the placed scene
node rather than on any `geometry_library` definition. For each exact
`studio/node/shell` object in `scene.nodes[i].extra[]`, the parser retains valid
`material_uvs` `[material-group-name, uv-set-name]` rows in authored extra/row
order and exposes them through
`DsonDocument_GetSceneNodeShellMaterialUVAssignment{Count,MaterialGroup,UVSetName}`.
These are node-indexed, verbatim reads. They do not fall back to or combine with
`DsonDocument_GetGeometryMaterialUVAssignment*`; resolving the UV-set name to an
external DSF remains importer work (R6.3/R6.4). Since 2.13.0.

This distinction matters when an importer needs the base asset definition versus
the configured scene instance.

Scene-instance transform channels prefer their authored `current_value`, falling
back to `value`; this preserves per-copy local placement stored only on the instance.

For sparse scene-node transform and joint fields, the C API exposes both raw
values and authored presence:

- `DsonDocument_GetSceneNodeTranslationPresenceMask`,
  `DsonDocument_GetSceneNodeRotationPresenceMask`, and
  `DsonDocument_GetSceneNodeScalePresenceMask` accompany their existing XYZ
  value accessors.
- `DsonDocument_GetSceneNodeHasGeneralScale` and
  `DsonDocument_GetSceneNodeHasRotationOrder` accompany their existing scalar
  and string value accessors.

- `DsonDocument_GetSceneNodeCenterPoint{X,Y,Z}` and
  `DsonDocument_GetSceneNodeCenterPointPresenceMask`.
- `DsonDocument_GetSceneNodeOrientation{X,Y,Z}` and
  `DsonDocument_GetSceneNodeOrientationPresenceMask`.
- `DsonDocument_GetSceneNodeInheritsScale` and
  `DsonDocument_GetSceneNodeHasInheritsScale`.

The vector masks OR `DSONPARSER_VECTOR_COMPONENT_X` (`0x1`), `_Y` (`0x2`), and
`_Z` (`0x4`). A bit is set only when that component's selected `current_value`
or `value` (or positional array element) is numeric, so explicit identity values
remain distinguishable from absence. The scalar/string has-flags likewise make
authored `general_scale: 1`, default-valued rotation order, and
`inherits_scale: false` distinguishable from absence. A zero mask / false
presence is also the invalid handle/index sentinel, so consumers query presence
before interpreting a value. These are faithful reads from the opened file's
`scene.nodes` entry only: the parser does not resolve its URL or fill absent
fields from a `node_library` definition. The importer performs that cross-file
merge.

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

### Scene Post-Load Scripts

A `scene.extra` entry of DAZ type `scene_post_load_script` names a DAZ Script
(`.dse`/`.dsa`) that DAZ Studio runs when the asset loads — runtime work a static
import **cannot** replicate. For example, the Genesis 9 base figure's `scene.extra`
carries a "Character Addon Loader" `.dse` (which assigns the card-eyebrow textures
that nothing in the static content references), and a `Card Eyebrows.duf` preset
carries a "Remove Duplicate Eyebrows" cleanup `.dse`. The parser stores each such
reference on `Scene::post_load_scripts` (`ScenePostLoadScript`: `type` / `name` /
`script`) and exposes it as a flat, document-ordered list:

- `DsonDocument_GetScenePostLoadScriptCount` — `0` on invalid handle / none.
- `DsonDocument_GetScenePostLoadScriptName` — the entry `name`.
- `DsonDocument_GetScenePostLoadScriptType` — the entry `type` (e.g.
  `scene_post_load_script`).
- `DsonDocument_GetScenePostLoadScriptFile` — the content-relative `.dse`/`.dsa`
  path; `""` when the entry names no script.

The index covers **every** `scene.extra` entry that carries a `script` string, in
document order — the gate is the presence of the `script` reference, not the `type`
literal, and `type` is surfaced verbatim so a consumer can narrow to
`scene_post_load_script` itself. This is faithful passthrough only: the parser
neither resolves, loads, nor **executes** the script (consistent with the
no-recursive-load boundary and R6.4), so the consumer reads the reference and
decides — typically warning that the script's runtime effects are not captured by
the static import. An entry that carries both a `script` and a `PostLoadAddons`
manifest (the Genesis 9 base does) appears in **both** this list and the Post-Load
Addons list above; the two are modeled independently.

### Scene Animations

DAZ `preset_hierarchical_material` presets (e.g. a Genesis 9 companion MAT preset)
often declare `scene.materials` channels as bare `{id,type}` placeholders and park
the real channel values and `image_file` paths in `scene.animations` as `{url, keys}`
keyframes, where **key 0 is initialization data** (not runtime animation). An
animated `preset_pose` uses the same shape to carry a real multi-frame animation on
its bone / dial channels — the two cases are distinguished by per-channel key count,
not by `asset_info.type` (an animated pose preset is still `"preset_pose"`). The
parser stores each entry faithfully on `Scene::animations` — the verbatim `url`
property pointer, the first key's typed value (the 1.2.0 initialization-data
surface), and, since 2.16.0, every authored key's time plus the numeric value at
DSON-native double precision — and exposes it:

- `DsonDocument_GetSceneAnimationCount`
- `DsonDocument_GetSceneAnimationUrl` — the raw DSON pointer, e.g.
  `Genesis9Mouth#materials/Mouth:?diffuse/image_file`.
- `DsonDocument_GetSceneAnimationValueKind` — first-key value kind:
  `0` null · `1` number · `2` bool · `3` string · `4` color (`-1` invalid).
- `DsonDocument_GetSceneAnimationFloat` / `…Bool` / `…String` / `…ColorR` /
  `…ColorG` / `…ColorB` — key 0's value, read per kind (the 1.2.0 init-data
  surface; unchanged).
- `DsonDocument_GetSceneAnimationKeyCount` — authored key count on the channel
  (all kinds; `0` on invalid handle/index). The distinguishing signal between
  an animated (`> 1`) and a static (`1`) channel.
- `DsonDocument_GetSceneAnimationKeyTime` — one key's authored time in seconds
  at DSON-native double precision, returned verbatim (no fps inference, no
  snapping). Populated for **all** value kinds — DAZ times are always numeric —
  so the accessor works even on non-numeric channels. `0.0` on invalid, but
  `0.0` is also a legitimate value (the first key is authored at `t=0`); gate on
  `KeyCount` first.
- `DsonDocument_GetSceneAnimationKeyFloat` — one key's numeric value at
  DSON-native double precision when `ValueKind == 1` (number); `0.0` on invalid
  or when the channel is non-numeric (call the kind-typed 1.2.0 accessor above
  for key 0's bool/string/color value; multi-key non-numeric channels are not
  covered by this family — no such content is in evidence).

Per **R6.4** the parser does **not** apply these onto `scene.materials`: it does not
resolve the pointer, match the target channel, or fill an empty channel, and it leaves
the `image_file` string as the verbatim DSON path (not resolved against
`image_library`). The consumer reads both sections and decides whether key 0 overrides
its material, and reconstructs a multi-frame animation from the per-key surface. Every
entry is exposed (including `image_modification`/tiling rows). Since 2.16.0 all keys
are retained — key 0's typed value stays on the 1.2.0 fields byte-identically, and
the additional keys populate parallel `key_times` / (numeric-only) `key_values`
vectors on `Scene::animations`. No cross-section merge, no interpolation-hint
exposure (zero 3-element keys in evidence; a sample would be needed to widen scope),
no fps/playrange inference (not authored on the file — importer-side interpretation,
not parsing).

## Geometry And Faces

Geometry vertices are stored as a flat XYZ array in `Geometry::vertices`.
DSON commonly represents vertices as `{ "count": N, "values": [[x,y,z], ...] }`,
and the parser also accepts legacy flat arrays where practical.

Polylist faces are flattened into `Geometry::polylist.values`, while
`polylist_face_offsets` records where each face begins. Each face preserves the
DSON leading group/material indices before the vertex indices. This lets callers
reconstruct variable-length faces and recover both polygon group and material
group assignments.

A geometry may also author `material_uvs` as source-order
`[material-group, uv-set-name]` pairs. These per-surface assignments are exposed
faithfully through `DsonDocument_GetGeometryMaterialUVAssignmentCount`,
`DsonDocument_GetGeometryMaterialUVAssignmentMaterialGroup`, and
`DsonDocument_GetGeometryMaterialUVAssignmentUVSetName`. Both strings are
returned verbatim. The parser does not resolve the UV-set name to a sibling DSF,
join it to a scene/library material, or replace the geometry's
`default_uv_set`; those are importer decisions (R6.4).

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
- `DsonDocument_GetSceneMaterialChannelLayer{BlendMode,Opacity,Active,Invert,ColorR,ColorG,ColorB,Rotation,ScaleX,ScaleY,OffsetX,OffsetY,MirrorX,MirrorY}` —
  the per-layer compositing metadata (see Per-Layer Compositing below).

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
- `DsonDocument_GetImageLayer{BlendMode,Opacity,Active,Invert,ColorR,ColorG,ColorB,Rotation,ScaleX,ScaleY,OffsetX,OffsetY,MirrorX,MirrorY}` —
  the same per-layer compositing metadata by `(imageIndex, layerIdx)` (see Per-Layer
  Compositing below).

Resolving the `scene.animations` `"#fragment"` reference to an image index
(percent-decode, then match against `GetImageId`) and compositing the layers remain
consumer responsibilities.

#### Per-Layer Compositing

Per-layer compositing metadata is also modeled (1.4.0). Each `map` element's blend
`operation`, `transparency` (opacity), `active`/`invert` flags, `color` tint, and 2D
transform (`rotation`, `xscale`/`yscale`, `xoffset`/`yoffset`, `xmirror`/`ymirror`) are
parsed verbatim onto `Image::layers` with DAZ-semantic defaults, and exposed by the 14
`…Layer{BlendMode,Opacity,Active,Invert,ColorR,ColorG,ColorB,Rotation,ScaleX,ScaleY,OffsetX,OffsetY,MirrorX,MirrorY}`
accessors on **each** of the two surfaces above. The parser performs **no** compositing,
blending, or transform evaluation (R6.4) — a downstream consumer re-composites from the
raw values; `Opacity` is the raw `transparency` (1 = opaque). Sentinels follow the R1
family contract (string `""`, bool `false`, numeric `0.0`, scales `1.0`), so bound-check
the layer `Count` first — e.g. `Opacity`'s `0.0` sentinel is also a legitimate value. A
color-only base layer with no `url` stays excluded from `Image::layers` (so its
compositing fields are unreachable), unchanged from 1.3.0.

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
`FormulaOperation` (`op`, plus `val` for a constant scalar `push`, `url` for a
channel-reference `push`, or `val_array` for an array-valued `push`). A `push`
operand's `val` may be a JSON array instead of a scalar: `spline_tcb` curves store
each TCB knot as `[input, output, tension, continuity, bias]`. These arrays are
now retained verbatim and exposed raw via `...FormulaOperationValArrayCount` /
`...FormulaOperationValArrayElement` on both families (since 2.1.0). When
`...ValArrayCount > 0` the operand is array-valued; the scalar `...OperationVal`
is `0.0` for that form and is not meaningful — the Count is the discriminator.
Operation fields beyond `op`/`val`/`url` are not modeled yet but are recorded in
the unknown-key diagnostics rather than dropped.

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

**Channel value coercion & type-mismatch (2.2.1–2.2.2).** A modifier or material
`channel` value is read numerically. A JSON **bool** coerces to `1.0`/`0.0` (since
2.2.1 — e.g. the on-by-default `JCMs On` base-joint-corrective gate now reads `1.0`,
not the dropped `0.0`); a number reads as-is. Any **other** present type the numeric
read cannot represent — a string, object, or non-color array — keeps the `0.0`
default but, since 2.2.2, is no longer dropped silently: the parser records a
decorated, self-describing entry of the form
`value [channel "<id>": type=<jsontype>, used default]` into that context's trail
(deduped, like the rest of the trail). Such an entry is *not* an unrecognized key —
filter on the `used default` marker to separate the two.

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
