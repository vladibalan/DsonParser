#pragma once

#ifdef DSONPARSER_EXPORTS
#define DSONPARSER_API __declspec(dllexport)
#else
#define DSONPARSER_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle to DsonDocument
typedef void* DsonDocumentHandle;

// Create a new DSON document
DSONPARSER_API DsonDocumentHandle DsonDocument_Create();

// Load DSON from file
DSONPARSER_API int DsonDocument_LoadFromFile(DsonDocumentHandle handle, const char* filepath);

// Load DSON from string
DSONPARSER_API int DsonDocument_LoadFromString(DsonDocumentHandle handle, const char* jsonString);

// Get file version
DSONPARSER_API const char* DsonDocument_GetFileVersion(DsonDocumentHandle handle);

// Get asset info
DSONPARSER_API const char* DsonDocument_GetAssetId(DsonDocumentHandle handle);
DSONPARSER_API const char* DsonDocument_GetAssetType(DsonDocumentHandle handle);

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
// Node center_point (joint origin) components
DSONPARSER_API double DsonDocument_GetNodeCenterPointX(DsonDocumentHandle handle, int index);
DSONPARSER_API double DsonDocument_GetNodeCenterPointY(DsonDocumentHandle handle, int index);
DSONPARSER_API double DsonDocument_GetNodeCenterPointZ(DsonDocumentHandle handle, int index);

// Get scene node info by index (the "scene.nodes" array, distinct from node_library).
// Scene nodes are instances: they reference a library node via Url and carry a Label,
// and typically have no Type of their own.
DSONPARSER_API int DsonDocument_GetSceneNodeCount(DsonDocumentHandle handle);
DSONPARSER_API const char* DsonDocument_GetSceneNodeId(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetSceneNodeName(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetSceneNodeLabel(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetSceneNodeType(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetSceneNodeUrl(DsonDocumentHandle handle, int index);

// Other scene instance collections (scene.modifiers / scene.materials / scene.uvs)
DSONPARSER_API int DsonDocument_GetSceneModifierCount(DsonDocumentHandle handle);
DSONPARSER_API const char* DsonDocument_GetSceneModifierId(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetSceneModifierUrl(DsonDocumentHandle handle, int index);

DSONPARSER_API int DsonDocument_GetSceneMaterialCount(DsonDocumentHandle handle);
DSONPARSER_API const char* DsonDocument_GetSceneMaterialId(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetSceneMaterialUrl(DsonDocumentHandle handle, int index);

DSONPARSER_API int DsonDocument_GetSceneUVSetCount(DsonDocumentHandle handle);
DSONPARSER_API const char* DsonDocument_GetSceneUVSetId(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetSceneUVSetUrl(DsonDocumentHandle handle, int index);

// Get geometry info by index
DSONPARSER_API const char* DsonDocument_GetGeometryId(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetGeometryName(DsonDocumentHandle handle, int index);
DSONPARSER_API int DsonDocument_GetGeometryVertexCount(DsonDocumentHandle handle, int index);
DSONPARSER_API int DsonDocument_GetGeometryPolygonCount(DsonDocumentHandle handle, int index);

// Get modifier info by index
DSONPARSER_API const char* DsonDocument_GetModifierId(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetModifierName(DsonDocumentHandle handle, int index);
DSONPARSER_API const char* DsonDocument_GetModifierType(DsonDocumentHandle handle, int index);
// Skin binding info for a modifier (0 if the modifier has no skin payload)
DSONPARSER_API int DsonDocument_GetModifierSkinVertexCount(DsonDocumentHandle handle, int index);
DSONPARSER_API int DsonDocument_GetModifierSkinJointCount(DsonDocumentHandle handle, int index);

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

#ifdef __cplusplus
}
#endif