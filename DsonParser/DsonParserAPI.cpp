#include "pch.h"
#include "DsonParserAPI.h"
#include "DsonTypes.h"
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <unordered_map>

// Per-handle context: bundles the parsed document with the caches that back the
// index-based enumeration APIs. Keeping this state on the handle (instead of in
// process globals) means distinct documents - several files open at once, or
// parsed on different threads - never clobber each other's caches.
namespace {

struct VertexInfluence {
    std::string boneNodeId;
    double weight;
};

struct VertexInfluenceCache {
    int modifierIndex = -1;
    std::unordered_map<int, std::vector<VertexInfluence>> data;
    bool built = false;
};

struct DsonContext {
    Dson::DsonDocument document;
    std::vector<std::string> contextCache;
    std::vector<std::string> unknownKeysCache;
    std::string unknownKeysCacheContext;
    std::vector<int> morphIndexCache;   // indices into document.modifiers where type == "morph"
    bool morphIndexCacheDirty = true;
    VertexInfluenceCache skinCache;
    std::string lastMorphGeometryId;    // stable storage for GetMorphGeometryId return value
};

// errno-style last error: each thread gets its own slot, so concurrent loads on
// different threads don't race. Read it on the thread that made the failing call.
thread_local std::string t_lastError;

DsonContext* GetContext(DsonDocumentHandle handle) {
    return static_cast<DsonContext*>(handle);
}

Dson::DsonDocument* GetDocument(DsonDocumentHandle handle) {
    return &GetContext(handle)->document;
}

void StoreLastError(const std::string& error) {
    t_lastError = error;
}

static void EnsureMorphCache(DsonContext* ctx) {
    if (!ctx->morphIndexCacheDirty) return;
    ctx->morphIndexCache.clear();
    for (int i = 0; i < static_cast<int>(ctx->document.modifiers.size()); i++) {
        if (ctx->document.modifiers[i].type.value == "morph") {
            ctx->morphIndexCache.push_back(i);
        }
    }
    ctx->morphIndexCacheDirty = false;
}

static void EnsureSkinCache(DsonContext* ctx, int modifierIndex) {
    if (ctx->skinCache.built && ctx->skinCache.modifierIndex == modifierIndex) return;

    ctx->skinCache.data.clear();
    ctx->skinCache.modifierIndex = modifierIndex;

    const auto& doc = ctx->document;
    if (modifierIndex < 0 || modifierIndex >= static_cast<int>(doc.modifiers.size()) ||
        !doc.modifiers[modifierIndex].has_skin) {
        ctx->skinCache.built = true;
        return;
    }

    const auto& joints = doc.modifiers[modifierIndex].skin.joints;
    for (int ji = 0; ji < static_cast<int>(joints.size()); ++ji) {
        const auto& joint = joints[ji];
        const std::string& boneNodeId = joint.node.value;
        for (int wi = 0; wi < static_cast<int>(joint.weight_indices.size()); ++wi) {
            int vertIdx = joint.weight_indices[wi];
            VertexInfluence inf;
            inf.boneNodeId = boneNodeId;
            inf.weight = joint.weights[wi];
            ctx->skinCache.data[vertIdx].push_back(inf);
        }
    }

    for (auto& kv : ctx->skinCache.data) {
        std::vector<VertexInfluence>& influences = kv.second;
        std::sort(influences.begin(), influences.end(),
            [](const VertexInfluence& a, const VertexInfluence& b) { return a.weight > b.weight; });
        double sum = 0.0;
        for (int i = 0; i < static_cast<int>(influences.size()); ++i) sum += influences[i].weight;
        if (sum > 0.0) {
            for (int i = 0; i < static_cast<int>(influences.size()); ++i) influences[i].weight /= sum;
        }
    }

    ctx->skinCache.built = true;
}

static const Dson::MaterialChannel* GetMaterialChannel(const Dson::Material& mat, int channelId) {
    switch (channelId) {
        case 0: return &mat.diffuse;
        case 1: return &mat.specular;
        case 2: return &mat.roughness;
        case 3: return &mat.normal;
        case 4: return &mat.opacity;
        case 5: return &mat.subsurface;
        case 6: return &mat.emission;
        case 7: return &mat.bump;
        default: return nullptr;
    }
}

} // namespace

DsonDocumentHandle DsonDocument_Create() {
    try {
        return new DsonContext();
    }
    catch (...) {
        StoreLastError("Failed to create DsonDocument");
        return nullptr;
    }
}

int DsonDocument_LoadFromFile(DsonDocumentHandle handle, const char* filepath) {
    if (!handle || !filepath) {
        StoreLastError("Invalid handle or filepath");
        return 0;
    }
    
    try {
        Dson::DsonDocument* doc = GetDocument(handle);
        std::string errorMsg;
        if (doc->LoadFromFile(filepath, errorMsg)) {
            StoreLastError("");
            DsonContext* ctx = GetContext(handle);
            ctx->contextCache = ctx->document.GetAllContextsWithUnknownKeys();
            ctx->unknownKeysCache.clear();
            ctx->unknownKeysCacheContext.clear();
            ctx->morphIndexCacheDirty = true;
            ctx->skinCache.data.clear();
            ctx->skinCache.built = false;
            ctx->skinCache.modifierIndex = -1;
            return 1;
        }
        StoreLastError(errorMsg);
        return 0;
    }
    catch (const std::exception& e) {
        StoreLastError(std::string("Exception: ") + e.what());
        return 0;
    }
    catch (...) {
        StoreLastError("Unknown exception occurred");
        return 0;
    }
}

int DsonDocument_LoadFromString(DsonDocumentHandle handle, const char* jsonString) {
    if (!handle || !jsonString) {
        StoreLastError("Invalid handle or string");
        return 0;
    }
    
    try {
        Dson::DsonDocument* doc = GetDocument(handle);
        std::string errorMsg;
        if (doc->LoadFromString(jsonString, errorMsg)) {
            StoreLastError("");
            DsonContext* ctx = GetContext(handle);
            ctx->contextCache = ctx->document.GetAllContextsWithUnknownKeys();
            ctx->unknownKeysCache.clear();
            ctx->unknownKeysCacheContext.clear();
            ctx->morphIndexCacheDirty = true;
            ctx->skinCache.data.clear();
            ctx->skinCache.built = false;
            ctx->skinCache.modifierIndex = -1;
            return 1;
        }
        StoreLastError(errorMsg);
        return 0;
    }
    catch (const std::exception& e) {
        StoreLastError(std::string("Exception: ") + e.what());
        return 0;
    }
    catch (...) {
        StoreLastError("Unknown exception occurred");
        return 0;
    }
}

const char* DsonDocument_GetFileVersion(DsonDocumentHandle handle) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    return doc->file_version.c_str();
}

const char* DsonDocument_GetAssetId(DsonDocumentHandle handle) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    return doc->asset_info.id.c_str();
}

const char* DsonDocument_GetAssetType(DsonDocumentHandle handle) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    return doc->asset_info.type.c_str();
}

double DsonDocument_GetUnitScale(DsonDocumentHandle handle) {
    if (!handle) return 1.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    return doc->asset_info.unit_scale;
}

int DsonDocument_GetNodeCount(DsonDocumentHandle handle) {
    if (!handle) return 0;
    Dson::DsonDocument* doc = GetDocument(handle);
    return static_cast<int>(doc->nodes.size());
}

int DsonDocument_GetGeometryCount(DsonDocumentHandle handle) {
    if (!handle) return 0;
    Dson::DsonDocument* doc = GetDocument(handle);
    return static_cast<int>(doc->geometries.size());
}

int DsonDocument_GetMaterialCount(DsonDocumentHandle handle) {
    if (!handle) return 0;
    Dson::DsonDocument* doc = GetDocument(handle);
    return static_cast<int>(doc->materials.size());
}

int DsonDocument_GetModifierCount(DsonDocumentHandle handle) {
    if (!handle) return 0;
    Dson::DsonDocument* doc = GetDocument(handle);
    return static_cast<int>(doc->modifiers.size());
}

int DsonDocument_GetImageCount(DsonDocumentHandle handle) {
    if (!handle) return 0;
    Dson::DsonDocument* doc = GetDocument(handle);
    return static_cast<int>(doc->images.size());
}

int DsonDocument_GetUVSetCount(DsonDocumentHandle handle) {
    if (!handle) return 0;
    Dson::DsonDocument* doc = GetDocument(handle);
    return static_cast<int>(doc->uv_sets.size());
}

const char* DsonDocument_GetNodeId(DsonDocumentHandle handle, int index) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (index < 0 || index >= static_cast<int>(doc->nodes.size())) return "";
    return doc->nodes[index].id.c_str();
}

const char* DsonDocument_GetNodeName(DsonDocumentHandle handle, int index) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (index < 0 || index >= static_cast<int>(doc->nodes.size())) return "";
    return doc->nodes[index].name.c_str();
}

const char* DsonDocument_GetNodeType(DsonDocumentHandle handle, int index) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (index < 0 || index >= static_cast<int>(doc->nodes.size())) return "";
    return doc->nodes[index].type.c_str();
}

double DsonDocument_GetNodeCenterPointX(DsonDocumentHandle handle, int index) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (index < 0 || index >= static_cast<int>(doc->nodes.size())) return 0.0;
    return doc->nodes[index].center_point.x;
}

double DsonDocument_GetNodeCenterPointY(DsonDocumentHandle handle, int index) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (index < 0 || index >= static_cast<int>(doc->nodes.size())) return 0.0;
    return doc->nodes[index].center_point.y;
}

double DsonDocument_GetNodeCenterPointZ(DsonDocumentHandle handle, int index) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (index < 0 || index >= static_cast<int>(doc->nodes.size())) return 0.0;
    return doc->nodes[index].center_point.z;
}

// Scene nodes (the "scene" array) are tracked separately from the node_library
// above, so they get their own count and accessors.
int DsonDocument_GetSceneNodeCount(DsonDocumentHandle handle) {
    if (!handle) return 0;
    Dson::DsonDocument* doc = GetDocument(handle);
    return static_cast<int>(doc->scene.nodes.size());
}

const char* DsonDocument_GetSceneNodeId(DsonDocumentHandle handle, int index) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (index < 0 || index >= static_cast<int>(doc->scene.nodes.size())) return "";
    return doc->scene.nodes[index].id.c_str();
}

const char* DsonDocument_GetSceneNodeName(DsonDocumentHandle handle, int index) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (index < 0 || index >= static_cast<int>(doc->scene.nodes.size())) return "";
    return doc->scene.nodes[index].name.c_str();
}

const char* DsonDocument_GetSceneNodeLabel(DsonDocumentHandle handle, int index) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (index < 0 || index >= static_cast<int>(doc->scene.nodes.size())) return "";
    return doc->scene.nodes[index].label.c_str();
}

const char* DsonDocument_GetSceneNodeType(DsonDocumentHandle handle, int index) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (index < 0 || index >= static_cast<int>(doc->scene.nodes.size())) return "";
    return doc->scene.nodes[index].type.c_str();
}

const char* DsonDocument_GetSceneNodeUrl(DsonDocumentHandle handle, int index) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (index < 0 || index >= static_cast<int>(doc->scene.nodes.size())) return "";
    return doc->scene.nodes[index].url.c_str();
}

int DsonDocument_GetSceneNodeGeometryCount(DsonDocumentHandle handle, int sceneNodeIndex) {
    if (!handle) return -1;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (sceneNodeIndex < 0 || sceneNodeIndex >= static_cast<int>(doc->scene.nodes.size())) return -1;
    return static_cast<int>(doc->scene.nodes[sceneNodeIndex].geometries.size());
}

const char* DsonDocument_GetSceneNodeGeometryId(DsonDocumentHandle handle, int sceneNodeIndex, int geomRefIndex) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (sceneNodeIndex < 0 || sceneNodeIndex >= static_cast<int>(doc->scene.nodes.size())) return "";
    const auto& geoms = doc->scene.nodes[sceneNodeIndex].geometries;
    if (geomRefIndex < 0 || geomRefIndex >= static_cast<int>(geoms.size())) return "";
    return geoms[geomRefIndex].id.c_str();
}

const char* DsonDocument_GetSceneNodeGeometryUrl(DsonDocumentHandle handle, int sceneNodeIndex, int geomRefIndex) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (sceneNodeIndex < 0 || sceneNodeIndex >= static_cast<int>(doc->scene.nodes.size())) return "";
    const auto& geoms = doc->scene.nodes[sceneNodeIndex].geometries;
    if (geomRefIndex < 0 || geomRefIndex >= static_cast<int>(geoms.size())) return "";
    return geoms[geomRefIndex].url.c_str();
}

// Scene modifier instances (scene.modifiers)
int DsonDocument_GetSceneModifierCount(DsonDocumentHandle handle) {
    if (!handle) return 0;
    Dson::DsonDocument* doc = GetDocument(handle);
    return static_cast<int>(doc->scene.modifiers.size());
}

const char* DsonDocument_GetSceneModifierId(DsonDocumentHandle handle, int index) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (index < 0 || index >= static_cast<int>(doc->scene.modifiers.size())) return "";
    return doc->scene.modifiers[index].id.c_str();
}

const char* DsonDocument_GetSceneModifierUrl(DsonDocumentHandle handle, int index) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (index < 0 || index >= static_cast<int>(doc->scene.modifiers.size())) return "";
    return doc->scene.modifiers[index].url.c_str();
}

// Scene material instances (scene.materials)
int DsonDocument_GetSceneMaterialCount(DsonDocumentHandle handle) {
    if (!handle) return 0;
    Dson::DsonDocument* doc = GetDocument(handle);
    return static_cast<int>(doc->scene.materials.size());
}

const char* DsonDocument_GetSceneMaterialId(DsonDocumentHandle handle, int index) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (index < 0 || index >= static_cast<int>(doc->scene.materials.size())) return "";
    return doc->scene.materials[index].id.c_str();
}

const char* DsonDocument_GetSceneMaterialUrl(DsonDocumentHandle handle, int index) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (index < 0 || index >= static_cast<int>(doc->scene.materials.size())) return "";
    return doc->scene.materials[index].url.c_str();
}

// Scene UV set instances (scene.uvs)
int DsonDocument_GetSceneUVSetCount(DsonDocumentHandle handle) {
    if (!handle) return 0;
    Dson::DsonDocument* doc = GetDocument(handle);
    return static_cast<int>(doc->scene.uvs.size());
}

const char* DsonDocument_GetSceneUVSetId(DsonDocumentHandle handle, int index) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (index < 0 || index >= static_cast<int>(doc->scene.uvs.size())) return "";
    return doc->scene.uvs[index].id.c_str();
}

const char* DsonDocument_GetSceneUVSetUrl(DsonDocumentHandle handle, int index) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (index < 0 || index >= static_cast<int>(doc->scene.uvs.size())) return "";
    return doc->scene.uvs[index].url.c_str();
}

const char* DsonDocument_GetMaterialId(DsonDocumentHandle handle, int index) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (index < 0 || index >= static_cast<int>(doc->materials.size())) return "";
    return doc->materials[index].id.c_str();
}

const char* DsonDocument_GetGeometryId(DsonDocumentHandle handle, int index) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (index < 0 || index >= static_cast<int>(doc->geometries.size())) return "";
    return doc->geometries[index].id.c_str();
}

const char* DsonDocument_GetGeometryName(DsonDocumentHandle handle, int index) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (index < 0 || index >= static_cast<int>(doc->geometries.size())) return "";
    return doc->geometries[index].name.c_str();
}

int DsonDocument_GetGeometryVertexCount(DsonDocumentHandle handle, int index) {
    if (!handle) return 0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (index < 0 || index >= static_cast<int>(doc->geometries.size())) return 0;
    return doc->geometries[index].vertex_count;
}

int DsonDocument_GetGeometryPolygonCount(DsonDocumentHandle handle, int index) {
    if (!handle) return 0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (index < 0 || index >= static_cast<int>(doc->geometries.size())) return 0;
    return doc->geometries[index].polygon_count;
}

const char* DsonDocument_GetGeometryDefaultUVSetId(DsonDocumentHandle handle, int geomIndex) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (geomIndex < 0 || geomIndex >= static_cast<int>(doc->geometries.size())) return "";
    return doc->geometries[geomIndex].default_uv_set_id.c_str();
}

const char* DsonDocument_GetModifierId(DsonDocumentHandle handle, int index) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (index < 0 || index >= static_cast<int>(doc->modifiers.size())) return "";
    return doc->modifiers[index].id.c_str();
}

const char* DsonDocument_GetModifierName(DsonDocumentHandle handle, int index) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (index < 0 || index >= static_cast<int>(doc->modifiers.size())) return "";
    return doc->modifiers[index].name.c_str();
}

const char* DsonDocument_GetModifierType(DsonDocumentHandle handle, int index) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (index < 0 || index >= static_cast<int>(doc->modifiers.size())) return "";
    return doc->modifiers[index].type.c_str();
}

int DsonDocument_GetModifierSkinVertexCount(DsonDocumentHandle handle, int index) {
    if (!handle) return 0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (index < 0 || index >= static_cast<int>(doc->modifiers.size())) return 0;
    return doc->modifiers[index].skin.vertex_count;
}

int DsonDocument_GetModifierSkinJointCount(DsonDocumentHandle handle, int index) {
    if (!handle) return 0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (index < 0 || index >= static_cast<int>(doc->modifiers.size())) return 0;
    return static_cast<int>(doc->modifiers[index].skin.joints.size());
}

// Unknown keys diagnostics
int DsonDocument_GetContextCount(DsonDocumentHandle handle) {
    if (!handle) return 0;
    return static_cast<int>(GetContext(handle)->contextCache.size());
}

const char* DsonDocument_GetContextName(DsonDocumentHandle handle, int index) {
    if (!handle) return "";
    DsonContext* ctx = GetContext(handle);
    if (index < 0 || index >= static_cast<int>(ctx->contextCache.size())) return "";
    return ctx->contextCache[index].c_str();
}

int DsonDocument_GetUnknownKeyCount(DsonDocumentHandle handle, const char* context) {
    if (!handle || !context) return 0;
    DsonContext* ctx = GetContext(handle);
    if (ctx->unknownKeysCacheContext != context) {
        ctx->unknownKeysCache = ctx->document.GetUnknownKeys(context);
        ctx->unknownKeysCacheContext = context;
    }
    return static_cast<int>(ctx->unknownKeysCache.size());
}

const char* DsonDocument_GetUnknownKey(DsonDocumentHandle handle, const char* context, int index) {
    if (!handle || !context) return "";
    DsonContext* ctx = GetContext(handle);
    if (ctx->unknownKeysCacheContext != context) {
        ctx->unknownKeysCache = ctx->document.GetUnknownKeys(context);
        ctx->unknownKeysCacheContext = context;
    }
    if (index < 0 || index >= static_cast<int>(ctx->unknownKeysCache.size())) return "";
    return ctx->unknownKeysCache[index].c_str();
}

void DsonDocument_Clear(DsonDocumentHandle handle) {
    if (!handle) return;
    DsonContext* ctx = GetContext(handle);
    ctx->document.Clear();
    ctx->contextCache.clear();
    ctx->unknownKeysCache.clear();
    ctx->unknownKeysCacheContext.clear();
    ctx->morphIndexCache.clear();
    ctx->morphIndexCacheDirty = true;
    ctx->skinCache.data.clear();
    ctx->skinCache.built = false;
    ctx->skinCache.modifierIndex = -1;
}

void DsonDocument_Destroy(DsonDocumentHandle handle) {
    if (!handle) return;
    delete GetContext(handle);
}

// ============================================================
// A. Geometry — vertex positions
// ============================================================

int DsonDocument_GetVertexCount(DsonDocumentHandle handle, int geomIndex) {
    if (!handle) return -1;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (geomIndex < 0 || geomIndex >= static_cast<int>(doc->geometries.size())) return -1;
    return doc->geometries[geomIndex].vertex_count;
}

double DsonDocument_GetVertexX(DsonDocumentHandle handle, int geomIndex, int vertexIndex) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (geomIndex < 0 || geomIndex >= static_cast<int>(doc->geometries.size())) return 0.0;
    const auto& verts = doc->geometries[geomIndex].vertices.values;
    int idx = vertexIndex * 3;
    if (vertexIndex < 0 || idx + 2 >= static_cast<int>(verts.size())) return 0.0;
    return verts[idx];
}

double DsonDocument_GetVertexY(DsonDocumentHandle handle, int geomIndex, int vertexIndex) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (geomIndex < 0 || geomIndex >= static_cast<int>(doc->geometries.size())) return 0.0;
    const auto& verts = doc->geometries[geomIndex].vertices.values;
    int idx = vertexIndex * 3;
    if (vertexIndex < 0 || idx + 2 >= static_cast<int>(verts.size())) return 0.0;
    return verts[idx + 1];
}

double DsonDocument_GetVertexZ(DsonDocumentHandle handle, int geomIndex, int vertexIndex) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (geomIndex < 0 || geomIndex >= static_cast<int>(doc->geometries.size())) return 0.0;
    const auto& verts = doc->geometries[geomIndex].vertices.values;
    int idx = vertexIndex * 3;
    if (vertexIndex < 0 || idx + 2 >= static_cast<int>(verts.size())) return 0.0;
    return verts[idx + 2];
}

// ---- Polylist ----
// Each face entry is: [polygon_groups_idx, polygon_material_groups_idx, v0, v1, ..., vN-1]
// Indices [0] and [1] are the two leading ints; vertex indices start at [2].
// polylist_face_offsets[i] holds the start index in polylist.values for face i,
// enabling correct indexing for variable-length (mixed tri/quad) faces.

static int GetFaceStartOffset(const Dson::Geometry& geom, int faceIndex) {
    if (faceIndex < 0 || faceIndex >= static_cast<int>(geom.polylist_face_offsets.size())) return -1;
    return geom.polylist_face_offsets[faceIndex];
}

static int GetFaceLength(const Dson::Geometry& geom, int faceIndex) {
    int n = static_cast<int>(geom.polylist_face_offsets.size());
    if (faceIndex < 0 || faceIndex >= n) return -1;
    int start = geom.polylist_face_offsets[faceIndex];
    int end = (faceIndex + 1 < n) ? geom.polylist_face_offsets[faceIndex + 1]
                                   : static_cast<int>(geom.polylist.values.size());
    return end - start;
}

int DsonDocument_GetPolylistCount(DsonDocumentHandle handle, int geomIndex) {
    if (!handle) return -1;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (geomIndex < 0 || geomIndex >= static_cast<int>(doc->geometries.size())) return -1;
    return doc->geometries[geomIndex].polygon_count;
}

int DsonDocument_GetPolylistFaceVertexCount(DsonDocumentHandle handle, int geomIndex, int faceIndex) {
    if (!handle) return -1;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (geomIndex < 0 || geomIndex >= static_cast<int>(doc->geometries.size())) return -1;
    const auto& geom = doc->geometries[geomIndex];
    if (faceIndex < 0 || faceIndex >= geom.polygon_count) return -1;
    int len = GetFaceLength(geom, faceIndex);
    if (len < 2) return -1;
    return len - 2;
}

int DsonDocument_GetPolylistFaceVertex(DsonDocumentHandle handle, int geomIndex, int faceIndex, int vertexIndex) {
    if (!handle) return -1;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (geomIndex < 0 || geomIndex >= static_cast<int>(doc->geometries.size())) return -1;
    const auto& geom = doc->geometries[geomIndex];
    if (faceIndex < 0 || faceIndex >= geom.polygon_count) return -1;
    int offset = GetFaceStartOffset(geom, faceIndex);
    int len = GetFaceLength(geom, faceIndex);
    if (offset < 0 || len < 2) return -1;
    int vertsPerFace = len - 2;
    if (vertexIndex < 0 || vertexIndex >= vertsPerFace) return -1;
    return geom.polylist.values[offset + 2 + vertexIndex];
}

int DsonDocument_GetPolylistFaceMaterialIndex(DsonDocumentHandle handle, int geomIndex, int faceIndex) {
    if (!handle) return -1;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (geomIndex < 0 || geomIndex >= static_cast<int>(doc->geometries.size())) return -1;
    const auto& geom = doc->geometries[geomIndex];
    if (faceIndex < 0 || faceIndex >= geom.polygon_count) return -1;
    int offset = GetFaceStartOffset(geom, faceIndex);
    if (offset < 0 || offset + 1 >= static_cast<int>(geom.polylist.values.size())) return -1;
    return geom.polylist.values[offset + 1];
}

int DsonDocument_GetPolylistFaceGroupIndex(DsonDocumentHandle handle, int geomIndex, int faceIndex) {
    if (!handle) return -1;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (geomIndex < 0 || geomIndex >= static_cast<int>(doc->geometries.size())) return -1;
    const auto& geom = doc->geometries[geomIndex];
    if (faceIndex < 0 || faceIndex >= geom.polygon_count) return -1;
    int offset = GetFaceStartOffset(geom, faceIndex);
    if (offset < 0 || offset >= static_cast<int>(geom.polylist.values.size())) return -1;
    return geom.polylist.values[offset];
}

// ---- Polygon groups (bone region groups) ----

int DsonDocument_GetPolygonGroupCount(DsonDocumentHandle handle, int geomIndex) {
    if (!handle) return -1;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (geomIndex < 0 || geomIndex >= static_cast<int>(doc->geometries.size())) return -1;
    return static_cast<int>(doc->geometries[geomIndex].polygon_groups.size());
}

const char* DsonDocument_GetPolygonGroupName(DsonDocumentHandle handle, int geomIndex, int groupIndex) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (geomIndex < 0 || geomIndex >= static_cast<int>(doc->geometries.size())) return "";
    const auto& groups = doc->geometries[geomIndex].polygon_groups;
    if (groupIndex < 0 || groupIndex >= static_cast<int>(groups.size())) return "";
    return groups[groupIndex].c_str();
}

// ---- Material groups ----

int DsonDocument_GetPolygonMaterialGroupCount(DsonDocumentHandle handle, int geomIndex) {
    if (!handle) return -1;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (geomIndex < 0 || geomIndex >= static_cast<int>(doc->geometries.size())) return -1;
    return static_cast<int>(doc->geometries[geomIndex].polygon_material_groups.size());
}

const char* DsonDocument_GetPolygonMaterialGroupName(DsonDocumentHandle handle, int geomIndex, int groupIndex) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (geomIndex < 0 || geomIndex >= static_cast<int>(doc->geometries.size())) return "";
    const auto& groups = doc->geometries[geomIndex].polygon_material_groups;
    if (groupIndex < 0 || groupIndex >= static_cast<int>(groups.size())) return "";
    return groups[groupIndex].c_str();
}

// ---- Material groups (library materials) ----

int DsonDocument_GetMaterialGroupCount(DsonDocumentHandle handle, int matIndex) {
    if (!handle) return -1;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (matIndex < 0 || matIndex >= static_cast<int>(doc->materials.size())) return -1;
    return static_cast<int>(doc->materials[matIndex].groups.size());
}

const char* DsonDocument_GetMaterialGroupName(DsonDocumentHandle handle, int matIndex, int groupIndex) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (matIndex < 0 || matIndex >= static_cast<int>(doc->materials.size())) return "";
    const auto& groups = doc->materials[matIndex].groups;
    if (groupIndex < 0 || groupIndex >= static_cast<int>(groups.size())) return "";
    return groups[groupIndex].c_str();
}

// ---- Material groups (scene material instances) ----

int DsonDocument_GetSceneMaterialGroupCount(DsonDocumentHandle handle, int matIndex) {
    if (!handle) return -1;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (matIndex < 0 || matIndex >= static_cast<int>(doc->scene.materials.size())) return -1;
    return static_cast<int>(doc->scene.materials[matIndex].groups.size());
}

const char* DsonDocument_GetSceneMaterialGroupName(DsonDocumentHandle handle, int matIndex, int groupIndex) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (matIndex < 0 || matIndex >= static_cast<int>(doc->scene.materials.size())) return "";
    const auto& groups = doc->scene.materials[matIndex].groups;
    if (groupIndex < 0 || groupIndex >= static_cast<int>(groups.size())) return "";
    return groups[groupIndex].c_str();
}

// ============================================================
// B. Skeleton / Nodes
// ============================================================

const char* DsonDocument_GetNodeParent(DsonDocumentHandle handle, int nodeIndex) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(doc->nodes.size())) return "";
    return doc->nodes[nodeIndex].parent.c_str();
}

double DsonDocument_GetNodeEndPointX(DsonDocumentHandle handle, int nodeIndex) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(doc->nodes.size())) return 0.0;
    return doc->nodes[nodeIndex].end_point.x;
}

double DsonDocument_GetNodeEndPointY(DsonDocumentHandle handle, int nodeIndex) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(doc->nodes.size())) return 0.0;
    return doc->nodes[nodeIndex].end_point.y;
}

double DsonDocument_GetNodeEndPointZ(DsonDocumentHandle handle, int nodeIndex) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(doc->nodes.size())) return 0.0;
    return doc->nodes[nodeIndex].end_point.z;
}

double DsonDocument_GetNodeOrientationX(DsonDocumentHandle handle, int nodeIndex) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(doc->nodes.size())) return 0.0;
    return doc->nodes[nodeIndex].orientation.x;
}

double DsonDocument_GetNodeOrientationY(DsonDocumentHandle handle, int nodeIndex) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(doc->nodes.size())) return 0.0;
    return doc->nodes[nodeIndex].orientation.y;
}

double DsonDocument_GetNodeOrientationZ(DsonDocumentHandle handle, int nodeIndex) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(doc->nodes.size())) return 0.0;
    return doc->nodes[nodeIndex].orientation.z;
}

const char* DsonDocument_GetNodeRotationOrder(DsonDocumentHandle handle, int nodeIndex) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(doc->nodes.size())) return "";
    return doc->nodes[nodeIndex].rotation_order.c_str();
}

double DsonDocument_GetNodeTranslationX(DsonDocumentHandle handle, int nodeIndex) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(doc->nodes.size())) return 0.0;
    return doc->nodes[nodeIndex].translation.x;
}

double DsonDocument_GetNodeTranslationY(DsonDocumentHandle handle, int nodeIndex) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(doc->nodes.size())) return 0.0;
    return doc->nodes[nodeIndex].translation.y;
}

double DsonDocument_GetNodeTranslationZ(DsonDocumentHandle handle, int nodeIndex) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(doc->nodes.size())) return 0.0;
    return doc->nodes[nodeIndex].translation.z;
}

double DsonDocument_GetNodeRotationX(DsonDocumentHandle handle, int nodeIndex) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(doc->nodes.size())) return 0.0;
    return doc->nodes[nodeIndex].rotation.x;
}

double DsonDocument_GetNodeRotationY(DsonDocumentHandle handle, int nodeIndex) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(doc->nodes.size())) return 0.0;
    return doc->nodes[nodeIndex].rotation.y;
}

double DsonDocument_GetNodeRotationZ(DsonDocumentHandle handle, int nodeIndex) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(doc->nodes.size())) return 0.0;
    return doc->nodes[nodeIndex].rotation.z;
}

double DsonDocument_GetNodeScaleX(DsonDocumentHandle handle, int nodeIndex) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(doc->nodes.size())) return 0.0;
    return doc->nodes[nodeIndex].scale.x;
}

double DsonDocument_GetNodeScaleY(DsonDocumentHandle handle, int nodeIndex) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(doc->nodes.size())) return 0.0;
    return doc->nodes[nodeIndex].scale.y;
}

double DsonDocument_GetNodeScaleZ(DsonDocumentHandle handle, int nodeIndex) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(doc->nodes.size())) return 0.0;
    return doc->nodes[nodeIndex].scale.z;
}

double DsonDocument_GetNodeGeneralScale(DsonDocumentHandle handle, int nodeIndex) {
    if (!handle) return 1.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(doc->nodes.size())) return 1.0;
    return doc->nodes[nodeIndex].general_scale;
}

// ============================================================
// C. Skin Weights
// ============================================================

int DsonDocument_GetSkinJointCount(DsonDocumentHandle handle, int modifierIndex) {
    if (!handle) return -1;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (modifierIndex < 0 || modifierIndex >= static_cast<int>(doc->modifiers.size())) return -1;
    return static_cast<int>(doc->modifiers[modifierIndex].skin.joints.size());
}

const char* DsonDocument_GetSkinJointNodeId(DsonDocumentHandle handle, int modifierIndex, int jointIndex) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (modifierIndex < 0 || modifierIndex >= static_cast<int>(doc->modifiers.size())) return "";
    const auto& joints = doc->modifiers[modifierIndex].skin.joints;
    if (jointIndex < 0 || jointIndex >= static_cast<int>(joints.size())) return "";
    return joints[jointIndex].node.c_str();
}

int DsonDocument_GetSkinJointWeightCount(DsonDocumentHandle handle, int modifierIndex, int jointIndex) {
    if (!handle) return -1;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (modifierIndex < 0 || modifierIndex >= static_cast<int>(doc->modifiers.size())) return -1;
    const auto& joints = doc->modifiers[modifierIndex].skin.joints;
    if (jointIndex < 0 || jointIndex >= static_cast<int>(joints.size())) return -1;
    return static_cast<int>(joints[jointIndex].weights.size());
}

int DsonDocument_GetSkinJointWeightVertexIndex(DsonDocumentHandle handle, int modifierIndex, int jointIndex, int weightIndex) {
    if (!handle) return -1;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (modifierIndex < 0 || modifierIndex >= static_cast<int>(doc->modifiers.size())) return -1;
    const auto& joints = doc->modifiers[modifierIndex].skin.joints;
    if (jointIndex < 0 || jointIndex >= static_cast<int>(joints.size())) return -1;
    const auto& joint = joints[jointIndex];
    if (weightIndex < 0 || weightIndex >= static_cast<int>(joint.weight_indices.size())) return -1;
    return joint.weight_indices[weightIndex];
}

double DsonDocument_GetSkinJointWeight(DsonDocumentHandle handle, int modifierIndex, int jointIndex, int weightIndex) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (modifierIndex < 0 || modifierIndex >= static_cast<int>(doc->modifiers.size())) return 0.0;
    const auto& joints = doc->modifiers[modifierIndex].skin.joints;
    if (jointIndex < 0 || jointIndex >= static_cast<int>(joints.size())) return 0.0;
    const auto& joint = joints[jointIndex];
    if (weightIndex < 0 || weightIndex >= static_cast<int>(joint.weights.size())) return 0.0;
    return joint.weights[weightIndex];
}

int DsonDocument_GetVertexInfluenceCount(DsonDocumentHandle handle, int modifierIndex, int vertexIndex, int maxInfluences) {
    if (!handle || maxInfluences < 1) return 0;
    DsonContext* ctx = GetContext(handle);
    EnsureSkinCache(ctx, modifierIndex);
    auto it = ctx->skinCache.data.find(vertexIndex);
    if (it == ctx->skinCache.data.end()) return 0;
    int count = static_cast<int>(it->second.size());
    return count < maxInfluences ? count : maxInfluences;
}

bool DsonDocument_GetVertexBoneInfluence(DsonDocumentHandle handle, int modifierIndex, int vertexIndex, int influenceIndex, const char** boneNodeId, double* weight) {
    if (boneNodeId) *boneNodeId = "";
    if (weight) *weight = 0.0;
    if (!handle || !boneNodeId || !weight) return false;
    DsonContext* ctx = GetContext(handle);
    EnsureSkinCache(ctx, modifierIndex);
    auto it = ctx->skinCache.data.find(vertexIndex);
    if (it == ctx->skinCache.data.end()) return false;
    const std::vector<VertexInfluence>& influences = it->second;
    if (influenceIndex < 0 || influenceIndex >= static_cast<int>(influences.size())) return false;
    *boneNodeId = influences[influenceIndex].boneNodeId.c_str();
    *weight = influences[influenceIndex].weight;
    return true;
}

bool DsonDocument_GetVertexBoneInfluenceCapped(DsonDocumentHandle handle, int modifierIndex, int vertexIndex, int influenceIndex, int maxInfluences, const char** boneNodeId, double* weight) {
    if (boneNodeId) *boneNodeId = "";
    if (weight) *weight = 0.0;
    if (!handle || !boneNodeId || !weight || maxInfluences < 1) return false;
    DsonContext* ctx = GetContext(handle);
    EnsureSkinCache(ctx, modifierIndex);
    auto it = ctx->skinCache.data.find(vertexIndex);
    if (it == ctx->skinCache.data.end()) return false;
    const std::vector<VertexInfluence>& influences = it->second;
    int cap = static_cast<int>(influences.size()) < maxInfluences ? static_cast<int>(influences.size()) : maxInfluences;
    if (influenceIndex < 0 || influenceIndex >= cap) return false;
    double sum = 0.0;
    for (int i = 0; i < cap; ++i) sum += influences[i].weight;
    *boneNodeId = influences[influenceIndex].boneNodeId.c_str();
    *weight = (sum > 0.0) ? influences[influenceIndex].weight / sum : 0.0;
    return true;
}

// ============================================================
// D. UV Sets (library uv_sets)
// ============================================================

const char* DsonDocument_GetUVSetId(DsonDocumentHandle handle, int uvSetIndex) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (uvSetIndex < 0 || uvSetIndex >= static_cast<int>(doc->uv_sets.size())) return "";
    return doc->uv_sets[uvSetIndex].id.c_str();
}

int DsonDocument_GetUVCount(DsonDocumentHandle handle, int uvSetIndex) {
    if (!handle) return -1;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (uvSetIndex < 0 || uvSetIndex >= static_cast<int>(doc->uv_sets.size())) return -1;
    return static_cast<int>(doc->uv_sets[uvSetIndex].uvs.values.size()) / 2;
}

double DsonDocument_GetUVU(DsonDocumentHandle handle, int uvSetIndex, int uvIndex) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (uvSetIndex < 0 || uvSetIndex >= static_cast<int>(doc->uv_sets.size())) return 0.0;
    const auto& uvs = doc->uv_sets[uvSetIndex].uvs.values;
    int idx = uvIndex * 2;
    if (uvIndex < 0 || idx + 1 >= static_cast<int>(uvs.size())) return 0.0;
    return uvs[idx];
}

double DsonDocument_GetUVV(DsonDocumentHandle handle, int uvSetIndex, int uvIndex) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (uvSetIndex < 0 || uvSetIndex >= static_cast<int>(doc->uv_sets.size())) return 0.0;
    const auto& uvs = doc->uv_sets[uvSetIndex].uvs.values;
    int idx = uvIndex * 2;
    if (uvIndex < 0 || idx + 1 >= static_cast<int>(uvs.size())) return 0.0;
    return uvs[idx + 1];
}

int DsonDocument_GetUVPolygonVertexIndexCount(DsonDocumentHandle handle, int uvSetIndex) {
    if (!handle) return -1;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (uvSetIndex < 0 || uvSetIndex >= static_cast<int>(doc->uv_sets.size())) return -1;
    return static_cast<int>(doc->uv_sets[uvSetIndex].polygon_vertex_indices.values.size());
}

int DsonDocument_GetUVPolygonVertexIndex(DsonDocumentHandle handle, int uvSetIndex, int index) {
    if (!handle) return -1;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (uvSetIndex < 0 || uvSetIndex >= static_cast<int>(doc->uv_sets.size())) return -1;
    const auto& pvi = doc->uv_sets[uvSetIndex].polygon_vertex_indices.values;
    if (index < 0 || index >= static_cast<int>(pvi.size())) return -1;
    return pvi[index];
}

// ============================================================
// E. Materials
// ============================================================

const char* DsonDocument_GetMaterialName(DsonDocumentHandle handle, int matIndex) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (matIndex < 0 || matIndex >= static_cast<int>(doc->materials.size())) return "";
    return doc->materials[matIndex].name.c_str();
}

const char* DsonDocument_GetMaterialGeometryId(DsonDocumentHandle handle, int matIndex) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (matIndex < 0 || matIndex >= static_cast<int>(doc->materials.size())) return "";
    return doc->materials[matIndex].geometry.c_str();
}

const char* DsonDocument_GetMaterialUVSetId(DsonDocumentHandle handle, int matIndex) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (matIndex < 0 || matIndex >= static_cast<int>(doc->materials.size())) return "";
    return doc->materials[matIndex].uv_set_id.c_str();
}

double DsonDocument_GetMaterialChannelValue(DsonDocumentHandle handle, int matIndex, int channelId) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (matIndex < 0 || matIndex >= static_cast<int>(doc->materials.size())) return 0.0;
    const Dson::MaterialChannel* ch = GetMaterialChannel(doc->materials[matIndex], channelId);
    if (!ch) return 0.0;
    return ch->value;
}

double DsonDocument_GetMaterialChannelColorR(DsonDocumentHandle handle, int matIndex, int channelId) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (matIndex < 0 || matIndex >= static_cast<int>(doc->materials.size())) return 0.0;
    const Dson::MaterialChannel* ch = GetMaterialChannel(doc->materials[matIndex], channelId);
    if (!ch) return 0.0;
    return ch->color.x;
}

double DsonDocument_GetMaterialChannelColorG(DsonDocumentHandle handle, int matIndex, int channelId) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (matIndex < 0 || matIndex >= static_cast<int>(doc->materials.size())) return 0.0;
    const Dson::MaterialChannel* ch = GetMaterialChannel(doc->materials[matIndex], channelId);
    if (!ch) return 0.0;
    return ch->color.y;
}

double DsonDocument_GetMaterialChannelColorB(DsonDocumentHandle handle, int matIndex, int channelId) {
    if (!handle) return 0.0;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (matIndex < 0 || matIndex >= static_cast<int>(doc->materials.size())) return 0.0;
    const Dson::MaterialChannel* ch = GetMaterialChannel(doc->materials[matIndex], channelId);
    if (!ch) return 0.0;
    return ch->color.z;
}

bool DsonDocument_GetMaterialChannelHasColor(DsonDocumentHandle handle, int matIndex, int channelId) {
    if (!handle) return false;
    Dson::DsonDocument* doc = GetDocument(handle);
    if (matIndex < 0 || matIndex >= static_cast<int>(doc->materials.size())) return false;
    const Dson::MaterialChannel* ch = GetMaterialChannel(doc->materials[matIndex], channelId);
    if (!ch) return false;
    return ch->has_color;
}

const char* DsonDocument_GetMaterialChannelImageUrl(DsonDocumentHandle handle, int matIndex, int channelId) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (matIndex < 0 || matIndex >= static_cast<int>(doc->materials.size())) return "";
    const Dson::MaterialChannel* ch = GetMaterialChannel(doc->materials[matIndex], channelId);
    if (!ch) return "";
    return ch->image_url.c_str();
}

const char* DsonDocument_GetMaterialChannelTexturePath(DsonDocumentHandle handle, int matIndex, int channelId) {
    if (!handle) return "";
    Dson::DsonDocument* doc = GetDocument(handle);
    if (matIndex < 0 || matIndex >= static_cast<int>(doc->materials.size())) return "";
    const Dson::MaterialChannel* ch = GetMaterialChannel(doc->materials[matIndex], channelId);
    if (!ch) return "";
    return ch->texture_path.c_str();
}

// ============================================================
// F. Morph Targets
// ============================================================

int DsonDocument_GetMorphCount(DsonDocumentHandle handle) {
    if (!handle) return 0;
    DsonContext* ctx = GetContext(handle);
    EnsureMorphCache(ctx);
    return static_cast<int>(ctx->morphIndexCache.size());
}

const char* DsonDocument_GetMorphName(DsonDocumentHandle handle, int morphIndex) {
    if (!handle) return "";
    DsonContext* ctx = GetContext(handle);
    EnsureMorphCache(ctx);
    if (morphIndex < 0 || morphIndex >= static_cast<int>(ctx->morphIndexCache.size())) return "";
    return ctx->document.modifiers[ctx->morphIndexCache[morphIndex]].name.c_str();
}

const char* DsonDocument_GetMorphLabel(DsonDocumentHandle handle, int morphIndex) {
    if (!handle) return "";
    DsonContext* ctx = GetContext(handle);
    EnsureMorphCache(ctx);
    if (morphIndex < 0 || morphIndex >= static_cast<int>(ctx->morphIndexCache.size())) return "";
    const auto& mod = ctx->document.modifiers[ctx->morphIndexCache[morphIndex]];
    if (!mod.channel_label.empty()) return mod.channel_label.c_str();
    return mod.name.c_str();
}

int DsonDocument_GetMorphDeltaCount(DsonDocumentHandle handle, int morphIndex) {
    if (!handle) return -1;
    DsonContext* ctx = GetContext(handle);
    EnsureMorphCache(ctx);
    if (morphIndex < 0 || morphIndex >= static_cast<int>(ctx->morphIndexCache.size())) return -1;
    return static_cast<int>(ctx->document.modifiers[ctx->morphIndexCache[morphIndex]].morph_deltas.size());
}

int DsonDocument_GetMorphDeltaVertexIndex(DsonDocumentHandle handle, int morphIndex, int deltaIndex) {
    if (!handle) return -1;
    DsonContext* ctx = GetContext(handle);
    EnsureMorphCache(ctx);
    if (morphIndex < 0 || morphIndex >= static_cast<int>(ctx->morphIndexCache.size())) return -1;
    const auto& deltas = ctx->document.modifiers[ctx->morphIndexCache[morphIndex]].morph_deltas;
    if (deltaIndex < 0 || deltaIndex >= static_cast<int>(deltas.size())) return -1;
    return deltas.GetIndex(deltaIndex);
}

double DsonDocument_GetMorphDeltaX(DsonDocumentHandle handle, int morphIndex, int deltaIndex) {
    if (!handle) return 0.0;
    DsonContext* ctx = GetContext(handle);
    EnsureMorphCache(ctx);
    if (morphIndex < 0 || morphIndex >= static_cast<int>(ctx->morphIndexCache.size())) return 0.0;
    const auto& deltas = ctx->document.modifiers[ctx->morphIndexCache[morphIndex]].morph_deltas;
    if (deltaIndex < 0 || deltaIndex >= static_cast<int>(deltas.size())) return 0.0;
    return deltas.GetValue(deltaIndex).x;
}

double DsonDocument_GetMorphDeltaY(DsonDocumentHandle handle, int morphIndex, int deltaIndex) {
    if (!handle) return 0.0;
    DsonContext* ctx = GetContext(handle);
    EnsureMorphCache(ctx);
    if (morphIndex < 0 || morphIndex >= static_cast<int>(ctx->morphIndexCache.size())) return 0.0;
    const auto& deltas = ctx->document.modifiers[ctx->morphIndexCache[morphIndex]].morph_deltas;
    if (deltaIndex < 0 || deltaIndex >= static_cast<int>(deltas.size())) return 0.0;
    return deltas.GetValue(deltaIndex).y;
}

double DsonDocument_GetMorphDeltaZ(DsonDocumentHandle handle, int morphIndex, int deltaIndex) {
    if (!handle) return 0.0;
    DsonContext* ctx = GetContext(handle);
    EnsureMorphCache(ctx);
    if (morphIndex < 0 || morphIndex >= static_cast<int>(ctx->morphIndexCache.size())) return 0.0;
    const auto& deltas = ctx->document.modifiers[ctx->morphIndexCache[morphIndex]].morph_deltas;
    if (deltaIndex < 0 || deltaIndex >= static_cast<int>(deltas.size())) return 0.0;
    return deltas.GetValue(deltaIndex).z;
}

int DsonDocument_GetMorphNormalDeltaCount(DsonDocumentHandle handle, int morphIndex) {
    if (!handle) return -1;
    DsonContext* ctx = GetContext(handle);
    EnsureMorphCache(ctx);
    if (morphIndex < 0 || morphIndex >= static_cast<int>(ctx->morphIndexCache.size())) return -1;
    return static_cast<int>(ctx->document.modifiers[ctx->morphIndexCache[morphIndex]].normal_deltas.size());
}

int DsonDocument_GetMorphNormalDeltaVertexIndex(DsonDocumentHandle handle, int morphIndex, int deltaIndex) {
    if (!handle) return -1;
    DsonContext* ctx = GetContext(handle);
    EnsureMorphCache(ctx);
    if (morphIndex < 0 || morphIndex >= static_cast<int>(ctx->morphIndexCache.size())) return -1;
    const auto& deltas = ctx->document.modifiers[ctx->morphIndexCache[morphIndex]].normal_deltas;
    if (deltaIndex < 0 || deltaIndex >= static_cast<int>(deltas.size())) return -1;
    return deltas.GetIndex(deltaIndex);
}

double DsonDocument_GetMorphNormalDeltaX(DsonDocumentHandle handle, int morphIndex, int deltaIndex) {
    if (!handle) return 0.0;
    DsonContext* ctx = GetContext(handle);
    EnsureMorphCache(ctx);
    if (morphIndex < 0 || morphIndex >= static_cast<int>(ctx->morphIndexCache.size())) return 0.0;
    const auto& deltas = ctx->document.modifiers[ctx->morphIndexCache[morphIndex]].normal_deltas;
    if (deltaIndex < 0 || deltaIndex >= static_cast<int>(deltas.size())) return 0.0;
    return deltas.GetValue(deltaIndex).x;
}

double DsonDocument_GetMorphNormalDeltaY(DsonDocumentHandle handle, int morphIndex, int deltaIndex) {
    if (!handle) return 0.0;
    DsonContext* ctx = GetContext(handle);
    EnsureMorphCache(ctx);
    if (morphIndex < 0 || morphIndex >= static_cast<int>(ctx->morphIndexCache.size())) return 0.0;
    const auto& deltas = ctx->document.modifiers[ctx->morphIndexCache[morphIndex]].normal_deltas;
    if (deltaIndex < 0 || deltaIndex >= static_cast<int>(deltas.size())) return 0.0;
    return deltas.GetValue(deltaIndex).y;
}

double DsonDocument_GetMorphNormalDeltaZ(DsonDocumentHandle handle, int morphIndex, int deltaIndex) {
    if (!handle) return 0.0;
    DsonContext* ctx = GetContext(handle);
    EnsureMorphCache(ctx);
    if (morphIndex < 0 || morphIndex >= static_cast<int>(ctx->morphIndexCache.size())) return 0.0;
    const auto& deltas = ctx->document.modifiers[ctx->morphIndexCache[morphIndex]].normal_deltas;
    if (deltaIndex < 0 || deltaIndex >= static_cast<int>(deltas.size())) return 0.0;
    return deltas.GetValue(deltaIndex).z;
}

const char* DsonDocument_GetMorphGeometryId(DsonDocumentHandle handle, int morphIndex) {
    if (!handle) return "";
    DsonContext* ctx = GetContext(handle);
    EnsureMorphCache(ctx);
    if (morphIndex < 0 || morphIndex >= static_cast<int>(ctx->morphIndexCache.size())) return "";
    const std::string& parent = ctx->document.modifiers[ctx->morphIndexCache[morphIndex]].parent.value;
    size_t pos = parent.rfind('#');
    if (pos == std::string::npos) return "";
    ctx->lastMorphGeometryId = parent.substr(pos + 1);
    return ctx->lastMorphGeometryId.c_str();
}

const char* DsonParser_GetLastError() {
    return t_lastError.c_str();
}