#include "pch.h"
#include "DsonParserAPI.h"
#include "DsonTypes.h"
#include <string>
#include <sstream>
#include <vector>

// Per-handle context: bundles the parsed document with the caches that back the
// index-based enumeration APIs. Keeping this state on the handle (instead of in
// process globals) means distinct documents - several files open at once, or
// parsed on different threads - never clobber each other's caches.
namespace {

struct DsonContext {
    Dson::DsonDocument document;
    std::vector<std::string> contextCache;
    std::vector<std::string> unknownKeysCache;
    std::string unknownKeysCacheContext;
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
}

void DsonDocument_Destroy(DsonDocumentHandle handle) {
    if (!handle) return;
    delete GetContext(handle);
}

const char* DsonParser_GetLastError() {
    return t_lastError.c_str();
}