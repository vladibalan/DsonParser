# Changelog — DsonParser C ABI

Changes to the flat C ABI (`DsonParserAPI.h`). Ships beside the header/DLL so
consumers see what changed without this repo's source. Newest first; stop at the
version you already integrate against. Policy: docs/versioning.md.

SemVer with C-ABI semantics: MAJOR = breaking ABI · MINOR = additive (binary-compatible) · PATCH = internal fix (`DsonParserAPI.h` byte-identical).
Entry sigils: `+` added · `~` changed · `-` removed/deprecated · `!` fixed.

## Unreleased

Nothing yet — new C-ABI changes land here, then move under a version heading on release.

## 2.11.0 — 2026-07-08 · MINOR (added)

Exposes each `modifier_library` item's authored `parent` URL verbatim. This lets
consumers associate multiple geometry-shell push modifiers with their individual
target geometries by URL fragment while keeping matching and URL resolution in
the importer (R6.4). The accessor is general to all library modifiers, uses the
raw `modifier_library` index, and returns `""` when `parent` is absent or the
handle/index is invalid. Purely additive: all existing symbols and behavior are
unchanged.
+ DsonDocument_GetModifierParent — authored modifier parent URL, including its target fragment

## 2.10.0 — 2026-07-07 · MINOR (added)

Exposes a geometry-library entry's complete authored `geometry.rigidity` block:
sparse vertex/weight rows plus every source-order rigidity group field. Values
remain raw and file-local (R6.4): vertex indices stay in the graft geometry's
index space, node references stay as-authored, and the parser performs no remap,
weld, reference resolution, scale derivation, or rigidity evaluation. Presence is
independent of array sizes, so an authored empty object remains distinguishable
from absence. The DAZ JSON key is parsed with its authored misspelling,
`use_tranform_bones_for_scale`; the public accessor uses correctly spelled API
English. Purely additive: all existing symbols and behavior are unchanged.
Counts return `0` on invalid input, strings `""`, bools `false`, weight `0.0`, and
vertex-index accessors `-1`. Verified by a deterministic whole-block regression
and the installed Genesis 9 Male Genitalia proof (1,354 sparse weights; complete
`Gens` group). Non-`none` rotation and mixed `primary`/`secondary`/`none` scale
modes are covered by the synthetic regression; no installed rotation-using graft
was found in the targeted asset search.
+ DsonDocument_GetGeometryHasRigidity — whether a valid authored rigidity object exists, including an empty object
+ DsonDocument_GetGeometryRigidityWeightCount — number of valid parsed sparse weight rows
+ DsonDocument_GetGeometryRigidityWeightVertexIndex — one sparse row's geometry-local vertex index
+ DsonDocument_GetGeometryRigidityWeight — one sparse row's raw authored weight
+ DsonDocument_GetGeometryRigidityGroupCount — number of valid source-order rigidity groups
+ DsonDocument_GetGeometryRigidityGroupId — group's authored id
+ DsonDocument_GetGeometryRigidityGroupRotationMode — group's authored rotation mode
+ DsonDocument_GetGeometryRigidityGroupScaleModeCount — number of authored per-axis scale modes
+ DsonDocument_GetGeometryRigidityGroupScaleMode — one authored per-axis scale mode
+ DsonDocument_GetGeometryRigidityGroupReferenceVertexCount — number of reference vertices
+ DsonDocument_GetGeometryRigidityGroupReferenceVertex — one geometry-local reference-vertex index
+ DsonDocument_GetGeometryRigidityGroupMaskVertexCount — number of mask vertices
+ DsonDocument_GetGeometryRigidityGroupMaskVertex — one geometry-local mask-vertex index
+ DsonDocument_GetGeometryRigidityGroupReference — authored reference node id/name
+ DsonDocument_GetGeometryRigidityGroupTransformNodeCount — number of authored transform-node references
+ DsonDocument_GetGeometryRigidityGroupTransformNode — one authored transform-node id/name
+ DsonDocument_GetGeometryRigidityGroupUseTransformBonesForScale — raw value of DAZ's misspelled `use_tranform_bones_for_scale`

## 2.9.0 — 2026-07-05 · MINOR (added)

Exposes the raw geograft **weld correspondence** from a geometry's `graft` block —
the two arrays `GetGeometryIsGraft` (1.5.0) already keys on but that reached no
consumer, plus the block's declared base counts. This is the parser side of the
DsonToUnreal composed-figure geograft weld: `vertex_pairs` gives the boundary weld
pairs as `[graft-local vertex, base-figure vertex]`, `hidden_polys` gives the
base-figure polygons the graft hides on weld (empty for an additive graft), and
`GraftBaseVertexCount`/`GraftBasePolyCount` give the graft block's declared
`vertex_count`/`poly_count` — the base-figure resolution the base-side pair indices
are expressed in. All raw and file-local (R6.4): the parser does **no** remap,
weld, or reorder; the importer owns the DSON→import-point mapping (same posture as
the raw LIE per-layer data and the rigid-follow reference vertices). Purely
additive — `GetGeometryIsGraft` and every other symbol are unchanged. The
pair-count accessor returns the parsed `values` length, which is authoritative over
DAZ's declared `vertex_pairs.count` (they can disagree). Count family → `0` on
invalid; the vertex/poly accessors → `-1` (index 0 is legitimate, as with the
rigid-follow reference-vertex family). Proof assets (values read directly from the
DSON, asserted against the accessors by the `DsonTest2` harness): Genesis 9
Anatomical Elements Female (`Genesis9FemaleGenitalia.dsf` — 82 weld pairs [DAZ
declares 84], 180 hidden polys, declared base 25182 verts / 25156 polys) and the
additive Toon Base Shorts Geograft (`BaseShortsGeoGraft_318.dsf` — 106 weld pairs,
0 hidden polys, declared base 8256 / 8130).
+ DsonDocument_GetGeometryGraftVertexPairCount — number of parsed weld pairs (values length, not DAZ's declared count); 0 when absent or invalid
+ DsonDocument_GetGeometryGraftVertexPairGraftVertex — a pair's graft-local vertex index; -1 when absent or invalid
+ DsonDocument_GetGeometryGraftVertexPairBaseVertex — a pair's base-figure vertex index; -1 when absent or invalid
+ DsonDocument_GetGeometryGraftHiddenPolyCount — number of base-figure polys hidden on weld (0 for an additive graft); 0 when absent or invalid
+ DsonDocument_GetGeometryGraftHiddenPoly — a hidden base-figure polygon index; -1 when absent or invalid
+ DsonDocument_GetGeometryGraftBaseVertexCount — graft block's declared vertex_count (base target resolution the base-side indices live in); 0 when absent or invalid
+ DsonDocument_GetGeometryGraftBasePolyCount — graft block's declared poly_count; 0 when absent or invalid

## 2.8.0 — 2026-07-03 · MINOR (added)

Adds faithful access to the rigid-follow rigidity group carried by node_library
nodes. The payload is nested in `extra[]` and gated on the
`studio/node/rigid_follow` marker, structurally mirroring the 2.7.0 push-modifier
support. Scene-node instances carry only bare markers, so these accessors remain
on the node_library family and perform no cross-section merge or follow
evaluation. This is purely additive: no existing symbol or behavior changes.
`ReferenceVertex` returns `-1` for invalid input because vertex index 0 is valid.
Verified on `JB Jewel Bikini Bottom And Wrap.duf`: 15 rigid-follow nodes with
reference-vertex counts 33, 40, 40, 39, 4, 4, 4, 6, 6, 6, 15, 4, 4, 4, 4
(213 total), all using rotation mode `full` and scale modes `[none, none, none]`.
+ DsonDocument_GetNodeHasRigidFollow — true iff the node_library item has a studio/node/rigid_follow extra entry carrying a rigidity_group
+ DsonDocument_GetNodeRigidFollowRotationMode — raw rigidity_group rotation_mode; empty when absent or invalid
+ DsonDocument_GetNodeRigidFollowScaleModeCount — number of authored per-axis scale modes; 0 when absent or invalid
+ DsonDocument_GetNodeRigidFollowScaleMode — raw scale mode by authored array index; empty when absent or invalid
+ DsonDocument_GetNodeRigidFollowReferenceVertexCount — number of raw reference-vertex indices; 0 when absent or invalid
+ DsonDocument_GetNodeRigidFollowReferenceVertex — raw followed-geometry vertex index; -1 when absent or invalid

## 2.7.0 — 2026-07-02 · MINOR (added)

Geometry-shell "Mesh Offset" **push modifier** identity + effective offset value.
A DAZ Geometry Shell wearable is the base mesh pushed outward along its normals by
a `studio/modifier/push` modifier's "Offset Distance (cm)" channel; an importer
reconstructing the shell needs that distance. Both the push **type marker**
(`extra[].type == "studio/modifier/push"`) and the offset **channel** (nested under
`extra[].type == "studio_modifier_channels"` → `channels[0].channel`) live inside
the modifier's `extra[]`, not at its top level — so `GetModifierType` (top-level
`type`, absent → `""`) and `GetModifierChannelValue` (top-level `channel`, absent →
`0.0`) never surfaced them. These two additive accessors read the nested data
faithfully: `IsPush` from the push marker, `PushOffset` from the offset channel
preferring `current_value` over `value` (the established ≥2.4.0 channel-read
convention), gated on the push marker so a non-push modifier's channels are never
read as an offset. Faithful/unevaluated (R6.4) — the raw value is returned (units
are the channel's cm; the consumer converts) and no cross-section merge is done.
`extra` is now in the modifier `knownKeys`, so a shell modifier no longer reports it
as an unknown key. **No existing accessor changes** — `GetModifierType` and
`GetModifierChannelValue` still return `""`/`0.0` for such a modifier
(binary-compatible; all existing symbols intact). `PushOffset`'s `0.0` is both the
sentinel and a legitimate authored value, so gate on `IsPush` first. Verified on the
G9 "Sexy Skinz" swimsuit shell (`IsPush=true`, `PushOffset=0.1`) via a crafted
`LoadFromString` fixture in the DsonTest2 harness.
+ DsonDocument_GetModifierIsPush — true iff the modifier_library item declares a studio/modifier/push entry in extra[] (false = not a push / invalid handle or index)
+ DsonDocument_GetModifierPushOffset — its effective "Offset Distance" channel value from the nested studio_modifier_channels (current_value → value; raw, cm); 0.0 when not a push, no offset channel, or invalid — gate on GetModifierIsPush

## 2.6.0 — 2026-07-01 · MINOR (added)

Completes the scene-node authored-presence surface begun in 2.5.0. Translation,
rotation, and scale now expose per-component presence masks, while
`general_scale` and `rotation_order` expose explicit presence queries. This lets
an importer distinguish deliberately authored identity values (translation or
rotation `0`, scale or general scale `1`, and the default rotation order) from an
absent sparse override. Values remain raw reads from the opened file's
`scene.nodes`; the parser does not merge referenced `node_library` definitions.
Existing symbols and semantics are unchanged.
+ DsonDocument_GetSceneNodeTranslationPresenceMask — authored numeric translation components, ORed from the public X/Y/Z bits (`0` when none or invalid)
+ DsonDocument_GetSceneNodeRotationPresenceMask — authored numeric rotation components, ORed from the public X/Y/Z bits (`0` when none or invalid)
+ DsonDocument_GetSceneNodeScalePresenceMask — authored numeric scale components, ORed from the public X/Y/Z bits (`0` when none or invalid)
+ DsonDocument_GetSceneNodeHasGeneralScale — distinguishes an authored numeric `general_scale` (including `1`) from absence or a non-numeric selected value
+ DsonDocument_GetSceneNodeHasRotationOrder — distinguishes an authored string `rotation_order` (including `"YXZ"`) from absence or a non-string value

## 2.5.0 — 2026-07-01 · MINOR (added)

Scene-node instances now expose their raw, as-authored `center_point`,
`orientation`, and `inherits_scale`. Vector presence masks and an explicit
`inherits_scale` presence query distinguish authored zero/false from an absent
override. The parser does not merge these values with `node_library` definitions;
the importer resolves the referenced definition, often from a second file, and
decides how to combine it. Existing symbols and semantics are unchanged.
+ DSONPARSER_VECTOR_COMPONENT_X / _Y / _Z — public `0x1` / `0x2` / `0x4` bits for scene-node vector presence masks
+ DsonDocument_GetSceneNodeCenterPointX — raw authored instance center X (`0.0` on invalid handle/index or absence; inspect the mask)
+ DsonDocument_GetSceneNodeCenterPointY — raw authored instance center Y (`0.0` on invalid handle/index or absence; inspect the mask)
+ DsonDocument_GetSceneNodeCenterPointZ — raw authored instance center Z (`0.0` on invalid handle/index or absence; inspect the mask)
+ DsonDocument_GetSceneNodeCenterPointPresenceMask — authored numeric center components, ORed from the public X/Y/Z bits (`0` when none or invalid)
+ DsonDocument_GetSceneNodeOrientationX — raw authored instance orientation X (`0.0` on invalid handle/index or absence; inspect the mask)
+ DsonDocument_GetSceneNodeOrientationY — raw authored instance orientation Y (`0.0` on invalid handle/index or absence; inspect the mask)
+ DsonDocument_GetSceneNodeOrientationZ — raw authored instance orientation Z (`0.0` on invalid handle/index or absence; inspect the mask)
+ DsonDocument_GetSceneNodeOrientationPresenceMask — authored numeric orientation components, ORed from the public X/Y/Z bits (`0` when none or invalid)
+ DsonDocument_GetSceneNodeInheritsScale — raw authored boolean value (`false` on invalid handle/index or absence; inspect `HasInheritsScale`)
+ DsonDocument_GetSceneNodeHasInheritsScale — distinguishes an authored `inherits_scale` (including `false`) from absence

## 2.4.0 — 2026-07-01 · MINOR (added)

Scene-node instances now expose their verbatim parent pointer and complete local
transform so importers can place repeated rigid parts whose per-copy offsets exist
only in `scene.nodes`. Transform channel objects now prefer `current_value` and fall
back to `value`, matching other scene-channel reads. The new exports are additive
and binary-compatible; existing symbols remain intact.
+ DsonDocument_GetSceneNodeParent — verbatim instance parent pointer ("" on invalid handle/index)
+ DsonDocument_GetSceneNodeTranslationX — local translation X (0.0 on invalid handle/index)
+ DsonDocument_GetSceneNodeTranslationY — local translation Y (0.0 on invalid handle/index)
+ DsonDocument_GetSceneNodeTranslationZ — local translation Z (0.0 on invalid handle/index)
+ DsonDocument_GetSceneNodeRotationX — local rotation X (0.0 on invalid handle/index)
+ DsonDocument_GetSceneNodeRotationY — local rotation Y (0.0 on invalid handle/index)
+ DsonDocument_GetSceneNodeRotationZ — local rotation Z (0.0 on invalid handle/index)
+ DsonDocument_GetSceneNodeScaleX — local scale X (0.0 on invalid handle/index; valid unauthored scale defaults to 1.0)
+ DsonDocument_GetSceneNodeScaleY — local scale Y (0.0 on invalid handle/index; valid unauthored scale defaults to 1.0)
+ DsonDocument_GetSceneNodeScaleZ — local scale Z (0.0 on invalid handle/index; valid unauthored scale defaults to 1.0)
+ DsonDocument_GetSceneNodeGeneralScale — local general scale (1.0 on invalid handle/index)
+ DsonDocument_GetSceneNodeRotationOrder — stored rotation order ("" on invalid handle/index; valid unauthored order defaults to "YXZ")
! ParseTransformVector3 — transform channels now prefer current_value before value for both scene-node and library-node reads, restoring placement authored only as current_value (including the Jewel Bikini scene instances and their Gem Drop library local-transform nodes, which previously read as zero); base-figure rigs remain unchanged because they author value-only transforms (verified on Genesis 9: 139 nodes, 2502 transform channels, 0 carrying current_value)

## 2.3.0 — 2026-06-22 · MINOR (added)

Exposes the DAZ post-load **script** references carried in `scene.extra` (DSON
`scene_post_load_script` entries) — the `name`, `type`, and `script` path (a
`.dse`/`.dsa`). DAZ Studio runs these scripts at load to do work a static import
cannot replicate (e.g. the Genesis 9 base "Character Addon Loader" `.dse` that
assigns the card-eyebrow textures nothing in the static content references, or a
"Remove Duplicate Eyebrows" cleanup), so the static import silently drops that work
and until now could not even see a script was present. Surfacing the references lets
a consumer warn (no-silent-fails) that the import may be incomplete and which script
is responsible. Faithful passthrough — the parser neither loads, resolves, nor runs
the script, and does not merge it onto any other section (R6.4). Captured for every
`scene.extra` entry that names a `script`, in document order; `type` is surfaced
verbatim so a consumer can narrow to `scene_post_load_script` itself. The same entry
may also appear in the existing `GetScenePostLoadAddon*` family when it carries both
(the Genesis 9 base does). `DsonParserAPI.h` gains four exports; binary-compatible,
all existing symbols intact:
+ DsonDocument_GetScenePostLoadScriptCount — number of scene.extra script references (0 on invalid handle / none)
+ DsonDocument_GetScenePostLoadScriptName — the entry "name" ("" if absent / invalid index)
+ DsonDocument_GetScenePostLoadScriptType — the entry "type", e.g. "scene_post_load_script" ("" if absent / invalid index)
+ DsonDocument_GetScenePostLoadScriptFile — the content-relative .dse/.dsa script path ("" if absent / invalid index)

## 2.2.3 — 2026-06-21 · PATCH (fix)

A gzip-wrapped DSON member whose 8-byte trailer (CRC32 + ISIZE) is all-zero now loads
instead of being rejected with "gzip CRC32 mismatch", as long as the DEFLATE stream itself
inflated cleanly. A real shipped DAZ product (3D Universe "Pose Architect P1", G8F) writes
52/53 of its .dsf members with a zeroed trailer; DAZ Studio loads them because it does not
validate the trailer, so our strict check was stricter than DAZ and silently dropped
legitimately-shipped, DAZ-loadable assets from a catalog. The internal inflater only
returns success on a final-block DEFLATE termination, so a clean inflate already proves the
payload is whole -- a blank trailer is therefore accepted and the comparison skipped.
Genuine truncation/corruption still fails inside inflate (before the trailer is read), and
any present, non-zero, mismatched trailer is still fully enforced; both trailer fields must
be zero to skip, so a coincidentally-zero CRC with a non-zero ISIZE still takes the strict
path. `DsonParserAPI.h` is byte-identical (no signature change); this changes only the
internal gzip loader behavior, observable through the existing loaders:
! DsonDocument_LoadFromBuffer / DsonDocument_LoadFromFile -- a gzip DSON with an all-zero trailer + clean DEFLATE now loads (was: rejected "gzip CRC32 mismatch")

## 2.2.2 — 2026-06-21 · PATCH (changed)

The unknown-key audit trail now also surfaces a channel value that is present but of a
type the numeric channel-value read cannot represent (after 2.2.1: anything other than a
number or a bool — a string, object, or non-color array). Previously such a value was
silently dropped to the 0.0 default and recorded nowhere — the same blind spot that hid the
bool "JCMs On" gate before 2.2.1. The numeric still falls back to its default (no new
coercion — there is no faithful numeric for those types); the drop is now VISIBLE as a
decorated entry in the existing per-context trail, distinguishable from a genuine unknown
key. `DsonParserAPI.h` is byte-identical (no new/changed symbol); this enriches what the
existing accessors report:
~ DsonDocument_GetUnknownKey / DsonDocument_GetUnknownKeyCount — now also list a decorated
  "<key> [channel ...: type=..., used default]" entry for a present-but-unrepresentable
  modifier or material channel value (deduped per context, as the trail already is).

## 2.2.1 — 2026-06-21 · PATCH (fix)

Boolean-typed channel values now coerce to numeric (true->1.0, false->0.0) in the
numeric channel-value reads, instead of silently dropping to the 0.0 default. A DSON
channel can be `type:"bool"` with a JSON-boolean `value` (e.g. the G8/G8.1 "JCMs On"
base-joint-corrective master gate, on-by-default); the number-only read could not
represent it, so a present `value:true` collapsed to 0.0 -- zeroing every corrective
that gate multiplies. The value is read faithfully now. `DsonParserAPI.h` is
byte-identical (no signature change); this corrects the behavior of existing accessors:
! DsonDocument_GetModifierChannelValue / DsonDocument_GetSceneModifierChannelValue -- bool channel value now 1.0/0.0 (was 0.0)
! DsonDocument_GetMaterialChannelValue / DsonDocument_GetSceneMaterialChannelValue -- bool channel value now 1.0/0.0 (was 0.0)

## 2.2.0 — 2026-06-20 · MINOR (additive)

Three more faithful modifier_library catalog fields, the additive sibling of the 1.5.0
presentation.type/label work, for an Importer building a control/expression inventory
over a figure's sliderable channels. DAZ stores a control's Parameter-Settings "Path" as a
modifier-level `group` and "Region" as a modifier-level `region` (siblings of
`channel`/`presentation`), and the per-control thumbnail as `presentation.icon_large`; all
three were parsed-as-known but dropped. They are now stored and exposed verbatim — no
interpretation/normalization; the icon path stays percent-encoded as stored, the consumer
resolves/loads it. "" when the field is absent or the index is invalid, the same sentinel as
GetModifierPresentation{Type,Label}. group/region were already in the modifier knownKeys set,
so the unknown-key audit is unchanged. Verified against TestFiles
(body_bs_NipplesFeminine_HD3.dsf modifier[0]: group="/Feminine", region="Chest",
icon="/data/Daz%203D/Genesis%209/Base/Morphs/Daz%203D/Base/body_bs_NipplesFeminine_HD3.png";
BaseJointCorrectives.dsf modifier[0] "JCMs On": group="/General/Misc", region absent -> "",
icon present-but-empty -> "").
+ DsonDocument_GetModifierGroup -> modifier_library item modifier-level "group" (DAZ Parameter Settings "Path"; "" = none/invalid)
+ DsonDocument_GetModifierRegion -> modifier_library item modifier-level "region" (DAZ Parameter Settings "Region"; "" = none/invalid)
+ DsonDocument_GetModifierPresentationIcon -> modifier_library item presentation.icon_large thumbnail path, raw/verbatim ("" = none/invalid)

## 2.1.0 — 2026-06-17 · MINOR (additive)

Array-valued formula operands (spline_tcb knots). A formula `push` operation may carry a
JSON array as its `val` instead of a scalar - this is how `spline_tcb` curves store their
TCB knots (`[input, output, tension, continuity, bias]` per knot). The scalar `val`
accessors could not represent that array, so the parser silently collapsed each knot array
to `0.0` at parse time (the `val` key is known, so the knots were never even in the
unknown-key trail) - losing the entire spline curve while the surrounding `push(input)`,
scalar `push(count)`, and `spline_tcb` token survived. The knot arrays are now retained
verbatim on each operation and exposed raw (numeric elements, in source order) on both
formula families; the parser still does not evaluate the spline. Disambiguation: when
...ValArrayCount > 0 the operand is array-valued - read its elements; the scalar
...OperationVal is not meaningful (stays 0.0). When ...ValArrayCount == 0 the operand is
scalar/url as before, so existing consumers that never call the new accessors get
byte-identical behavior. 0.0 is also a legitimate element value (tension/continuity/bias
are observed 0), so bound-check the Count first - the Count, not a 0.0 element, is the
array-vs-scalar discriminator.
+ DsonDocument_GetModifierFormulaOperationValArrayCount -> element count of an array-valued operand; 0 if the operand is scalar/url/absent (modifier_library)
+ DsonDocument_GetModifierFormulaOperationValArrayElement -> one raw array element by index, in source order; 0.0 if out of range (modifier_library)
+ DsonDocument_GetSceneModifierFormulaOperationValArrayCount -> as above, scene.modifiers family
+ DsonDocument_GetSceneModifierFormulaOperationValArrayElement -> as above, scene.modifiers family

## 2.0.0 — 2026-06-13 · MAJOR (breaking)

Removed two dead UV accessors that surfaced the legacy flat-int "polygon vertex index"
representation. Real DAZ uv_set DSFs encode `polygon_vertex_indices` as sparse
`[face, corner, uv_index]` triplets, which the parser models as UV overrides; the flat-int
path has been empty since that migration, so these two functions returned nothing for any
real asset. No known consumer binds them — UV index data is read through the GetUVOverride*
family, which is **unchanged**. The internal `UVSet::polygon_vertex_indices` field and its
parse branch are removed with them; the sparse triplet path (uv_overrides) is intact.
Migration: a caller wanting per-corner UV indices uses DsonDocument_GetUVSetVertexCount +
the GetUVOverride{Count,Face,Corner,UVIndex} family (seed identity uv_index = vertex_index,
then apply the sparse overrides) — exactly what the importer already does.
- DsonDocument_GetUVPolygonVertexIndexCount — removed (dead legacy flat-int count; use the GetUVOverride* sparse family)
- DsonDocument_GetUVPolygonVertexIndex — removed (dead legacy flat-int accessor; use the GetUVOverride* sparse family)

## 1.6.0 — 2026-06-12 · MINOR (additive)

Threading contract. DsonParser is now safe for concurrent use across threads on **distinct**
document handles: all parsed data, lazy caches, and returned scratch strings live on the handle
(DsonContext), so two threads reading two handles share no mutable state. The sole former
exception — the process-global last-error slot behind DsonParser_GetLastError() — is now a
per-thread (function-local thread_local) slot, so concurrent DsonDocument_Create/LoadFrom* calls
no longer race on it and each thread's GetLastError() reflects its own last call. No symbol,
signature, or single-threaded behavior changed; the header now states the contract. Same-handle
concurrent use remains the caller's responsibility (lazy caches mutate on first read).
! DsonParser_GetLastError -> per-thread (thread_local) storage; was process-global - fixes a data race under concurrent loads, no API/signature change

## 1.5.0 — 2026-06-12 · MINOR (additive)

Declared asset-catalog metadata, for an Importer building a faithful catalog of installed
`.duf`/`.dsf` assets from declared data only (no folder inference). `presentation.{type,label}`
is exposed **per library item** — the parser does not pick a single "asset" content type or
select a defining item; the consumer maps `presentation.type` (the DAZ "Content Type") and
chooses. Node items carry it for figures/clothing/hair/props ("Follower", "Wardrobe/…"),
modifier items for shapes ("Modifier/Shape"); a preset with no presentation reports `""`
(→ the consumer's "unknown"). The geograft signal is a **populated** graft: an empty
`"graft": {}` (carried by base figures and by Genesis 9 Eyes/Eyelashes) is NOT a graft —
only a graft with `vertex_pairs` is. Faithful single-file exposure (R6.4): no classification,
no document-level resolution, no cross-section merge. Adding the three keys to their
`knownKeys` sets also clears them from the unknown-key audit noise. Verified against
TestFiles (`test.dsf` modifier = "Modifier/Shape"; `Genesis9.json` base geom is_graft=false,
node[0]="Actor") and an external geograft (`Genesis9FemaleGenitalia.dsf` → is_graft=true,
84 vertex_pairs; node[0]="Follower").
+ DsonDocument_GetNodePresentationType → node_library item presentation.type (DAZ "Content Type"; "" = none/invalid)
+ DsonDocument_GetNodePresentationLabel → node_library item presentation.label ("" = none/invalid)
+ DsonDocument_GetModifierPresentationType → modifier_library item presentation.type ("" = none/invalid)
+ DsonDocument_GetModifierPresentationLabel → modifier_library item presentation.label ("" = none/invalid)
+ DsonDocument_GetGeometryIsGraft → true iff the geometry declares a populated graft (vertex_pairs present); false for empty/absent graft

## 1.4.0 — 2026-06-10 · MINOR (additive)

Per-layer LIE compositing metadata. DAZ Layered Image (LIE) `map` elements carry the
per-layer instructions a faithful re-composite needs — blend `operation`, `transparency`
(opacity), `active`/`invert` flags, `color` tint, and a 2D transform (`rotation`,
`xscale`/`yscale`, `xoffset`/`yoffset`, `xmirror`/`ymirror`). Through 1.3.0 only each
layer's `url`/`label` was retained (1.3.0: "per-layer blend op/transform stay
unmodeled"); these are now parsed faithfully onto `Image::layers` (raw values, DAZ-
semantic defaults) and exposed on **both** existing layer surfaces — the per-image
index family (1.3.0) and the per-scene-material-channel family (1.0.0) — at parity with
the path+label accessors. Parser stays faithful (R6.4): raw passthrough, no compositing
performed, no cross-section merge; the consumer re-composites from the raw layers. A
color-only base layer with no `url` stays excluded from `Image::layers` (so its fields
are unreachable), unchanged from 1.3.0. Verified against TestFiles/HID_Nancy_9.duf (G9
HID Nancy head diffuse + SSS Color, 4-layer stacks) and a crafted inline-`#id` snippet.

28 new accessors = 14 shared suffixes × 2 prefixes (per-image
`DsonDocument_GetImageLayer…`, args `(handle, imageIndex, layerIdx)`; per-channel
`DsonDocument_GetSceneMaterialChannelLayer…`, args `(handle, sceneMatIndex, channelIdx, layerIdx)`):
+ …BlendMode → raw "operation" blend string, e.g. "blend_source_over"/"blend_multiply" ("" = invalid/absent)
+ …Opacity → raw "transparency" (1.0 = opaque). NB sentinel 0.0 collides with a legitimately-transparent layer — bound-check Count first
+ …Active → "active" flag (false = invalid)
+ …Invert → "invert" flag (false = invalid)
+ …ColorR / …ColorG / …ColorB → "color" RGB tint components (0.0 = invalid)
+ …Rotation → "rotation" in degrees (0.0 = invalid)
+ …ScaleX / …ScaleY → "xscale" / "yscale" (1.0 = invalid; scale exception per the R1 contract)
+ …OffsetX / …OffsetY → "xoffset" / "yoffset" (0.0 = invalid)
+ …MirrorX / …MirrorY → "xmirror" / "ymirror" mirror flags (false = invalid)

## 1.3.0 — 2026-06-09 · MINOR (additive)

Per-layer LIE map stack of an `image_library` entry, reachable **by image index**.
The layers were already parsed onto `Image::layers` but exposed only incidentally —
copied onto a material channel that inline-references the image. An image referenced
from elsewhere (e.g. a `scene.animations` `diffuse/image` binding to a base-figure LIE
such as the Genesis 9 eyes) had its layer stack unreachable; `GetSceneAnimationString`
returned only the raw `"#fragment"`. These accessors read the same parsed
`Image::layers` over the `GetImageId` index space, at parity with the per-channel
`…ChannelLayer*` surface. Path + label + count only — per-layer blend op/transform stay
unmodeled (the eye case is all blend_source_over with identity transforms). Parser
unchanged: faithful exposure of already-parsed data, no merge onto `scene.materials` (R6.4).
+ DsonDocument_GetImageLayerCount → textured-layer count of the entry's map stack (1 = plain single texture, N = LIE, 0 = no array-form map / invalid; a color-only no-url base layer is not counted). NB unlike GetSceneMaterialChannelLayerCount, which is 0 for a plain channel.
+ DsonDocument_GetImageLayerTexturePath → layer texture path by (imageIndex, layerIdx); layer 0 = first textured map element ("" = invalid)
+ DsonDocument_GetImageLayerLabel → LIE layer label by (imageIndex, layerIdx) ("" = invalid)

## 1.2.0 — 2026-06-08 · MINOR (additive)

Faithful exposure of `scene.animations` keyframe channels. DAZ
`preset_hierarchical_material` presets park real channel values and `image_file`
paths under `scene.animations` (`{url, keys}`, key 0 = initialization data) while
leaving `scene.materials` channels as bare placeholders. The parser now stores each
entry verbatim — the raw `url` pointer plus the first key's typed value — and exposes
it; it does **not** apply them onto `scene.materials`, resolve the pointer, or resolve
the `image_file` string against `image_library` (parser stays faithful — the consumer
reads both sections and decides the override). First key only; `image_modification`/
tiling and multi-key are recognized in the data but not modeled.
+ DsonDocument_GetSceneAnimationCount → entry count (0 = none/invalid handle)
+ DsonDocument_GetSceneAnimationUrl → raw DSON property pointer, verbatim ("" = invalid)
+ DsonDocument_GetSceneAnimationValueKind → first-key value kind: 0 null · 1 number · 2 bool · 3 string · 4 color (-1 = invalid)
+ DsonDocument_GetSceneAnimationFloat → number value (0.0 if kind ≠ number/invalid)
+ DsonDocument_GetSceneAnimationBool → bool value (false if kind ≠ bool/invalid)
+ DsonDocument_GetSceneAnimationString → string value, e.g. an image_file path ("" if kind ≠ string/invalid)
+ DsonDocument_GetSceneAnimationColorR / …ColorG / …ColorB → RGB from a ≥3-number array (0.0 if kind ≠ color/invalid)

## 1.1.0 — 2026-06-08 · MINOR (additive)

First typed modeling of `scene.extra`: the DAZ "Character Addon Loader"
`PostLoadAddons` manifest. Lets an importer discover companion conforming figures
(Genesis 9 eyes/mouth/eyelashes/tear/eyebrows) a `character` preset instances but
does not list in `scene.nodes`. Paths only — resolving against content roots and
loading the referenced files stay importer responsibilities.
+ DsonDocument_GetScenePostLoadAddonCount → slot count, flattened across every scene.extra PostLoadAddons map in document order (0 = none)
+ DsonDocument_GetScenePostLoadAddonSlot → DAZ slot key (e.g. Follower/Attachment/Head/Face/Eyes)
+ DsonDocument_GetScenePostLoadAddonAssetName → addon asset name
+ DsonDocument_GetScenePostLoadAddonAssetFile → content-relative loader .duf path
+ DsonDocument_GetScenePostLoadAddonMatPreset → content-relative MAT preset .duf path ("" = no preset)

## 1.0.0 — 2026-06-07 · baseline

First versioned release — labels the entire current C ABI (~180 functions). Full
inventory: DsonParser_Roadmap.md (this repo only); pre-versioning history is not
retro-numbered. Baseline covers geometry, skeleton/nodes, skin binding (per-vertex
influence cache + capped/renormalized weights), UV sets, source-order materials,
and morph targets. Most recent additive surfaces an importer may not yet bind:
+ Image pixel dimensions: DsonDocument_GetImageId, DsonDocument_GetImageMapWidth, DsonDocument_GetImageMapHeight
+ Per scene-material-channel LIE layers: DsonDocument_GetSceneMaterialChannelLayerCount (0 = plain, N≥2 = layered), DsonDocument_GetSceneMaterialChannelLayerTexturePath, DsonDocument_GetSceneMaterialChannelLayerLabel
+ Formula (RPN) storage for modifier_library + scene.modifiers: DsonDocument_GetModifierFormulaCount / DsonDocument_GetSceneModifierFormulaCount, each with matching FormulaOutput / FormulaStage / FormulaOperation* accessors — stored, not evaluated (importer-side)
