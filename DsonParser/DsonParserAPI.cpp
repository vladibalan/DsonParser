#include "pch.h"
#include "DsonParserAPI.h"
#include "DsonTypes.h"
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <unordered_map>

// C API orientation:
// This file exposes the parsed DsonDocument through a flat extern "C" ABI for
// callers that do not want to link against the C++ model directly. The API uses
// an opaque DsonDocumentHandle; internally that handle points to a DsonContext
// containing the parsed document plus small caches that support index-based
// queries.
//
// Responsibilities:
// - Manage document lifetime, loading, clearing, and error reporting.
// - Provide bounds-checked accessors for the typed data parsed in DsonTypes.cpp.
// - Expose scene-material texture paths plus retained LIE layer paths/labels and
//   per-layer compositing metadata (blend op, opacity, active/invert, color, transform).
// - Keep return values simple and ABI-friendly: strings are parser-owned
//   const char*; invalid handles/indexes return "" / 0 / false. Count functions
//   always return 0 on error (never -1); -1 is reserved for value/index
//   accessors reporting "no such element".
// - Expose raw stored formula RPN payloads without evaluating them.
// - Expose authored geometry rigidity weights/groups without remapping or
//   interpreting their geometry-local indices and node references.
// - Build lazy query caches for morph indexes and per-vertex skin influences.
//
// Important behavior:
// - Morph APIs use a filtered morph index, not the raw modifier_library index.
// - Skin influence APIs invert joint->vertex weights into vertex->bone lists,
//   sort descending, normalize, and optionally cap/renormalize for UE import.
// - This layer does not parse DSON itself; parsing rules live in DsonTypes.cpp.

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

// All const char* values returned by the API point into storage owned by this
// context or by its DsonDocument. Callers should copy strings they need after
// DsonDocument_Clear/Destroy, or after an API call that reuses a scratch string
// such as lastMorphGeometryId.
struct DsonContext {
    Dson::DsonDocument document;
    std::vector<std::string> contextCache;
    std::vector<std::string> unknownKeysCache;
    std::string unknownKeysCacheContext;
    std::vector<int> morphIndexCache;   // indices into document.modifiers for morph modifiers
    bool morphIndexCacheDirty = true;
    VertexInfluenceCache skinCache;
    std::string lastMorphGeometryId;    // stable storage for GetMorphGeometryId return value
};

// Per-thread last-error slot. Each thread gets its own std::string, lazily
// constructed on first use, so concurrent DsonDocument_Create/LoadFrom* calls on
// distinct handles never race on it, and each thread's DsonParser_GetLastError()
// reflects only its own most recent call.
//
// It must be a FUNCTION-LOCAL thread_local, not a file-scope one: a function-local
// thread_local initializes on first access *per thread*, which sidesteps the
// Windows DLL TLS pitfall where a file-scope thread_local image is not initialized
// for threads that predate a dynamically loaded DLL (e.g. the UE5 main thread
// loading via GetDllHandle). That pitfall is exactly why this slot had been a
// process-global; first-use init avoids it without giving up thread isolation.
static std::string& LastErrorSlot() {
    thread_local std::string slot;
    return slot;
}

DsonContext* GetContext(DsonDocumentHandle handle) {
    return static_cast<DsonContext*>(handle);
}

Dson::DsonDocument* GetDocument(DsonDocumentHandle handle) {
    return &GetContext(handle)->document;
}

// Null-safe document accessor: returns nullptr instead of dereferencing a null
// handle, so callers can fold the handle check into the same guard as bounds.
static Dson::DsonDocument* Doc(DsonDocumentHandle handle) {
    return handle ? GetDocument(handle) : nullptr;
}

// Bounds-checked element fetch for any indexable container: returns a pointer to
// element i, or nullptr if i is out of range. Centralizes the index guard that
// every index-based accessor would otherwise repeat inline.
template <class Coll>
static auto At(const Coll& c, int i) -> const typename Coll::value_type* {
    return (i < 0 || i >= static_cast<int>(c.size())) ? nullptr : &c[i];
}

void StoreLastError(const std::string& error) {
    LastErrorSlot() = error;
}

static void ClearQueryCaches(DsonContext* ctx) {
    if (!ctx) return;
    ctx->contextCache.clear();
    ctx->unknownKeysCache.clear();
    ctx->unknownKeysCacheContext.clear();
    ctx->morphIndexCache.clear();
    ctx->morphIndexCacheDirty = true;
    ctx->skinCache.data.clear();
    ctx->skinCache.built = false;
    ctx->skinCache.modifierIndex = -1;
}

static void RefreshCachesAfterLoad(DsonContext* ctx) {
    if (!ctx) return;
    ClearQueryCaches(ctx);
    ctx->contextCache = ctx->document.GetAllContextsWithUnknownKeys();
}

using MaterialChannelList = std::vector<std::pair<std::string, Dson::MaterialChannel>>;

static const std::pair<std::string, Dson::MaterialChannel>* GetMaterialChannel(
    const MaterialChannelList& channels,
    int channelIdx) {
    return At(channels, channelIdx);
}

static int GetMaterialChannelCount(const MaterialChannelList& channels) {
    return static_cast<int>(channels.size());
}

static const char* GetMaterialChannelId(const MaterialChannelList& channels, int channelIdx) {
    const auto* channel = GetMaterialChannel(channels, channelIdx);
    if (!channel) return "";
    return channel->first.c_str();
}

static const char* GetMaterialChannelType(const MaterialChannelList& channels, int channelIdx) {
    const auto* channel = GetMaterialChannel(channels, channelIdx);
    if (!channel) return "";
    return channel->second.type.c_str();
}

static double GetMaterialChannelValue(const MaterialChannelList& channels, int channelIdx) {
    const auto* channel = GetMaterialChannel(channels, channelIdx);
    if (!channel) return 0.0;
    return channel->second.value;
}

static double GetMaterialChannelColorR(const MaterialChannelList& channels, int channelIdx) {
    const auto* channel = GetMaterialChannel(channels, channelIdx);
    if (!channel) return 0.0;
    return channel->second.color.x;
}

static double GetMaterialChannelColorG(const MaterialChannelList& channels, int channelIdx) {
    const auto* channel = GetMaterialChannel(channels, channelIdx);
    if (!channel) return 0.0;
    return channel->second.color.y;
}

static double GetMaterialChannelColorB(const MaterialChannelList& channels, int channelIdx) {
    const auto* channel = GetMaterialChannel(channels, channelIdx);
    if (!channel) return 0.0;
    return channel->second.color.z;
}

static bool GetMaterialChannelHasColor(const MaterialChannelList& channels, int channelIdx) {
    const auto* channel = GetMaterialChannel(channels, channelIdx);
    if (!channel) return false;
    return channel->second.has_color;
}

static const char* GetMaterialChannelImageUrl(const MaterialChannelList& channels, int channelIdx) {
    const auto* channel = GetMaterialChannel(channels, channelIdx);
    if (!channel) return "";
    return channel->second.image_url.c_str();
}

static const char* GetMaterialChannelTexturePath(const MaterialChannelList& channels, int channelIdx) {
    const auto* channel = GetMaterialChannel(channels, channelIdx);
    if (!channel) return "";
    return channel->second.texture_path.c_str();
}

static int GetMaterialChannelLayerCount(const MaterialChannelList& channels, int channelIdx) {
    const auto* channel = GetMaterialChannel(channels, channelIdx);
    return channel ? static_cast<int>(channel->second.layers.size()) : 0;
}

static const char* GetMaterialChannelLayerTexturePath(
    const MaterialChannelList& channels,
    int channelIdx,
    int layerIdx) {
    const auto* channel = GetMaterialChannel(channels, channelIdx);
    const Dson::ImageLayer* layer = channel ? At(channel->second.layers, layerIdx) : nullptr;
    return layer ? layer->url.c_str() : "";
}

static const char* GetMaterialChannelLayerLabel(
    const MaterialChannelList& channels,
    int channelIdx,
    int layerIdx) {
    const auto* channel = GetMaterialChannel(channels, channelIdx);
    const Dson::ImageLayer* layer = channel ? At(channel->second.layers, layerIdx) : nullptr;
    return layer ? layer->label.c_str() : "";
}

// Shared resolver for per-channel layer compositing accessors.
static const Dson::ImageLayer* GetMaterialChannelLayerPtr(
    const MaterialChannelList& channels, int channelIdx, int layerIdx) {
    const auto* channel = GetMaterialChannel(channels, channelIdx);
    return channel ? At(channel->second.layers, layerIdx) : nullptr;
}

static const char* GetMaterialChannelLayerBlendMode(
    const MaterialChannelList& channels, int channelIdx, int layerIdx) {
    const Dson::ImageLayer* L = GetMaterialChannelLayerPtr(channels, channelIdx, layerIdx);
    return L ? L->blend_op.c_str() : "";
}
static double GetMaterialChannelLayerOpacity(
    const MaterialChannelList& channels, int channelIdx, int layerIdx) {
    const Dson::ImageLayer* L = GetMaterialChannelLayerPtr(channels, channelIdx, layerIdx);
    return L ? L->opacity : 0.0;
}
static bool GetMaterialChannelLayerActive(
    const MaterialChannelList& channels, int channelIdx, int layerIdx) {
    const Dson::ImageLayer* L = GetMaterialChannelLayerPtr(channels, channelIdx, layerIdx);
    return L ? L->active : false;
}
static bool GetMaterialChannelLayerInvert(
    const MaterialChannelList& channels, int channelIdx, int layerIdx) {
    const Dson::ImageLayer* L = GetMaterialChannelLayerPtr(channels, channelIdx, layerIdx);
    return L ? L->invert : false;
}
static double GetMaterialChannelLayerColorR(
    const MaterialChannelList& channels, int channelIdx, int layerIdx) {
    const Dson::ImageLayer* L = GetMaterialChannelLayerPtr(channels, channelIdx, layerIdx);
    return L ? L->color.x : 0.0;
}
static double GetMaterialChannelLayerColorG(
    const MaterialChannelList& channels, int channelIdx, int layerIdx) {
    const Dson::ImageLayer* L = GetMaterialChannelLayerPtr(channels, channelIdx, layerIdx);
    return L ? L->color.y : 0.0;
}
static double GetMaterialChannelLayerColorB(
    const MaterialChannelList& channels, int channelIdx, int layerIdx) {
    const Dson::ImageLayer* L = GetMaterialChannelLayerPtr(channels, channelIdx, layerIdx);
    return L ? L->color.z : 0.0;
}
static double GetMaterialChannelLayerRotation(
    const MaterialChannelList& channels, int channelIdx, int layerIdx) {
    const Dson::ImageLayer* L = GetMaterialChannelLayerPtr(channels, channelIdx, layerIdx);
    return L ? L->rotation : 0.0;
}
static double GetMaterialChannelLayerScaleX(
    const MaterialChannelList& channels, int channelIdx, int layerIdx) {
    const Dson::ImageLayer* L = GetMaterialChannelLayerPtr(channels, channelIdx, layerIdx);
    return L ? L->scale_x : 1.0;
}
static double GetMaterialChannelLayerScaleY(
    const MaterialChannelList& channels, int channelIdx, int layerIdx) {
    const Dson::ImageLayer* L = GetMaterialChannelLayerPtr(channels, channelIdx, layerIdx);
    return L ? L->scale_y : 1.0;
}
static double GetMaterialChannelLayerOffsetX(
    const MaterialChannelList& channels, int channelIdx, int layerIdx) {
    const Dson::ImageLayer* L = GetMaterialChannelLayerPtr(channels, channelIdx, layerIdx);
    return L ? L->offset_x : 0.0;
}
static double GetMaterialChannelLayerOffsetY(
    const MaterialChannelList& channels, int channelIdx, int layerIdx) {
    const Dson::ImageLayer* L = GetMaterialChannelLayerPtr(channels, channelIdx, layerIdx);
    return L ? L->offset_y : 0.0;
}
static bool GetMaterialChannelLayerMirrorX(
    const MaterialChannelList& channels, int channelIdx, int layerIdx) {
    const Dson::ImageLayer* L = GetMaterialChannelLayerPtr(channels, channelIdx, layerIdx);
    return L ? L->mirror_x : false;
}
static bool GetMaterialChannelLayerMirrorY(
    const MaterialChannelList& channels, int channelIdx, int layerIdx) {
    const Dson::ImageLayer* L = GetMaterialChannelLayerPtr(channels, channelIdx, layerIdx);
    return L ? L->mirror_y : false;
}

// Morph accessors expose a dense list of morph modifiers. Real DAZ morphs are
// identified by their nested "morph" payload; legacy flat files may use
// type == "morph".
// The public morphIndex is therefore not the same number as the modifier_library
// index used by DsonDocument_GetModifier* and skin-binding APIs.
static void EnsureMorphCache(DsonContext* ctx) {
    if (!ctx->morphIndexCacheDirty) return;
    ctx->morphIndexCache.clear();
    for (int i = 0; i < static_cast<int>(ctx->document.modifiers.size()); i++) {
        const Dson::Modifier& mod = ctx->document.modifiers[i];
        if (mod.type.value == "morph" || mod.has_morph) {
            ctx->morphIndexCache.push_back(i);
        }
    }
    ctx->morphIndexCacheDirty = false;
}

static const Dson::Modifier* GetMorphByFilteredIndex(DsonContext* ctx, int morphIndex) {
    if (!ctx) return nullptr;
    EnsureMorphCache(ctx);
    if (morphIndex < 0 || morphIndex >= static_cast<int>(ctx->morphIndexCache.size())) return nullptr;
    return &ctx->document.modifiers[ctx->morphIndexCache[morphIndex]];
}

static int GetIndexedVector3Count(const Dson::IndexedVector3Array& deltas) {
    return static_cast<int>(deltas.size());
}

static int GetIndexedVector3Index(const Dson::IndexedVector3Array& deltas, int deltaIndex) {
    if (deltaIndex < 0 || deltaIndex >= static_cast<int>(deltas.size())) return -1;
    return deltas.GetIndex(deltaIndex);
}

static double GetIndexedVector3Component(const Dson::IndexedVector3Array& deltas, int deltaIndex, int component) {
    if (deltaIndex < 0 || deltaIndex >= static_cast<int>(deltas.size())) return 0.0;
    const Dson::Vector3& value = deltas.GetValue(deltaIndex);
    if (component == 0) return value.x;
    if (component == 1) return value.y;
    if (component == 2) return value.z;
    return 0.0;
}

static const Dson::Node* GetLibraryNode(DsonDocumentHandle handle, int nodeIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    return doc ? At(doc->nodes, nodeIndex) : nullptr;
}

static const Dson::GeometryRigidityGroup* GetGeometryRigidityGroup(
    DsonDocumentHandle handle, int geomIndex, int groupIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Geometry* geom = doc ? At(doc->geometries, geomIndex) : nullptr;
    return geom ? At(geom->rigidity_groups, groupIndex) : nullptr;
}

static const Dson::Node* GetSceneNode(DsonDocumentHandle handle, int sceneNodeIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    return doc ? At(doc->scene.nodes, sceneNodeIndex) : nullptr;
}

static const Dson::Modifier* GetLibraryModifier(DsonDocumentHandle handle, int modifierIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    return doc ? At(doc->modifiers, modifierIndex) : nullptr;
}

static const Dson::Modifier* GetSceneModifier(DsonDocumentHandle handle, int sceneModifierIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    return doc ? At(doc->scene.modifiers, sceneModifierIndex) : nullptr;
}

enum ModifierChannelComponent {
    ModifierChannelValue,
    ModifierChannelMin,
    ModifierChannelMax
};

static double GetModifierChannelDouble(const Dson::Modifier* mod, ModifierChannelComponent component) {
    if (!mod) return 0.0;
    if (component == ModifierChannelMin) return mod->channel_min;
    if (component == ModifierChannelMax) return mod->channel_max;
    return mod->channel_value;
}

static bool GetModifierChannelClamped(const Dson::Modifier* mod) {
    return mod ? mod->channel_clamped : false;
}

static const Dson::Formula* FormulaAt(const Dson::Modifier* mod, int formulaIndex) {
    return mod ? At(mod->formulas, formulaIndex) : nullptr;
}

static const Dson::FormulaOperation* FormulaOpAt(
    const Dson::Modifier* mod,
    int formulaIndex,
    int opIndex) {
    const Dson::Formula* formula = FormulaAt(mod, formulaIndex);
    return formula ? At(formula->operations, opIndex) : nullptr;
}

static double GetVector3Component(const Dson::Vector3& v, int component) {
    if (component == 0) return v.x;
    if (component == 1) return v.y;
    if (component == 2) return v.z;
    return 0.0;
}

static double GetNodeVector3Component(
    DsonDocumentHandle handle,
    int nodeIndex,
    const Dson::Vector3 Dson::Node::* member,
    int component) {
    const Dson::Node* node = GetLibraryNode(handle, nodeIndex);
    if (!node) return 0.0;
    return GetVector3Component(node->*member, component);
}

static double GetSceneNodeVector3Component(
    DsonDocumentHandle handle,
    int sceneNodeIndex,
    const Dson::Vector3 Dson::Node::* member,
    int component) {
    const Dson::Node* node = GetSceneNode(handle, sceneNodeIndex);
    if (!node) return 0.0;
    return GetVector3Component(node->*member, component);
}

static int GetSceneNodePresenceMask(
    DsonDocumentHandle handle,
    int sceneNodeIndex,
    const unsigned int Dson::Node::* member) {
    const Dson::Node* node = GetSceneNode(handle, sceneNodeIndex);
    return node ? static_cast<int>(node->*member) : 0;
}

static const Dson::Geometry* GetGeometry(DsonDocumentHandle handle, int geomIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    return doc ? At(doc->geometries, geomIndex) : nullptr;
}

static double GetGeometryVertexComponent(
    DsonDocumentHandle handle,
    int geomIndex,
    int vertexIndex,
    int component) {
    if (component < 0 || component > 2) return 0.0;
    const Dson::Geometry* geom = GetGeometry(handle, geomIndex);
    if (!geom) return 0.0;
    const auto& verts = geom->vertices.values;
    int idx = vertexIndex * 3;
    if (vertexIndex < 0 || idx + 2 >= static_cast<int>(verts.size())) return 0.0;
    return verts[idx + component];
}

static int GetStringVectorCount(const std::vector<std::string>& values) {
    return static_cast<int>(values.size());
}

static const char* GetStringVectorValue(const std::vector<std::string>& values, int index) {
    if (index < 0 || index >= static_cast<int>(values.size())) return "";
    return values[index].c_str();
}

// Skin data is parsed in DSON's native joint->vertex layout. Importers usually
// need vertex->bone influences, so this cache inverts the mapping for one
// modifier at a time, sorts each vertex's influences by weight, and normalizes
// the full influence list. The capped API does an additional top-N
// renormalization without mutating this cache.
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
    for (const auto& joint : joints) {
        const std::string& boneNodeId = joint.node.value;
        // weight_indices and weights are parallel arrays, so index both together.
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
        double sum = std::accumulate(influences.begin(), influences.end(), 0.0,
            [](double acc, const VertexInfluence& inf) { return acc + inf.weight; });
        if (sum > 0.0) {
            for (VertexInfluence& inf : influences) inf.weight /= sum;
        }
    }

    ctx->skinCache.built = true;
}

// Shared resolver for per-image layer compositing accessors.
static const Dson::ImageLayer* GetImageLayer(DsonDocumentHandle handle, int imageIndex, int layerIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Image* img = doc ? At(doc->images, imageIndex) : nullptr;
    return img ? At(img->layers, layerIdx) : nullptr;
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
        return 1;
    }

    try {
        Dson::DsonDocument* doc = GetDocument(handle);
        std::string errorMsg;
        if (doc->LoadFromFile(filepath, errorMsg)) {
            StoreLastError("");
            DsonContext* ctx = GetContext(handle);
            RefreshCachesAfterLoad(ctx);
            return 0;
        }
        StoreLastError(errorMsg);
        return 1;
    }
    catch (const std::exception& e) {
        StoreLastError(std::string("Exception: ") + e.what());
        return 1;
    }
    catch (...) {
        StoreLastError("Unknown exception occurred");
        return 1;
    }
}

int DsonDocument_LoadFromString(DsonDocumentHandle handle, const char* jsonString) {
    if (!handle || !jsonString) {
        StoreLastError("Invalid handle or string");
        return 1;
    }

    try {
        Dson::DsonDocument* doc = GetDocument(handle);
        std::string errorMsg;
        if (doc->LoadFromString(jsonString, errorMsg)) {
            StoreLastError("");
            DsonContext* ctx = GetContext(handle);
            RefreshCachesAfterLoad(ctx);
            return 0;
        }
        StoreLastError(errorMsg);
        return 1;
    }
    catch (const std::exception& e) {
        StoreLastError(std::string("Exception: ") + e.what());
        return 1;
    }
    catch (...) {
        StoreLastError("Unknown exception occurred");
        return 1;
    }
}

int DsonDocument_LoadFromBuffer(DsonDocumentHandle handle, const char* data, int length) {
    if (!handle || !data || length < 0) {
        StoreLastError("Invalid handle or buffer");
        return 1;
    }

    try {
        Dson::DsonDocument* doc = GetDocument(handle);
        std::string errorMsg;
        if (doc->LoadFromBuffer(data, static_cast<size_t>(length), errorMsg)) {
            StoreLastError("");
            DsonContext* ctx = GetContext(handle);
            RefreshCachesAfterLoad(ctx);
            return 0;
        }
        StoreLastError(errorMsg);
        return 1;
    }
    catch (const std::exception& e) {
        StoreLastError(std::string("Exception: ") + e.what());
        return 1;
    }
    catch (...) {
        StoreLastError("Unknown exception occurred");
        return 1;
    }
}

const char* DsonDocument_GetFileVersion(DsonDocumentHandle handle) {
    Dson::DsonDocument* doc = Doc(handle);
    return doc ? doc->file_version.c_str() : "";
}

const char* DsonDocument_GetAssetId(DsonDocumentHandle handle) {
    Dson::DsonDocument* doc = Doc(handle);
    return doc ? doc->asset_info.id.c_str() : "";
}

const char* DsonDocument_GetAssetType(DsonDocumentHandle handle) {
    Dson::DsonDocument* doc = Doc(handle);
    return doc ? doc->asset_info.type.c_str() : "";
}

double DsonDocument_GetUnitScale(DsonDocumentHandle handle) {
    Dson::DsonDocument* doc = Doc(handle);
    return doc ? doc->asset_info.unit_scale : 1.0;
}

int DsonDocument_GetNodeCount(DsonDocumentHandle handle) {
    Dson::DsonDocument* doc = Doc(handle);
    return doc ? static_cast<int>(doc->nodes.size()) : 0;
}

int DsonDocument_GetGeometryCount(DsonDocumentHandle handle) {
    Dson::DsonDocument* doc = Doc(handle);
    return doc ? static_cast<int>(doc->geometries.size()) : 0;
}

int DsonDocument_GetMaterialCount(DsonDocumentHandle handle) {
    Dson::DsonDocument* doc = Doc(handle);
    return doc ? static_cast<int>(doc->materials.size()) : 0;
}

int DsonDocument_GetModifierCount(DsonDocumentHandle handle) {
    Dson::DsonDocument* doc = Doc(handle);
    return doc ? static_cast<int>(doc->modifiers.size()) : 0;
}

int DsonDocument_GetImageCount(DsonDocumentHandle handle) {
    Dson::DsonDocument* doc = Doc(handle);
    return doc ? static_cast<int>(doc->images.size()) : 0;
}

int DsonDocument_GetUVSetCount(DsonDocumentHandle handle) {
    Dson::DsonDocument* doc = Doc(handle);
    return doc ? static_cast<int>(doc->uv_sets.size()) : 0;
}

const char* DsonDocument_GetNodeId(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Node* node = doc ? At(doc->nodes, index) : nullptr;
    return node ? node->id.c_str() : "";
}

const char* DsonDocument_GetNodeName(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Node* node = doc ? At(doc->nodes, index) : nullptr;
    return node ? node->name.c_str() : "";
}

const char* DsonDocument_GetNodeType(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Node* node = doc ? At(doc->nodes, index) : nullptr;
    return node ? node->type.c_str() : "";
}

const char* DsonDocument_GetNodePresentationType(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Node* node = doc ? At(doc->nodes, index) : nullptr;
    return node ? node->presentation_type.c_str() : "";
}

const char* DsonDocument_GetNodePresentationLabel(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Node* node = doc ? At(doc->nodes, index) : nullptr;
    return node ? node->presentation_label.c_str() : "";
}

double DsonDocument_GetNodeCenterPointX(DsonDocumentHandle handle, int index) {
    return GetNodeVector3Component(handle, index, &Dson::Node::center_point, 0);
}

double DsonDocument_GetNodeCenterPointY(DsonDocumentHandle handle, int index) {
    return GetNodeVector3Component(handle, index, &Dson::Node::center_point, 1);
}

double DsonDocument_GetNodeCenterPointZ(DsonDocumentHandle handle, int index) {
    return GetNodeVector3Component(handle, index, &Dson::Node::center_point, 2);
}

bool DsonDocument_GetNodeHasRigidFollow(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Node* node = doc ? At(doc->nodes, index) : nullptr;
    return node ? node->has_rigid_follow : false;
}

const char* DsonDocument_GetNodeRigidFollowRotationMode(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Node* node = doc ? At(doc->nodes, index) : nullptr;
    return node ? node->rigid_follow_rotation_mode.c_str() : "";
}

int DsonDocument_GetNodeRigidFollowScaleModeCount(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Node* node = doc ? At(doc->nodes, index) : nullptr;
    return node ? static_cast<int>(node->rigid_follow_scale_modes.size()) : 0;
}

const char* DsonDocument_GetNodeRigidFollowScaleMode(DsonDocumentHandle handle, int index, int axisIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Node* node = doc ? At(doc->nodes, index) : nullptr;
    const std::string* mode = node ? At(node->rigid_follow_scale_modes, axisIndex) : nullptr;
    return mode ? mode->c_str() : "";
}

int DsonDocument_GetNodeRigidFollowReferenceVertexCount(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Node* node = doc ? At(doc->nodes, index) : nullptr;
    return node ? static_cast<int>(node->rigid_follow_reference_vertices.size()) : 0;
}

int DsonDocument_GetNodeRigidFollowReferenceVertex(DsonDocumentHandle handle, int index, int refIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Node* node = doc ? At(doc->nodes, index) : nullptr;
    const int* vertex = node ? At(node->rigid_follow_reference_vertices, refIndex) : nullptr;
    return vertex ? *vertex : -1;
}

// Scene nodes (scene.nodes) are instances, not library definitions. They may
// reference node_library entries through Url and carry labels or geometry refs
// that are meaningful to the placed asset. Keep scene.* and *_library accessors
// separate so callers can decide whether they want definitions or instances.
int DsonDocument_GetSceneNodeCount(DsonDocumentHandle handle) {
    Dson::DsonDocument* doc = Doc(handle);
    return doc ? static_cast<int>(doc->scene.nodes.size()) : 0;
}

const char* DsonDocument_GetSceneNodeId(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Node* node = doc ? At(doc->scene.nodes, index) : nullptr;
    return node ? node->id.c_str() : "";
}

const char* DsonDocument_GetSceneNodeName(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Node* node = doc ? At(doc->scene.nodes, index) : nullptr;
    return node ? node->name.c_str() : "";
}

const char* DsonDocument_GetSceneNodeLabel(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Node* node = doc ? At(doc->scene.nodes, index) : nullptr;
    return node ? node->label.c_str() : "";
}

const char* DsonDocument_GetSceneNodeType(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Node* node = doc ? At(doc->scene.nodes, index) : nullptr;
    return node ? node->type.c_str() : "";
}

const char* DsonDocument_GetSceneNodeUrl(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Node* node = doc ? At(doc->scene.nodes, index) : nullptr;
    return node ? node->url.c_str() : "";
}

const char* DsonDocument_GetSceneNodeParent(DsonDocumentHandle handle, int index) {
    const Dson::Node* node = GetSceneNode(handle, index);
    return node ? node->parent.c_str() : "";
}

double DsonDocument_GetSceneNodeTranslationX(DsonDocumentHandle handle, int index) {
    return GetSceneNodeVector3Component(handle, index, &Dson::Node::translation, 0);
}

double DsonDocument_GetSceneNodeTranslationY(DsonDocumentHandle handle, int index) {
    return GetSceneNodeVector3Component(handle, index, &Dson::Node::translation, 1);
}

double DsonDocument_GetSceneNodeTranslationZ(DsonDocumentHandle handle, int index) {
    return GetSceneNodeVector3Component(handle, index, &Dson::Node::translation, 2);
}

int DsonDocument_GetSceneNodeTranslationPresenceMask(DsonDocumentHandle handle, int index) {
    return GetSceneNodePresenceMask(handle, index, &Dson::Node::translation_presence);
}

double DsonDocument_GetSceneNodeRotationX(DsonDocumentHandle handle, int index) {
    return GetSceneNodeVector3Component(handle, index, &Dson::Node::rotation, 0);
}

double DsonDocument_GetSceneNodeRotationY(DsonDocumentHandle handle, int index) {
    return GetSceneNodeVector3Component(handle, index, &Dson::Node::rotation, 1);
}

double DsonDocument_GetSceneNodeRotationZ(DsonDocumentHandle handle, int index) {
    return GetSceneNodeVector3Component(handle, index, &Dson::Node::rotation, 2);
}

int DsonDocument_GetSceneNodeRotationPresenceMask(DsonDocumentHandle handle, int index) {
    return GetSceneNodePresenceMask(handle, index, &Dson::Node::rotation_presence);
}

double DsonDocument_GetSceneNodeScaleX(DsonDocumentHandle handle, int index) {
    return GetSceneNodeVector3Component(handle, index, &Dson::Node::scale, 0);
}

double DsonDocument_GetSceneNodeScaleY(DsonDocumentHandle handle, int index) {
    return GetSceneNodeVector3Component(handle, index, &Dson::Node::scale, 1);
}

double DsonDocument_GetSceneNodeScaleZ(DsonDocumentHandle handle, int index) {
    return GetSceneNodeVector3Component(handle, index, &Dson::Node::scale, 2);
}

int DsonDocument_GetSceneNodeScalePresenceMask(DsonDocumentHandle handle, int index) {
    return GetSceneNodePresenceMask(handle, index, &Dson::Node::scale_presence);
}

double DsonDocument_GetSceneNodeGeneralScale(DsonDocumentHandle handle, int index) {
    const Dson::Node* node = GetSceneNode(handle, index);
    return node ? node->general_scale : 1.0;
}

bool DsonDocument_GetSceneNodeHasGeneralScale(DsonDocumentHandle handle, int index) {
    const Dson::Node* node = GetSceneNode(handle, index);
    return node ? node->has_general_scale : false;
}

const char* DsonDocument_GetSceneNodeRotationOrder(DsonDocumentHandle handle, int index) {
    const Dson::Node* node = GetSceneNode(handle, index);
    return node ? node->rotation_order.c_str() : "";
}

bool DsonDocument_GetSceneNodeHasRotationOrder(DsonDocumentHandle handle, int index) {
    const Dson::Node* node = GetSceneNode(handle, index);
    return node ? node->has_rotation_order : false;
}

double DsonDocument_GetSceneNodeCenterPointX(DsonDocumentHandle handle, int index) {
    return GetSceneNodeVector3Component(handle, index, &Dson::Node::center_point, 0);
}

double DsonDocument_GetSceneNodeCenterPointY(DsonDocumentHandle handle, int index) {
    return GetSceneNodeVector3Component(handle, index, &Dson::Node::center_point, 1);
}

double DsonDocument_GetSceneNodeCenterPointZ(DsonDocumentHandle handle, int index) {
    return GetSceneNodeVector3Component(handle, index, &Dson::Node::center_point, 2);
}

int DsonDocument_GetSceneNodeCenterPointPresenceMask(DsonDocumentHandle handle, int index) {
    return GetSceneNodePresenceMask(handle, index, &Dson::Node::center_point_presence);
}

double DsonDocument_GetSceneNodeOrientationX(DsonDocumentHandle handle, int index) {
    return GetSceneNodeVector3Component(handle, index, &Dson::Node::orientation, 0);
}

double DsonDocument_GetSceneNodeOrientationY(DsonDocumentHandle handle, int index) {
    return GetSceneNodeVector3Component(handle, index, &Dson::Node::orientation, 1);
}

double DsonDocument_GetSceneNodeOrientationZ(DsonDocumentHandle handle, int index) {
    return GetSceneNodeVector3Component(handle, index, &Dson::Node::orientation, 2);
}

int DsonDocument_GetSceneNodeOrientationPresenceMask(DsonDocumentHandle handle, int index) {
    return GetSceneNodePresenceMask(handle, index, &Dson::Node::orientation_presence);
}

bool DsonDocument_GetSceneNodeInheritsScale(DsonDocumentHandle handle, int index) {
    const Dson::Node* node = GetSceneNode(handle, index);
    return node ? node->inherits_scale : false;
}

bool DsonDocument_GetSceneNodeHasInheritsScale(DsonDocumentHandle handle, int index) {
    const Dson::Node* node = GetSceneNode(handle, index);
    return node ? node->has_inherits_scale : false;
}

int DsonDocument_GetSceneNodeGeometryCount(DsonDocumentHandle handle, int sceneNodeIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Node* node = doc ? At(doc->scene.nodes, sceneNodeIndex) : nullptr;
    return node ? static_cast<int>(node->geometries.size()) : 0;
}

const char* DsonDocument_GetSceneNodeGeometryId(DsonDocumentHandle handle, int sceneNodeIndex, int geomRefIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Node* node = doc ? At(doc->scene.nodes, sceneNodeIndex) : nullptr;
    const Dson::NodeGeometryRef* geom = node ? At(node->geometries, geomRefIndex) : nullptr;
    return geom ? geom->id.c_str() : "";
}

const char* DsonDocument_GetSceneNodeGeometryUrl(DsonDocumentHandle handle, int sceneNodeIndex, int geomRefIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Node* node = doc ? At(doc->scene.nodes, sceneNodeIndex) : nullptr;
    const Dson::NodeGeometryRef* geom = node ? At(node->geometries, geomRefIndex) : nullptr;
    return geom ? geom->url.c_str() : "";
}

// Scene post-load addon manifest (scene.extra "Character Addon Loader")
int DsonDocument_GetScenePostLoadAddonCount(DsonDocumentHandle handle) {
    Dson::DsonDocument* doc = Doc(handle);
    return doc ? static_cast<int>(doc->scene.post_load_addons.size()) : 0;
}

const char* DsonDocument_GetScenePostLoadAddonSlot(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::ScenePostLoadAddon* addon = doc ? At(doc->scene.post_load_addons, index) : nullptr;
    return addon ? addon->slot.c_str() : "";
}

const char* DsonDocument_GetScenePostLoadAddonAssetName(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::ScenePostLoadAddon* addon = doc ? At(doc->scene.post_load_addons, index) : nullptr;
    return addon ? addon->asset_name.c_str() : "";
}

const char* DsonDocument_GetScenePostLoadAddonAssetFile(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::ScenePostLoadAddon* addon = doc ? At(doc->scene.post_load_addons, index) : nullptr;
    return addon ? addon->asset_file.c_str() : "";
}

const char* DsonDocument_GetScenePostLoadAddonMatPreset(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::ScenePostLoadAddon* addon = doc ? At(doc->scene.post_load_addons, index) : nullptr;
    return addon ? addon->mat_preset.c_str() : "";
}

// Scene post-load scripts (scene.extra "scene_post_load_script" — DAZ Scripts
// the static import does NOT execute; surfaced so the consumer can warn).
int DsonDocument_GetScenePostLoadScriptCount(DsonDocumentHandle handle) {
    Dson::DsonDocument* doc = Doc(handle);
    return doc ? static_cast<int>(doc->scene.post_load_scripts.size()) : 0;
}

const char* DsonDocument_GetScenePostLoadScriptName(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::ScenePostLoadScript* scr = doc ? At(doc->scene.post_load_scripts, index) : nullptr;
    return scr ? scr->name.c_str() : "";
}

const char* DsonDocument_GetScenePostLoadScriptType(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::ScenePostLoadScript* scr = doc ? At(doc->scene.post_load_scripts, index) : nullptr;
    return scr ? scr->type.c_str() : "";
}

const char* DsonDocument_GetScenePostLoadScriptFile(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::ScenePostLoadScript* scr = doc ? At(doc->scene.post_load_scripts, index) : nullptr;
    return scr ? scr->script.c_str() : "";
}

// Scene animations (scene.animations)
int DsonDocument_GetSceneAnimationCount(DsonDocumentHandle handle) {
    Dson::DsonDocument* doc = Doc(handle);
    return doc ? static_cast<int>(doc->scene.animations.size()) : 0;
}

const char* DsonDocument_GetSceneAnimationUrl(DsonDocumentHandle handle, int animIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::SceneAnimation* anim = doc ? At(doc->scene.animations, animIndex) : nullptr;
    return anim ? anim->url.c_str() : "";
}

int DsonDocument_GetSceneAnimationValueKind(DsonDocumentHandle handle, int animIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::SceneAnimation* anim = doc ? At(doc->scene.animations, animIndex) : nullptr;
    return anim ? anim->kind : -1;
}

double DsonDocument_GetSceneAnimationFloat(DsonDocumentHandle handle, int animIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::SceneAnimation* anim = doc ? At(doc->scene.animations, animIndex) : nullptr;
    return (anim && anim->kind == Dson::SceneAnimation::KindNumber) ? anim->number : 0.0;
}

bool DsonDocument_GetSceneAnimationBool(DsonDocumentHandle handle, int animIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::SceneAnimation* anim = doc ? At(doc->scene.animations, animIndex) : nullptr;
    return (anim && anim->kind == Dson::SceneAnimation::KindBool) ? anim->boolean : false;
}

const char* DsonDocument_GetSceneAnimationString(DsonDocumentHandle handle, int animIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::SceneAnimation* anim = doc ? At(doc->scene.animations, animIndex) : nullptr;
    return (anim && anim->kind == Dson::SceneAnimation::KindString) ? anim->str.c_str() : "";
}

double DsonDocument_GetSceneAnimationColorR(DsonDocumentHandle handle, int animIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::SceneAnimation* anim = doc ? At(doc->scene.animations, animIndex) : nullptr;
    return (anim && anim->kind == Dson::SceneAnimation::KindColor) ? anim->color.x : 0.0;
}

double DsonDocument_GetSceneAnimationColorG(DsonDocumentHandle handle, int animIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::SceneAnimation* anim = doc ? At(doc->scene.animations, animIndex) : nullptr;
    return (anim && anim->kind == Dson::SceneAnimation::KindColor) ? anim->color.y : 0.0;
}

double DsonDocument_GetSceneAnimationColorB(DsonDocumentHandle handle, int animIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::SceneAnimation* anim = doc ? At(doc->scene.animations, animIndex) : nullptr;
    return (anim && anim->kind == Dson::SceneAnimation::KindColor) ? anim->color.z : 0.0;
}

// Scene modifier instances (scene.modifiers)
int DsonDocument_GetSceneModifierCount(DsonDocumentHandle handle) {
    Dson::DsonDocument* doc = Doc(handle);
    return doc ? static_cast<int>(doc->scene.modifiers.size()) : 0;
}

const char* DsonDocument_GetSceneModifierId(DsonDocumentHandle handle, int index) {
    const Dson::Modifier* mod = GetSceneModifier(handle, index);
    return mod ? mod->id.c_str() : "";
}

const char* DsonDocument_GetSceneModifierUrl(DsonDocumentHandle handle, int index) {
    const Dson::Modifier* mod = GetSceneModifier(handle, index);
    return mod ? mod->url.c_str() : "";
}

double DsonDocument_GetSceneModifierChannelValue(DsonDocumentHandle handle, int sceneModifierIndex) {
    return GetModifierChannelDouble(GetSceneModifier(handle, sceneModifierIndex), ModifierChannelValue);
}

double DsonDocument_GetSceneModifierChannelMin(DsonDocumentHandle handle, int sceneModifierIndex) {
    return GetModifierChannelDouble(GetSceneModifier(handle, sceneModifierIndex), ModifierChannelMin);
}

double DsonDocument_GetSceneModifierChannelMax(DsonDocumentHandle handle, int sceneModifierIndex) {
    return GetModifierChannelDouble(GetSceneModifier(handle, sceneModifierIndex), ModifierChannelMax);
}

bool DsonDocument_GetSceneModifierChannelClamped(DsonDocumentHandle handle, int sceneModifierIndex) {
    return GetModifierChannelClamped(GetSceneModifier(handle, sceneModifierIndex));
}

int DsonDocument_GetSceneModifierFormulaCount(DsonDocumentHandle handle, int sceneModifierIndex) {
    const Dson::Modifier* mod = GetSceneModifier(handle, sceneModifierIndex);
    return mod ? static_cast<int>(mod->formulas.size()) : 0;
}

const char* DsonDocument_GetSceneModifierFormulaOutput(
    DsonDocumentHandle handle,
    int sceneModifierIndex,
    int formulaIndex) {
    const Dson::Formula* formula = FormulaAt(GetSceneModifier(handle, sceneModifierIndex), formulaIndex);
    return formula ? formula->output.c_str() : "";
}

const char* DsonDocument_GetSceneModifierFormulaStage(
    DsonDocumentHandle handle,
    int sceneModifierIndex,
    int formulaIndex) {
    const Dson::Formula* formula = FormulaAt(GetSceneModifier(handle, sceneModifierIndex), formulaIndex);
    return formula ? formula->stage.c_str() : "";
}

int DsonDocument_GetSceneModifierFormulaOperationCount(
    DsonDocumentHandle handle,
    int sceneModifierIndex,
    int formulaIndex) {
    const Dson::Formula* formula = FormulaAt(GetSceneModifier(handle, sceneModifierIndex), formulaIndex);
    return formula ? static_cast<int>(formula->operations.size()) : 0;
}

const char* DsonDocument_GetSceneModifierFormulaOperationOp(
    DsonDocumentHandle handle,
    int sceneModifierIndex,
    int formulaIndex,
    int opIndex) {
    const Dson::FormulaOperation* op = FormulaOpAt(GetSceneModifier(handle, sceneModifierIndex), formulaIndex, opIndex);
    return op ? op->op.c_str() : "";
}

double DsonDocument_GetSceneModifierFormulaOperationVal(
    DsonDocumentHandle handle,
    int sceneModifierIndex,
    int formulaIndex,
    int opIndex) {
    const Dson::FormulaOperation* op = FormulaOpAt(GetSceneModifier(handle, sceneModifierIndex), formulaIndex, opIndex);
    return op ? op->val : 0.0;
}

const char* DsonDocument_GetSceneModifierFormulaOperationUrl(
    DsonDocumentHandle handle,
    int sceneModifierIndex,
    int formulaIndex,
    int opIndex) {
    const Dson::FormulaOperation* op = FormulaOpAt(GetSceneModifier(handle, sceneModifierIndex), formulaIndex, opIndex);
    return op ? op->url.c_str() : "";
}

int DsonDocument_GetSceneModifierFormulaOperationValArrayCount(
    DsonDocumentHandle handle, int sceneModifierIndex, int formulaIndex, int opIndex) {
    const Dson::FormulaOperation* op = FormulaOpAt(GetSceneModifier(handle, sceneModifierIndex), formulaIndex, opIndex);
    return op ? static_cast<int>(op->val_array.size()) : 0;
}

double DsonDocument_GetSceneModifierFormulaOperationValArrayElement(
    DsonDocumentHandle handle, int sceneModifierIndex, int formulaIndex, int opIndex, int elementIndex) {
    const Dson::FormulaOperation* op = FormulaOpAt(GetSceneModifier(handle, sceneModifierIndex), formulaIndex, opIndex);
    const double* el = op ? At(op->val_array, elementIndex) : nullptr;
    return el ? *el : 0.0;
}

// Scene material instances (scene.materials)
int DsonDocument_GetSceneMaterialCount(DsonDocumentHandle handle) {
    Dson::DsonDocument* doc = Doc(handle);
    return doc ? static_cast<int>(doc->scene.materials.size()) : 0;
}

const char* DsonDocument_GetSceneMaterialId(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, index) : nullptr;
    return mat ? mat->id.c_str() : "";
}

const char* DsonDocument_GetSceneMaterialUrl(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, index) : nullptr;
    return mat ? mat->url.c_str() : "";
}

// Scene UV set instances (scene.uvs)
int DsonDocument_GetSceneUVSetCount(DsonDocumentHandle handle) {
    Dson::DsonDocument* doc = Doc(handle);
    return doc ? static_cast<int>(doc->scene.uvs.size()) : 0;
}

const char* DsonDocument_GetSceneUVSetId(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::UVSet* uv = doc ? At(doc->scene.uvs, index) : nullptr;
    return uv ? uv->id.c_str() : "";
}

const char* DsonDocument_GetSceneUVSetUrl(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::UVSet* uv = doc ? At(doc->scene.uvs, index) : nullptr;
    return uv ? uv->url.c_str() : "";
}

const char* DsonDocument_GetMaterialId(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->materials, index) : nullptr;
    return mat ? mat->id.c_str() : "";
}

const char* DsonDocument_GetGeometryId(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Geometry* geom = doc ? At(doc->geometries, index) : nullptr;
    return geom ? geom->id.c_str() : "";
}

const char* DsonDocument_GetGeometryName(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Geometry* geom = doc ? At(doc->geometries, index) : nullptr;
    return geom ? geom->name.c_str() : "";
}

int DsonDocument_GetGeometryVertexCount(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Geometry* geom = doc ? At(doc->geometries, index) : nullptr;
    return geom ? static_cast<int>(geom->vertex_count) : 0;
}

int DsonDocument_GetGeometryPolygonCount(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Geometry* geom = doc ? At(doc->geometries, index) : nullptr;
    return geom ? static_cast<int>(geom->polygon_count) : 0;
}

const char* DsonDocument_GetGeometryDefaultUVSetId(DsonDocumentHandle handle, int geomIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Geometry* geom = doc ? At(doc->geometries, geomIndex) : nullptr;
    return geom ? geom->default_uv_set_id.c_str() : "";
}

bool DsonDocument_GetGeometryIsGraft(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Geometry* geom = doc ? At(doc->geometries, index) : nullptr;
    return geom ? geom->is_graft : false;
}

int DsonDocument_GetGeometryGraftVertexPairCount(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Geometry* geom = doc ? At(doc->geometries, index) : nullptr;
    return geom ? static_cast<int>(geom->graft_vertex_pairs.size()) : 0;
}

int DsonDocument_GetGeometryGraftVertexPairGraftVertex(DsonDocumentHandle handle, int index, int pairIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Geometry* geom = doc ? At(doc->geometries, index) : nullptr;
    const Dson::GraftVertexPair* pair = geom ? At(geom->graft_vertex_pairs, pairIndex) : nullptr;
    return pair ? pair->graft_vertex : -1;
}

int DsonDocument_GetGeometryGraftVertexPairBaseVertex(DsonDocumentHandle handle, int index, int pairIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Geometry* geom = doc ? At(doc->geometries, index) : nullptr;
    const Dson::GraftVertexPair* pair = geom ? At(geom->graft_vertex_pairs, pairIndex) : nullptr;
    return pair ? pair->base_vertex : -1;
}

int DsonDocument_GetGeometryGraftHiddenPolyCount(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Geometry* geom = doc ? At(doc->geometries, index) : nullptr;
    return geom ? static_cast<int>(geom->graft_hidden_polys.size()) : 0;
}

int DsonDocument_GetGeometryGraftHiddenPoly(DsonDocumentHandle handle, int index, int hiddenIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Geometry* geom = doc ? At(doc->geometries, index) : nullptr;
    const int* poly = geom ? At(geom->graft_hidden_polys, hiddenIndex) : nullptr;
    return poly ? *poly : -1;
}

int DsonDocument_GetGeometryGraftBaseVertexCount(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Geometry* geom = doc ? At(doc->geometries, index) : nullptr;
    return geom ? static_cast<int>(geom->graft_base_vertex_count) : 0;
}

int DsonDocument_GetGeometryGraftBasePolyCount(DsonDocumentHandle handle, int index) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Geometry* geom = doc ? At(doc->geometries, index) : nullptr;
    return geom ? static_cast<int>(geom->graft_base_poly_count) : 0;
}

bool DsonDocument_GetGeometryHasRigidity(DsonDocumentHandle handle, int geomIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Geometry* geom = doc ? At(doc->geometries, geomIndex) : nullptr;
    return geom ? geom->has_rigidity : false;
}

int DsonDocument_GetGeometryRigidityWeightCount(DsonDocumentHandle handle, int geomIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Geometry* geom = doc ? At(doc->geometries, geomIndex) : nullptr;
    return geom ? static_cast<int>(geom->rigidity_weights.size()) : 0;
}

int DsonDocument_GetGeometryRigidityWeightVertexIndex(DsonDocumentHandle handle, int geomIndex, int weightIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Geometry* geom = doc ? At(doc->geometries, geomIndex) : nullptr;
    const Dson::GeometryRigidityWeight* weight = geom ? At(geom->rigidity_weights, weightIndex) : nullptr;
    return weight ? weight->vertex_index : -1;
}

double DsonDocument_GetGeometryRigidityWeight(DsonDocumentHandle handle, int geomIndex, int weightIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Geometry* geom = doc ? At(doc->geometries, geomIndex) : nullptr;
    const Dson::GeometryRigidityWeight* weight = geom ? At(geom->rigidity_weights, weightIndex) : nullptr;
    return weight ? weight->weight : 0.0;
}

int DsonDocument_GetGeometryRigidityGroupCount(DsonDocumentHandle handle, int geomIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Geometry* geom = doc ? At(doc->geometries, geomIndex) : nullptr;
    return geom ? static_cast<int>(geom->rigidity_groups.size()) : 0;
}

const char* DsonDocument_GetGeometryRigidityGroupId(DsonDocumentHandle handle, int geomIndex, int groupIndex) {
    const Dson::GeometryRigidityGroup* group = GetGeometryRigidityGroup(handle, geomIndex, groupIndex);
    return group ? group->id.c_str() : "";
}

const char* DsonDocument_GetGeometryRigidityGroupRotationMode(DsonDocumentHandle handle, int geomIndex, int groupIndex) {
    const Dson::GeometryRigidityGroup* group = GetGeometryRigidityGroup(handle, geomIndex, groupIndex);
    return group ? group->rotation_mode.c_str() : "";
}

int DsonDocument_GetGeometryRigidityGroupScaleModeCount(DsonDocumentHandle handle, int geomIndex, int groupIndex) {
    const Dson::GeometryRigidityGroup* group = GetGeometryRigidityGroup(handle, geomIndex, groupIndex);
    return group ? static_cast<int>(group->scale_modes.size()) : 0;
}

const char* DsonDocument_GetGeometryRigidityGroupScaleMode(DsonDocumentHandle handle, int geomIndex, int groupIndex, int modeIndex) {
    const Dson::GeometryRigidityGroup* group = GetGeometryRigidityGroup(handle, geomIndex, groupIndex);
    const std::string* mode = group ? At(group->scale_modes, modeIndex) : nullptr;
    return mode ? mode->c_str() : "";
}

int DsonDocument_GetGeometryRigidityGroupReferenceVertexCount(DsonDocumentHandle handle, int geomIndex, int groupIndex) {
    const Dson::GeometryRigidityGroup* group = GetGeometryRigidityGroup(handle, geomIndex, groupIndex);
    return group ? static_cast<int>(group->reference_vertices.size()) : 0;
}

int DsonDocument_GetGeometryRigidityGroupReferenceVertex(DsonDocumentHandle handle, int geomIndex, int groupIndex, int vertexIndex) {
    const Dson::GeometryRigidityGroup* group = GetGeometryRigidityGroup(handle, geomIndex, groupIndex);
    const int* vertex = group ? At(group->reference_vertices, vertexIndex) : nullptr;
    return vertex ? *vertex : -1;
}

int DsonDocument_GetGeometryRigidityGroupMaskVertexCount(DsonDocumentHandle handle, int geomIndex, int groupIndex) {
    const Dson::GeometryRigidityGroup* group = GetGeometryRigidityGroup(handle, geomIndex, groupIndex);
    return group ? static_cast<int>(group->mask_vertices.size()) : 0;
}

int DsonDocument_GetGeometryRigidityGroupMaskVertex(DsonDocumentHandle handle, int geomIndex, int groupIndex, int vertexIndex) {
    const Dson::GeometryRigidityGroup* group = GetGeometryRigidityGroup(handle, geomIndex, groupIndex);
    const int* vertex = group ? At(group->mask_vertices, vertexIndex) : nullptr;
    return vertex ? *vertex : -1;
}

const char* DsonDocument_GetGeometryRigidityGroupReference(DsonDocumentHandle handle, int geomIndex, int groupIndex) {
    const Dson::GeometryRigidityGroup* group = GetGeometryRigidityGroup(handle, geomIndex, groupIndex);
    return group ? group->reference.c_str() : "";
}

int DsonDocument_GetGeometryRigidityGroupTransformNodeCount(DsonDocumentHandle handle, int geomIndex, int groupIndex) {
    const Dson::GeometryRigidityGroup* group = GetGeometryRigidityGroup(handle, geomIndex, groupIndex);
    return group ? static_cast<int>(group->transform_nodes.size()) : 0;
}

const char* DsonDocument_GetGeometryRigidityGroupTransformNode(DsonDocumentHandle handle, int geomIndex, int groupIndex, int nodeIndex) {
    const Dson::GeometryRigidityGroup* group = GetGeometryRigidityGroup(handle, geomIndex, groupIndex);
    const std::string* node = group ? At(group->transform_nodes, nodeIndex) : nullptr;
    return node ? node->c_str() : "";
}

bool DsonDocument_GetGeometryRigidityGroupUseTransformBonesForScale(DsonDocumentHandle handle, int geomIndex, int groupIndex) {
    const Dson::GeometryRigidityGroup* group = GetGeometryRigidityGroup(handle, geomIndex, groupIndex);
    return group ? group->use_transform_bones_for_scale : false;
}

const char* DsonDocument_GetModifierId(DsonDocumentHandle handle, int index) {
    const Dson::Modifier* mod = GetLibraryModifier(handle, index);
    return mod ? mod->id.c_str() : "";
}

const char* DsonDocument_GetModifierName(DsonDocumentHandle handle, int index) {
    const Dson::Modifier* mod = GetLibraryModifier(handle, index);
    return mod ? mod->name.c_str() : "";
}

const char* DsonDocument_GetModifierType(DsonDocumentHandle handle, int index) {
    const Dson::Modifier* mod = GetLibraryModifier(handle, index);
    return mod ? mod->type.c_str() : "";
}

const char* DsonDocument_GetModifierPresentationType(DsonDocumentHandle handle, int index) {
    const Dson::Modifier* mod = GetLibraryModifier(handle, index);
    return mod ? mod->presentation_type.c_str() : "";
}

const char* DsonDocument_GetModifierPresentationLabel(DsonDocumentHandle handle, int index) {
    const Dson::Modifier* mod = GetLibraryModifier(handle, index);
    return mod ? mod->presentation_label.c_str() : "";
}

const char* DsonDocument_GetModifierGroup(DsonDocumentHandle handle, int index) {
    const Dson::Modifier* mod = GetLibraryModifier(handle, index);
    return mod ? mod->group.c_str() : "";
}

const char* DsonDocument_GetModifierRegion(DsonDocumentHandle handle, int index) {
    const Dson::Modifier* mod = GetLibraryModifier(handle, index);
    return mod ? mod->region.c_str() : "";
}

const char* DsonDocument_GetModifierPresentationIcon(DsonDocumentHandle handle, int index) {
    const Dson::Modifier* mod = GetLibraryModifier(handle, index);
    return mod ? mod->presentation_icon.c_str() : "";
}

double DsonDocument_GetModifierChannelValue(DsonDocumentHandle handle, int modifierIndex) {
    return GetModifierChannelDouble(GetLibraryModifier(handle, modifierIndex), ModifierChannelValue);
}

double DsonDocument_GetModifierChannelMin(DsonDocumentHandle handle, int modifierIndex) {
    return GetModifierChannelDouble(GetLibraryModifier(handle, modifierIndex), ModifierChannelMin);
}

double DsonDocument_GetModifierChannelMax(DsonDocumentHandle handle, int modifierIndex) {
    return GetModifierChannelDouble(GetLibraryModifier(handle, modifierIndex), ModifierChannelMax);
}

bool DsonDocument_GetModifierChannelClamped(DsonDocumentHandle handle, int modifierIndex) {
    return GetModifierChannelClamped(GetLibraryModifier(handle, modifierIndex));
}

bool DsonDocument_GetModifierIsPush(DsonDocumentHandle handle, int modifierIndex) {
    const Dson::Modifier* mod = GetLibraryModifier(handle, modifierIndex);
    return mod ? mod->is_push : false;
}

double DsonDocument_GetModifierPushOffset(DsonDocumentHandle handle, int modifierIndex) {
    const Dson::Modifier* mod = GetLibraryModifier(handle, modifierIndex);
    return mod ? mod->push_offset_value : 0.0;
}

int DsonDocument_GetModifierFormulaCount(DsonDocumentHandle handle, int modifierIndex) {
    const Dson::Modifier* mod = GetLibraryModifier(handle, modifierIndex);
    return mod ? static_cast<int>(mod->formulas.size()) : 0;
}

const char* DsonDocument_GetModifierFormulaOutput(
    DsonDocumentHandle handle,
    int modifierIndex,
    int formulaIndex) {
    const Dson::Formula* formula = FormulaAt(GetLibraryModifier(handle, modifierIndex), formulaIndex);
    return formula ? formula->output.c_str() : "";
}

const char* DsonDocument_GetModifierFormulaStage(
    DsonDocumentHandle handle,
    int modifierIndex,
    int formulaIndex) {
    const Dson::Formula* formula = FormulaAt(GetLibraryModifier(handle, modifierIndex), formulaIndex);
    return formula ? formula->stage.c_str() : "";
}

int DsonDocument_GetModifierFormulaOperationCount(
    DsonDocumentHandle handle,
    int modifierIndex,
    int formulaIndex) {
    const Dson::Formula* formula = FormulaAt(GetLibraryModifier(handle, modifierIndex), formulaIndex);
    return formula ? static_cast<int>(formula->operations.size()) : 0;
}

const char* DsonDocument_GetModifierFormulaOperationOp(
    DsonDocumentHandle handle,
    int modifierIndex,
    int formulaIndex,
    int opIndex) {
    const Dson::FormulaOperation* op = FormulaOpAt(GetLibraryModifier(handle, modifierIndex), formulaIndex, opIndex);
    return op ? op->op.c_str() : "";
}

double DsonDocument_GetModifierFormulaOperationVal(
    DsonDocumentHandle handle,
    int modifierIndex,
    int formulaIndex,
    int opIndex) {
    const Dson::FormulaOperation* op = FormulaOpAt(GetLibraryModifier(handle, modifierIndex), formulaIndex, opIndex);
    return op ? op->val : 0.0;
}

const char* DsonDocument_GetModifierFormulaOperationUrl(
    DsonDocumentHandle handle,
    int modifierIndex,
    int formulaIndex,
    int opIndex) {
    const Dson::FormulaOperation* op = FormulaOpAt(GetLibraryModifier(handle, modifierIndex), formulaIndex, opIndex);
    return op ? op->url.c_str() : "";
}

int DsonDocument_GetModifierFormulaOperationValArrayCount(
    DsonDocumentHandle handle, int modifierIndex, int formulaIndex, int opIndex) {
    const Dson::FormulaOperation* op = FormulaOpAt(GetLibraryModifier(handle, modifierIndex), formulaIndex, opIndex);
    return op ? static_cast<int>(op->val_array.size()) : 0;
}

double DsonDocument_GetModifierFormulaOperationValArrayElement(
    DsonDocumentHandle handle, int modifierIndex, int formulaIndex, int opIndex, int elementIndex) {
    const Dson::FormulaOperation* op = FormulaOpAt(GetLibraryModifier(handle, modifierIndex), formulaIndex, opIndex);
    const double* el = op ? At(op->val_array, elementIndex) : nullptr;
    return el ? *el : 0.0;
}

int DsonDocument_GetModifierSkinVertexCount(DsonDocumentHandle handle, int index) {
    const Dson::Modifier* mod = GetLibraryModifier(handle, index);
    return mod ? static_cast<int>(mod->skin.vertex_count) : 0;
}

int DsonDocument_GetModifierSkinJointCount(DsonDocumentHandle handle, int index) {
    const Dson::Modifier* mod = GetLibraryModifier(handle, index);
    return mod ? static_cast<int>(mod->skin.joints.size()) : 0;
}

// ---- Images (image_library) ----
const char* DsonDocument_GetImageId(DsonDocumentHandle handle, int imageIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Image* img = doc ? At(doc->images, imageIndex) : nullptr;
    return img ? img->id.c_str() : "";
}

int DsonDocument_GetImageMapWidth(DsonDocumentHandle handle, int imageIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Image* img = doc ? At(doc->images, imageIndex) : nullptr;
    return img ? static_cast<int>(img->map_width) : 0;
}

int DsonDocument_GetImageMapHeight(DsonDocumentHandle handle, int imageIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Image* img = doc ? At(doc->images, imageIndex) : nullptr;
    return img ? static_cast<int>(img->map_height) : 0;
}

int DsonDocument_GetImageLayerCount(DsonDocumentHandle handle, int imageIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Image* img = doc ? At(doc->images, imageIndex) : nullptr;
    return img ? static_cast<int>(img->layers.size()) : 0;
}

const char* DsonDocument_GetImageLayerTexturePath(DsonDocumentHandle handle, int imageIndex, int layerIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Image* img = doc ? At(doc->images, imageIndex) : nullptr;
    const Dson::ImageLayer* layer = img ? At(img->layers, layerIdx) : nullptr;
    return layer ? layer->url.c_str() : "";
}

const char* DsonDocument_GetImageLayerLabel(DsonDocumentHandle handle, int imageIndex, int layerIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Image* img = doc ? At(doc->images, imageIndex) : nullptr;
    const Dson::ImageLayer* layer = img ? At(img->layers, layerIdx) : nullptr;
    return layer ? layer->label.c_str() : "";
}

const char* DsonDocument_GetImageLayerBlendMode(DsonDocumentHandle handle, int imageIndex, int layerIdx) {
    const Dson::ImageLayer* L = GetImageLayer(handle, imageIndex, layerIdx);
    return L ? L->blend_op.c_str() : "";
}
double DsonDocument_GetImageLayerOpacity(DsonDocumentHandle handle, int imageIndex, int layerIdx) {
    const Dson::ImageLayer* L = GetImageLayer(handle, imageIndex, layerIdx);
    return L ? L->opacity : 0.0;
}
bool DsonDocument_GetImageLayerActive(DsonDocumentHandle handle, int imageIndex, int layerIdx) {
    const Dson::ImageLayer* L = GetImageLayer(handle, imageIndex, layerIdx);
    return L ? L->active : false;
}
bool DsonDocument_GetImageLayerInvert(DsonDocumentHandle handle, int imageIndex, int layerIdx) {
    const Dson::ImageLayer* L = GetImageLayer(handle, imageIndex, layerIdx);
    return L ? L->invert : false;
}
double DsonDocument_GetImageLayerColorR(DsonDocumentHandle handle, int imageIndex, int layerIdx) {
    const Dson::ImageLayer* L = GetImageLayer(handle, imageIndex, layerIdx);
    return L ? L->color.x : 0.0;
}
double DsonDocument_GetImageLayerColorG(DsonDocumentHandle handle, int imageIndex, int layerIdx) {
    const Dson::ImageLayer* L = GetImageLayer(handle, imageIndex, layerIdx);
    return L ? L->color.y : 0.0;
}
double DsonDocument_GetImageLayerColorB(DsonDocumentHandle handle, int imageIndex, int layerIdx) {
    const Dson::ImageLayer* L = GetImageLayer(handle, imageIndex, layerIdx);
    return L ? L->color.z : 0.0;
}
double DsonDocument_GetImageLayerRotation(DsonDocumentHandle handle, int imageIndex, int layerIdx) {
    const Dson::ImageLayer* L = GetImageLayer(handle, imageIndex, layerIdx);
    return L ? L->rotation : 0.0;
}
double DsonDocument_GetImageLayerScaleX(DsonDocumentHandle handle, int imageIndex, int layerIdx) {
    const Dson::ImageLayer* L = GetImageLayer(handle, imageIndex, layerIdx);
    return L ? L->scale_x : 1.0;
}
double DsonDocument_GetImageLayerScaleY(DsonDocumentHandle handle, int imageIndex, int layerIdx) {
    const Dson::ImageLayer* L = GetImageLayer(handle, imageIndex, layerIdx);
    return L ? L->scale_y : 1.0;
}
double DsonDocument_GetImageLayerOffsetX(DsonDocumentHandle handle, int imageIndex, int layerIdx) {
    const Dson::ImageLayer* L = GetImageLayer(handle, imageIndex, layerIdx);
    return L ? L->offset_x : 0.0;
}
double DsonDocument_GetImageLayerOffsetY(DsonDocumentHandle handle, int imageIndex, int layerIdx) {
    const Dson::ImageLayer* L = GetImageLayer(handle, imageIndex, layerIdx);
    return L ? L->offset_y : 0.0;
}
bool DsonDocument_GetImageLayerMirrorX(DsonDocumentHandle handle, int imageIndex, int layerIdx) {
    const Dson::ImageLayer* L = GetImageLayer(handle, imageIndex, layerIdx);
    return L ? L->mirror_x : false;
}
bool DsonDocument_GetImageLayerMirrorY(DsonDocumentHandle handle, int imageIndex, int layerIdx) {
    const Dson::ImageLayer* L = GetImageLayer(handle, imageIndex, layerIdx);
    return L ? L->mirror_y : false;
}

// Unknown-key diagnostics expose the parser's audit trail. Context names are
// top-level parse scopes such as "geometry_library" or "scene"; values are keys
// that were present in the JSON but not consumed by the current parser version.
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
    ClearQueryCaches(ctx);
}

void DsonDocument_Destroy(DsonDocumentHandle handle) {
    if (!handle) return;
    delete GetContext(handle);
}

// ============================================================
// A. Geometry — vertex positions
// ============================================================

int DsonDocument_GetVertexCount(DsonDocumentHandle handle, int geomIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Geometry* geom = doc ? At(doc->geometries, geomIndex) : nullptr;
    return geom ? static_cast<int>(geom->vertex_count) : 0;
}

double DsonDocument_GetVertexX(DsonDocumentHandle handle, int geomIndex, int vertexIndex) {
    return GetGeometryVertexComponent(handle, geomIndex, vertexIndex, 0);
}

double DsonDocument_GetVertexY(DsonDocumentHandle handle, int geomIndex, int vertexIndex) {
    return GetGeometryVertexComponent(handle, geomIndex, vertexIndex, 1);
}

double DsonDocument_GetVertexZ(DsonDocumentHandle handle, int geomIndex, int vertexIndex) {
    return GetGeometryVertexComponent(handle, geomIndex, vertexIndex, 2);
}

// ---- Polylist ----
// Each face entry is: [polygon_groups_idx, polygon_material_groups_idx, v0, v1, ..., vN-1]
// Indices [0] and [1] are the two leading ints; vertex indices start at [2].
// polylist_face_offsets[i] holds the start index in polylist.values for face i,
// enabling correct indexing for variable-length (mixed tri/quad) faces.

static int GetPolylistFaceStart(const Dson::Geometry& geom, int faceIndex) {
    if (faceIndex < 0 || faceIndex >= geom.polygon_count) return -1;
    if (faceIndex >= static_cast<int>(geom.polylist_face_offsets.size())) return -1;
    return geom.polylist_face_offsets[faceIndex];
}

static int GetPolylistFaceEnd(const Dson::Geometry& geom, int faceIndex) {
    int start = GetPolylistFaceStart(geom, faceIndex);
    if (start < 0) return -1;
    if (faceIndex + 1 < static_cast<int>(geom.polylist_face_offsets.size())) {
        return geom.polylist_face_offsets[faceIndex + 1];
    }
    return static_cast<int>(geom.polylist.values.size());
}

static int GetPolylistFaceValue(const Dson::Geometry& geom, int faceIndex, int offsetInFace) {
    if (offsetInFace < 0) return -1;
    int start = GetPolylistFaceStart(geom, faceIndex);
    int end = GetPolylistFaceEnd(geom, faceIndex);
    if (start < 0 || end < 0) return -1;
    int index = start + offsetInFace;
    if (index < start || index >= end || index >= static_cast<int>(geom.polylist.values.size())) return -1;
    return geom.polylist.values[index];
}

int DsonDocument_GetPolylistCount(DsonDocumentHandle handle, int geomIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Geometry* geom = doc ? At(doc->geometries, geomIndex) : nullptr;
    return geom ? static_cast<int>(geom->polygon_count) : 0;
}

int DsonDocument_GetPolylistFaceVertexCount(DsonDocumentHandle handle, int geomIndex, int faceIndex) {
    if (!handle) return 0;
    const Dson::Geometry* geom = GetGeometry(handle, geomIndex);
    if (!geom) return 0;
    int start = GetPolylistFaceStart(*geom, faceIndex);
    int end = GetPolylistFaceEnd(*geom, faceIndex);
    if (start < 0 || end < 0) return 0;
    int len = end - start;
    if (len < 2) return 0;
    return len - 2;
}

int DsonDocument_GetPolylistFaceVertex(DsonDocumentHandle handle, int geomIndex, int faceIndex, int vertexIndex) {
    if (!handle) return -1;
    const Dson::Geometry* geom = GetGeometry(handle, geomIndex);
    if (!geom) return -1;
    int start = GetPolylistFaceStart(*geom, faceIndex);
    int end = GetPolylistFaceEnd(*geom, faceIndex);
    if (start < 0 || end < 0) return -1;
    int len = end - start;
    if (len < 2) return -1;
    int vertsPerFace = len - 2;
    if (vertexIndex < 0 || vertexIndex >= vertsPerFace) return -1;
    return GetPolylistFaceValue(*geom, faceIndex, vertexIndex + 2);
}

int DsonDocument_GetPolylistFaceMaterialIndex(DsonDocumentHandle handle, int geomIndex, int faceIndex) {
    if (!handle) return -1;
    const Dson::Geometry* geom = GetGeometry(handle, geomIndex);
    if (!geom) return -1;
    return GetPolylistFaceValue(*geom, faceIndex, 1);
}

int DsonDocument_GetPolylistFaceGroupIndex(DsonDocumentHandle handle, int geomIndex, int faceIndex) {
    if (!handle) return -1;
    const Dson::Geometry* geom = GetGeometry(handle, geomIndex);
    if (!geom) return -1;
    return GetPolylistFaceValue(*geom, faceIndex, 0);
}

// ---- Polygon groups (bone region groups) ----

int DsonDocument_GetPolygonGroupCount(DsonDocumentHandle handle, int geomIndex) {
    if (!handle) return 0;
    const Dson::Geometry* geom = GetGeometry(handle, geomIndex);
    if (!geom) return 0;
    return GetStringVectorCount(geom->polygon_groups);
}

const char* DsonDocument_GetPolygonGroupName(DsonDocumentHandle handle, int geomIndex, int groupIndex) {
    if (!handle) return "";
    const Dson::Geometry* geom = GetGeometry(handle, geomIndex);
    if (!geom) return "";
    return GetStringVectorValue(geom->polygon_groups, groupIndex);
}

// ---- Material groups ----

int DsonDocument_GetPolygonMaterialGroupCount(DsonDocumentHandle handle, int geomIndex) {
    if (!handle) return 0;
    const Dson::Geometry* geom = GetGeometry(handle, geomIndex);
    if (!geom) return 0;
    return GetStringVectorCount(geom->polygon_material_groups);
}

const char* DsonDocument_GetPolygonMaterialGroupName(DsonDocumentHandle handle, int geomIndex, int groupIndex) {
    if (!handle) return "";
    const Dson::Geometry* geom = GetGeometry(handle, geomIndex);
    if (!geom) return "";
    return GetStringVectorValue(geom->polygon_material_groups, groupIndex);
}

// ---- Material groups (library materials) ----

int DsonDocument_GetMaterialGroupCount(DsonDocumentHandle handle, int matIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->materials, matIndex) : nullptr;
    return mat ? GetStringVectorCount(mat->groups) : 0;
}

const char* DsonDocument_GetMaterialGroupName(DsonDocumentHandle handle, int matIndex, int groupIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->materials, matIndex) : nullptr;
    return mat ? GetStringVectorValue(mat->groups, groupIndex) : "";
}

// ---- Material groups (scene material instances) ----

int DsonDocument_GetSceneMaterialGroupCount(DsonDocumentHandle handle, int matIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, matIndex) : nullptr;
    return mat ? GetStringVectorCount(mat->groups) : 0;
}

const char* DsonDocument_GetSceneMaterialGroupName(DsonDocumentHandle handle, int matIndex, int groupIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, matIndex) : nullptr;
    return mat ? GetStringVectorValue(mat->groups, groupIndex) : "";
}

// ============================================================
// B. Skeleton / Nodes
// ============================================================

const char* DsonDocument_GetNodeParent(DsonDocumentHandle handle, int nodeIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Node* node = doc ? At(doc->nodes, nodeIndex) : nullptr;
    return node ? node->parent.c_str() : "";
}

double DsonDocument_GetNodeEndPointX(DsonDocumentHandle handle, int nodeIndex) {
    return GetNodeVector3Component(handle, nodeIndex, &Dson::Node::end_point, 0);
}

double DsonDocument_GetNodeEndPointY(DsonDocumentHandle handle, int nodeIndex) {
    return GetNodeVector3Component(handle, nodeIndex, &Dson::Node::end_point, 1);
}

double DsonDocument_GetNodeEndPointZ(DsonDocumentHandle handle, int nodeIndex) {
    return GetNodeVector3Component(handle, nodeIndex, &Dson::Node::end_point, 2);
}

double DsonDocument_GetNodeOrientationX(DsonDocumentHandle handle, int nodeIndex) {
    return GetNodeVector3Component(handle, nodeIndex, &Dson::Node::orientation, 0);
}

double DsonDocument_GetNodeOrientationY(DsonDocumentHandle handle, int nodeIndex) {
    return GetNodeVector3Component(handle, nodeIndex, &Dson::Node::orientation, 1);
}

double DsonDocument_GetNodeOrientationZ(DsonDocumentHandle handle, int nodeIndex) {
    return GetNodeVector3Component(handle, nodeIndex, &Dson::Node::orientation, 2);
}

const char* DsonDocument_GetNodeRotationOrder(DsonDocumentHandle handle, int nodeIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Node* node = doc ? At(doc->nodes, nodeIndex) : nullptr;
    return node ? node->rotation_order.c_str() : "";
}

double DsonDocument_GetNodeTranslationX(DsonDocumentHandle handle, int nodeIndex) {
    return GetNodeVector3Component(handle, nodeIndex, &Dson::Node::translation, 0);
}

double DsonDocument_GetNodeTranslationY(DsonDocumentHandle handle, int nodeIndex) {
    return GetNodeVector3Component(handle, nodeIndex, &Dson::Node::translation, 1);
}

double DsonDocument_GetNodeTranslationZ(DsonDocumentHandle handle, int nodeIndex) {
    return GetNodeVector3Component(handle, nodeIndex, &Dson::Node::translation, 2);
}

double DsonDocument_GetNodeRotationX(DsonDocumentHandle handle, int nodeIndex) {
    return GetNodeVector3Component(handle, nodeIndex, &Dson::Node::rotation, 0);
}

double DsonDocument_GetNodeRotationY(DsonDocumentHandle handle, int nodeIndex) {
    return GetNodeVector3Component(handle, nodeIndex, &Dson::Node::rotation, 1);
}

double DsonDocument_GetNodeRotationZ(DsonDocumentHandle handle, int nodeIndex) {
    return GetNodeVector3Component(handle, nodeIndex, &Dson::Node::rotation, 2);
}

double DsonDocument_GetNodeScaleX(DsonDocumentHandle handle, int nodeIndex) {
    return GetNodeVector3Component(handle, nodeIndex, &Dson::Node::scale, 0);
}

double DsonDocument_GetNodeScaleY(DsonDocumentHandle handle, int nodeIndex) {
    return GetNodeVector3Component(handle, nodeIndex, &Dson::Node::scale, 1);
}

double DsonDocument_GetNodeScaleZ(DsonDocumentHandle handle, int nodeIndex) {
    return GetNodeVector3Component(handle, nodeIndex, &Dson::Node::scale, 2);
}

double DsonDocument_GetNodeGeneralScale(DsonDocumentHandle handle, int nodeIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Node* node = doc ? At(doc->nodes, nodeIndex) : nullptr;
    return node ? node->general_scale : 1.0;
}

// ============================================================
// C. Skin Weights
// ============================================================

// Raw skin queries below expose the parsed joint->vertex data directly from a
// skin_binding modifier. Use the vertex influence queries when building engine
// vertex buffers, because those return vertex->bone influences sorted and
// normalized for each vertex.
int DsonDocument_GetSkinJointCount(DsonDocumentHandle handle, int modifierIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Modifier* mod = doc ? At(doc->modifiers, modifierIndex) : nullptr;
    return mod ? static_cast<int>(mod->skin.joints.size()) : 0;
}

const char* DsonDocument_GetSkinJointNodeId(DsonDocumentHandle handle, int modifierIndex, int jointIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Modifier* mod = doc ? At(doc->modifiers, modifierIndex) : nullptr;
    const Dson::SkinJoint* joint = mod ? At(mod->skin.joints, jointIndex) : nullptr;
    return joint ? joint->node.c_str() : "";
}

int DsonDocument_GetSkinJointWeightCount(DsonDocumentHandle handle, int modifierIndex, int jointIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Modifier* mod = doc ? At(doc->modifiers, modifierIndex) : nullptr;
    const Dson::SkinJoint* joint = mod ? At(mod->skin.joints, jointIndex) : nullptr;
    return joint ? static_cast<int>(joint->weights.size()) : 0;
}

int DsonDocument_GetSkinJointWeightVertexIndex(DsonDocumentHandle handle, int modifierIndex, int jointIndex, int weightIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Modifier* mod = doc ? At(doc->modifiers, modifierIndex) : nullptr;
    const Dson::SkinJoint* joint = mod ? At(mod->skin.joints, jointIndex) : nullptr;
    const int* vertIdx = joint ? At(joint->weight_indices, weightIndex) : nullptr;
    return vertIdx ? *vertIdx : -1;
}

double DsonDocument_GetSkinJointWeight(DsonDocumentHandle handle, int modifierIndex, int jointIndex, int weightIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Modifier* mod = doc ? At(doc->modifiers, modifierIndex) : nullptr;
    const Dson::SkinJoint* joint = mod ? At(mod->skin.joints, jointIndex) : nullptr;
    const double* weight = joint ? At(joint->weights, weightIndex) : nullptr;
    return weight ? *weight : 0.0;
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

// UE-style import paths usually cap each vertex to a fixed number of influences
// (for example eight). This accessor reads from the full normalized cache, then
// renormalizes only the top maxInfluences weights so the returned capped set
// still sums to 1.0.
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
    double sum = std::accumulate(influences.begin(), influences.begin() + cap, 0.0,
        [](double acc, const VertexInfluence& inf) { return acc + inf.weight; });
    *boneNodeId = influences[influenceIndex].boneNodeId.c_str();
    *weight = (sum > 0.0) ? influences[influenceIndex].weight / sum : 0.0;
    return true;
}

// ============================================================
// D. UV Sets (library uv_sets)
// ============================================================

const char* DsonDocument_GetUVSetId(DsonDocumentHandle handle, int uvSetIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::UVSet* uv = doc ? At(doc->uv_sets, uvSetIndex) : nullptr;
    return uv ? uv->id.c_str() : "";
}

int DsonDocument_GetUVCount(DsonDocumentHandle handle, int uvSetIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::UVSet* uv = doc ? At(doc->uv_sets, uvSetIndex) : nullptr;
    return uv ? static_cast<int>(uv->uvs.values.size()) / 2 : 0;
}

double DsonDocument_GetUVU(DsonDocumentHandle handle, int uvSetIndex, int uvIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::UVSet* uv = doc ? At(doc->uv_sets, uvSetIndex) : nullptr;
    if (!uv) return 0.0;
    const auto& uvs = uv->uvs.values;
    int idx = uvIndex * 2;
    if (uvIndex < 0 || idx + 1 >= static_cast<int>(uvs.size())) return 0.0;
    return uvs[idx];
}

double DsonDocument_GetUVV(DsonDocumentHandle handle, int uvSetIndex, int uvIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::UVSet* uv = doc ? At(doc->uv_sets, uvSetIndex) : nullptr;
    if (!uv) return 0.0;
    const auto& uvs = uv->uvs.values;
    int idx = uvIndex * 2;
    if (uvIndex < 0 || idx + 1 >= static_cast<int>(uvs.size())) return 0.0;
    return uvs[idx + 1];
}

int DsonDocument_GetUVSetVertexCount(DsonDocumentHandle handle, int uvSetIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::UVSet* uv = doc ? At(doc->uv_sets, uvSetIndex) : nullptr;
    return uv ? uv->vertex_count : 0;
}

int DsonDocument_GetUVOverrideCount(DsonDocumentHandle handle, int uvSetIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::UVSet* uv = doc ? At(doc->uv_sets, uvSetIndex) : nullptr;
    return uv ? static_cast<int>(uv->uv_overrides.size()) : 0;
}

int DsonDocument_GetUVOverrideFace(DsonDocumentHandle handle, int uvSetIndex, int overrideIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::UVSet* uv = doc ? At(doc->uv_sets, uvSetIndex) : nullptr;
    const Dson::UVOverride* ov = uv ? At(uv->uv_overrides, overrideIndex) : nullptr;
    return ov ? ov->face : -1;
}

int DsonDocument_GetUVOverrideCorner(DsonDocumentHandle handle, int uvSetIndex, int overrideIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::UVSet* uv = doc ? At(doc->uv_sets, uvSetIndex) : nullptr;
    const Dson::UVOverride* ov = uv ? At(uv->uv_overrides, overrideIndex) : nullptr;
    return ov ? ov->corner : -1;
}

int DsonDocument_GetUVOverrideUVIndex(DsonDocumentHandle handle, int uvSetIndex, int overrideIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::UVSet* uv = doc ? At(doc->uv_sets, uvSetIndex) : nullptr;
    const Dson::UVOverride* ov = uv ? At(uv->uv_overrides, overrideIndex) : nullptr;
    return ov ? ov->uv_index : -1;
}

// ============================================================
// E. Materials
// ============================================================

const char* DsonDocument_GetMaterialName(DsonDocumentHandle handle, int matIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->materials, matIndex) : nullptr;
    return mat ? mat->name.c_str() : "";
}

const char* DsonDocument_GetMaterialGeometryId(DsonDocumentHandle handle, int matIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->materials, matIndex) : nullptr;
    return mat ? mat->geometry.c_str() : "";
}

const char* DsonDocument_GetMaterialUVSetId(DsonDocumentHandle handle, int matIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->materials, matIndex) : nullptr;
    return mat ? mat->uv_set_id.c_str() : "";
}

const char* DsonDocument_GetMaterialType(DsonDocumentHandle handle, int matIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->materials, matIndex) : nullptr;
    return mat ? mat->type.c_str() : "";
}

const char* DsonDocument_GetMaterialShaderType(DsonDocumentHandle handle, int matIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->materials, matIndex) : nullptr;
    return mat ? mat->shader_type.c_str() : "";
}

// ============================================================
// E continued — indexed channel accessors (library materials)
// ============================================================

int DsonDocument_GetMaterialChannelCount(DsonDocumentHandle handle, int matIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->materials, matIndex) : nullptr;
    return mat ? GetMaterialChannelCount(mat->channels) : 0;
}

const char* DsonDocument_GetMaterialChannelId(DsonDocumentHandle handle, int matIndex, int channelIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->materials, matIndex) : nullptr;
    return mat ? GetMaterialChannelId(mat->channels, channelIdx) : "";
}

const char* DsonDocument_GetMaterialChannelType(DsonDocumentHandle handle, int matIndex, int channelIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->materials, matIndex) : nullptr;
    return mat ? GetMaterialChannelType(mat->channels, channelIdx) : "";
}

double DsonDocument_GetMaterialChannelValue(DsonDocumentHandle handle, int matIndex, int channelIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->materials, matIndex) : nullptr;
    return mat ? GetMaterialChannelValue(mat->channels, channelIdx) : 0.0;
}

double DsonDocument_GetMaterialChannelColorR(DsonDocumentHandle handle, int matIndex, int channelIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->materials, matIndex) : nullptr;
    return mat ? GetMaterialChannelColorR(mat->channels, channelIdx) : 0.0;
}

double DsonDocument_GetMaterialChannelColorG(DsonDocumentHandle handle, int matIndex, int channelIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->materials, matIndex) : nullptr;
    return mat ? GetMaterialChannelColorG(mat->channels, channelIdx) : 0.0;
}

double DsonDocument_GetMaterialChannelColorB(DsonDocumentHandle handle, int matIndex, int channelIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->materials, matIndex) : nullptr;
    return mat ? GetMaterialChannelColorB(mat->channels, channelIdx) : 0.0;
}

bool DsonDocument_GetMaterialChannelHasColor(DsonDocumentHandle handle, int matIndex, int channelIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->materials, matIndex) : nullptr;
    return mat ? GetMaterialChannelHasColor(mat->channels, channelIdx) : false;
}

const char* DsonDocument_GetMaterialChannelImageUrl(DsonDocumentHandle handle, int matIndex, int channelIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->materials, matIndex) : nullptr;
    return mat ? GetMaterialChannelImageUrl(mat->channels, channelIdx) : "";
}

const char* DsonDocument_GetMaterialChannelTexturePath(DsonDocumentHandle handle, int matIndex, int channelIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->materials, matIndex) : nullptr;
    return mat ? GetMaterialChannelTexturePath(mat->channels, channelIdx) : "";
}

// ============================================================
// E continued — scene material surface-level accessors
// ============================================================

const char* DsonDocument_GetSceneMaterialName(DsonDocumentHandle handle, int sceneMatIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? mat->name.c_str() : "";
}

const char* DsonDocument_GetSceneMaterialGeometryId(DsonDocumentHandle handle, int sceneMatIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? mat->geometry.c_str() : "";
}

const char* DsonDocument_GetSceneMaterialUVSetId(DsonDocumentHandle handle, int sceneMatIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? mat->uv_set_id.c_str() : "";
}

const char* DsonDocument_GetSceneMaterialType(DsonDocumentHandle handle, int sceneMatIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? mat->type.c_str() : "";
}

const char* DsonDocument_GetSceneMaterialShaderType(DsonDocumentHandle handle, int sceneMatIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? mat->shader_type.c_str() : "";
}

// ============================================================
// E continued — indexed channel accessors (scene materials)
// ============================================================

int DsonDocument_GetSceneMaterialChannelCount(DsonDocumentHandle handle, int sceneMatIndex) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? GetMaterialChannelCount(mat->channels) : 0;
}

const char* DsonDocument_GetSceneMaterialChannelId(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? GetMaterialChannelId(mat->channels, channelIdx) : "";
}

const char* DsonDocument_GetSceneMaterialChannelType(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? GetMaterialChannelType(mat->channels, channelIdx) : "";
}

double DsonDocument_GetSceneMaterialChannelValue(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? GetMaterialChannelValue(mat->channels, channelIdx) : 0.0;
}

double DsonDocument_GetSceneMaterialChannelColorR(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? GetMaterialChannelColorR(mat->channels, channelIdx) : 0.0;
}

double DsonDocument_GetSceneMaterialChannelColorG(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? GetMaterialChannelColorG(mat->channels, channelIdx) : 0.0;
}

double DsonDocument_GetSceneMaterialChannelColorB(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? GetMaterialChannelColorB(mat->channels, channelIdx) : 0.0;
}

bool DsonDocument_GetSceneMaterialChannelHasColor(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? GetMaterialChannelHasColor(mat->channels, channelIdx) : false;
}

const char* DsonDocument_GetSceneMaterialChannelImageUrl(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? GetMaterialChannelImageUrl(mat->channels, channelIdx) : "";
}

const char* DsonDocument_GetSceneMaterialChannelTexturePath(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? GetMaterialChannelTexturePath(mat->channels, channelIdx) : "";
}

int DsonDocument_GetSceneMaterialChannelLayerCount(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? GetMaterialChannelLayerCount(mat->channels, channelIdx) : 0;
}

const char* DsonDocument_GetSceneMaterialChannelLayerTexturePath(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? GetMaterialChannelLayerTexturePath(mat->channels, channelIdx, layerIdx) : "";
}

const char* DsonDocument_GetSceneMaterialChannelLayerLabel(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? GetMaterialChannelLayerLabel(mat->channels, channelIdx, layerIdx) : "";
}

const char* DsonDocument_GetSceneMaterialChannelLayerBlendMode(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? GetMaterialChannelLayerBlendMode(mat->channels, channelIdx, layerIdx) : "";
}
double DsonDocument_GetSceneMaterialChannelLayerOpacity(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? GetMaterialChannelLayerOpacity(mat->channels, channelIdx, layerIdx) : 0.0;
}
bool DsonDocument_GetSceneMaterialChannelLayerActive(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? GetMaterialChannelLayerActive(mat->channels, channelIdx, layerIdx) : false;
}
bool DsonDocument_GetSceneMaterialChannelLayerInvert(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? GetMaterialChannelLayerInvert(mat->channels, channelIdx, layerIdx) : false;
}
double DsonDocument_GetSceneMaterialChannelLayerColorR(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? GetMaterialChannelLayerColorR(mat->channels, channelIdx, layerIdx) : 0.0;
}
double DsonDocument_GetSceneMaterialChannelLayerColorG(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? GetMaterialChannelLayerColorG(mat->channels, channelIdx, layerIdx) : 0.0;
}
double DsonDocument_GetSceneMaterialChannelLayerColorB(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? GetMaterialChannelLayerColorB(mat->channels, channelIdx, layerIdx) : 0.0;
}
double DsonDocument_GetSceneMaterialChannelLayerRotation(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? GetMaterialChannelLayerRotation(mat->channels, channelIdx, layerIdx) : 0.0;
}
double DsonDocument_GetSceneMaterialChannelLayerScaleX(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? GetMaterialChannelLayerScaleX(mat->channels, channelIdx, layerIdx) : 1.0;
}
double DsonDocument_GetSceneMaterialChannelLayerScaleY(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? GetMaterialChannelLayerScaleY(mat->channels, channelIdx, layerIdx) : 1.0;
}
double DsonDocument_GetSceneMaterialChannelLayerOffsetX(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? GetMaterialChannelLayerOffsetX(mat->channels, channelIdx, layerIdx) : 0.0;
}
double DsonDocument_GetSceneMaterialChannelLayerOffsetY(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? GetMaterialChannelLayerOffsetY(mat->channels, channelIdx, layerIdx) : 0.0;
}
bool DsonDocument_GetSceneMaterialChannelLayerMirrorX(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? GetMaterialChannelLayerMirrorX(mat->channels, channelIdx, layerIdx) : false;
}
bool DsonDocument_GetSceneMaterialChannelLayerMirrorY(DsonDocumentHandle handle, int sceneMatIndex, int channelIdx, int layerIdx) {
    Dson::DsonDocument* doc = Doc(handle);
    const Dson::Material* mat = doc ? At(doc->scene.materials, sceneMatIndex) : nullptr;
    return mat ? GetMaterialChannelLayerMirrorY(mat->channels, channelIdx, layerIdx) : false;
}

// ============================================================
// F. Morph Targets
// ============================================================

// Morph APIs operate on the filtered morph list built by EnsureMorphCache.
// Returned deltas are sparse: each delta has a source vertex index plus XYZ
// offset, so consumers should apply them only to listed vertices.
int DsonDocument_GetMorphCount(DsonDocumentHandle handle) {
    if (!handle) return 0;
    DsonContext* ctx = GetContext(handle);
    EnsureMorphCache(ctx);
    return static_cast<int>(ctx->morphIndexCache.size());
}

const char* DsonDocument_GetMorphName(DsonDocumentHandle handle, int morphIndex) {
    if (!handle) return "";
    DsonContext* ctx = GetContext(handle);
    const Dson::Modifier* mod = GetMorphByFilteredIndex(ctx, morphIndex);
    if (!mod) return "";
    return mod->name.c_str();
}

const char* DsonDocument_GetMorphId(DsonDocumentHandle handle, int morphIndex) {
    if (!handle) return "";
    DsonContext* ctx = GetContext(handle);
    const Dson::Modifier* mod = GetMorphByFilteredIndex(ctx, morphIndex);
    return mod ? mod->id.c_str() : "";
}

const char* DsonDocument_GetMorphLabel(DsonDocumentHandle handle, int morphIndex) {
    if (!handle) return "";
    DsonContext* ctx = GetContext(handle);
    const Dson::Modifier* mod = GetMorphByFilteredIndex(ctx, morphIndex);
    if (!mod) return "";
    if (!mod->channel_label.empty()) return mod->channel_label.c_str();
    return mod->name.c_str();
}

int DsonDocument_GetMorphDeltaCount(DsonDocumentHandle handle, int morphIndex) {
    if (!handle) return 0;
    DsonContext* ctx = GetContext(handle);
    const Dson::Modifier* mod = GetMorphByFilteredIndex(ctx, morphIndex);
    if (!mod) return 0;
    return GetIndexedVector3Count(mod->morph_deltas);
}

int DsonDocument_GetMorphDeltaVertexIndex(DsonDocumentHandle handle, int morphIndex, int deltaIndex) {
    if (!handle) return -1;
    DsonContext* ctx = GetContext(handle);
    const Dson::Modifier* mod = GetMorphByFilteredIndex(ctx, morphIndex);
    if (!mod) return -1;
    return GetIndexedVector3Index(mod->morph_deltas, deltaIndex);
}

double DsonDocument_GetMorphDeltaX(DsonDocumentHandle handle, int morphIndex, int deltaIndex) {
    if (!handle) return 0.0;
    DsonContext* ctx = GetContext(handle);
    const Dson::Modifier* mod = GetMorphByFilteredIndex(ctx, morphIndex);
    if (!mod) return 0.0;
    return GetIndexedVector3Component(mod->morph_deltas, deltaIndex, 0);
}

double DsonDocument_GetMorphDeltaY(DsonDocumentHandle handle, int morphIndex, int deltaIndex) {
    if (!handle) return 0.0;
    DsonContext* ctx = GetContext(handle);
    const Dson::Modifier* mod = GetMorphByFilteredIndex(ctx, morphIndex);
    if (!mod) return 0.0;
    return GetIndexedVector3Component(mod->morph_deltas, deltaIndex, 1);
}

double DsonDocument_GetMorphDeltaZ(DsonDocumentHandle handle, int morphIndex, int deltaIndex) {
    if (!handle) return 0.0;
    DsonContext* ctx = GetContext(handle);
    const Dson::Modifier* mod = GetMorphByFilteredIndex(ctx, morphIndex);
    if (!mod) return 0.0;
    return GetIndexedVector3Component(mod->morph_deltas, deltaIndex, 2);
}

int DsonDocument_GetMorphNormalDeltaCount(DsonDocumentHandle handle, int morphIndex) {
    if (!handle) return 0;
    DsonContext* ctx = GetContext(handle);
    const Dson::Modifier* mod = GetMorphByFilteredIndex(ctx, morphIndex);
    if (!mod) return 0;
    return GetIndexedVector3Count(mod->normal_deltas);
}

int DsonDocument_GetMorphNormalDeltaVertexIndex(DsonDocumentHandle handle, int morphIndex, int deltaIndex) {
    if (!handle) return -1;
    DsonContext* ctx = GetContext(handle);
    const Dson::Modifier* mod = GetMorphByFilteredIndex(ctx, morphIndex);
    if (!mod) return -1;
    return GetIndexedVector3Index(mod->normal_deltas, deltaIndex);
}

double DsonDocument_GetMorphNormalDeltaX(DsonDocumentHandle handle, int morphIndex, int deltaIndex) {
    if (!handle) return 0.0;
    DsonContext* ctx = GetContext(handle);
    const Dson::Modifier* mod = GetMorphByFilteredIndex(ctx, morphIndex);
    if (!mod) return 0.0;
    return GetIndexedVector3Component(mod->normal_deltas, deltaIndex, 0);
}

double DsonDocument_GetMorphNormalDeltaY(DsonDocumentHandle handle, int morphIndex, int deltaIndex) {
    if (!handle) return 0.0;
    DsonContext* ctx = GetContext(handle);
    const Dson::Modifier* mod = GetMorphByFilteredIndex(ctx, morphIndex);
    if (!mod) return 0.0;
    return GetIndexedVector3Component(mod->normal_deltas, deltaIndex, 1);
}

double DsonDocument_GetMorphNormalDeltaZ(DsonDocumentHandle handle, int morphIndex, int deltaIndex) {
    if (!handle) return 0.0;
    DsonContext* ctx = GetContext(handle);
    const Dson::Modifier* mod = GetMorphByFilteredIndex(ctx, morphIndex);
    if (!mod) return 0.0;
    return GetIndexedVector3Component(mod->normal_deltas, deltaIndex, 2);
}

const char* DsonDocument_GetMorphGeometryId(DsonDocumentHandle handle, int morphIndex) {
    if (!handle) return "";
    DsonContext* ctx = GetContext(handle);
    const Dson::Modifier* mod = GetMorphByFilteredIndex(ctx, morphIndex);
    if (!mod) return "";
    const std::string& parent = mod->parent.value;
    size_t pos = parent.rfind('#');
    if (pos == std::string::npos) return "";
    ctx->lastMorphGeometryId = parent.substr(pos + 1);
    return ctx->lastMorphGeometryId.c_str();
}

const char* DsonParser_GetLastError() {
    return LastErrorSlot().c_str();
}

const char* DsonParser_GetVersion(void) {
    // Static string literal: always valid, parser-owned, and cannot fail (no handle).
    return DSONPARSER_VERSION_STRING;
}
