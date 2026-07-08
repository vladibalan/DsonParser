#pragma once

#ifdef DSONPARSER_EXPORTS
#define DSONPARSER_API __declspec(dllexport)
#else
#define DSONPARSER_API __declspec(dllimport)
#endif

// Canonical library-version macros (DSONPARSER_VERSION_*), published with this header.
#include "DsonParserVersion.h"

// Public C ABI orientation:
// v2.1.0 — runtime: DsonParser_GetVersion(); compile-time: DSONPARSER_VERSION_*.
// Release history: CHANGELOG.md; SemVer/C-ABI policy: docs/versioning.md.
// What's new in 2.11.0: DsonDocument_GetModifierParent - complete authored
//   modifier.parent URL for each raw modifier_library item, exposed verbatim.
// What's new in 2.10.0: DsonDocument_GetGeometryRigidity* - complete raw
//   geometry.rigidity weights/groups, in the authored geometry's index space.
// What's new in 2.9.0: DsonDocument_GetGeometryGraft{VertexPair*,HiddenPoly*,
//   BaseVertexCount,BasePolyCount} - raw geograft weld correspondence
//   (vertex_pairs [graft-local, base-figure], hidden_polys, declared base
//   vertex/poly counts) for the composed-figure weld; faithful, no remap.
// What's new in 2.8.0: DsonDocument_GetNodeRigidFollow* - raw node_library
//   rigidity-group presence, rotation/scale modes, and reference-vertex indices.
// What's new in 2.7.0: DsonDocument_GetModifier{IsPush,PushOffset} - geometry-shell
//   "Mesh Offset" push modifier identity + effective offset-distance channel value,
//   read from the modifier's nested extra[] (studio/modifier/push + studio_modifier_channels).
// What's new in 2.6.0: scene-node translation/rotation/scale presence masks plus
//   general_scale and rotation_order authored-presence queries.
// What's new in 2.5.0: DsonDocument_GetSceneNode{CenterPoint*,Orientation*,InheritsScale*}
//   exposes raw authored scene-instance joint fields with explicit presence.
// What's new in 2.4.0: DsonDocument_GetSceneNode{Parent,Translation*,Rotation*,Scale*,GeneralScale,RotationOrder}
//   exposes scene-instance local placement, including current-value-authored transforms.
// What's new in 2.3.0: DsonDocument_GetScenePostLoadScript* — scene.extra DAZ
//   "scene_post_load_script" references (name/type/.dse path) exposed faithfully
//   so the consumer can warn that the static import does not execute them.
// What's new in 2.2.0: DsonDocument_GetModifier{Group,Region} + DsonDocument_GetModifierPresentationIcon — 
//    modifier control-inventory metadata (DAZ Parameter-Settings path / region + per-control icon) 
//    for a controls UI; faithful passthrough.
// What's new in 2.1.0: DsonDocument_Get{Modifier,SceneModifier}FormulaOperationValArray{Count,Element}
//   - raw array-valued formula operands (spline_tcb TCB knots) that the scalar val
//   accessors collapsed to 0.0; the parser still does not evaluate the spline.
// What's new in 2.0.0 (BREAKING): removed DsonDocument_GetUVPolygonVertexIndexCount
//   and DsonDocument_GetUVPolygonVertexIndex - the dead legacy flat-int UV path,
//   empty for every real DAZ DSF since the sparse migration. UV index data comes
//   from the unchanged GetUVOverride* sparse family.
// What's new in 1.6.0: documented threading contract - distinct handles are safe
//   for concurrent cross-thread use; DsonParser_GetLastError() is now per-thread
//   (thread_local), no longer a process-global slot.
// What's new in 1.5.0: DsonDocument_Get{Node,Modifier}Presentation{Type,Label} +
//   DsonDocument_GetGeometryIsGraft - declared asset-catalog metadata (content type,
//   display label, geograft signal), exposed faithfully per library item.
// What's new in 1.4.0: DsonDocument_GetImageLayer*/...SceneMaterialChannelLayer* per-layer LIE compositing (blend op, opacity, active, invert, color, transform).
// What's new in 1.3.0: DsonDocument_GetImageLayer* — image_library per-layer LIE map stack (texture path + label) reachable by image index.
// What's new in 1.2.0: DsonDocument_GetSceneAnimation* — scene.animations keyframe channels exposed faithfully (per R6.4, never applied onto scene.materials).
// What's new in 1.1.0: DsonDocument_GetScenePostLoadAddon* — "Character Addon Loader" companion figures (not in scene.nodes).
//
// This header exposes a parsed DSON/DSF/DUF document through an opaque handle and
// index-based accessors. The implementation owns all returned const char*
// strings; copy them if they must survive DsonDocument_Clear/Destroy or later
// scratch-string API calls. Invalid handles or indexes return a family-specific
// "empty" value: count functions and numeric getters return 0 (count functions
// never return -1), bool getters return false, string getters return "", and the
// value/index accessors that have no element to report return -1.
//
// Threading / concurrency:
// - Distinct handles are independent. Each DsonDocumentHandle owns all of its
//   parsed data, its lazy query caches, and the scratch strings its accessors
//   return, so two threads operating on two different handles share no mutable
//   state and need no locking - including concurrent DsonDocument_Create /
//   LoadFrom* and any mix of read accessors.
// - DsonParser_GetLastError() is per-thread: it reports the error (or "") from the
//   calling thread's own most recent DsonDocument_Create/LoadFrom*. Read it on the
//   same thread that performed the load.
// - A single handle is NOT safe for concurrent use: its lazy caches and scratch
//   return strings are built/overwritten on read. Serialize calls on one handle,
//   or give each thread its own handle.
//
// Index conventions:
// - Node/geometry/material/modifier indexes address the corresponding library
//   arrays parsed from *_library sections.
// - Image indexes address the image_library array (size = GetImageCount).
// - Scene node/material/modifier/UV indexes address scene.* instance arrays.
// - Morph indexes address a filtered list of modifiers where type == "morph";
//   they are not raw modifier_library indexes.
// - Skin APIs use raw modifier_library indexes for skin_binding modifiers.
// - Formula APIs are exposed for both raw modifier_library indexes and
//   scene.modifiers indexes; they store RPN data only and do not evaluate it.

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle to DsonDocument
typedef void* DsonDocumentHandle;

// Bits returned by scene-node vector presence masks. OR the bits to test more
// than one component. A zero mask means no numeric component was authored or
// the handle/index is invalid.
// @since 2.5.0
#define DSONPARSER_VECTOR_COMPONENT_X 0x1u
#define DSONPARSER_VECTOR_COMPONENT_Y 0x2u
#define DSONPARSER_VECTOR_COMPONENT_Z 0x4u

// Create a new DSON document
DSONPARSER_API DsonDocumentHandle DsonDocument_Create();

// Returns 0 on success, non-zero on failure.
// Call DsonParser_GetLastError() for error detail on failure.
DSONPARSER_API int DsonDocument_LoadFromFile(DsonDocumentHandle handle, const char* filepath);

// Returns 0 on success, non-zero on failure.
// Call DsonParser_GetLastError() for error detail on failure.
DSONPARSER_API int DsonDocument_LoadFromString(DsonDocumentHandle handle, const char* jsonString);

// Returns 0 on success, non-zero on failure.
// Accepts plain JSON bytes or gzip-wrapped JSON bytes with an explicit length.
// Call DsonParser_GetLastError() for error detail on failure.
DSONPARSER_API int DsonDocument_LoadFromBuffer(DsonDocumentHandle handle, const char* data, int length);

// Get file version
DSONPARSER_API const char* DsonDocument_GetFileVersion(DsonDocumentHandle handle);

// Get asset info
DSONPARSER_API const char* DsonDocument_GetAssetId(DsonDocumentHandle handle);
DSONPARSER_API const char* DsonDocument_GetAssetType(DsonDocumentHandle handle);
DSONPARSER_API double      DsonDocument_GetUnitScale(DsonDocumentHandle handle);

// Get counts
DSONPARSER_API int DsonDocument_GetNodeCount(DsonDocumentHandle handle);
DSONPARSER_API int DsonDocument_GetGeometryCount(DsonDocumentHandle handle);
DSONPARSER_API int DsonDocument_GetMaterialCount(DsonDocumentHandle handle);
DSONPARSER_API int DsonDocument_GetModifierCount(DsonDocumentHandle handle);
DSONPARSER_API int DsonDocument_GetImageCount(DsonDocumentHandle handle);
DSONPARSER_API int DsonDocument_GetUVSetCount(DsonDocumentHandle handle);

// Get node info by index
DSONPARSER_API const char* DsonDocument_GetNodeId(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetNodeName(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetNodeType(DsonDocumentHandle handle, int index);
// presentation.type (DAZ "Content Type", e.g. "Follower") / presentation.label for this node_library item; "" when absent or index invalid.
// @since 1.5.0
DSONPARSER_API const char* DsonDocument_GetNodePresentationType(DsonDocumentHandle handle, int index);
// @since 1.5.0
DSONPARSER_API const char* DsonDocument_GetNodePresentationLabel(DsonDocumentHandle handle, int index);
// Node center_point (joint origin) components
DSONPARSER_API double DsonDocument_GetNodeCenterPointX(DsonDocumentHandle handle, int index);
DSONPARSER_API double DsonDocument_GetNodeCenterPointY(DsonDocumentHandle handle, int index);
DSONPARSER_API double DsonDocument_GetNodeCenterPointZ(DsonDocumentHandle handle, int index);
// Rigid-follow rigidity_group on a node_library item. HasRigidFollow is the presence discriminator;
// bound-check ScaleModeCount/ReferenceVertexCount before indexing. ReferenceVertex returns -1 on
// invalid input because vertex index 0 is legitimate. Values are raw and unevaluated.
// @since 2.8.0
DSONPARSER_API bool        DsonDocument_GetNodeHasRigidFollow(DsonDocumentHandle handle, int index);
// @since 2.8.0
DSONPARSER_API const char* DsonDocument_GetNodeRigidFollowRotationMode(DsonDocumentHandle handle, int index);
// @since 2.8.0
DSONPARSER_API int         DsonDocument_GetNodeRigidFollowScaleModeCount(DsonDocumentHandle handle, int index);
// @since 2.8.0
DSONPARSER_API const char* DsonDocument_GetNodeRigidFollowScaleMode(DsonDocumentHandle handle, int index, int axisIndex);
// @since 2.8.0
DSONPARSER_API int         DsonDocument_GetNodeRigidFollowReferenceVertexCount(DsonDocumentHandle handle, int index);
// @since 2.8.0
DSONPARSER_API int         DsonDocument_GetNodeRigidFollowReferenceVertex(DsonDocumentHandle handle, int index, int refIndex);

// Get scene node info by index (the "scene.nodes" array, distinct from node_library).
// Scene nodes are instances: they reference a library node via Url and carry a Label,
// and typically have no Type of their own.
DSONPARSER_API int DsonDocument_GetSceneNodeCount(DsonDocumentHandle handle);
DSONPARSER_API const char* DsonDocument_GetSceneNodeId(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetSceneNodeName(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetSceneNodeLabel(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetSceneNodeType(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetSceneNodeUrl(DsonDocumentHandle handle, int index);
// @since 2.4.0
DSONPARSER_API const char* DsonDocument_GetSceneNodeParent(DsonDocumentHandle handle, int index);
// @since 2.4.0
DSONPARSER_API double DsonDocument_GetSceneNodeTranslationX(DsonDocumentHandle handle, int index);
// @since 2.4.0
DSONPARSER_API double DsonDocument_GetSceneNodeTranslationY(DsonDocumentHandle handle, int index);
// @since 2.4.0
DSONPARSER_API double DsonDocument_GetSceneNodeTranslationZ(DsonDocumentHandle handle, int index);
// @since 2.6.0
DSONPARSER_API int DsonDocument_GetSceneNodeTranslationPresenceMask(DsonDocumentHandle handle, int index);
// @since 2.4.0
DSONPARSER_API double DsonDocument_GetSceneNodeRotationX(DsonDocumentHandle handle, int index);
// @since 2.4.0
DSONPARSER_API double DsonDocument_GetSceneNodeRotationY(DsonDocumentHandle handle, int index);
// @since 2.4.0
DSONPARSER_API double DsonDocument_GetSceneNodeRotationZ(DsonDocumentHandle handle, int index);
// @since 2.6.0
DSONPARSER_API int DsonDocument_GetSceneNodeRotationPresenceMask(DsonDocumentHandle handle, int index);
// @since 2.4.0
DSONPARSER_API double DsonDocument_GetSceneNodeScaleX(DsonDocumentHandle handle, int index);
// @since 2.4.0
DSONPARSER_API double DsonDocument_GetSceneNodeScaleY(DsonDocumentHandle handle, int index);
// @since 2.4.0
DSONPARSER_API double DsonDocument_GetSceneNodeScaleZ(DsonDocumentHandle handle, int index);
// @since 2.6.0
DSONPARSER_API int DsonDocument_GetSceneNodeScalePresenceMask(DsonDocumentHandle handle, int index);
// @since 2.4.0
DSONPARSER_API double DsonDocument_GetSceneNodeGeneralScale(DsonDocumentHandle handle, int index);
// @since 2.6.0
DSONPARSER_API bool DsonDocument_GetSceneNodeHasGeneralScale(DsonDocumentHandle handle, int index);
// @since 2.4.0
DSONPARSER_API const char* DsonDocument_GetSceneNodeRotationOrder(DsonDocumentHandle handle, int index);
// @since 2.6.0
DSONPARSER_API bool DsonDocument_GetSceneNodeHasRotationOrder(DsonDocumentHandle handle, int index);
// Raw scene.nodes center_point. PresenceMask distinguishes explicit zero from absence.
// @since 2.5.0
DSONPARSER_API double DsonDocument_GetSceneNodeCenterPointX(DsonDocumentHandle handle, int index);
// @since 2.5.0
DSONPARSER_API double DsonDocument_GetSceneNodeCenterPointY(DsonDocumentHandle handle, int index);
// @since 2.5.0
DSONPARSER_API double DsonDocument_GetSceneNodeCenterPointZ(DsonDocumentHandle handle, int index);
// @since 2.5.0
DSONPARSER_API int DsonDocument_GetSceneNodeCenterPointPresenceMask(DsonDocumentHandle handle, int index);
// Raw scene.nodes orientation. PresenceMask distinguishes explicit zero from absence.
// @since 2.5.0
DSONPARSER_API double DsonDocument_GetSceneNodeOrientationX(DsonDocumentHandle handle, int index);
// @since 2.5.0
DSONPARSER_API double DsonDocument_GetSceneNodeOrientationY(DsonDocumentHandle handle, int index);
// @since 2.5.0
DSONPARSER_API double DsonDocument_GetSceneNodeOrientationZ(DsonDocumentHandle handle, int index);
// @since 2.5.0
DSONPARSER_API int DsonDocument_GetSceneNodeOrientationPresenceMask(DsonDocumentHandle handle, int index);
// Raw scene.nodes inherits_scale value and authored-presence discriminator.
// @since 2.5.0
DSONPARSER_API bool DsonDocument_GetSceneNodeInheritsScale(DsonDocumentHandle handle, int index);
// @since 2.5.0
DSONPARSER_API bool DsonDocument_GetSceneNodeHasInheritsScale(DsonDocumentHandle handle, int index);
DSONPARSER_API int         DsonDocument_GetSceneNodeGeometryCount(DsonDocumentHandle handle, int sceneNodeIndex);
DSONPARSER_API const char* DsonDocument_GetSceneNodeGeometryId(DsonDocumentHandle handle, int sceneNodeIndex, int geomRefIndex);
DSONPARSER_API const char* DsonDocument_GetSceneNodeGeometryUrl(DsonDocumentHandle handle, int sceneNodeIndex, int geomRefIndex);

// Other scene instance collections (scene.modifiers / scene.materials / scene.uvs)
DSONPARSER_API int DsonDocument_GetSceneModifierCount(DsonDocumentHandle handle);
DSONPARSER_API const char* DsonDocument_GetSceneModifierId(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetSceneModifierUrl(DsonDocumentHandle handle, int index);
DSONPARSER_API double      DsonDocument_GetSceneModifierChannelValue(DsonDocumentHandle handle, int sceneModifierIndex);
DSONPARSER_API double      DsonDocument_GetSceneModifierChannelMin(DsonDocumentHandle handle, int sceneModifierIndex);
DSONPARSER_API double      DsonDocument_GetSceneModifierChannelMax(DsonDocumentHandle handle, int sceneModifierIndex);
DSONPARSER_API bool        DsonDocument_GetSceneModifierChannelClamped(DsonDocumentHandle handle, int sceneModifierIndex);
DSONPARSER_API int         DsonDocument_GetSceneModifierFormulaCount(DsonDocumentHandle handle, int sceneModifierIndex);
DSONPARSER_API const char* DsonDocument_GetSceneModifierFormulaOutput(DsonDocumentHandle handle, int sceneModifierIndex, int formulaIndex);
DSONPARSER_API const char* DsonDocument_GetSceneModifierFormulaStage(DsonDocumentHandle handle, int sceneModifierIndex, int formulaIndex);
DSONPARSER_API int         DsonDocument_GetSceneModifierFormulaOperationCount(DsonDocumentHandle handle, int sceneModifierIndex, int formulaIndex);
DSONPARSER_API const char* DsonDocument_GetSceneModifierFormulaOperationOp(DsonDocumentHandle handle, int sceneModifierIndex, int formulaIndex, int opIndex);
DSONPARSER_API double      DsonDocument_GetSceneModifierFormulaOperationVal(DsonDocumentHandle handle, int sceneModifierIndex, int formulaIndex, int opIndex);
DSONPARSER_API const char* DsonDocument_GetSceneModifierFormulaOperationUrl(DsonDocumentHandle handle, int sceneModifierIndex, int formulaIndex, int opIndex);
// @since 2.1.0
DSONPARSER_API int    DsonDocument_GetSceneModifierFormulaOperationValArrayCount(DsonDocumentHandle handle, int sceneModifierIndex, int formulaIndex, int opIndex);
// @since 2.1.0
DSONPARSER_API double DsonDocument_GetSceneModifierFormulaOperationValArrayElement(DsonDocumentHandle handle, int sceneModifierIndex, int formulaIndex, int opIndex, int elementIndex);

DSONPARSER_API int DsonDocument_GetSceneMaterialCount(DsonDocumentHandle handle);
DSONPARSER_API const char* DsonDocument_GetSceneMaterialId(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetSceneMaterialUrl(DsonDocumentHandle handle, int index);

DSONPARSER_API int DsonDocument_GetSceneUVSetCount(DsonDocumentHandle handle);
DSONPARSER_API const char* DsonDocument_GetSceneUVSetId(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetSceneUVSetUrl(DsonDocumentHandle handle, int index);

// Get material info by index
DSONPARSER_API const char* DsonDocument_GetMaterialId(DsonDocumentHandle handle, int index);

// Get geometry info by index
DSONPARSER_API const char* DsonDocument_GetGeometryId(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetGeometryName(DsonDocumentHandle handle, int index);
DSONPARSER_API int DsonDocument_GetGeometryVertexCount(DsonDocumentHandle handle, int index);
DSONPARSER_API int DsonDocument_GetGeometryPolygonCount(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetGeometryDefaultUVSetId(DsonDocumentHandle handle, int geomIndex);
// true iff the geometry declares a populated graft (vertex_pairs present); false for empty/absent graft. Bool family -> false on invalid handle/index (R1).
// @since 1.5.0
DSONPARSER_API bool DsonDocument_GetGeometryIsGraft(DsonDocumentHandle handle, int index);
// Geograft weld correspondence (raw DSON, file-local index space; only
// meaningful when GetGeometryIsGraft). The importer owns the remap/weld.
// Count family -> 0 on invalid; vertex/poly accessors -> -1 (index 0 is
// legitimate). Pair member order: [graft-local vertex, base-figure vertex].
// The pair count is the parsed values length, not DAZ's declared count.
// @since 2.9.0
DSONPARSER_API int DsonDocument_GetGeometryGraftVertexPairCount(DsonDocumentHandle handle, int index);
DSONPARSER_API int DsonDocument_GetGeometryGraftVertexPairGraftVertex(DsonDocumentHandle handle, int index, int pairIndex);
DSONPARSER_API int DsonDocument_GetGeometryGraftVertexPairBaseVertex(DsonDocumentHandle handle, int index, int pairIndex);
DSONPARSER_API int DsonDocument_GetGeometryGraftHiddenPolyCount(DsonDocumentHandle handle, int index);
DSONPARSER_API int DsonDocument_GetGeometryGraftHiddenPoly(DsonDocumentHandle handle, int index, int hiddenIndex);
DSONPARSER_API int DsonDocument_GetGeometryGraftBaseVertexCount(DsonDocumentHandle handle, int index);
DSONPARSER_API int DsonDocument_GetGeometryGraftBasePolyCount(DsonDocumentHandle handle, int index);
// Complete authored geometry.rigidity passthrough. Presence is independent of
// array sizes. Counts return 0 on invalid; vertex-index accessors return -1;
// weight returns 0.0; strings return ""; bool returns false.
// @since 2.10.0
DSONPARSER_API bool DsonDocument_GetGeometryHasRigidity(DsonDocumentHandle handle, int geomIndex);
// @since 2.10.0
DSONPARSER_API int DsonDocument_GetGeometryRigidityWeightCount(DsonDocumentHandle handle, int geomIndex);
// @since 2.10.0
DSONPARSER_API int DsonDocument_GetGeometryRigidityWeightVertexIndex(DsonDocumentHandle handle, int geomIndex, int weightIndex);
// @since 2.10.0
DSONPARSER_API double DsonDocument_GetGeometryRigidityWeight(DsonDocumentHandle handle, int geomIndex, int weightIndex);
// @since 2.10.0
DSONPARSER_API int DsonDocument_GetGeometryRigidityGroupCount(DsonDocumentHandle handle, int geomIndex);
// @since 2.10.0
DSONPARSER_API const char* DsonDocument_GetGeometryRigidityGroupId(DsonDocumentHandle handle, int geomIndex, int groupIndex);
// @since 2.10.0
DSONPARSER_API const char* DsonDocument_GetGeometryRigidityGroupRotationMode(DsonDocumentHandle handle, int geomIndex, int groupIndex);
// @since 2.10.0
DSONPARSER_API int DsonDocument_GetGeometryRigidityGroupScaleModeCount(DsonDocumentHandle handle, int geomIndex, int groupIndex);
// @since 2.10.0
DSONPARSER_API const char* DsonDocument_GetGeometryRigidityGroupScaleMode(DsonDocumentHandle handle, int geomIndex, int groupIndex, int modeIndex);
// @since 2.10.0
DSONPARSER_API int DsonDocument_GetGeometryRigidityGroupReferenceVertexCount(DsonDocumentHandle handle, int geomIndex, int groupIndex);
// @since 2.10.0
DSONPARSER_API int DsonDocument_GetGeometryRigidityGroupReferenceVertex(DsonDocumentHandle handle, int geomIndex, int groupIndex, int vertexIndex);
// @since 2.10.0
DSONPARSER_API int DsonDocument_GetGeometryRigidityGroupMaskVertexCount(DsonDocumentHandle handle, int geomIndex, int groupIndex);
// @since 2.10.0
DSONPARSER_API int DsonDocument_GetGeometryRigidityGroupMaskVertex(DsonDocumentHandle handle, int geomIndex, int groupIndex, int vertexIndex);
// @since 2.10.0
DSONPARSER_API const char* DsonDocument_GetGeometryRigidityGroupReference(DsonDocumentHandle handle, int geomIndex, int groupIndex);
// @since 2.10.0
DSONPARSER_API int DsonDocument_GetGeometryRigidityGroupTransformNodeCount(DsonDocumentHandle handle, int geomIndex, int groupIndex);
// @since 2.10.0
DSONPARSER_API const char* DsonDocument_GetGeometryRigidityGroupTransformNode(DsonDocumentHandle handle, int geomIndex, int groupIndex, int nodeIndex);
// @since 2.10.0
DSONPARSER_API bool DsonDocument_GetGeometryRigidityGroupUseTransformBonesForScale(DsonDocumentHandle handle, int geomIndex, int groupIndex);

// ---- A. Geometry: vertex positions ----
DSONPARSER_API int    DsonDocument_GetVertexCount(DsonDocumentHandle handle, int geomIndex);
DSONPARSER_API double DsonDocument_GetVertexX(DsonDocumentHandle handle, int geomIndex, int vertexIndex);
DSONPARSER_API double DsonDocument_GetVertexY(DsonDocumentHandle handle, int geomIndex, int vertexIndex);
DSONPARSER_API double DsonDocument_GetVertexZ(DsonDocumentHandle handle, int geomIndex, int vertexIndex);

// Polygon face list (polylist)
// Each face: [polygon_group_idx, material_group_idx, v0, v1, v2, v3] — two leading ints.
DSONPARSER_API int    DsonDocument_GetPolylistCount(DsonDocumentHandle handle, int geomIndex);
DSONPARSER_API int    DsonDocument_GetPolylistFaceVertexCount(DsonDocumentHandle handle, int geomIndex, int faceIndex);
DSONPARSER_API int    DsonDocument_GetPolylistFaceVertex(DsonDocumentHandle handle, int geomIndex, int faceIndex, int vertexIndex);
DSONPARSER_API int    DsonDocument_GetPolylistFaceGroupIndex(DsonDocumentHandle handle, int geomIndex, int faceIndex);
DSONPARSER_API int    DsonDocument_GetPolylistFaceMaterialIndex(DsonDocumentHandle handle, int geomIndex, int faceIndex);

// Polygon groups (bone region groups, e.g. l_forearm, head, pelvis)
DSONPARSER_API int         DsonDocument_GetPolygonGroupCount(DsonDocumentHandle handle, int geomIndex);
DSONPARSER_API const char* DsonDocument_GetPolygonGroupName(DsonDocumentHandle handle, int geomIndex, int groupIndex);

// Material groups
DSONPARSER_API int         DsonDocument_GetPolygonMaterialGroupCount(DsonDocumentHandle handle, int geomIndex);
DSONPARSER_API const char* DsonDocument_GetPolygonMaterialGroupName(DsonDocumentHandle handle, int geomIndex, int groupIndex);

// Material groups on material instances (library materials — typically empty for library entries)
DSONPARSER_API int         DsonDocument_GetMaterialGroupCount(DsonDocumentHandle handle, int matIndex);
DSONPARSER_API const char* DsonDocument_GetMaterialGroupName(DsonDocumentHandle handle, int matIndex, int groupIndex);

// Material groups on scene material instances (scene.materials — "groups" maps to polygon_material_groups)
DSONPARSER_API int         DsonDocument_GetSceneMaterialGroupCount(DsonDocumentHandle handle, int matIndex);
DSONPARSER_API const char* DsonDocument_GetSceneMaterialGroupName(DsonDocumentHandle handle, int matIndex, int groupIndex);

// Get modifier info by index
DSONPARSER_API const char* DsonDocument_GetModifierId(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetModifierName(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetModifierType(DsonDocumentHandle handle, int index);
// presentation.type (DAZ "Content Type", e.g. "Modifier/Shape") / presentation.label for this modifier_library item; "" when absent or index invalid.
// @since 1.5.0
DSONPARSER_API const char* DsonDocument_GetModifierPresentationType(DsonDocumentHandle handle, int index);
// @since 1.5.0
DSONPARSER_API const char* DsonDocument_GetModifierPresentationLabel(DsonDocumentHandle handle, int index);
// modifier-level "group" (DAZ Parameter Settings "Path", e.g. "/Pose Controls/Head/Expressions") / "region" (e.g. "Head") for this modifier_library item; raw verbatim, "" when absent or index invalid.
// @since 2.2.0
DSONPARSER_API const char* DsonDocument_GetModifierGroup(DsonDocumentHandle handle, int index);
// @since 2.2.0
DSONPARSER_API const char* DsonDocument_GetModifierRegion(DsonDocumentHandle handle, int index);
// presentation.icon_large thumbnail path for this modifier_library item, raw/verbatim (percent-encoded as stored; the consumer resolves/loads it); "" when absent or index invalid.
// @since 2.2.0
DSONPARSER_API const char* DsonDocument_GetModifierPresentationIcon(DsonDocumentHandle handle, int index);
DSONPARSER_API double      DsonDocument_GetModifierChannelValue(DsonDocumentHandle handle, int modifierIndex);
DSONPARSER_API double      DsonDocument_GetModifierChannelMin(DsonDocumentHandle handle, int modifierIndex);
DSONPARSER_API double      DsonDocument_GetModifierChannelMax(DsonDocumentHandle handle, int modifierIndex);
DSONPARSER_API bool        DsonDocument_GetModifierChannelClamped(DsonDocumentHandle handle, int modifierIndex);
// Geometry-shell "Mesh Offset" push modifier (studio/modifier/push nested in extra[]).
// IsPush: true iff this modifier_library item declares a studio/modifier/push extra entry (false = not a push / invalid).
// PushOffset: its effective "Offset Distance" channel value (current_value -> value; raw, cm), 0.0 when not a push / no offset channel / invalid - gate on IsPush.
// @since 2.7.0
DSONPARSER_API bool        DsonDocument_GetModifierIsPush(DsonDocumentHandle handle, int modifierIndex);
// @since 2.7.0
DSONPARSER_API double      DsonDocument_GetModifierPushOffset(DsonDocumentHandle handle, int modifierIndex);
// Raw/verbatim modifier.parent for a raw modifier_library index: the complete authored URL,
// including any #fragment. Returns "" when absent or the handle/index is invalid.
// @since 2.11.0
DSONPARSER_API const char* DsonDocument_GetModifierParent(DsonDocumentHandle handle, int modifierIndex);
DSONPARSER_API int         DsonDocument_GetModifierFormulaCount(DsonDocumentHandle handle, int modifierIndex);
DSONPARSER_API const char* DsonDocument_GetModifierFormulaOutput(DsonDocumentHandle handle, int modifierIndex, int formulaIndex);
DSONPARSER_API const char* DsonDocument_GetModifierFormulaStage(DsonDocumentHandle handle, int modifierIndex, int formulaIndex);
DSONPARSER_API int         DsonDocument_GetModifierFormulaOperationCount(DsonDocumentHandle handle, int modifierIndex, int formulaIndex);
DSONPARSER_API const char* DsonDocument_GetModifierFormulaOperationOp(DsonDocumentHandle handle, int modifierIndex, int formulaIndex, int opIndex);
DSONPARSER_API double      DsonDocument_GetModifierFormulaOperationVal(DsonDocumentHandle handle, int modifierIndex, int formulaIndex, int opIndex);
DSONPARSER_API const char* DsonDocument_GetModifierFormulaOperationUrl(DsonDocumentHandle handle, int modifierIndex, int formulaIndex, int opIndex);
// @since 2.1.0
DSONPARSER_API int    DsonDocument_GetModifierFormulaOperationValArrayCount(DsonDocumentHandle handle, int modifierIndex, int formulaIndex, int opIndex);
// @since 2.1.0
DSONPARSER_API double DsonDocument_GetModifierFormulaOperationValArrayElement(DsonDocumentHandle handle, int modifierIndex, int formulaIndex, int opIndex, int elementIndex);
// Skin binding info for a modifier (0 if the modifier has no skin payload)
DSONPARSER_API int DsonDocument_GetModifierSkinVertexCount(DsonDocumentHandle handle, int index);
DSONPARSER_API int DsonDocument_GetModifierSkinJointCount(DsonDocumentHandle handle, int index);

// ---- Images (image_library) ----
// imageIndex addresses the image_library array (count = DsonDocument_GetImageCount).
// Map dimensions come from the entry's map_size [width, height]; 0 if absent.
DSONPARSER_API const char* DsonDocument_GetImageId(DsonDocumentHandle handle, int imageIndex);
DSONPARSER_API int         DsonDocument_GetImageMapWidth(DsonDocumentHandle handle, int imageIndex);
DSONPARSER_API int         DsonDocument_GetImageMapHeight(DsonDocumentHandle handle, int imageIndex);

// Layered-image (LIE) map stack for an image_library entry, addressable by the
// same imageIndex as DsonDocument_GetImageId. Parity with the per-channel
// DsonDocument_GetSceneMaterialChannelLayer* surface, reading the same parsed
// Image::layers vector — use this when an image is referenced from OUTSIDE an
// inline material channel (e.g. a scene.animations "#fragment" diffuse/image
// binding) so the per-channel accessors cannot reach it.
//   layerIdx 0 is the first map element that carries a texture; higher indexes
//   are overlays in document order. A color-only LIE base layer (a map element
//   with no "url") is NOT represented — only textured layers are counted/returned.
//   So a plain single-texture image (DAZ wraps it in a 1-element map array)
//   reports count 1, a true LIE reports its textured-layer count, and an image
//   whose "map" is a bare string/object (non-array) or absent reports 0.
//   This differs from DsonDocument_GetSceneMaterialChannelLayerCount (0 for a
//   plain non-LIE channel): the per-image count is the faithful size of the
//   parsed layer stack (1 for plain, N for LIE).
// @since 1.3.0
DSONPARSER_API int         DsonDocument_GetImageLayerCount(DsonDocumentHandle handle, int imageIndex);
// @since 1.3.0
DSONPARSER_API const char* DsonDocument_GetImageLayerTexturePath(DsonDocumentHandle handle, int imageIndex, int layerIdx);
// @since 1.3.0
DSONPARSER_API const char* DsonDocument_GetImageLayerLabel(DsonDocumentHandle handle, int imageIndex, int layerIdx);
// Per-layer LIE compositing metadata — raw DAZ map-element values, verbatim; no compositing performed.
// Layer 0 is the base; higher indexes are overlays in document order.
// Use GetImageLayerCount to bounds-check before calling (out-of-range returns sentinel per family).
// @since 1.4.0
DSONPARSER_API const char* DsonDocument_GetImageLayerBlendMode(DsonDocumentHandle handle, int imageIndex, int layerIdx);
// Opacity is the raw "transparency" value (1 = opaque). Sentinel 0.0 collides with a legitimately-transparent layer; bound-check with GetImageLayerCount first.
// @since 1.4.0
DSONPARSER_API double      DsonDocument_GetImageLayerOpacity(DsonDocumentHandle handle, int imageIndex, int layerIdx);
// @since 1.4.0
DSONPARSER_API bool        DsonDocument_GetImageLayerActive(DsonDocumentHandle handle, int imageIndex, int layerIdx);
// @since 1.4.0
DSONPARSER_API bool        DsonDocument_GetImageLayerInvert(DsonDocumentHandle handle, int imageIndex, int layerIdx);
// @since 1.4.0
DSONPARSER_API double      DsonDocument_GetImageLayerColorR(DsonDocumentHandle handle, int imageIndex, int layerIdx);
// @since 1.4.0
DSONPARSER_API double      DsonDocument_GetImageLayerColorG(DsonDocumentHandle handle, int imageIndex, int layerIdx);
// @since 1.4.0
DSONPARSER_API double      DsonDocument_GetImageLayerColorB(DsonDocumentHandle handle, int imageIndex, int layerIdx);
// @since 1.4.0
DSONPARSER_API double      DsonDocument_GetImageLayerRotation(DsonDocumentHandle handle, int imageIndex, int layerIdx);
// Returns 1.0 for an invalid layerIdx (scale exception per R1 contract).
// @since 1.4.0
DSONPARSER_API double      DsonDocument_GetImageLayerScaleX(DsonDocumentHandle handle, int imageIndex, int layerIdx);
// Returns 1.0 for an invalid layerIdx (scale exception per R1 contract).
// @since 1.4.0
DSONPARSER_API double      DsonDocument_GetImageLayerScaleY(DsonDocumentHandle handle, int imageIndex, int layerIdx);
// @since 1.4.0
DSONPARSER_API double      DsonDocument_GetImageLayerOffsetX(DsonDocumentHandle handle, int imageIndex, int layerIdx);
// @since 1.4.0
DSONPARSER_API double      DsonDocument_GetImageLayerOffsetY(DsonDocumentHandle handle, int imageIndex, int layerIdx);
// @since 1.4.0
DSONPARSER_API bool        DsonDocument_GetImageLayerMirrorX(DsonDocumentHandle handle, int imageIndex, int layerIdx);
// @since 1.4.0
DSONPARSER_API bool        DsonDocument_GetImageLayerMirrorY(DsonDocumentHandle handle, int imageIndex, int layerIdx);

// ---- B. Skeleton / Nodes ----
DSONPARSER_API const char* DsonDocument_GetNodeParent(DsonDocumentHandle handle, int nodeIndex);
DSONPARSER_API double      DsonDocument_GetNodeEndPointX(DsonDocumentHandle handle, int nodeIndex);
DSONPARSER_API double      DsonDocument_GetNodeEndPointY(DsonDocumentHandle handle, int nodeIndex);
DSONPARSER_API double      DsonDocument_GetNodeEndPointZ(DsonDocumentHandle handle, int nodeIndex);
DSONPARSER_API double      DsonDocument_GetNodeOrientationX(DsonDocumentHandle handle, int nodeIndex);
DSONPARSER_API double      DsonDocument_GetNodeOrientationY(DsonDocumentHandle handle, int nodeIndex);
DSONPARSER_API double      DsonDocument_GetNodeOrientationZ(DsonDocumentHandle handle, int nodeIndex);
DSONPARSER_API const char* DsonDocument_GetNodeRotationOrder(DsonDocumentHandle handle, int nodeIndex);
DSONPARSER_API double      DsonDocument_GetNodeTranslationX(DsonDocumentHandle handle, int nodeIndex);
DSONPARSER_API double      DsonDocument_GetNodeTranslationY(DsonDocumentHandle handle, int nodeIndex);
DSONPARSER_API double      DsonDocument_GetNodeTranslationZ(DsonDocumentHandle handle, int nodeIndex);
DSONPARSER_API double      DsonDocument_GetNodeRotationX(DsonDocumentHandle handle, int nodeIndex);
DSONPARSER_API double      DsonDocument_GetNodeRotationY(DsonDocumentHandle handle, int nodeIndex);
DSONPARSER_API double      DsonDocument_GetNodeRotationZ(DsonDocumentHandle handle, int nodeIndex);
DSONPARSER_API double      DsonDocument_GetNodeScaleX(DsonDocumentHandle handle, int nodeIndex);
DSONPARSER_API double      DsonDocument_GetNodeScaleY(DsonDocumentHandle handle, int nodeIndex);
DSONPARSER_API double      DsonDocument_GetNodeScaleZ(DsonDocumentHandle handle, int nodeIndex);
DSONPARSER_API double      DsonDocument_GetNodeGeneralScale(DsonDocumentHandle handle, int nodeIndex);

// ---- C. Skin Weights ----
// modifierIndex is an index into the full modifier library (same as GetModifierId etc.)
DSONPARSER_API int         DsonDocument_GetSkinJointCount(DsonDocumentHandle handle, int modifierIndex);
DSONPARSER_API const char* DsonDocument_GetSkinJointNodeId(DsonDocumentHandle handle, int modifierIndex, int jointIndex);
DSONPARSER_API int         DsonDocument_GetSkinJointWeightCount(DsonDocumentHandle handle, int modifierIndex, int jointIndex);
DSONPARSER_API int         DsonDocument_GetSkinJointWeightVertexIndex(DsonDocumentHandle handle, int modifierIndex, int jointIndex, int weightIndex);
DSONPARSER_API double      DsonDocument_GetSkinJointWeight(DsonDocumentHandle handle, int modifierIndex, int jointIndex, int weightIndex);

// Processed per-vertex skin weight queries (inverted, sorted descending, normalized, capped).
// Call GetVertexInfluenceCount first to size arrays, then GetVertexBoneInfluence for each influence.
// Pass maxInfluences=8 for UE5 FSoftSkinVertex; must be >= 1.
DSONPARSER_API int  DsonDocument_GetVertexInfluenceCount(DsonDocumentHandle handle, int modifierIndex, int vertexIndex, int maxInfluences);

// Returns the bone node id string and normalized weight [0,1] for influence i on vertexIndex.
// Influences are sorted descending by weight; all influences for this vertex sum to 1.0.
// Sets *boneNodeId="" and *weight=0.0 and returns false if influenceIndex is out of range.
DSONPARSER_API bool DsonDocument_GetVertexBoneInfluence(DsonDocumentHandle handle, int modifierIndex, int vertexIndex, int influenceIndex, const char** boneNodeId, double* weight);

// Same as GetVertexBoneInfluence but weights are renormalized over the top
// min(totalInfluences, maxInfluences) influences so they sum to 1.0.
// Use this instead of GetVertexBoneInfluence when building FSoftSkinVertex.
DSONPARSER_API bool DsonDocument_GetVertexBoneInfluenceCapped(DsonDocumentHandle handle, int modifierIndex, int vertexIndex, int influenceIndex, int maxInfluences, const char** boneNodeId, double* weight);

// ---- D. UV Sets (library uv_sets, not scene instances) ----
DSONPARSER_API const char* DsonDocument_GetUVSetId(DsonDocumentHandle handle, int uvSetIndex);
DSONPARSER_API int         DsonDocument_GetUVCount(DsonDocumentHandle handle, int uvSetIndex);
DSONPARSER_API double      DsonDocument_GetUVU(DsonDocumentHandle handle, int uvSetIndex, int uvIndex);
DSONPARSER_API double      DsonDocument_GetUVV(DsonDocumentHandle handle, int uvSetIndex, int uvIndex);
// vertex_count from the uv_set entry — needed for identity-default expansion
// when consumers expand sparse overrides into a flat per-corner array.
DSONPARSER_API int DsonDocument_GetUVSetVertexCount(DsonDocumentHandle handle, int uvSetIndex);

// Sparse UV override entries from polygon_vertex_indices.
// Each entry is [face, corner, uv_index] for a face corner whose UV index
// differs from its vertex index. Consumers expand to a flat per-corner array
// by seeding with identity (uv_index = vertex_index) and applying overrides.
DSONPARSER_API int DsonDocument_GetUVOverrideCount(DsonDocumentHandle handle, int uvSetIndex);
DSONPARSER_API int DsonDocument_GetUVOverrideFace(DsonDocumentHandle handle, int uvSetIndex, int overrideIndex);
DSONPARSER_API int DsonDocument_GetUVOverrideCorner(DsonDocumentHandle handle, int uvSetIndex, int overrideIndex);
DSONPARSER_API int DsonDocument_GetUVOverrideUVIndex(DsonDocumentHandle handle, int uvSetIndex, int overrideIndex);

// ---- E. Materials (library materials by index) ----
//
// Channels are accessed by index in the range [0, ChannelCount-1]. Each channel carries a DAZ
// id string (e.g. "Diffuse Color", "Normal Map") and a DAZ type string (e.g. "float_color",
// "float"). Consumers iterate or search by id to interpret them; the parser stores everything
// in source-file order and drops nothing.
//
DSONPARSER_API const char* DsonDocument_GetMaterialName(DsonDocumentHandle handle, int matIndex);
DSONPARSER_API const char* DsonDocument_GetMaterialGeometryId(DsonDocumentHandle handle, int matIndex);
DSONPARSER_API const char* DsonDocument_GetMaterialUVSetId(DsonDocumentHandle handle, int matIndex);

// Returns the top-level material type field (e.g. "studio/material/iray").
DSONPARSER_API const char* DsonDocument_GetMaterialType(DsonDocumentHandle handle, int matIndex);
// Returns the shader type from the material's extra[] (e.g. "studio/material/uber_iray", "studio/material/pbr_skin").
// Points to parser-owned memory; copy immediately if retention past another API call is needed.
// Returns "" when no matching extra entry exists or matIndex is out of range.
DSONPARSER_API const char* DsonDocument_GetMaterialShaderType(DsonDocumentHandle handle, int matIndex);

// Indexed channel accessors for library materials (matIndex into material_library).
DSONPARSER_API int         DsonDocument_GetMaterialChannelCount(DsonDocumentHandle handle, int matIndex);
DSONPARSER_API const char* DsonDocument_GetMaterialChannelId(DsonDocumentHandle handle, int matIndex, int channelIdx);
DSONPARSER_API const char* DsonDocument_GetMaterialChannelType(DsonDocumentHandle handle, int matIndex, int channelIdx);
DSONPARSER_API double      DsonDocument_GetMaterialChannelValue(DsonDocumentHandle handle, int matIndex, int channelIdx);
DSONPARSER_API double      DsonDocument_GetMaterialChannelColorR(DsonDocumentHandle handle, int matIndex, int channelIdx);
DSONPARSER_API double      DsonDocument_GetMaterialChannelColorG(DsonDocumentHandle handle, int matIndex, int channelIdx);
DSONPARSER_API double      DsonDocument_GetMaterialChannelColorB(DsonDocumentHandle handle, int matIndex, int channelIdx);
DSONPARSER_API bool        DsonDocument_GetMaterialChannelHasColor(DsonDocumentHandle handle, int matIndex, int channelIdx);
DSONPARSER_API const char* DsonDocument_GetMaterialChannelImageUrl(DsonDocumentHandle handle, int matIndex, int channelIdx);
DSONPARSER_API const char* DsonDocument_GetMaterialChannelTexturePath(DsonDocumentHandle handle, int matIndex, int channelIdx);

// Surface-level accessors for scene material instances (scene.materials).
DSONPARSER_API const char* DsonDocument_GetSceneMaterialName(DsonDocumentHandle handle, int sceneMatIndex);
DSONPARSER_API const char* DsonDocument_GetSceneMaterialGeometryId(DsonDocumentHandle handle, int sceneMatIndex);
DSONPARSER_API const char* DsonDocument_GetSceneMaterialUVSetId(DsonDocumentHandle handle, int sceneMatIndex);
DSONPARSER_API const char* DsonDocument_GetSceneMaterialType(DsonDocumentHandle handle, int sceneMatIndex);
DSONPARSER_API const char* DsonDocument_GetSceneMaterialShaderType(DsonDocumentHandle handle, int sceneMatIndex);

// Indexed channel accessors for scene material instances (scene.materials).
DSONPARSER_API int         DsonDocument_GetSceneMaterialChannelCount(DsonDocumentHandle handle, int sceneMatIndex);
DSONPARSER_API const char* DsonDocument_GetSceneMaterialChannelId(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx);
DSONPARSER_API const char* DsonDocument_GetSceneMaterialChannelType(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx);
DSONPARSER_API double      DsonDocument_GetSceneMaterialChannelValue(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx);
DSONPARSER_API double      DsonDocument_GetSceneMaterialChannelColorR(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx);
DSONPARSER_API double      DsonDocument_GetSceneMaterialChannelColorG(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx);
DSONPARSER_API double      DsonDocument_GetSceneMaterialChannelColorB(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx);
DSONPARSER_API bool        DsonDocument_GetSceneMaterialChannelHasColor(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx);
DSONPARSER_API const char* DsonDocument_GetSceneMaterialChannelImageUrl(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx);
DSONPARSER_API const char* DsonDocument_GetSceneMaterialChannelTexturePath(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx);
DSONPARSER_API int         DsonDocument_GetSceneMaterialChannelLayerCount(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx);
DSONPARSER_API const char* DsonDocument_GetSceneMaterialChannelLayerTexturePath(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx);
DSONPARSER_API const char* DsonDocument_GetSceneMaterialChannelLayerLabel(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx);
// Per-layer LIE compositing metadata — raw DAZ map-element values, verbatim; no compositing performed.
// Layer 0 is the base; higher indexes are overlays in document order.
// Use GetSceneMaterialChannelLayerCount to bounds-check before calling.
// @since 1.4.0
DSONPARSER_API const char* DsonDocument_GetSceneMaterialChannelLayerBlendMode(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx);
// Opacity is the raw "transparency" value (1 = opaque). Sentinel 0.0 collides with a legitimately-transparent layer; bound-check with GetSceneMaterialChannelLayerCount first.
// @since 1.4.0
DSONPARSER_API double      DsonDocument_GetSceneMaterialChannelLayerOpacity(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx);
// @since 1.4.0
DSONPARSER_API bool        DsonDocument_GetSceneMaterialChannelLayerActive(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx);
// @since 1.4.0
DSONPARSER_API bool        DsonDocument_GetSceneMaterialChannelLayerInvert(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx);
// @since 1.4.0
DSONPARSER_API double      DsonDocument_GetSceneMaterialChannelLayerColorR(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx);
// @since 1.4.0
DSONPARSER_API double      DsonDocument_GetSceneMaterialChannelLayerColorG(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx);
// @since 1.4.0
DSONPARSER_API double      DsonDocument_GetSceneMaterialChannelLayerColorB(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx);
// @since 1.4.0
DSONPARSER_API double      DsonDocument_GetSceneMaterialChannelLayerRotation(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx);
// Returns 1.0 for an invalid layerIdx (scale exception per R1 contract).
// @since 1.4.0
DSONPARSER_API double      DsonDocument_GetSceneMaterialChannelLayerScaleX(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx);
// Returns 1.0 for an invalid layerIdx (scale exception per R1 contract).
// @since 1.4.0
DSONPARSER_API double      DsonDocument_GetSceneMaterialChannelLayerScaleY(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx);
// @since 1.4.0
DSONPARSER_API double      DsonDocument_GetSceneMaterialChannelLayerOffsetX(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx);
// @since 1.4.0
DSONPARSER_API double      DsonDocument_GetSceneMaterialChannelLayerOffsetY(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx);
// @since 1.4.0
DSONPARSER_API bool        DsonDocument_GetSceneMaterialChannelLayerMirrorX(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx);
// @since 1.4.0
DSONPARSER_API bool        DsonDocument_GetSceneMaterialChannelLayerMirrorY(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx);

// Scene post-load addon manifest (scene.extra "Character Addon Loader",
// settings.PostLoadAddons): companion figures a character preset loads but does
// not list in scene.nodes (e.g. Genesis 9 eyes/mouth/eyelashes/tears). index is a
// flat walk across every manifest entry's slots, in document (.duf) order.
// MatPreset may be "" for an addon slot that has no MAT preset.
// @since 1.1.0
DSONPARSER_API int         DsonDocument_GetScenePostLoadAddonCount(DsonDocumentHandle handle);
DSONPARSER_API const char* DsonDocument_GetScenePostLoadAddonSlot(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetScenePostLoadAddonAssetName(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetScenePostLoadAddonAssetFile(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetScenePostLoadAddonMatPreset(DsonDocumentHandle handle, int index);

// Scene post-load scripts (scene.extra "scene_post_load_script"): DAZ Scripts a
// preset runs at load that the static import does NOT execute (e.g. the Genesis 9
// "Character Addon Loader" .dse, or a "Remove Duplicate Eyebrows" cleanup script).
// Surfaced verbatim — Name, Type, and the content-relative .dse/.dsa path (File may
// be "" when the entry names no script) — so a consumer can warn (no-silent-fails)
// that the script's runtime effects are not captured. The parser neither loads nor
// runs the script (cf. the no-recursive-load boundary). index is a flat,
// document-order walk across the scene.extra entries that carry a script; an entry
// may also appear in the PostLoadAddon family above when it carries both.
// @since 2.3.0
DSONPARSER_API int         DsonDocument_GetScenePostLoadScriptCount(DsonDocumentHandle handle);
DSONPARSER_API const char* DsonDocument_GetScenePostLoadScriptName(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetScenePostLoadScriptType(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetScenePostLoadScriptFile(DsonDocumentHandle handle, int index);

// Scene animations (scene.animations): one entry per keyframe channel.
// Each entry carries the verbatim DSON property pointer (url) and the first key's
// typed value. Per R6.4 this surface is a raw passthrough — the parser does NOT
// apply these onto scene.materials. The consumer reads both surfaces and decides.
// @since 1.2.0
DSONPARSER_API int         DsonDocument_GetSceneAnimationCount(DsonDocumentHandle handle);
DSONPARSER_API const char* DsonDocument_GetSceneAnimationUrl(DsonDocumentHandle handle, int animIndex);
// Returns the ValueKind of keys[0][1]: 0=null, 1=number, 2=bool, 3=string, 4=color.
// Returns -1 when handle is invalid or animIndex is out of range (value/index family).
// @since 1.2.0
DSONPARSER_API int         DsonDocument_GetSceneAnimationValueKind(DsonDocumentHandle handle, int animIndex);
// @since 1.2.0
DSONPARSER_API double      DsonDocument_GetSceneAnimationFloat(DsonDocumentHandle handle, int animIndex);
// @since 1.2.0
DSONPARSER_API bool        DsonDocument_GetSceneAnimationBool(DsonDocumentHandle handle, int animIndex);
// @since 1.2.0
DSONPARSER_API const char* DsonDocument_GetSceneAnimationString(DsonDocumentHandle handle, int animIndex);
// @since 1.2.0
DSONPARSER_API double      DsonDocument_GetSceneAnimationColorR(DsonDocumentHandle handle, int animIndex);
// @since 1.2.0
DSONPARSER_API double      DsonDocument_GetSceneAnimationColorG(DsonDocumentHandle handle, int animIndex);
// @since 1.2.0
DSONPARSER_API double      DsonDocument_GetSceneAnimationColorB(DsonDocumentHandle handle, int animIndex);

// ---- F. Morph Targets ----
// morphIndex is an index into the filtered list of modifiers where type == "morph"
DSONPARSER_API int         DsonDocument_GetMorphCount(DsonDocumentHandle handle);
DSONPARSER_API const char* DsonDocument_GetMorphId(DsonDocumentHandle handle, int morphIndex);
DSONPARSER_API const char* DsonDocument_GetMorphName(DsonDocumentHandle handle, int morphIndex);
DSONPARSER_API const char* DsonDocument_GetMorphLabel(DsonDocumentHandle handle, int morphIndex);
DSONPARSER_API int         DsonDocument_GetMorphDeltaCount(DsonDocumentHandle handle, int morphIndex);
DSONPARSER_API int         DsonDocument_GetMorphDeltaVertexIndex(DsonDocumentHandle handle, int morphIndex, int deltaIndex);
DSONPARSER_API double      DsonDocument_GetMorphDeltaX(DsonDocumentHandle handle, int morphIndex, int deltaIndex);
DSONPARSER_API double      DsonDocument_GetMorphDeltaY(DsonDocumentHandle handle, int morphIndex, int deltaIndex);
DSONPARSER_API double      DsonDocument_GetMorphDeltaZ(DsonDocumentHandle handle, int morphIndex, int deltaIndex);

DSONPARSER_API int         DsonDocument_GetMorphNormalDeltaCount(DsonDocumentHandle handle, int morphIndex);
DSONPARSER_API int         DsonDocument_GetMorphNormalDeltaVertexIndex(DsonDocumentHandle handle, int morphIndex, int deltaIndex);
DSONPARSER_API double      DsonDocument_GetMorphNormalDeltaX(DsonDocumentHandle handle, int morphIndex, int deltaIndex);
DSONPARSER_API double      DsonDocument_GetMorphNormalDeltaY(DsonDocumentHandle handle, int morphIndex, int deltaIndex);
DSONPARSER_API double      DsonDocument_GetMorphNormalDeltaZ(DsonDocumentHandle handle, int morphIndex, int deltaIndex);
// Returns the geometry id fragment from the morph modifier's parent URL (the part after '#').
// e.g. parent = ".../Genesis9.dsf#Genesis9-1" → returns "Genesis9-1". Returns "" if no '#' or out of range.
DSONPARSER_API const char* DsonDocument_GetMorphGeometryId(DsonDocumentHandle handle, int morphIndex);

// Unknown keys diagnostics
DSONPARSER_API int DsonDocument_GetContextCount(DsonDocumentHandle handle);
DSONPARSER_API const char* DsonDocument_GetContextName(DsonDocumentHandle handle, int index);
DSONPARSER_API int DsonDocument_GetUnknownKeyCount(DsonDocumentHandle handle, const char* context);
DSONPARSER_API const char* DsonDocument_GetUnknownKey(DsonDocumentHandle handle, const char* context, int index);

// Clear document
DSONPARSER_API void DsonDocument_Clear(DsonDocumentHandle handle);

// Destroy document
DSONPARSER_API void DsonDocument_Destroy(DsonDocumentHandle handle);

// Get last error message
DSONPARSER_API const char* DsonParser_GetLastError();

// Get this library's version string, e.g. "1.0.0" (mirrors DSONPARSER_VERSION_STRING).
// Always returns a non-null, parser-owned static literal; it cannot fail.
// This is the LIBRARY's own version — distinct from DsonDocument_GetFileVersion(),
// which returns a parsed asset's file_version field.
// @since 1.0.0
DSONPARSER_API const char* DsonParser_GetVersion(void);

#ifdef __cplusplus
}
#endif
