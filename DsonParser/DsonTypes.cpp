#include "pch.h"
#include "DsonTypes.h"
#include "DsonHelpers.h"
#include "DsonInflate.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <sstream>

// Parser orientation:
// This file converts a RapidJSON document into the typed Dson::* model declared
// in DsonTypes.h. Each ParseFromJson method owns one DSON object shape and is
// intentionally permissive: optional or malformed subfields usually keep their
// default values, while unrecognized keys are recorded for audit diagnostics.
//
// Main flow:
// - DsonDocument::LoadFromFile/LoadFromString/LoadFromBuffer parses JSON syntax
//   with RapidJSON, transparently inflating gzip-wrapped input first.
// - DsonDocument::ParseFromJson dispatches top-level DSON sections:
//   asset_info, scene, node_library, geometry_library, material_library,
//   modifier_library, image_library, and uv_set_library.
// - Section structs parse only the fields needed by the public C API and UE5
//   import pipeline: geometry and scene-node shell material-UV assignments,
//   geometry graft/rigidity blocks, skeleton
//   nodes, skin weights, UVs, materials, images, morph deltas, formula payloads,
//   and scene instances.
// - A post-parse pass resolves material channel image references to image
//   texture paths and, for identity-linked LIE images, their per-layer paths.
//   Broader cross-file asset resolution is outside this parser.
//
// Deliberately not handled here:
// - Formula evaluation / driven morph chains.
// - Loading referenced external DSF/DUF assets.
// - Semantic validation beyond basic JSON shape checks and unknown-key tracking.

namespace Dson {

// Parse a single object member into a Dson value wrapper, if the key is present.
// Uses one FindMember lookup (vs. HasMember + operator[]), and mirrors the prior
// behavior: a missing key leaves `out` untouched, a present key is parsed (its
// bool result is ignored, exactly as the original call sites did).
template <class T>
static void ParseMember(const rapidjson::Value& obj, const char* key, T& out) {
    auto it = obj.FindMember(key);
    if (it != obj.MemberEnd()) {
        out.ParseFromJson(it->value);
    }
}

// Parse an array-of-objects member (a DSON library or scene instance array) into
// a vector, appending each element that parses successfully. Element type T must
// expose ParseFromJson(const rapidjson::Value&, std::set<std::string>*). Mirrors
// the prior hand-written reserve/loop/push blocks exactly, including skipping
// elements whose ParseFromJson returns false.
template <class T>
static void ParseObjectArray(const rapidjson::Value& container, const char* key,
                             std::vector<T>& out, std::set<std::string>* unknownKeys) {
    const rapidjson::Value* arr = nullptr;
    if (!JsonHelper::GetArray(container, key, arr)) {
        return;
    }
    out.reserve(out.size() + arr->Size());
    for (rapidjson::SizeType i = 0; i < arr->Size(); i++) {
        T item;
        if (item.ParseFromJson((*arr)[i], unknownKeys)) {
            out.push_back(item);
        }
    }
}

// Helper to track unknown keys
static void TrackUnknownKeys(const rapidjson::Value& obj, const std::set<std::string>& knownKeys, std::set<std::string>* unknownKeys) {
    if (!unknownKeys || !obj.IsObject()) {
        return;
    }

    for (auto it = obj.MemberBegin(); it != obj.MemberEnd(); ++it) {
        std::string key = it->name.GetString();
        if (knownKeys.find(key) == knownKeys.end()) {
            unknownKeys->insert(key);
        }
    }
}

// Record a channel value that is present but of a type GetNumberOrBool cannot
// represent (not a number, not a bool). Inserts a decorated, self-describing
// entry into the existing per-context audit trail so the drop is visible.
// The entry is clearly distinguishable from a genuine unknown key by its form:
//   "<valueKey> [channel "<id>": type=<type>, used default]"
static void TrackChannelTypeMismatch(std::set<std::string>* unknownKeys,
                                     const std::string& channelId,
                                     const char* valueKey,
                                     const rapidjson::Value& v) {
    if (!unknownKeys) return;
    unknownKeys->insert(std::string(valueKey) + " [channel \"" + channelId +
        "\": type=" + JsonHelper::JsonTypeName(v) + ", used default]");
}

static int HexValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

static std::string PercentDecode(const std::string& text) {
    std::string decoded;
    decoded.reserve(text.size());

    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '%' && i + 2 < text.size()) {
            const int hi = HexValue(text[i + 1]);
            const int lo = HexValue(text[i + 2]);
            if (hi >= 0 && lo >= 0) {
                decoded.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        decoded.push_back(text[i]);
    }

    return decoded;
}

static bool GetImageMapPath(const rapidjson::Value& mapVal, std::string& path) {
    if (mapVal.IsString()) {
        path = mapVal.GetString();
        return true;
    }

    if (mapVal.IsObject()) {
        if (!JsonHelper::GetString(mapVal, "url", path)) {
            JsonHelper::GetString(mapVal, "file", path);
        }
        return !path.empty();
    }

    if (mapVal.IsArray() && mapVal.Size() > 0) {
        return GetImageMapPath(mapVal[0], path);
    }

    return false;
}

// Read a transform array that is either plain [x,y,z] numbers or an array of
// channel objects [{id:"x",value:..}, ...] (the DSF node format) into a Vector3.
// When requested, retain which final components were authored numerically;
// explicit zero is present, while a missing or non-numeric value is absent.
// When requested, also retain object-channel metadata in source order. Plain
// numeric arrays author no channels.
static void ParseTransformVector3(const rapidjson::Value& arr, Vector3& out,
                                  unsigned int* presenceMask = nullptr,
                                  std::vector<NodeTransformChannel>* outChannels = nullptr) {
    if (!arr.IsArray()) {
        return;
    }
    if (presenceMask) {
        *presenceMask = 0;
    }
    if (outChannels) {
        outChannels->reserve(outChannels->size() + arr.Size());
    }
    for (rapidjson::SizeType i = 0; i < arr.Size(); i++) {
        const rapidjson::Value& el = arr[i];
        double val = 0.0;
        bool hasNumericValue = false;
        int component = -1;
        std::string id;
        if (el.IsObject()) {
            const char* valKey = el.HasMember("current_value") ? "current_value" : "value";
            val = JsonHelper::GetDoubleOrDefault(el, valKey, 0.0);
            auto valIt = el.FindMember(valKey);
            hasNumericValue = valIt != el.MemberEnd() && valIt->value.IsNumber();
            id = JsonHelper::GetStringOrDefault(el, "id");
            if (id == "x") component = 0;
            else if (id == "y") component = 1;
            else if (id == "z") component = 2;

            if (outChannels) {
                NodeTransformChannel channel;
                channel.id = id;
                channel.label = JsonHelper::GetStringOrDefault(el, "label");

                auto minIt = el.FindMember("min");
                if (minIt != el.MemberEnd() && minIt->value.IsNumber()) {
                    channel.min = minIt->value.GetDouble();
                    channel.field_presence |= 0x1u;
                }

                auto maxIt = el.FindMember("max");
                if (maxIt != el.MemberEnd() && maxIt->value.IsNumber()) {
                    channel.max = maxIt->value.GetDouble();
                    channel.field_presence |= 0x2u;
                }

                auto clampedIt = el.FindMember("clamped");
                if (clampedIt != el.MemberEnd() && clampedIt->value.IsBool()) {
                    channel.clamped = clampedIt->value.GetBool();
                    channel.field_presence |= 0x4u;
                }

                outChannels->push_back(channel);
            }
        } else if (el.IsNumber()) {
            val = el.GetDouble();
            hasNumericValue = true;
        } else {
            continue;
        }
        // Positional fallback (plain [x,y,z] or unlabeled channels)
        if (component < 0 && i < 3) component = static_cast<int>(i);
        if (component == 0) out.x = val;
        else if (component == 1) out.y = val;
        else if (component == 2) out.z = val;
        else continue;

        if (presenceMask) {
            const unsigned int bit = 1u << component;
            *presenceMask &= ~bit;
            if (hasNumericValue) {
                *presenceMask |= bit;
            }
        }
    }
}

// Resolve a member that DSON encodes as either a plain array or a
// { count, values:[...] } wrapper object down to its values array. Returns
// nullptr if the key is absent or in neither shape. Callers that also need the
// wrapper's "count" field (vertices, polylist) read it separately.
static const rapidjson::Value* GetValuesArray(const rapidjson::Value& container, const char* key) {
    auto it = container.FindMember(key);
    if (it == container.MemberEnd()) {
        return nullptr;
    }
    const rapidjson::Value& v = it->value;
    if (v.IsArray()) {
        return &v;
    }
    if (v.IsObject()) {
        auto vit = v.FindMember("values");
        if (vit != v.MemberEnd() && vit->value.IsArray()) {
            return &vit->value;
        }
    }
    return nullptr;
}

// Read a { count, values:[...strings] } object (or a plain string array) into out.
static void ParseStringValuedArray(const rapidjson::Value& container, const char* key, std::vector<std::string>& out) {
    const rapidjson::Value* values = GetValuesArray(container, key);
    if (!values) {
        return;
    }
    out.reserve(values->Size());
    for (rapidjson::SizeType i = 0; i < values->Size(); i++) {
        if ((*values)[i].IsString()) {
            out.push_back((*values)[i].GetString());
        }
    }
}

// Append valid [material-group-name, uv-set-name] rows from an array or
// {count,values} member. Declared counts and later row elements are ignored.
static void ParseMaterialUVAssignments(
    const rapidjson::Value& container,
    const char* key,
    std::vector<MaterialUVAssignment>& out) {
    const rapidjson::Value* assignments = GetValuesArray(container, key);
    if (!assignments) {
        return;
    }
    out.reserve(out.size() + assignments->Size());
    for (rapidjson::SizeType i = 0; i < assignments->Size(); i++) {
        const rapidjson::Value& row = (*assignments)[i];
        if (row.IsArray() && row.Size() >= 2 && row[0].IsString() && row[1].IsString()) {
            MaterialUVAssignment assignment;
            assignment.material_group = row[0].GetString();
            assignment.uv_set_name = row[1].GetString();
            out.push_back(assignment);
        }
    }
}

// Read a { count, values:[...ints] } object (or a plain int array) into out.
// Malformed elements are skipped, consistent with the parser's permissive
// array handling.
static void ParseIntValuedArray(const rapidjson::Value& container, const char* key, std::vector<int>& out) {
    const rapidjson::Value* values = GetValuesArray(container, key);
    if (!values) {
        return;
    }
    out.reserve(values->Size());
    for (rapidjson::SizeType i = 0; i < values->Size(); i++) {
        if ((*values)[i].IsInt()) {
            out.push_back((*values)[i].GetInt());
        }
    }
}

// JsonHelper implementations moved to DsonHelpers.cpp.

// AssetInfo implementation
bool AssetInfo::ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys) {
    if (!json.IsObject()) {
        return false;
    }
    
    static const std::set<std::string> knownKeys = {
        "id", "type", "contributor", "revision", "modified", "unit_scale"
    };
    
    ParseMember(json, "id", id);
    ParseMember(json, "type", type);

    if (JsonHelper::HasMember(json, "contributor")) {
        const rapidjson::Value* contributor = nullptr;
        if (JsonHelper::GetObject(json, "contributor", contributor)) {
            contributor_name.value = JsonHelper::GetStringOrDefault(*contributor, "author");
            contributor_email.value = JsonHelper::GetStringOrDefault(*contributor, "email");
        }
    }

    ParseMember(json, "revision", revision);
    ParseMember(json, "modified", modified);

    unit_scale = JsonHelper::GetDoubleOrDefault(json, "unit_scale", 1.0);

    TrackUnknownKeys(json, knownKeys, unknownKeys);
    return true;
}

// Node implementation
bool Node::ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys) {
    if (!json.IsObject()) {
        return false;
    }
    
    static const std::set<std::string> knownKeys = {
        "id", "name", "label", "type", "parent", "conform_target", "url", "translation", "rotation", "scale",
        "general_scale", "center_point", "end_point", "orientation", "rotation_order",
        "inherits_scale", "geometries", "presentation", "preview", "extra"
    };

    ParseMember(json, "id", id);
    ParseMember(json, "name", name);
    ParseMember(json, "label", label);
    ParseMember(json, "type", type);
    ParseMember(json, "parent", parent);
    ParseMember(json, "conform_target", conform_target);
    ParseMember(json, "url", url);

    const rapidjson::Value* transArray = nullptr;
    if (JsonHelper::GetArray(json, "translation", transArray)) {
        ParseTransformVector3(*transArray, translation, &translation_presence, &translation_channels);
    }

    const rapidjson::Value* rotArray = nullptr;
    if (JsonHelper::GetArray(json, "rotation", rotArray)) {
        ParseTransformVector3(*rotArray, rotation, &rotation_presence, &rotation_channels);
    }

    scale.x = scale.y = scale.z = 1.0; // default to identity scale
    const rapidjson::Value* scaleArray = nullptr;
    if (JsonHelper::GetArray(json, "scale", scaleArray)) {
        ParseTransformVector3(*scaleArray, scale, &scale_presence, &scale_channels);
    }

    const rapidjson::Value* gsObj = nullptr;
    if (JsonHelper::GetObject(json, "general_scale", gsObj)) {
        general_scale = JsonHelper::GetDoubleOrDefault(*gsObj, "value", 1.0);
        auto valueIt = gsObj->FindMember("value");
        has_general_scale = valueIt != gsObj->MemberEnd() && valueIt->value.IsNumber();
    }

    const rapidjson::Value* centerArray = nullptr;
    if (JsonHelper::GetArray(json, "center_point", centerArray)) {
        ParseTransformVector3(*centerArray, center_point, &center_point_presence);
    }

    const rapidjson::Value* endArray = nullptr;
    if (JsonHelper::GetArray(json, "end_point", endArray)) {
        ParseTransformVector3(*endArray, end_point);
    }

    const rapidjson::Value* orientArray = nullptr;
    if (JsonHelper::GetArray(json, "orientation", orientArray)) {
        ParseTransformVector3(*orientArray, orientation, &orientation_presence);
    }

    auto inheritsScaleIt = json.FindMember("inherits_scale");
    if (inheritsScaleIt != json.MemberEnd() && inheritsScaleIt->value.IsBool()) {
        inherits_scale = inheritsScaleIt->value.GetBool();
        has_inherits_scale = true;
    }

    has_rotation_order = JsonHelper::GetString(json, "rotation_order", rotation_order);

    const rapidjson::Value* geomsArray = nullptr;
    if (JsonHelper::GetArray(json, "geometries", geomsArray)) {
        geometries.reserve(geomsArray->Size());
        for (rapidjson::SizeType i = 0; i < geomsArray->Size(); i++) {
            const auto& el = (*geomsArray)[i];
            if (!el.IsObject()) continue;
            NodeGeometryRef ref;
            JsonHelper::GetString(el, "id", ref.id);
            JsonHelper::GetString(el, "url", ref.url);
            geometries.push_back(ref);
        }
    }

    const rapidjson::Value* presObj = nullptr;
    if (JsonHelper::GetObject(json, "presentation", presObj)) {
        presentation_type             = JsonHelper::GetStringOrDefault(*presObj, "type");
        presentation_label            = JsonHelper::GetStringOrDefault(*presObj, "label");
        presentation_preferred_base   = JsonHelper::GetStringOrDefault(*presObj, "preferred_base");
    }

    // Walk node extras once for the typed payloads exposed by the model.
    // Rigid-follow data is raw and unevaluated. Shell material_uvs rows are
    // appended across every exact studio/node/shell entry in authored order.
    // Neither path resolves references or merges sections (R6.3/R6.4).
    const rapidjson::Value* rfExtra = nullptr;
    if (JsonHelper::GetArray(json, "extra", rfExtra)) {
        for (rapidjson::SizeType i = 0; i < rfExtra->Size(); i++) {
            const auto& item = (*rfExtra)[i];
            if (!item.IsObject()) continue;
            const std::string extraType = JsonHelper::GetStringOrDefault(item, "type");
            if (extraType == "studio/node/shell") {
                ParseMaterialUVAssignments(item, "material_uvs", shell_material_uv_assignments);
                continue;
            }
            if (extraType != "studio/node/rigid_follow" || has_rigid_follow) continue;

            const rapidjson::Value* group = nullptr;
            if (!JsonHelper::GetObject(item, "rigidity_group", group)) continue;

            has_rigid_follow = true;
            rigid_follow_rotation_mode = JsonHelper::GetStringOrDefault(*group, "rotation_mode");

            const rapidjson::Value* modes = nullptr;
            if (JsonHelper::GetArray(*group, "scale_modes", modes)) {
                rigid_follow_scale_modes.reserve(modes->Size());
                for (rapidjson::SizeType s = 0; s < modes->Size(); s++) {
                    if ((*modes)[s].IsString())
                        rigid_follow_scale_modes.push_back((*modes)[s].GetString());
                }
            }

            const rapidjson::Value* refv = nullptr;
            if (JsonHelper::GetObject(*group, "reference_vertices", refv)) {
                const rapidjson::Value* values = nullptr;
                if (JsonHelper::GetArray(*refv, "values", values)) {
                    rigid_follow_reference_vertices.reserve(values->Size());
                    for (rapidjson::SizeType v = 0; v < values->Size(); v++) {
                        if ((*values)[v].IsInt())
                            rigid_follow_reference_vertices.push_back((*values)[v].GetInt());
                    }
                }
            }
        }
    }

    TrackUnknownKeys(json, knownKeys, unknownKeys);
    return true;
}

bool GeometryRigidityGroup::ParseFromJson(const rapidjson::Value& json, std::set<std::string>*) {
    if (!json.IsObject()) {
        return false;
    }

    id = JsonHelper::GetStringOrDefault(json, "id");
    rotation_mode = JsonHelper::GetStringOrDefault(json, "rotation_mode");
    ParseStringValuedArray(json, "scale_modes", scale_modes);
    ParseIntValuedArray(json, "reference_vertices", reference_vertices);
    ParseIntValuedArray(json, "mask_vertices", mask_vertices);
    reference = JsonHelper::GetStringOrDefault(json, "reference");
    ParseStringValuedArray(json, "transform_nodes", transform_nodes);
    // DAZ authors this key with the missing 's'; preserve that exact schema.
    use_transform_bones_for_scale =
        JsonHelper::GetBoolOrDefault(json, "use_tranform_bones_for_scale", false);
    return true;
}

static bool ParseGeometryChannel(const rapidjson::Value& wrapper, GeometryChannel& out) {
    if (!wrapper.IsObject()) {
        return false;
    }

    const rapidjson::Value* channelObj = nullptr;
    if (!JsonHelper::GetObject(wrapper, "channel", channelObj)) {
        return false;
    }

    out.group = JsonHelper::GetStringOrDefault(wrapper, "group");
    out.id = JsonHelper::GetStringOrDefault(*channelObj, "id");
    out.type = JsonHelper::GetStringOrDefault(*channelObj, "type");
    out.label = JsonHelper::GetStringOrDefault(*channelObj, "label");

    auto valueIt = channelObj->FindMember("value");
    if (valueIt != channelObj->MemberEnd() && valueIt->value.IsNumber()) {
        out.value = valueIt->value.GetDouble();
        out.field_presence |= 0x10u;
    }

    auto minIt = channelObj->FindMember("min");
    if (minIt != channelObj->MemberEnd() && minIt->value.IsNumber()) {
        out.min = minIt->value.GetDouble();
        out.field_presence |= 0x1u;
    }

    auto maxIt = channelObj->FindMember("max");
    if (maxIt != channelObj->MemberEnd() && maxIt->value.IsNumber()) {
        out.max = maxIt->value.GetDouble();
        out.field_presence |= 0x2u;
    }

    auto clampedIt = channelObj->FindMember("clamped");
    if (clampedIt != channelObj->MemberEnd() && clampedIt->value.IsBool()) {
        out.clamped = clampedIt->value.GetBool();
        out.field_presence |= 0x4u;
    }

    auto stepIt = channelObj->FindMember("step_size");
    if (stepIt != channelObj->MemberEnd() && stepIt->value.IsNumber()) {
        out.step_size = stepIt->value.GetDouble();
        out.field_presence |= 0x8u;
    }

    const rapidjson::Value* enumValues = nullptr;
    if (JsonHelper::GetArray(*channelObj, "enum_values", enumValues)) {
        out.enum_values.reserve(enumValues->Size());
        for (rapidjson::SizeType i = 0; i < enumValues->Size(); i++) {
            if ((*enumValues)[i].IsString()) {
                out.enum_values.push_back((*enumValues)[i].GetString());
            }
        }
    }

    return true;
}

// Geometry parser:
// Captures mesh topology exactly enough for downstream importers to rebuild the
// surface: vertex positions, face lists, polygon groups, material groups, and
// the geometry's default UV-set reference and authored per-material UV-set
// names. Also retains the geometry's subdivision declaration: the declared type,
// sibling edge/normal subdivision strings, and every extra[].studio_geometry_channels
// "/General/Mesh Resolution" channel in source order. DSON commonly wraps arrays
// as {count, values:[...]}; legacy flat arrays are accepted where practical.
// Polylist faces are kept flattened, with a per-face offset table, because DSON
// faces may vary in length and include leading group/material indices.
bool Geometry::ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys) {
    if (!json.IsObject()) {
        return false;
    }
    
    static const std::set<std::string> knownKeys = {
        "id", "name", "type", "url", "vertices", "polygons", "polylist",
        "vertex_count", "polygon_count", "edge_interpolation_mode",
        "subd_normal_smoothing_mode", "default_uv_set", "polygon_groups",
        "polygon_material_groups", "material_uvs", "extra", "graft", "rigidity"
    };
    
    ParseMember(json, "id", id);
    ParseMember(json, "name", name);
    ParseMember(json, "type", type);
    ParseMember(json, "url", url);
    ParseMember(json, "vertex_count", vertex_count);
    ParseMember(json, "polygon_count", polygon_count);

    // Vertices: a flat number array (legacy) or a { count, values:[[x,y,z],...] } object
    if (JsonHelper::HasMember(json, "vertices")) {
        const rapidjson::Value& v = json["vertices"];
        if (v.IsObject()) {
            if (v.HasMember("count") && v["count"].IsInt()) {
                vertex_count.value = v["count"].GetInt();
            }
            const rapidjson::Value* values = nullptr;
            if (JsonHelper::GetArray(v, "values", values)) {
                vertices.values.reserve(values->Size() * 3);
                for (rapidjson::SizeType i = 0; i < values->Size(); i++) {
                    const auto& p = (*values)[i];
                    if (p.IsArray() && p.Size() >= 3 &&
                        p[0].IsNumber() && p[1].IsNumber() && p[2].IsNumber()) {
                        vertices.values.push_back(p[0].GetDouble());
                        vertices.values.push_back(p[1].GetDouble());
                        vertices.values.push_back(p[2].GetDouble());
                    }
                }
                if (vertex_count.value == 0) {
                    vertex_count.value = static_cast<int>(values->Size());
                }
            }
        } else if (v.IsArray()) {
            vertices.ParseFromJson(v);
        }
    }

    if (const rapidjson::Value* polyArray = GetValuesArray(json, "polygons")) {
        polygons.ParseFromJson(*polyArray);
    }

    // Polylist: an array of faces (legacy) or a { count, values:[[g,m,v0,v1,...],...] } object.
    // Faces are flattened, including the leading group/material indices of each face.
    if (JsonHelper::HasMember(json, "polylist")) {
        const rapidjson::Value& pl = json["polylist"];
        const rapidjson::Value* faces = nullptr;
        if (pl.IsObject()) {
            if (pl.HasMember("count") && pl["count"].IsInt()) {
                polygon_count.value = pl["count"].GetInt();
            }
            JsonHelper::GetArray(pl, "values", faces);
        } else if (pl.IsArray()) {
            faces = &pl;
        }
        if (faces) {
            polylist.values.reserve(faces->Size() * 6);
            polylist_face_offsets.reserve(faces->Size());
            for (rapidjson::SizeType i = 0; i < faces->Size(); i++) {
                if ((*faces)[i].IsArray()) {
                    const auto& face = (*faces)[i];
                    polylist_face_offsets.push_back(static_cast<int>(polylist.values.size()));
                    for (rapidjson::SizeType j = 0; j < face.Size(); j++) {
                        if (face[j].IsInt()) {
                            polylist.values.push_back(face[j].GetInt());
                        }
                    }
                }
            }
            if (polygon_count.value == 0) {
                polygon_count.value = static_cast<int>(faces->Size());
            }
        }
    }

    ParseStringValuedArray(json, "polygon_groups", polygon_groups);
    ParseStringValuedArray(json, "polygon_material_groups", polygon_material_groups);

    JsonHelper::GetString(json, "default_uv_set", default_uv_set_id);
    edge_interpolation_mode =
        JsonHelper::GetStringOrDefault(json, "edge_interpolation_mode");
    subd_normal_smoothing_mode =
        JsonHelper::GetStringOrDefault(json, "subd_normal_smoothing_mode");

    // Authored per-surface UV selection, retained byte-for-byte in valid-row
    // source order. The wrapper's declared count is not authoritative.
    ParseMaterialUVAssignments(json, "material_uvs", material_uv_assignments);

    // Geometry extra[] may contain multiple payload families. Only expose the
    // authored studio_geometry_channels blocks here; material_selection_sets and
    // other extras are recognized only as "extra" and remain out of scope.
    const rapidjson::Value* extraArr = nullptr;
    if (JsonHelper::GetArray(json, "extra", extraArr)) {
        for (rapidjson::SizeType i = 0; i < extraArr->Size(); i++) {
            const rapidjson::Value& extraItem = (*extraArr)[i];
            if (!extraItem.IsObject()) continue;
            if (JsonHelper::GetStringOrDefault(extraItem, "type") != "studio_geometry_channels") continue;

            const rapidjson::Value* channelArr = nullptr;
            if (!JsonHelper::GetArray(extraItem, "channels", channelArr)) continue;

            channels.reserve(channels.size() + channelArr->Size());
            for (rapidjson::SizeType j = 0; j < channelArr->Size(); j++) {
                GeometryChannel channel;
                if (ParseGeometryChannel((*channelArr)[j], channel)) {
                    channels.push_back(channel);
                }
            }
        }
    }

    // Geograft weld correspondence. A populated graft (non-empty vertex_pairs)
    // marks a geograft; an empty "graft": {} (base figures, G9 eyes/eyelashes)
    // leaves is_graft false and the arrays empty. Retain the raw weld arrays
    // faithfully (R6.4) in the file's own DSON index space — no remap/weld
    // (the importer owns that): vertex_pairs[i] = [graft-local, base-figure];
    // hidden_polys = base-figure poly indices. The values array is
    // authoritative for the pair count — DAZ's declared vertex_pairs "count"
    // can disagree (Genesis9FemaleGenitalia declares 84, ships 82), matching
    // is_graft's existing size check.
    const rapidjson::Value* graftObj = nullptr;
    if (JsonHelper::GetObject(json, "graft", graftObj)) {
        if (const rapidjson::Value* vp = GetValuesArray(*graftObj, "vertex_pairs")) {
            is_graft = vp->Size() > 0;
            graft_vertex_pairs.reserve(vp->Size());
            for (rapidjson::SizeType i = 0; i < vp->Size(); i++) {
                const auto& pair = (*vp)[i];
                if (pair.IsArray() && pair.Size() >= 2 &&
                    pair[0].IsInt() && pair[1].IsInt()) {
                    GraftVertexPair gp;
                    gp.graft_vertex = pair[0].GetInt();
                    gp.base_vertex  = pair[1].GetInt();
                    graft_vertex_pairs.push_back(gp);
                }
            }
        }
        if (const rapidjson::Value* hp = GetValuesArray(*graftObj, "hidden_polys")) {
            graft_hidden_polys.reserve(hp->Size());
            for (rapidjson::SizeType i = 0; i < hp->Size(); i++) {
                if ((*hp)[i].IsInt()) {
                    graft_hidden_polys.push_back((*hp)[i].GetInt());
                }
            }
        }
        ParseMember(*graftObj, "vertex_count", graft_base_vertex_count);
        ParseMember(*graftObj, "poly_count",   graft_base_poly_count);
    }

    // Authored geometry rigidity, retained in this geometry's raw index space.
    // Object presence is independent of whether weights/groups contain rows.
    const rapidjson::Value* rigidityObj = nullptr;
    if (JsonHelper::GetObject(json, "rigidity", rigidityObj)) {
        has_rigidity = true;
        if (const rapidjson::Value* weights = GetValuesArray(*rigidityObj, "weights")) {
            rigidity_weights.reserve(weights->Size());
            for (rapidjson::SizeType i = 0; i < weights->Size(); i++) {
                const rapidjson::Value& row = (*weights)[i];
                if (row.IsArray() && row.Size() >= 2 && row[0].IsInt() && row[1].IsNumber()) {
                    GeometryRigidityWeight parsed;
                    parsed.vertex_index = row[0].GetInt();
                    parsed.weight = row[1].GetDouble();
                    rigidity_weights.push_back(parsed);
                }
            }
        }
        ParseObjectArray(*rigidityObj, "groups", rigidity_groups, unknownKeys);
    }

    TrackUnknownKeys(json, knownKeys, unknownKeys);
    return true;
}

// Parse one material channel container object (has "channel" sub-object with value + image).
// Works for both the top-level "diffuse" key and entries inside extra.studio_material_channels.channels[].
static MaterialChannel ParseMaterialChannel(const rapidjson::Value& container, std::set<std::string>* unknownKeys) {
    MaterialChannel result;
    if (!container.IsObject()) return result;

    const rapidjson::Value* ch = nullptr;
    if (!JsonHelper::GetObject(container, "channel", ch)) return result;

    result.type = JsonHelper::GetStringOrDefault(*ch, "type");

    // Prefer current_value (scene override); fall back to value (library default).
    // Value: [r,g,b] array → color channel; single number/bool → scalar channel.
    const char* valueKeyName = nullptr;
    const rapidjson::Value* valSrc = nullptr;
    if (ch->HasMember("current_value")) {
        valueKeyName = "current_value";
        valSrc = &(*ch)["current_value"];
    } else if (ch->HasMember("value")) {
        valueKeyName = "value";
        valSrc = &(*ch)["value"];
    }

    if (valSrc) {
        if (valSrc->IsArray() && valSrc->Size() >= 3 &&
            (*valSrc)[0].IsNumber() && (*valSrc)[1].IsNumber() && (*valSrc)[2].IsNumber()) {
            result.color.x = (*valSrc)[0].GetDouble();
            result.color.y = (*valSrc)[1].GetDouble();
            result.color.z = (*valSrc)[2].GetDouble();
            result.has_color = true;
        } else if (!JsonHelper::GetNumberOrBool(*valSrc, result.value)) {
            TrackChannelTypeMismatch(unknownKeys, JsonHelper::GetStringOrDefault(*ch, "id"),
                                     valueKeyName, *valSrc);
        }
    }

    // DAZ stores per-channel texture refs under "image_file"; "image" is a legacy fallback.
    if (!JsonHelper::GetString(*ch, "image_file", result.image_url)) {
        JsonHelper::GetString(*ch, "image", result.image_url);
    }

    return result;
}

// Material parser:
// Preserves source-order material channels rather than mapping them to a fixed
// engine shader model here. DAZ material data can appear as a top-level diffuse
// block and/or inside extra[].studio_material_channels; both are normalized into
// Material::channels as pairs of DAZ channel id plus parsed scalar/color/texture
// data. Texture URLs are stored raw here; DsonDocument::ParseFromJson resolves
// them to Image::map_file and identity-linked LIE layers after image_library
// has been parsed.
bool Material::ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys) {
    if (!json.IsObject()) {
        return false;
    }

    static const std::set<std::string> knownKeys = {
        "id", "name", "type", "url", "geometry", "uv_set", "groups", "extra", "diffuse"
    };

    ParseMember(json, "id", id);
    ParseMember(json, "name", name);
    ParseMember(json, "type", type);
    ParseMember(json, "url", url);
    ParseMember(json, "geometry", geometry);

    ParseMember(json, "uv_set", uv_set_id);

    ParseStringValuedArray(json, "groups", groups);

    // Top-level "diffuse" channel
    const rapidjson::Value* diffuseObj = nullptr;
    if (JsonHelper::GetObject(json, "diffuse", diffuseObj)) {
        channels.push_back({"diffuse", ParseMaterialChannel(*diffuseObj, unknownKeys)});
    }

    // extra[] carries the shader-type entry ("studio/material/<name>") and the PBR channel data
    const rapidjson::Value* extraArr = nullptr;
    if (JsonHelper::GetArray(json, "extra", extraArr)) {
        bool shaderTypeCaptured = false;
        for (rapidjson::SizeType i = 0; i < extraArr->Size(); i++) {
            const auto& extraItem = (*extraArr)[i];
            if (!extraItem.IsObject()) continue;
            const std::string extraType = JsonHelper::GetStringOrDefault(extraItem, "type");

            // Capture the first studio/material/<shader> entry as the shader type.
            // First match wins; chained shader entries are not expected in DSON files.
            if (!shaderTypeCaptured && extraType.compare(0, 16, "studio/material/") == 0) {
                shader_type = extraType;
                shaderTypeCaptured = true;
            }

            if (extraType != "studio_material_channels") continue;

            const rapidjson::Value* channelsArr = nullptr;
            if (!JsonHelper::GetArray(extraItem, "channels", channelsArr)) continue;

            for (rapidjson::SizeType j = 0; j < channelsArr->Size(); j++) {
                const auto& entry = (*channelsArr)[j];
                if (!entry.IsObject()) continue;

                const rapidjson::Value* chObj = nullptr;
                if (!JsonHelper::GetObject(entry, "channel", chObj)) continue;
                const std::string chId = JsonHelper::GetStringOrDefault(*chObj, "id");

                channels.push_back({chId, ParseMaterialChannel(entry, unknownKeys)});
            }
            break; // Only one studio_material_channels block per material
        }
    }

    TrackUnknownKeys(json, knownKeys, unknownKeys);
    return true;
}

// Skin joint parser:
// Reads one bone's vertex weights from the skin binding payload. DAZ stores the
// primary data in node_weights.values as [vertex_index, weight] pairs; some files
// use local_weights instead, so this parser falls back to that shape. The raw
// joint->vertex layout is preserved here; the C API later inverts and normalizes
// it per vertex for engine import.
bool SkinJoint::ParseFromJson(const rapidjson::Value& json) {
    if (!json.IsObject()) {
        return false;
    }

    ParseMember(json, "id", id);
    ParseMember(json, "node", node);

    // node_weights = { count, values:[[vertexIndex, weight], ...] }
    auto parseWeightBlock = [&](const rapidjson::Value& block) {
        if (block.HasMember("count") && block["count"].IsInt()) {
            weight_count.value = block["count"].GetInt();
        }
        const rapidjson::Value* values = nullptr;
        if (JsonHelper::GetArray(block, "values", values)) {
            weight_indices.reserve(values->Size());
            weights.reserve(values->Size());
            for (rapidjson::SizeType i = 0; i < values->Size(); i++) {
                const auto& pair = (*values)[i];
                if (pair.IsArray() && pair.Size() >= 2 &&
                    pair[0].IsInt() && pair[1].IsNumber()) {
                    weight_indices.push_back(pair[0].GetInt());
                    weights.push_back(pair[1].GetDouble());
                }
            }
            if (weight_count.value == 0) {
                weight_count.value = static_cast<int>(weights.size());
            }
        }
    };

    const rapidjson::Value* nw = nullptr;
    if (JsonHelper::GetObject(json, "node_weights", nw)) {
        parseWeightBlock(*nw);
    }

    // Fallback: some DAZ assets use local_weights with the same format
    if (weight_indices.empty()) {
        const rapidjson::Value* lw = nullptr;
        if (JsonHelper::GetObject(json, "local_weights", lw)) {
            parseWeightBlock(*lw);
        }
    }

    return true;
}

// Skin binding parser:
// Captures the skeleton node reference, bound geometry reference, declared
// vertex count, and all weighted joints. It does not validate that every vertex
// has weights or that joint node references exist; consumers can inspect the raw
// stored binding or use the API's processed per-vertex influence queries.
bool SkinBinding::ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys) {
    if (!json.IsObject()) {
        return false;
    }

    static const std::set<std::string> knownKeys = {
        "node", "geometry", "vertex_count", "joints", "selection_map", "selection_sets"
    };

    ParseMember(json, "node", node);
    ParseMember(json, "geometry", geometry);
    ParseMember(json, "vertex_count", vertex_count);

    const rapidjson::Value* jointsArray = nullptr;
    if (JsonHelper::GetArray(json, "joints", jointsArray)) {
        joints.reserve(jointsArray->Size());
        for (rapidjson::SizeType i = 0; i < jointsArray->Size(); i++) {
            SkinJoint joint;
            if (joint.ParseFromJson((*jointsArray)[i])) {
                joints.push_back(joint);
            }
        }
    }

    TrackUnknownKeys(json, knownKeys, unknownKeys);
    return true;
}

// Formula operation parser:
// Stores one RPN operation exactly as authored. Unknown fields are preserved in
// diagnostics so richer op payloads can be modeled later without hiding data.
bool FormulaOperation::ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys) {
    if (!json.IsObject()) {
        return false;
    }

    static const std::set<std::string> knownKeys = {
        "op", "val", "url"
    };

    op = JsonHelper::GetStringOrDefault(json, "op");
    url = JsonHelper::GetStringOrDefault(json, "url");
    val = JsonHelper::GetDoubleOrDefault(json, "val", 0.0);

    // A push operand's "val" may be a JSON array instead of a scalar: spline_tcb
    // stores each TCB knot as [input, output, tension, continuity, bias]. Retain it
    // verbatim (raw, unevaluated) so a consumer can reconstruct the curve; the
    // scalar val above stays 0.0 for this form. A non-numeric element keeps its
    // slot as 0.0 to preserve the positional knot layout (real spline_tcb knots are
    // all-numeric); nothing is dropped.
    auto valIt = json.FindMember("val");
    if (valIt != json.MemberEnd() && valIt->value.IsArray()) {
        const rapidjson::Value& arr = valIt->value;
        val_array.reserve(arr.Size());
        for (rapidjson::SizeType i = 0; i < arr.Size(); i++) {
            val_array.push_back(arr[i].IsNumber() ? arr[i].GetDouble() : 0.0);
        }
    }

    TrackUnknownKeys(json, knownKeys, unknownKeys);
    return true;
}

// Formula parser:
// Captures the target output channel and source-order RPN operations only. The
// importer remains responsible for evaluating operations and resolving URLs.
bool Formula::ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys) {
    if (!json.IsObject()) {
        return false;
    }

    static const std::set<std::string> knownKeys = {
        "output", "operations", "stage"
    };

    output = JsonHelper::GetStringOrDefault(json, "output");
    stage = JsonHelper::GetStringOrDefault(json, "stage");
    ParseObjectArray(json, "operations", operations, unknownKeys);

    TrackUnknownKeys(json, knownKeys, unknownKeys);
    return true;
}

// Modifier parser:
// Handles morph deltas, normal deltas, channel metadata, skin_binding "skin"
// data, and stored formula RPN payloads. Formula evaluation and referenced-file
// loading remain importer responsibilities.
bool Modifier::ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys) {
    if (!json.IsObject()) {
        return false;
    }
    
    static const std::set<std::string> knownKeys = {
        "id", "name", "type", "url", "parent", "skin_binding", "channel",
        "deltas", "normal_deltas", "vertex_count", "formulas", "region", "group", "skin", "morph",
        "presentation", "extra"
    };
    
    ParseMember(json, "id", id);
    ParseMember(json, "name", name);
    ParseMember(json, "type", type);
    ParseMember(json, "url", url);
    ParseMember(json, "parent", parent);
    ParseMember(json, "skin_binding", skin_binding);

    // Parse channel reference
    const rapidjson::Value* channelObj = nullptr;
    if (JsonHelper::GetObject(json, "channel", channelObj)) {
        channel.value = JsonHelper::GetStringOrDefault(*channelObj, "id");
        channel_label = JsonHelper::GetStringOrDefault(*channelObj, "label");
        {
            const char* valKey = channelObj->HasMember("current_value") ? "current_value" : "value";
            if (channelObj->HasMember(valKey)) {
                const rapidjson::Value& v = (*channelObj)[valKey];
                if (v.IsNumber()) {
                    channel_value = v.GetDouble();
                    channel_value_kind = KindNumber;
                } else if (v.IsBool()) {
                    channel_value = v.GetBool() ? 1.0 : 0.0;
                    channel_value_kind = KindBool;
                } else if (v.IsString()) {
                    channel_value_string = v.GetString();
                    channel_value_kind = KindString;
                    TrackChannelTypeMismatch(unknownKeys, channel.value, valKey, v);
                    // channel_value stays 0.0
                } else {
                    TrackChannelTypeMismatch(unknownKeys, channel.value, valKey, v);
                    // channel_value stays its default (0.0)
                }
            }
        }
        channel_min = JsonHelper::GetDoubleOrDefault(*channelObj, "min", 0.0);
        channel_max = JsonHelper::GetDoubleOrDefault(*channelObj, "max", 1.0);
        channel_clamped = JsonHelper::GetBoolOrDefault(*channelObj, "clamped", false);
    }

    // Parse morph deltas as indexed array
    const rapidjson::Value* deltasObj = nullptr;
    if (JsonHelper::GetObject(json, "deltas", deltasObj)) {
        morph_deltas.ParseFromJson(*deltasObj);
    }

    // Parse normal deltas (same format as deltas)
    const rapidjson::Value* normalDeltasObj = nullptr;
    if (JsonHelper::GetObject(json, "normal_deltas", normalDeltasObj)) {
        normal_deltas.ParseFromJson(*normalDeltasObj);
    }

    // Real DAZ morph modifiers nest deltas under a "morph" payload.
    const rapidjson::Value* morphObj = nullptr;
    if (JsonHelper::GetObject(json, "morph", morphObj)) {
        has_morph = true;

        const rapidjson::Value* morphDeltasObj = nullptr;
        if (JsonHelper::GetObject(*morphObj, "deltas", morphDeltasObj)) {
            morph_deltas.ParseFromJson(*morphDeltasObj);
        }

        const rapidjson::Value* morphNormalDeltasObj = nullptr;
        if (JsonHelper::GetObject(*morphObj, "normal_deltas", morphNormalDeltasObj)) {
            normal_deltas.ParseFromJson(*morphNormalDeltasObj);
        }
    }

    // Parse skin binding payload (the actual key is "skin", not "skin_binding")
    const rapidjson::Value* skinObj = nullptr;
    if (JsonHelper::GetObject(json, "skin", skinObj)) {
        has_skin = skin.ParseFromJson(*skinObj, unknownKeys);
    }

    // Parse formulas (stored, not evaluated; key stays in knownKeys above).
    ParseObjectArray(json, "formulas", formulas, unknownKeys);

    const rapidjson::Value* presObj = nullptr;
    if (JsonHelper::GetObject(json, "presentation", presObj)) {
        presentation_type  = JsonHelper::GetStringOrDefault(*presObj, "type");
        presentation_label = JsonHelper::GetStringOrDefault(*presObj, "label");
        presentation_icon  = JsonHelper::GetStringOrDefault(*presObj, "icon_large");
    }

    // Modifier-level catalog metadata (DAZ Parameter Settings): "group" = Path,
    // "region" = Region. Siblings of "channel"/"presentation"; already in
    // knownKeys; stored verbatim, no interpretation (R6.4).
    group  = JsonHelper::GetStringOrDefault(json, "group");
    region = JsonHelper::GetStringOrDefault(json, "region");

    // Geometry-shell "Mesh Offset" push modifier: the push type marker and
    // the "Offset Distance (cm)" channel live nested in extra[], not at the
    // modifier top level (mirrors the material extra[] walk). Faithful,
    // unevaluated (R6.4): is_push from studio/modifier/push; push_offset_value
    // from the first studio_modifier_channels channel, current_value -> value.
    // The offset is committed only when the push marker is present, so a
    // non-push modifier's channels are never mis-read as an offset.
    const rapidjson::Value* extraArr = nullptr;
    if (JsonHelper::GetArray(json, "extra", extraArr)) {
        bool sawPush = false;
        bool sawOffset = false;
        double offset = 0.0;
        for (rapidjson::SizeType i = 0; i < extraArr->Size(); i++) {
            const auto& extraItem = (*extraArr)[i];
            if (!extraItem.IsObject()) continue;
            const std::string extraType = JsonHelper::GetStringOrDefault(extraItem, "type");

            if (extraType == "studio/modifier/push") {
                sawPush = true;
                continue;
            }
            if (extraType != "studio_modifier_channels" || sawOffset) continue;

            const rapidjson::Value* channelsArr = nullptr;
            if (!JsonHelper::GetArray(extraItem, "channels", channelsArr) ||
                channelsArr->Size() == 0) continue;

            const auto& entry = (*channelsArr)[0];
            const rapidjson::Value* chObj = nullptr;
            if (!entry.IsObject() || !JsonHelper::GetObject(entry, "channel", chObj)) continue;

            const char* valKey = chObj->HasMember("current_value") ? "current_value" : "value";
            if (chObj->HasMember(valKey)) {
                double tmp = 0.0;
                if (JsonHelper::GetNumberOrBool((*chObj)[valKey], tmp)) {
                    offset = tmp;
                    sawOffset = true;
                }
            }
        }
        is_push = sawPush;
        if (is_push && sawOffset) push_offset_value = offset;
    }

    TrackUnknownKeys(json, knownKeys, unknownKeys);
    return true;
}

// Image implementation
bool Image::ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys) {
    if (!json.IsObject()) {
        return false;
    }
    
    static const std::set<std::string> knownKeys = {
        "id", "name", "url", "map", "map_file", "map_size", "map_gamma"
    };
    
    ParseMember(json, "id", id);
    ParseMember(json, "name", name);
    ParseMember(json, "url", url);

    // Try "map" key first: can be a bare string, an object {"url":"..."} / {"file":"..."},
    // or an array whose first element is the base LIE layer. Keep this assignment
    // as the base map path so existing texture resolution remains unchanged.
    if (JsonHelper::HasMember(json, "map")) {
        if (json["map"].IsArray()) {
            const rapidjson::Value& map = json["map"];
            layers.reserve(map.Size());
            for (rapidjson::SizeType i = 0; i < map.Size(); i++) {
                ImageLayer layer;
                if (!GetImageMapPath(map[i], layer.url) || layer.url.empty()) {
                    continue;
                }
                if (map[i].IsObject()) {
                    layer.label    = JsonHelper::GetStringOrDefault(map[i], "label");
                    layer.blend_op = JsonHelper::GetStringOrDefault(map[i], "operation");
                    layer.opacity  = JsonHelper::GetDoubleOrDefault(map[i], "transparency", 1.0);
                    layer.active   = JsonHelper::GetBoolOrDefault  (map[i], "active",  true);
                    layer.invert   = JsonHelper::GetBoolOrDefault  (map[i], "invert",  false);
                    layer.rotation = JsonHelper::GetDoubleOrDefault(map[i], "rotation", 0.0);
                    layer.scale_x  = JsonHelper::GetDoubleOrDefault(map[i], "xscale",  1.0);
                    layer.scale_y  = JsonHelper::GetDoubleOrDefault(map[i], "yscale",  1.0);
                    layer.offset_x = JsonHelper::GetDoubleOrDefault(map[i], "xoffset", 0.0);
                    layer.offset_y = JsonHelper::GetDoubleOrDefault(map[i], "yoffset", 0.0);
                    layer.mirror_x = JsonHelper::GetBoolOrDefault  (map[i], "xmirror", false);
                    layer.mirror_y = JsonHelper::GetBoolOrDefault  (map[i], "ymirror", false);
                    const rapidjson::Value* c = nullptr;
                    if (JsonHelper::GetArray(map[i], "color", c) && c->Size() >= 3) {
                        if ((*c)[0].IsNumber()) layer.color.x = (*c)[0].GetDouble();
                        if ((*c)[1].IsNumber()) layer.color.y = (*c)[1].GetDouble();
                        if ((*c)[2].IsNumber()) layer.color.z = (*c)[2].GetDouble();
                    }
                }
                layers.push_back(layer);
            }
        }

        std::string path;
        if (GetImageMapPath(json["map"], path)) {
            map_file.value = path;
        }
    }
    // Try alternate key
    else {
        ParseMember(json, "map_file", map_file);
    }
    
    // map_size is a [width, height] int array (e.g. [4096, 4096]).
    // Permissive: absent or any other shape leaves the defaults (0).
    const rapidjson::Value* mapSize = nullptr;
    if (JsonHelper::GetArray(json, "map_size", mapSize) && mapSize->Size() >= 2) {
        if ((*mapSize)[0].IsInt()) map_width.value  = (*mapSize)[0].GetInt();
        if ((*mapSize)[1].IsInt()) map_height.value = (*mapSize)[1].GetInt();
    }

    TrackUnknownKeys(json, knownKeys, unknownKeys);
    return true;
}

// UV set parser:
// Reads UV coordinates plus DAZ's face-varying polygon_vertex_indices mapping.
// The common DAZ shape is sparse triplets [face, corner, uv_index], meaning the
// default mapping is identity (uv index == vertex index) and only exceptions are
// listed; these are modeled as uv_overrides. A legacy flat-int
// representation is not modeled (no real-DSF occurrence, no consumer)
// and is ignored.
bool UVSet::ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys) {
    if (!json.IsObject()) {
        return false;
    }
    
    static const std::set<std::string> knownKeys = {
        "id", "name", "label", "url", "uvs", "polygon_vertex_indices", "vertex_count"
    };

    ParseMember(json, "id", id);
    ParseMember(json, "name", name);
    ParseMember(json, "label", label);
    ParseMember(json, "url", url);

    if (JsonHelper::HasMember(json, "vertex_count") && json["vertex_count"].IsInt()) {
        vertex_count = json["vertex_count"].GetInt();
    }

    // Parse UVs - plain [[u,v],...] array or {"count":N,"values":[[u,v],...]} object
    if (const rapidjson::Value* uvsArray = GetValuesArray(json, "uvs")) {
        uvs.values.reserve(uvsArray->Size() * 2);
        for (rapidjson::SizeType i = 0; i < uvsArray->Size(); i++) {
            if ((*uvsArray)[i].IsArray()) {
                const auto& uv = (*uvsArray)[i];
                if (uv.Size() >= 2) {
                    if (uv[0].IsNumber() && uv[1].IsNumber()) {
                        uvs.values.push_back(uv[0].GetDouble());
                        uvs.values.push_back(uv[1].GetDouble());
                    }
                }
            }
        }
    }
    
    if (const rapidjson::Value* polyVertIndices = GetValuesArray(json, "polygon_vertex_indices")) {
        // Detect sparse triplet format: first element is a 3-int array.
        // DAZ DSON spec: each entry is [face_index, corner_index, uv_index]
        // listing only corners where uv_index != vertex_index.
        bool isSparse = false;
        if (polyVertIndices->Size() > 0) {
            const rapidjson::Value& first = (*polyVertIndices)[0];
            isSparse = first.IsArray() && first.Size() == 3;
        }

        if (isSparse) {
            uv_overrides.reserve(polyVertIndices->Size());
            for (rapidjson::SizeType i = 0; i < polyVertIndices->Size(); i++) {
                const rapidjson::Value& elem = (*polyVertIndices)[i];
                if (!elem.IsArray() || elem.Size() < 3) continue;
                if (!elem[0].IsInt() || !elem[1].IsInt() || !elem[2].IsInt()) continue;
                UVOverride ov;
                ov.face     = elem[0].GetInt();
                ov.corner   = elem[1].GetInt();
                ov.uv_index = elem[2].GetInt();
                uv_overrides.push_back(ov);
            }
        }
    }
    
    TrackUnknownKeys(json, knownKeys, unknownKeys);
    return true;
}

// SceneAnimation parser:
// Reads one scene.animations entry: the verbatim url property pointer, the
// first key's typed value (the 1.2.0 kind-typed surface), and, as of 2.16.0,
// every authored key's time plus the numeric value at DSON-native double
// precision on the parallel key_times / key_values vectors. Permissive —
// missing url/keys or an unmodeled value shape leaves kind at KindNull and
// url/str empty; malformed rows (not [t, v] with a numeric time) are skipped
// from key_times. Per R6.4, no data is applied onto scene.materials; this is
// a raw passthrough only.
bool SceneAnimation::ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys) {
    if (!json.IsObject()) return false;

    static const std::set<std::string> knownKeys = {"url", "keys"};

    JsonHelper::GetString(json, "url", url);

    const rapidjson::Value* keysArr = nullptr;
    if (JsonHelper::GetArray(json, "keys", keysArr)) {
        const rapidjson::SizeType n = keysArr->Size();
        key_times.reserve(n);

        for (rapidjson::SizeType i = 0; i < n; ++i) {
            const rapidjson::Value& row = (*keysArr)[i];
            if (!row.IsArray() || row.Size() < 2) continue;
            const rapidjson::Value& tv = row[0];
            const rapidjson::Value& vv = row[1];
            if (!tv.IsNumber()) continue;
            key_times.push_back(tv.GetDouble());

            if (i == 0) {
                if (vv.IsNull()) {
                    kind = KindNull;
                } else if (vv.IsBool()) {
                    kind = KindBool;
                    boolean = vv.GetBool();
                } else if (vv.IsNumber()) {
                    kind = KindNumber;
                    number = vv.GetDouble();
                } else if (vv.IsString()) {
                    kind = KindString;
                    str = vv.GetString();
                } else if (vv.IsArray() && vv.Size() >= 3 &&
                           vv[0].IsNumber() && vv[1].IsNumber() && vv[2].IsNumber()) {
                    kind = KindColor;
                    color.x = vv[0].GetDouble();
                    color.y = vv[1].GetDouble();
                    color.z = vv[2].GetDouble();
                }
                // else: unmodeled shape — keep kind = KindNull (faithful boundary)
            }

            if (kind == KindNumber && vv.IsNumber()) {
                key_values.push_back(vv.GetDouble());
            }
        }
    }

    TrackUnknownKeys(json, knownKeys, unknownKeys);
    return true;
}

// Scene parser:
// Parses scene instance arrays separately from the library definitions. Scene
// nodes/materials/modifiers/uvs usually reference library entries through URL
// fields and may carry instance-level labels, surface groups, or channel values.
// scene.animations is now parsed faithfully onto Scene::animations — per R6.4 it
// is NOT applied onto scene.materials; the consumer resolves the pointers and
// decides. extra is read for its PostLoadAddons "Character Addon Loader" manifest
// (post_load_addons) and for scene_post_load_script references (post_load_scripts);
// presentation and current_camera remain recognized-but-unparsed so the
// unknown-key diagnostics stay focused on new structure.
bool Scene::ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys) {
    if (!json.IsObject()) {
        return false;
    }

    static const std::set<std::string> knownKeys = {
        "presentation", "nodes", "uvs", "modifiers", "materials",
        "animations", "current_camera", "extra"
    };

    ParseObjectArray(json, "nodes", nodes, unknownKeys);
    ParseObjectArray(json, "modifiers", modifiers, unknownKeys);
    ParseObjectArray(json, "materials", materials, unknownKeys);
    ParseObjectArray(json, "uvs", uvs, unknownKeys);
    ParseObjectArray(json, "animations", animations, unknownKeys);

    // scene.extra "Character Addon Loader" manifest: companion figures a character
    // preset loads but does not list in scene.nodes. Flat walk across every
    // scene.extra entry that carries settings.PostLoadAddons.value, in document
    // order. PostLoadAddons.value is an object keyed by slot (not an array), so it
    // is iterated by member rather than via ParseObjectArray. Every hop is a
    // checked JsonHelper accessor and any missing one just skips that entry/slot,
    // so the parse stays permissive (never errors). The IsObject() guards are
    // required: GetObject/GetString call HasMember, which requires an object.
    const rapidjson::Value* extra = nullptr;
    if (JsonHelper::GetArray(json, "extra", extra)) {
        for (rapidjson::SizeType i = 0; i < extra->Size(); ++i) {
            const rapidjson::Value& entry = (*extra)[i];
            if (!entry.IsObject()) continue;
            // scene_post_load_script (and any scene.extra entry that references a DAZ
            // Script): capture the script reference so a consumer can warn it is not
            // executed. Faithful passthrough — the parser neither loads nor runs the
            // .dse/.dsa (R6.4; cf. the no-recursive-load boundary). Gated on a present
            // "script" string so an entry that references no script is skipped; "type"
            // is surfaced verbatim so the consumer can narrow to "scene_post_load_script".
            // Independent of the PostLoadAddons walk below — one entry may carry both.
            std::string scriptRef;
            if (JsonHelper::GetString(entry, "script", scriptRef)) {
                ScenePostLoadScript scr;
                scr.script = scriptRef;
                JsonHelper::GetString(entry, "type", scr.type);
                JsonHelper::GetString(entry, "name", scr.name);
                post_load_scripts.push_back(scr);
            }
            const rapidjson::Value* settings = nullptr;
            const rapidjson::Value* addons = nullptr;
            const rapidjson::Value* slots = nullptr;
            if (!JsonHelper::GetObject(entry, "settings", settings)) continue;
            if (!JsonHelper::GetObject(*settings, "PostLoadAddons", addons)) continue;
            if (!JsonHelper::GetObject(*addons, "value", slots)) continue;
            for (auto it = slots->MemberBegin(); it != slots->MemberEnd(); ++it) {
                const rapidjson::Value* sv = nullptr;
                if (!it->value.IsObject() ||
                    !JsonHelper::GetObject(it->value, "value", sv)) {
                    continue;
                }
                ScenePostLoadAddon addon;
                addon.slot = it->name.GetString();
                JsonHelper::GetString(*sv, "AssetName", addon.asset_name);
                JsonHelper::GetString(*sv, "AssetFile", addon.asset_file);
                const rapidjson::Value* presets = nullptr;
                const rapidjson::Value* presetsValue = nullptr;
                const rapidjson::Value* mat = nullptr;
                const rapidjson::Value* matValue = nullptr;
                if (JsonHelper::GetObject(*sv, "Presets", presets) &&
                    JsonHelper::GetObject(*presets, "value", presetsValue) &&
                    JsonHelper::GetObject(*presetsValue, "Mat", mat) &&
                    JsonHelper::GetObject(*mat, "value", matValue)) {
                    JsonHelper::GetString(*matValue, "PresetFile", addon.mat_preset);
                }
                if (!addon.asset_file.empty()) { // permissive: only keep loadable addons
                    post_load_addons.push_back(addon);
                }
            }
        }
    }

    // presentation and current_camera are recognized but not parsed yet;
    // extra is parsed for its PostLoadAddons manifest and script references above (rest is unmodeled).
    TrackUnknownKeys(json, knownKeys, unknownKeys);
    return true;
}

// Root document parser:
// Dispatches each top-level DSON section to the matching domain parser, records
// unknown keys per context, and performs the only post-parse linkage currently
// in scope: material channel image references are matched against image_library
// entries to fill texture_path. This is still a single-document parser; external
// URLs are preserved as strings rather than recursively loaded.
bool DsonDocument::ParseFromJson(const rapidjson::Document& doc) {
    if (!doc.IsObject()) {
        return false;
    }
    
    unknown_keys.clear();
    
    static const std::set<std::string> knownTopLevelKeys = {
        "file_version", "asset_info", "scene", "node_library", "geometry_library",
        "material_library", "modifier_library", "image_library", "uv_set_library"
    };
    
    ParseMember(doc, "file_version", file_version);

    const rapidjson::Value* assetInfoObj = nullptr;
    if (JsonHelper::GetObject(doc, "asset_info", assetInfoObj)) {
        asset_info.ParseFromJson(*assetInfoObj, &unknown_keys["asset_info"]);
    }
    
    // Parse scene (an object holding the instance arrays)
    const rapidjson::Value* sceneObj = nullptr;
    if (JsonHelper::GetObject(doc, "scene", sceneObj)) {
        scene.ParseFromJson(*sceneObj, &unknown_keys["scene"]);
    }
    
    ParseObjectArray(doc, "node_library", nodes, &unknown_keys["node_library"]);
    ParseObjectArray(doc, "geometry_library", geometries, &unknown_keys["geometry_library"]);
    ParseObjectArray(doc, "material_library", materials, &unknown_keys["material_library"]);
    ParseObjectArray(doc, "modifier_library", modifiers, &unknown_keys["modifier_library"]);
    ParseObjectArray(doc, "image_library", images, &unknown_keys["image_library"]);
    ParseObjectArray(doc, "uv_set_library", uv_sets, &unknown_keys["uv_set_library"]);
    
    // Post-parse: resolve image_url -> texture_path for every material channel in every collection.
    // Matching order: Image.id -> Image.url -> Image.map_file. LIE layers copy only
    // on identity matches so bare-path channels do not inherit colliding overlays.
    auto resolveChannel = [&](MaterialChannel& ch) {
        if (ch.image_url.empty()) return;
        std::string lookupId = ch.image_url;
        if (!lookupId.empty() && lookupId[0] == '#') {
            lookupId = lookupId.substr(1);
        }
        lookupId = PercentDecode(lookupId);
        for (const auto& img : images) {
            bool identityMatch = (img.id.value == lookupId) ||
                                 (!img.url.value.empty() && img.url.value == ch.image_url);
            bool pathMatch = (img.map_file.value == ch.image_url);
            if (identityMatch || pathMatch) {
                ch.texture_path = img.map_file.value;
                if (identityMatch && img.layers.size() >= 2) {
                    ch.layers = img.layers;
                }
                return;
            }
        }
    };

    auto resolveAllChannels = [&](Material& mat) {
        for (auto& pair : mat.channels) {
            resolveChannel(pair.second);
        }
    };

    for (auto& mat : materials) {
        resolveAllChannels(mat);
    }
    for (auto& mat : scene.materials) {
        resolveAllChannels(mat);
    }

    // Track unknown top-level keys
    TrackUnknownKeys(doc, knownTopLevelKeys, &unknown_keys["document"]);
    
    return true;
}

std::vector<std::string> DsonDocument::GetUnknownKeys(const std::string& context) const {
    std::vector<std::string> result;
    auto it = unknown_keys.find(context);
    if (it != unknown_keys.end()) {
        result.assign(it->second.begin(), it->second.end());
    }
    return result;
}

std::vector<std::string> DsonDocument::GetAllContextsWithUnknownKeys() const {
    std::vector<std::string> contexts;
    for (const auto& pair : unknown_keys) {
        if (!pair.second.empty()) {
            contexts.push_back(pair.first);
        }
    }
    return contexts;
}

bool DsonDocument::LoadFromFile(const char* filepath, std::string& errorMsg) {
    FILE* fp = nullptr;
    if (fopen_s(&fp, filepath, "rb") != 0 || !fp) {
        std::ostringstream oss;
        oss << "Failed to open file: " << filepath;
        errorMsg = oss.str();
        return false;
    }

    std::unique_ptr<FILE, int(*)(FILE*)> file(fp, fclose);
    std::string buffer;
    char readBuffer[65536];
    for (;;) {
        size_t bytesRead = fread(readBuffer, 1, sizeof(readBuffer), file.get());
        if (bytesRead > 0) {
            buffer.append(readBuffer, bytesRead);
        }
        if (bytesRead < sizeof(readBuffer)) {
            if (ferror(file.get())) {
                std::ostringstream oss;
                oss << "Failed to read file: " << filepath;
                errorMsg = oss.str();
                return false;
            }
            break;
        }
    }

    return LoadFromBuffer(buffer.data(), buffer.size(), errorMsg);
}

bool DsonDocument::LoadFromString(const char* jsonString, std::string& errorMsg) {
    size_t size = strlen(jsonString);
    if (IsGzip(jsonString, size)) {
        errorMsg = "LoadFromString cannot read binary gzip data; use LoadFromBuffer with an explicit length";
        return false;
    }
    return LoadFromBuffer(jsonString, size, errorMsg);
}

bool DsonDocument::LoadFromBuffer(const char* data, size_t size, std::string& errorMsg) {
    if (!data && size > 0) {
        errorMsg = "Invalid buffer";
        return false;
    }

    std::string inflated;
    const char* parseData = (size == 0) ? "" : data;
    size_t parseSize = size;

    if (IsGzip(data, size)) {
        if (!TryGunzip(data, size, inflated, errorMsg)) {
            return false;
        }
        parseData = inflated.data();
        parseSize = inflated.size();
    }

    rapidjson::Document doc;
    doc.Parse(parseData, parseSize);
    
    if (doc.HasParseError()) {
        std::ostringstream oss;
        oss << "JSON parse error at offset " << doc.GetErrorOffset()
            << ": " << rapidjson::GetParseError_En(doc.GetParseError());
        errorMsg = oss.str();
        return false;
    }

    if (!ParseFromJson(doc)) {
        errorMsg = "Failed to parse DSON structure from JSON";
        return false;
    }

    errorMsg.clear();
    return true;
}

// Legacy overloads
bool DsonDocument::LoadFromFile(const char* filepath) {
    std::string errorMsg;
    return LoadFromFile(filepath, errorMsg);
}

bool DsonDocument::LoadFromString(const char* jsonString) {
    std::string errorMsg;
    return LoadFromString(jsonString, errorMsg);
}

void DsonDocument::Clear() {
    file_version.value.clear();
    scene = Scene();
    nodes.clear();
    geometries.clear();
    materials.clear();
    modifiers.clear();
    images.clear();
    uv_sets.clear();
    unknown_keys.clear();
}

} // namespace Dson
