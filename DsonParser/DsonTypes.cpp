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
//   import pipeline: geometry, skeleton nodes, skin weights, UVs, materials,
//   images, morph deltas, formula payloads, and scene instances.
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
static void ParseTransformVector3(const rapidjson::Value& arr, Vector3& out) {
    if (!arr.IsArray()) {
        return;
    }
    for (rapidjson::SizeType i = 0; i < arr.Size(); i++) {
        const rapidjson::Value& el = arr[i];
        double val = 0.0;
        if (el.IsObject()) {
            val = JsonHelper::GetDoubleOrDefault(el, "value", 0.0);
            std::string id = JsonHelper::GetStringOrDefault(el, "id");
            if (id == "x") { out.x = val; continue; }
            if (id == "y") { out.y = val; continue; }
            if (id == "z") { out.z = val; continue; }
        } else if (el.IsNumber()) {
            val = el.GetDouble();
        } else {
            continue;
        }
        // Positional fallback (plain [x,y,z] or unlabeled channels)
        if (i == 0) out.x = val;
        else if (i == 1) out.y = val;
        else if (i == 2) out.z = val;
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
        "id", "name", "label", "type", "parent", "url", "translation", "rotation", "scale",
        "general_scale", "center_point", "end_point", "orientation", "rotation_order",
        "geometries", "preview", "extra"
    };

    ParseMember(json, "id", id);
    ParseMember(json, "name", name);
    ParseMember(json, "label", label);
    ParseMember(json, "type", type);
    ParseMember(json, "parent", parent);
    ParseMember(json, "url", url);

    const rapidjson::Value* transArray = nullptr;
    if (JsonHelper::GetArray(json, "translation", transArray)) {
        ParseTransformVector3(*transArray, translation);
    }

    const rapidjson::Value* rotArray = nullptr;
    if (JsonHelper::GetArray(json, "rotation", rotArray)) {
        ParseTransformVector3(*rotArray, rotation);
    }

    scale.x = scale.y = scale.z = 1.0; // default to identity scale
    const rapidjson::Value* scaleArray = nullptr;
    if (JsonHelper::GetArray(json, "scale", scaleArray)) {
        ParseTransformVector3(*scaleArray, scale);
    }

    const rapidjson::Value* gsObj = nullptr;
    if (JsonHelper::GetObject(json, "general_scale", gsObj)) {
        general_scale = JsonHelper::GetDoubleOrDefault(*gsObj, "value", 1.0);
    }

    const rapidjson::Value* centerArray = nullptr;
    if (JsonHelper::GetArray(json, "center_point", centerArray)) {
        ParseTransformVector3(*centerArray, center_point);
    }

    const rapidjson::Value* endArray = nullptr;
    if (JsonHelper::GetArray(json, "end_point", endArray)) {
        ParseTransformVector3(*endArray, end_point);
    }

    const rapidjson::Value* orientArray = nullptr;
    if (JsonHelper::GetArray(json, "orientation", orientArray)) {
        ParseTransformVector3(*orientArray, orientation);
    }

    JsonHelper::GetString(json, "rotation_order", rotation_order);

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

    TrackUnknownKeys(json, knownKeys, unknownKeys);
    return true;
}

// Geometry parser:
// Captures mesh topology exactly enough for downstream importers to rebuild the
// surface: vertex positions, face lists, polygon groups, material groups, and
// the geometry's default UV-set reference. DSON commonly wraps arrays as
// {count, values:[...]}; legacy flat arrays are accepted where practical.
// Polylist faces are kept flattened, with a per-face offset table, because DSON
// faces may vary in length and include leading group/material indices.
bool Geometry::ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys) {
    if (!json.IsObject()) {
        return false;
    }
    
    static const std::set<std::string> knownKeys = {
        "id", "name", "type", "url", "vertices", "polygons", "polylist",
        "vertex_count", "polygon_count", "edge_interpolation_mode", "default_uv_set",
        "polygon_groups", "polygon_material_groups"
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

    TrackUnknownKeys(json, knownKeys, unknownKeys);
    return true;
}

// Parse one material channel container object (has "channel" sub-object with value + image).
// Works for both the top-level "diffuse" key and entries inside extra.studio_material_channels.channels[].
static MaterialChannel ParseMaterialChannel(const rapidjson::Value& container) {
    MaterialChannel result;
    if (!container.IsObject()) return result;

    const rapidjson::Value* ch = nullptr;
    if (!JsonHelper::GetObject(container, "channel", ch)) return result;

    result.type = JsonHelper::GetStringOrDefault(*ch, "type");

    // Prefer current_value (scene override); fall back to value (library default).
    // Value: [r,g,b] array → color channel; single number → scalar channel.
    const rapidjson::Value* valSrc = nullptr;
    if (ch->HasMember("current_value")) {
        valSrc = &(*ch)["current_value"];
    } else if (ch->HasMember("value")) {
        valSrc = &(*ch)["value"];
    }

    if (valSrc) {
        if (valSrc->IsArray() && valSrc->Size() >= 3 &&
            (*valSrc)[0].IsNumber() && (*valSrc)[1].IsNumber() && (*valSrc)[2].IsNumber()) {
            result.color.x = (*valSrc)[0].GetDouble();
            result.color.y = (*valSrc)[1].GetDouble();
            result.color.z = (*valSrc)[2].GetDouble();
            result.has_color = true;
        } else if (valSrc->IsNumber()) {
            result.value = valSrc->GetDouble();
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
        channels.push_back({"diffuse", ParseMaterialChannel(*diffuseObj)});
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

                channels.push_back({chId, ParseMaterialChannel(entry)});
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
        "deltas", "normal_deltas", "vertex_count", "formulas", "region", "group", "skin", "morph"
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
        channel_value = channelObj->HasMember("current_value")
            ? JsonHelper::GetDoubleOrDefault(*channelObj, "current_value", 0.0)
            : JsonHelper::GetDoubleOrDefault(*channelObj, "value", 0.0);
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
                layer.label = map[i].IsObject() ? JsonHelper::GetStringOrDefault(map[i], "label") : "";
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
// listed. A legacy flat-int representation is still accepted into
// polygon_vertex_indices but is not the normal DSF case.
bool UVSet::ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys) {
    if (!json.IsObject()) {
        return false;
    }
    
    static const std::set<std::string> knownKeys = {
        "id", "name", "url", "uvs", "polygon_vertex_indices", "vertex_count"
    };
    
    ParseMember(json, "id", id);
    ParseMember(json, "name", name);
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
        } else {
            // Flat-int format (legacy, hypothetical — not observed in real DSFs).
            polygon_vertex_indices.ParseFromJson(*polyVertIndices);
        }
    }
    
    TrackUnknownKeys(json, knownKeys, unknownKeys);
    return true;
}

// Scene parser:
// Parses scene instance arrays separately from the library definitions. Scene
// nodes/materials/modifiers/uvs usually reference library entries through URL
// fields and may carry instance-level labels, surface groups, or channel values.
// extra is read only for its PostLoadAddons "Character Addon Loader" manifest
// (post_load_addons); presentation, animations, and camera remain recognized-but-
// unparsed so the unknown-key diagnostics stay focused on new structure.
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

    // presentation, animations, current_camera are recognized but not parsed yet;
    // extra is parsed only for its PostLoadAddons manifest above (rest is unmodeled).
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
