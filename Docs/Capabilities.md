# DsonParser - Capabilities

The current-state catalog of what the flat C ABI (`DsonParserAPI.h`) exposes, grouped by DSON data
area - the answer to "what can a consumer read out of a DAZ file." Cold/on-demand.

- **Authoritative signatures + `@since` tags:** `DsonParserAPI.h` (the single export header, CPP-3).
- **Per-release change history:** [`CHANGELOG.md`](../CHANGELOG.md) (ships beside the header/DLL).
- **Parse behavior, faithfulness rules, sentinels:** [`Docs/dson-parsing-overview.md`](dson-parsing-overview.md).
- **Return-value contract per function family:** [`Docs/Reference.md`](Reference.md).

Scope: the C ABI is **faithful and non-interpretive** (COD-101) - it exposes what the file declares
and never evaluates formulas, merges sections, or resolves references; that is consumer work
(`Docs/Intent.md`). Everything below ships in the current `2.x` line. "v1"/"v2" name informal
capability epochs, not library versions (`Docs/Reference.md`, "Version carriers"). Accessor names
below are cited for catalog navigation; the header is authoritative for exact spelling and `@since`.
The internal file/model map is in `Docs/Architecture.md` + the overview's File Map, not restated here.

## Geometry

- **Vertices** - X/Y/Z per vertex.
- **Polylist faces** - variable-stride offset array; both leading face ints (face-group `[0]`,
  material-group `[1]`) then vertex indices; `polygon_groups` / `polygon_material_groups` name arrays.
- **`default_uv_set_id`** per geometry.
- **Source-order `material_uvs`** - `GetGeometryMaterialUVAssignment{Count,MaterialGroup,UVSetName}`:
  verbatim per-surface material-group -> UV-set names; resolution left to the importer (2.12.0).
- **Geograft signal** - `GetGeometryIsGraft`: `true` only for a populated `graft` (non-empty
  `vertex_pairs`); empty `"graft": {}` -> `false` (1.5.0).
- **Geograft weld correspondence** - `GetGeometryGraft{VertexPair*,HiddenPoly*,BaseVertexCount,
  BasePolyCount}`: raw `vertex_pairs` (`[graft-local, base-figure]`), `hidden_polys`, declared base
  vertex/poly counts, file-local (2.9.0).
- **Rigidity** - `GetGeometryHasRigidity` + `GetGeometryRigidity{Weight*,Group*}`: complete raw
  `geometry.rigidity` sparse weights and groups, geometry-local and unevaluated (2.10.0).
- **Subdivision declaration** - `GetGeometryType` (the `subdivision_surface` / `polygon_mesh` gate,
  never defaulted), `GetGeometryEdgeInterpolationMode` / `GetGeometrySubDNormalSmoothingMode`, and
  `GetGeometryChannel*` (the `extra[]` `studio_geometry_channels` `/General/Mesh Resolution` block,
  view/render SubD levels, algorithm, edge/normal enums) with a field-presence mask. Raw: no SubD
  performed, enum `value` unresolved (2.19.0).
- **Polyline list (strand/curve geometry - dForce Strand-Based Hair)** -
  `GetPolyline{Count,SegmentCount,VertexCount,Vertex,GroupIndex,MaterialIndex}`: the `polyline_list`
  sibling of `polylist`. `polylist` vs `polyline_list` is the strand discriminator (never the `type`
  label); `segment_count` is its own authored datum. Kept separate from `polygon_count`/`polylist`
  (2.20.0).

## Skeleton / Nodes

- **Full `node_library`** - id, name, type, parent; per-bone `center_point`, `end_point`,
  `orientation`, `rotation_order` (default `"YXZ"`); per-node `translation`/`rotation`/`scale` (X/Y/Z),
  `general_scale` (default `1.0`); `unit_scale` from `asset_info` (default `1.0`); `Node::geometries`
  (`NodeGeometryRef`: id + url).
- **Authored transform channels** - `GetNode{Translation,Rotation,Scale}Channel{Count,Id,Label,Min,
  Max,Clamped,FieldPresenceMask}`: per-channel `id`/`label`/`min`/`max`/`clamped` + presence mask,
  beside the unchanged `...{X,Y,Z}` value reads (presence is the contract) (2.18.0).
- **Presentation** - `GetNodePresentation{Type,Label}` (DAZ "Content Type" / display name; `""` if
  absent) (1.5.0); `GetNodePresentationPreferredBase` - `presentation.preferred_base` conform-target
  base figure, verbatim (2.15.0).
- **Rigid-follow** - `GetNodeHasRigidFollow` / `GetNodeRigidFollow{RotationMode,ScaleMode*,
  ReferenceVertex*}`: the `node_library` `studio/node/rigid_follow` inline `rigidity_group`, raw and
  gated on the marker (2.8.0).
- **Scene-instance reads** (`scene.nodes`, kept separate from library): `GetSceneNode*` - parent,
  local translation/rotation/scale, `general_scale`, `rotation_order` (2.4.0); raw `center_point` /
  `orientation` XYZ + presence masks, `inherits_scale` value/presence (2.5.0); translation/rotation/
  scale XYZ presence masks + `general_scale`/`rotation_order` presence (2.6.0); `GetSceneNodeConform
  Target` - the `conform_target` "Fit To" URL, distinct from `parent` (2.17.0).
- **Scene-node shell `material_uvs`** - `GetSceneNodeShellMaterialUVAssignment{Count,MaterialGroup,
  UVSetName}`: `[material-group, uv-set-name]` from `studio/node/shell` extras, kept separate from the
  geometry-library assignments (2.13.0).
- **Scene-node authored channels** - `GetSceneNodeExtraChannel{Count,Id,Type,Label,Group,Value,Min,
  Max,Clamped,StepSize,FieldPresenceMask,EnumValueCount,EnumValue}`: every `scene.nodes` `extra[]`
  `studio_node_channels` channel in source order; motivating payload is the eye-icon `Visible` bool
  (`Value` = effective `current_value -> value`). Scene-only; effective-visibility resolution stays
  consumer-side (2.22.0).

## Skin binding

- `node_weights` primary + `local_weights` fallback; per-vertex influence cache (per-bone ->
  per-vertex inversion).
- `GetSkinJoint{Count,NodeId,WeightCount,WeightVertexIndex,Weight}` - raw parsed joint-to-vertex
  layout.
- `GetVertexInfluenceCount(handle, modifierIndex, vertexIndex, maxInfluences)`.
- `GetVertexBoneInfluence` - pre-cap normalized weights.
- `GetVertexBoneInfluenceCapped` - renormalized over top-M influences (correct for UE5
  `FSoftSkinVertex`).

## UV sets

- UV coordinates (U/V per UV vertex); multiple UV channels.
- `polygon_vertex_indices` face-varying mapping, exposed as sparse `[face, corner, uv_index]`
  overrides (`GetUVOverride*`).
- `GetUVSetId` / `GetUVSetName` / `GetUVSetLabel` - authored id/name/label (`label` = DAZ display
  name, verbatim; since 2.14.0).

## Materials and images

- **Source-order channels keyed by raw DAZ channel id** - no fixed engine-slot layout.
  `GetMaterialChannelCount` / `GetMaterialChannelId` (e.g. `"diffuse"`, `"Metallic Weight"`); the
  top-level `diffuse` block plus every `extra[].studio_material_channels.channels[]` entry, in file
  order. Per channel: `value`, `color` (RGB), `has_color`, `image_url`, `texture_path`. Material
  `groups`, `geometry_id`, `uv_set_id`.
- **Image linkage** - post-parse `image_url -> texture_path` (incl. percent-decoded fragment ids).
- **`map_size`** - `GetImage{Id,MapWidth,MapHeight}` (`Image::map_width`/`map_height`).
- **Layered image (LIE)** - `Image::map` handles plain string, `{"url":...}`, and LIE `map` arrays;
  all layers on `Image::layers` (url + label), `map_file` kept as the base layer.
  - Per scene-material-channel: `GetSceneMaterialChannelLayer{Count,TexturePath,Label}` (`Count` `0`
    plain / `N >= 2` layered; layer `0` == base `TexturePath`); layers attach only on an identity
    (id/url) match.
  - Per `image_library` index: `GetImageLayer{Count,TexturePath,Label}` (faithful stack size `1`
    plain / `N` LIE / `0` none), for images referenced outside an inline channel (1.3.0).
  - Per-layer compositing on **both** surfaces: `...Layer{BlendMode,Opacity,Active,Invert,ColorR,
    ColorG,ColorB,Rotation,ScaleX,ScaleY,OffsetX,OffsetY,MirrorX,MirrorY}` (1.4.0; raw, no
    compositing).

## Morph targets

- Sparse position deltas `[vertex_idx, dx, dy, dz]` + normal deltas (same format).
- `GetMorphName` / morph `channel_label` (with `name` fallback); `GetMorphId` - the modifier `id`
  in the filtered morph index space; `GetMorphGeometryId` - geometry id from the `parent` URL
  fragment.
- O(1) morph access via lazy `morphIndexCache`. The public `morphIndex` is a **filtered** index
  (modifiers with morph data), distinct from the raw `modifier_library` index.

## Modifiers (non-morph)

- **Push modifier** (`studio/modifier/push`, Geometry Shell "Mesh Offset") - `GetModifierIsPush` /
  `GetModifierPushOffset`: identity + effective "Offset Distance" (nested `extra[]`; offset prefers
  `current_value -> value`, raw cm; `0.0` is both sentinel and legit - gate on `IsPush`) (2.7.0).
- **Modifier target** - `GetModifierParent`: raw `modifier_library` `parent` URL verbatim incl.
  fragment (2.11.0).
- **Catalog / control tags** - `GetModifierPresentation{Type,Label}` (1.5.0); `GetModifierGroup` /
  `GetModifierRegion` / `GetModifierPresentationIcon` (Parameter-Settings Path/Region + `icon_large`,
  verbatim) (2.2.0).
- **Modifier extra channels** - `GetModifierExtraChannel{Count,Id,Type,Label,Group,Value,Min,Max,
  Clamped,StepSize,FieldPresenceMask,EnumValueCount,EnumValue}`: the `studio_modifier_channels`
  block; `...Value` returns the **effective** `current_value -> value` (load-bearing for dForce SBH
  generation settings) (2.21.0).
- **Scene-modifier target + value-kind** - `GetSceneModifierParent` (the `scene.modifiers` fit/dial
  link); `GetSceneModifierChannelValueKind` (`0` null / `1` number / `2` bool / `3` string; `-1`
  invalid) + `GetSceneModifierChannelValueString`, alongside the double `GetSceneModifierChannelValue`
  (bool coerced per 2.2.1) (2.17.0).

## Scene manifest and animations

Scene-level references and keyframe data a `character`/`preset` file carries beyond `scene.nodes`.
Faithful passthrough: the parser surfaces the references but does not resolve paths, load referenced
files, execute scripts, or apply keyframes onto `scene.materials` (R6.4).

- **Post-load addon manifest** (DAZ "Character Addon Loader") - `GetScenePostLoadAddon{Count,Slot,
  AssetName,AssetFile,MatPreset}`: companion conforming figures (Genesis 9 eyes/mouth/eyelashes/tear/
  eyebrows) a preset instances but does not list in `scene.nodes`; flat document-ordered index over
  every `scene.extra` `PostLoadAddons` map (1.1.0).
- **Post-load scripts** - `GetScenePostLoadScript{Count,Name,Type,File}`: `scene.extra`
  `scene_post_load_script` DAZ Script (`.dse`/`.dsa`) references DAZ Studio runs at load; the parser
  neither resolves nor executes them (2.3.0).
- **Scene animations** - `GetSceneAnimation{Count,Url,ValueKind,Float,Bool,String,ColorR,ColorG,
  ColorB}`: each `scene.animations` `{url, keys}` channel's verbatim pointer + key-0 typed value
  (material-init data for `preset_hierarchical_material`; `ValueKind` `0` null / `1` number / `2` bool
  / `3` string / `4` color, `-1` invalid) (1.2.0). Full-keyframe surface -
  `GetSceneAnimationKey{Count,Time,Float}`: authored key count (the animated-vs-static signal),
  per-key time (s, verbatim), and numeric value (2.16.0).

## Formulas

DAZ formulas drive corrective (JCM/FHM) and character/control morphs. The parser **stores and
exposes** each modifier's `formulas` without evaluating them; evaluation, RPN solving, `output`/`url`
resolution, and recursive external-file loading are consumer work. Formulas attach to ordinary
modifiers (a control morph often has no `morph`/`deltas`), so the accessors live on **both** modifier
index spaces, not on `morphIndex`.

- **By raw `modifier_library` index** - `GetModifierFormula{Count,Output,Stage}`,
  `GetModifierFormulaOperation{Count,Op,Val,Url}` (JCM/FHM correctives + control-morph children in
  their own `.dsf` files).
- **By `scene.modifiers` index** - `GetSceneModifierFormula{Count,Output,Stage}`,
  `GetSceneModifierFormulaOperation{Count,Op,Val,Url}` (the `.duf` control-morph top node).
- **Array-valued operands** - `...FormulaOperationValArray{Count,Element}` on both families:
  `spline_tcb` TCB knots ride the `val` key as a JSON array, retained verbatim; `...ValArrayCount > 0`
  is the discriminator (the scalar `...OperationVal` is `0.0` for that form) (2.1.0).
- **Dial state for evaluation** - `Get{Modifier,SceneModifier}Channel{Value,Min,Max,Clamped}` (the
  control's `current_value -> value` dial plus clamp range) and `GetMorphId` (maps a formula `output`
  `#fragment` to its delta-bearing leaf morph). These complete the walk-and-evaluate surface; the
  parser stays single-document and non-evaluating.

## Known limitations

- **Formula evaluation is consumer-side** - formulas are stored and exposed (above), never evaluated;
  pose-driven correctives and character-control composition fire only after the importer integrates.
- **Windows only** - validated/used on Win64; no Mac/Linux build configuration yet.
- **No weight normalization at parse time** - raw weights stored as-is; normalization/capping happen
  at query time via `GetVertexBoneInfluenceCapped` (by design - lossless parser).
- **Single-document** - no recursive loading of external referenced DSF/DUF assets; no full DSON
  semantic validation; single-member gzip only (see the overview's Current Boundaries).

## Audit status

The v1 coverage audit closed after 4 passes with 0 remaining gaps (`Docs/AuditGuide.md` carries the
audit procedure; the v2 formula-section addendum is folded in there). Per-pass gap history is in git.
