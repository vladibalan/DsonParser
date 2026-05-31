#include "pch.h"
#include "DsonTypes.h"
#include "DsonHelpers.h"
#include <cstdio>
#include <sstream>

namespace Dson {

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

// Read a { count, values:[...strings] } object (or a plain string array) into out.
static void ParseStringValuedArray(const rapidjson::Value& container, const char* key, std::vector<std::string>& out) {
    if (!container.HasMember(key)) {
        return;
    }
    const rapidjson::Value& v = container[key];
    const rapidjson::Value* values = nullptr;
    if (v.IsObject()) {
        JsonHelper::GetArray(v, "values", values);
    } else if (v.IsArray()) {
        values = &v;
    }
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

// JsonHelper implementations
std::string JsonHelper::GetStringOrDefault(const rapidjson::Value& obj, const char* key, const std::string& defaultValue) {
    if (obj.HasMember(key) && obj[key].IsString()) {
        return obj[key].GetString();
    }
    return defaultValue;
}

bool JsonHelper::GetString(const rapidjson::Value& obj, const char* key, std::string& out) {
    if (obj.HasMember(key) && obj[key].IsString()) {
        out = obj[key].GetString();
        return true;
    }
    return false;
}

double JsonHelper::GetDoubleOrDefault(const rapidjson::Value& obj, const char* key, double defaultValue) {
    if (obj.HasMember(key) && obj[key].IsNumber()) {
        return obj[key].GetDouble();
    }
    return defaultValue;
}

int JsonHelper::GetIntOrDefault(const rapidjson::Value& obj, const char* key, int defaultValue) {
    if (obj.HasMember(key) && obj[key].IsInt()) {
        return obj[key].GetInt();
    }
    return defaultValue;
}

bool JsonHelper::GetDouble(const rapidjson::Value& obj, const char* key, double& out) {
    if (obj.HasMember(key) && obj[key].IsNumber()) {
        out = obj[key].GetDouble();
        return true;
    }
    return false;
}

bool JsonHelper::GetInt(const rapidjson::Value& obj, const char* key, int& out) {
    if (obj.HasMember(key) && obj[key].IsInt()) {
        out = obj[key].GetInt();
        return true;
    }
    return false;
}

bool JsonHelper::GetBoolOrDefault(const rapidjson::Value& obj, const char* key, bool defaultValue) {
    if (obj.HasMember(key) && obj[key].IsBool()) {
        return obj[key].GetBool();
    }
    return defaultValue;
}

bool JsonHelper::GetArray(const rapidjson::Value& obj, const char* key, const rapidjson::Value*& out) {
    if (obj.HasMember(key) && obj[key].IsArray()) {
        out = &obj[key];
        return true;
    }
    return false;
}

bool JsonHelper::GetObject(const rapidjson::Value& obj, const char* key, const rapidjson::Value*& out) {
    if (obj.HasMember(key) && obj[key].IsObject()) {
        out = &obj[key];
        return true;
    }
    return false;
}

bool JsonHelper::HasMember(const rapidjson::Value& obj, const char* key) {
    return obj.HasMember(key);
}

// AssetInfo implementation
bool AssetInfo::ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys) {
    if (!json.IsObject()) {
        return false;
    }
    
    static const std::set<std::string> knownKeys = {
        "id", "type", "contributor", "revision", "modified", "unit_scale"
    };
    
    if (JsonHelper::HasMember(json, "id")) {
        id.ParseFromJson(json["id"]);
    }
    
    if (JsonHelper::HasMember(json, "type")) {
        type.ParseFromJson(json["type"]);
    }
    
    if (JsonHelper::HasMember(json, "contributor")) {
        const rapidjson::Value* contributor = nullptr;
        if (JsonHelper::GetObject(json, "contributor", contributor)) {
            contributor_name.value = JsonHelper::GetStringOrDefault(*contributor, "author");
            contributor_email.value = JsonHelper::GetStringOrDefault(*contributor, "email");
        }
    }
    
    if (JsonHelper::HasMember(json, "revision")) {
        revision.ParseFromJson(json["revision"]);
    }
    
    if (JsonHelper::HasMember(json, "modified")) {
        modified.ParseFromJson(json["modified"]);
    }

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
        "center_point", "end_point", "orientation", "rotation_order",
        "geometries", "preview", "extra"
    };

    if (JsonHelper::HasMember(json, "id")) {
        id.ParseFromJson(json["id"]);
    }

    if (JsonHelper::HasMember(json, "name")) {
        name.ParseFromJson(json["name"]);
    }

    if (JsonHelper::HasMember(json, "label")) {
        label.ParseFromJson(json["label"]);
    }

    if (JsonHelper::HasMember(json, "type")) {
        type.ParseFromJson(json["type"]);
    }
    
    if (JsonHelper::HasMember(json, "parent")) {
        parent.ParseFromJson(json["parent"]);
    }
    
    if (JsonHelper::HasMember(json, "url")) {
        url.ParseFromJson(json["url"]);
    }
    
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

    TrackUnknownKeys(json, knownKeys, unknownKeys);
    return true;
}

// Geometry implementation
bool Geometry::ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys) {
    if (!json.IsObject()) {
        return false;
    }
    
    static const std::set<std::string> knownKeys = {
        "id", "name", "type", "url", "vertices", "polygons", "polylist",
        "vertex_count", "polygon_count", "edge_interpolation_mode", "default_uv_set",
        "polygon_groups", "polygon_material_groups"
    };
    
    if (JsonHelper::HasMember(json, "id")) {
        id.ParseFromJson(json["id"]);
    }
    
    if (JsonHelper::HasMember(json, "name")) {
        name.ParseFromJson(json["name"]);
    }
    
    if (JsonHelper::HasMember(json, "type")) {
        type.ParseFromJson(json["type"]);
    }
    
    if (JsonHelper::HasMember(json, "url")) {
        url.ParseFromJson(json["url"]);
    }
    
    if (JsonHelper::HasMember(json, "vertex_count")) {
        vertex_count.ParseFromJson(json["vertex_count"]);
    }
    
    if (JsonHelper::HasMember(json, "polygon_count")) {
        polygon_count.ParseFromJson(json["polygon_count"]);
    }
    
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

    if (JsonHelper::HasMember(json, "polygons")) {
        const rapidjson::Value& poly = json["polygons"];
        const rapidjson::Value* polyArray = nullptr;
        if (poly.IsObject()) {
            JsonHelper::GetArray(poly, "values", polyArray);
        } else if (poly.IsArray()) {
            polyArray = &poly;
        }
        if (polyArray) {
            polygons.ParseFromJson(*polyArray);
        }
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
            for (rapidjson::SizeType i = 0; i < faces->Size(); i++) {
                if ((*faces)[i].IsArray()) {
                    const auto& face = (*faces)[i];
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

    // Value: [r,g,b] array → color channel; single number → scalar channel
    if (ch->HasMember("value")) {
        const rapidjson::Value& val = (*ch)["value"];
        if (val.IsArray() && val.Size() >= 3 &&
            val[0].IsNumber() && val[1].IsNumber() && val[2].IsNumber()) {
            result.color.x = val[0].GetDouble();
            result.color.y = val[1].GetDouble();
            result.color.z = val[2].GetDouble();
            result.has_color = true;
        } else if (val.IsNumber()) {
            result.value = val.GetDouble();
        }
    }

    // Image reference is stored inside the channel sub-object
    if (ch->HasMember("image") && (*ch)["image"].IsString()) {
        result.image_url = (*ch)["image"].GetString();
    }

    return result;
}

// Material implementation
bool Material::ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys) {
    if (!json.IsObject()) {
        return false;
    }

    static const std::set<std::string> knownKeys = {
        "id", "name", "type", "url", "geometry", "uv_set", "groups", "extra", "diffuse"
    };

    if (JsonHelper::HasMember(json, "id")) {
        id.ParseFromJson(json["id"]);
    }

    if (JsonHelper::HasMember(json, "name")) {
        name.ParseFromJson(json["name"]);
    }

    if (JsonHelper::HasMember(json, "type")) {
        type.ParseFromJson(json["type"]);
    }

    if (JsonHelper::HasMember(json, "url")) {
        url.ParseFromJson(json["url"]);
    }

    if (JsonHelper::HasMember(json, "geometry")) {
        geometry.ParseFromJson(json["geometry"]);
    }

    if (json.HasMember("uv_set") && json["uv_set"].IsString()) {
        uv_set_id.value = json["uv_set"].GetString();
    }

    // Top-level "diffuse" channel
    const rapidjson::Value* diffuseObj = nullptr;
    if (JsonHelper::GetObject(json, "diffuse", diffuseObj)) {
        diffuse = ParseMaterialChannel(*diffuseObj);
    }

    // Remaining PBR channels live in extra[?].channels[] (type == "studio_material_channels")
    const rapidjson::Value* extraArr = nullptr;
    if (JsonHelper::GetArray(json, "extra", extraArr)) {
        for (rapidjson::SizeType i = 0; i < extraArr->Size(); i++) {
            const auto& extraItem = (*extraArr)[i];
            if (!extraItem.IsObject()) continue;
            if (JsonHelper::GetStringOrDefault(extraItem, "type") != "studio_material_channels") continue;

            const rapidjson::Value* channelsArr = nullptr;
            if (!JsonHelper::GetArray(extraItem, "channels", channelsArr)) continue;

            // Track priority for specular: Glossy Color (2) > Specular Color (1) > Metallic Weight (0)
            int specularPriority = -1;

            for (rapidjson::SizeType j = 0; j < channelsArr->Size(); j++) {
                const auto& entry = (*channelsArr)[j];
                if (!entry.IsObject()) continue;

                const rapidjson::Value* chObj = nullptr;
                if (!JsonHelper::GetObject(entry, "channel", chObj)) continue;
                const std::string chId = JsonHelper::GetStringOrDefault(*chObj, "id");

                if (chId == "Glossy Color") {
                    specular = ParseMaterialChannel(entry);
                    specularPriority = 2;
                } else if (chId == "Specular Color" && specularPriority < 1) {
                    specular = ParseMaterialChannel(entry);
                    specularPriority = 1;
                } else if (chId == "Metallic Weight" && specularPriority < 0) {
                    specular = ParseMaterialChannel(entry);
                    specularPriority = 0;
                } else if (chId == "Glossy Roughness" || chId == "Roughness") {
                    roughness = ParseMaterialChannel(entry);
                } else if (chId == "Normal Map" || chId == "Bump Strength") {
                    normal = ParseMaterialChannel(entry);
                } else if (chId == "Cutout Opacity" || chId == "Opacity Strength") {
                    opacity = ParseMaterialChannel(entry);
                } else if (chId == "Transmitted Color" || chId == "Subsurface Color") {
                    subsurface = ParseMaterialChannel(entry);
                } else if (chId == "Emission Color") {
                    emission = ParseMaterialChannel(entry);
                }
            }
            break; // Only one studio_material_channels block per material
        }
    }

    TrackUnknownKeys(json, knownKeys, unknownKeys);
    return true;
}

// SkinJoint implementation
bool SkinJoint::ParseFromJson(const rapidjson::Value& json) {
    if (!json.IsObject()) {
        return false;
    }

    if (JsonHelper::HasMember(json, "id")) {
        id.ParseFromJson(json["id"]);
    }
    if (JsonHelper::HasMember(json, "node")) {
        node.ParseFromJson(json["node"]);
    }

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

// SkinBinding implementation
bool SkinBinding::ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys) {
    if (!json.IsObject()) {
        return false;
    }

    static const std::set<std::string> knownKeys = {
        "node", "geometry", "vertex_count", "joints", "selection_map", "selection_sets"
    };

    if (JsonHelper::HasMember(json, "node")) {
        node.ParseFromJson(json["node"]);
    }
    if (JsonHelper::HasMember(json, "geometry")) {
        geometry.ParseFromJson(json["geometry"]);
    }
    if (JsonHelper::HasMember(json, "vertex_count")) {
        vertex_count.ParseFromJson(json["vertex_count"]);
    }

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

// Modifier implementation
bool Modifier::ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys) {
    if (!json.IsObject()) {
        return false;
    }
    
    static const std::set<std::string> knownKeys = {
        "id", "name", "type", "url", "parent", "skin_binding", "channel",
        "deltas", "normal_deltas", "vertex_count", "formulas", "region", "group", "skin"
    };
    
    if (JsonHelper::HasMember(json, "id")) {
        id.ParseFromJson(json["id"]);
    }
    
    if (JsonHelper::HasMember(json, "name")) {
        name.ParseFromJson(json["name"]);
    }
    
    if (JsonHelper::HasMember(json, "type")) {
        type.ParseFromJson(json["type"]);
    }
    
    if (JsonHelper::HasMember(json, "url")) {
        url.ParseFromJson(json["url"]);
    }
    
    if (JsonHelper::HasMember(json, "parent")) {
        parent.ParseFromJson(json["parent"]);
    }
    
    if (JsonHelper::HasMember(json, "skin_binding")) {
        skin_binding.ParseFromJson(json["skin_binding"]);
    }
    
    // Parse channel reference
    const rapidjson::Value* channelObj = nullptr;
    if (JsonHelper::GetObject(json, "channel", channelObj)) {
        channel.value = JsonHelper::GetStringOrDefault(*channelObj, "id");
        channel_label = JsonHelper::GetStringOrDefault(*channelObj, "label");
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

    // Parse skin binding payload (the actual key is "skin", not "skin_binding")
    const rapidjson::Value* skinObj = nullptr;
    if (JsonHelper::GetObject(json, "skin", skinObj)) {
        has_skin = skin.ParseFromJson(*skinObj, unknownKeys);
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
    
    if (JsonHelper::HasMember(json, "id")) {
        id.ParseFromJson(json["id"]);
    }
    
    if (JsonHelper::HasMember(json, "name")) {
        name.ParseFromJson(json["name"]);
    }
    
    if (JsonHelper::HasMember(json, "url")) {
        url.ParseFromJson(json["url"]);
    }
    
    // Try "map" key first — can be a bare string or an object {"url":"..."} / {"file":"..."}
    if (JsonHelper::HasMember(json, "map")) {
        const rapidjson::Value& mapVal = json["map"];
        if (mapVal.IsString()) {
            map_file.value = mapVal.GetString();
        } else if (mapVal.IsObject()) {
            std::string path;
            if (!JsonHelper::GetString(mapVal, "url", path)) {
                JsonHelper::GetString(mapVal, "file", path);
            }
            map_file.value = path;
        }
    }
    // Try alternate key
    else if (JsonHelper::HasMember(json, "map_file")) {
        map_file.ParseFromJson(json["map_file"]);
    }
    
    if (JsonHelper::HasMember(json, "map_size")) {
        map_width.ParseFromJson(json["map_size"]);
    }
    
    TrackUnknownKeys(json, knownKeys, unknownKeys);
    return true;
}

// UVSet implementation
bool UVSet::ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys) {
    if (!json.IsObject()) {
        return false;
    }
    
    static const std::set<std::string> knownKeys = {
        "id", "name", "url", "uvs", "polygon_vertex_indices", "vertex_count"
    };
    
    if (JsonHelper::HasMember(json, "id")) {
        id.ParseFromJson(json["id"]);
    }
    
    if (JsonHelper::HasMember(json, "name")) {
        name.ParseFromJson(json["name"]);
    }
    
    if (JsonHelper::HasMember(json, "url")) {
        url.ParseFromJson(json["url"]);
    }
    
    // Parse UVs - plain [[u,v],...] array or {"count":N,"values":[[u,v],...]} object
    if (JsonHelper::HasMember(json, "uvs")) {
        const rapidjson::Value& uvsVal = json["uvs"];
        const rapidjson::Value* uvsArray = nullptr;
        if (uvsVal.IsObject()) {
            JsonHelper::GetArray(uvsVal, "values", uvsArray);
        } else if (uvsVal.IsArray()) {
            uvsArray = &uvsVal;
        }
        if (uvsArray) {
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
    }
    
    if (JsonHelper::HasMember(json, "polygon_vertex_indices")) {
        const rapidjson::Value& pviVal = json["polygon_vertex_indices"];
        const rapidjson::Value* polyVertIndices = nullptr;
        if (pviVal.IsObject()) {
            JsonHelper::GetArray(pviVal, "values", polyVertIndices);
        } else if (pviVal.IsArray()) {
            polyVertIndices = &pviVal;
        }
        if (polyVertIndices) {
            polygon_vertex_indices.ParseFromJson(*polyVertIndices);
        }
    }
    
    TrackUnknownKeys(json, knownKeys, unknownKeys);
    return true;
}

// Scene implementation
bool Scene::ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys) {
    if (!json.IsObject()) {
        return false;
    }

    static const std::set<std::string> knownKeys = {
        "presentation", "nodes", "uvs", "modifiers", "materials",
        "animations", "current_camera", "extra"
    };

    const rapidjson::Value* nodeArray = nullptr;
    if (JsonHelper::GetArray(json, "nodes", nodeArray)) {
        nodes.reserve(nodeArray->Size());
        for (rapidjson::SizeType i = 0; i < nodeArray->Size(); i++) {
            Node node;
            if (node.ParseFromJson((*nodeArray)[i], unknownKeys)) {
                nodes.push_back(node);
            }
        }
    }

    const rapidjson::Value* modArray = nullptr;
    if (JsonHelper::GetArray(json, "modifiers", modArray)) {
        modifiers.reserve(modArray->Size());
        for (rapidjson::SizeType i = 0; i < modArray->Size(); i++) {
            Modifier mod;
            if (mod.ParseFromJson((*modArray)[i], unknownKeys)) {
                modifiers.push_back(mod);
            }
        }
    }

    const rapidjson::Value* matArray = nullptr;
    if (JsonHelper::GetArray(json, "materials", matArray)) {
        materials.reserve(matArray->Size());
        for (rapidjson::SizeType i = 0; i < matArray->Size(); i++) {
            Material mat;
            if (mat.ParseFromJson((*matArray)[i], unknownKeys)) {
                materials.push_back(mat);
            }
        }
    }

    const rapidjson::Value* uvArray = nullptr;
    if (JsonHelper::GetArray(json, "uvs", uvArray)) {
        uvs.reserve(uvArray->Size());
        for (rapidjson::SizeType i = 0; i < uvArray->Size(); i++) {
            UVSet uv;
            if (uv.ParseFromJson((*uvArray)[i], unknownKeys)) {
                uvs.push_back(uv);
            }
        }
    }

    // presentation, animations, current_camera, extra are recognized but not parsed yet
    TrackUnknownKeys(json, knownKeys, unknownKeys);
    return true;
}

// DsonDocument implementation
bool DsonDocument::ParseFromJson(const rapidjson::Document& doc) {
    if (!doc.IsObject()) {
        return false;
    }
    
    unknown_keys.clear();
    
    static const std::set<std::string> knownTopLevelKeys = {
        "file_version", "asset_info", "scene", "node_library", "geometry_library",
        "material_library", "modifier_library", "image_library", "uv_set_library"
    };
    
    if (JsonHelper::HasMember(doc, "file_version")) {
        file_version.ParseFromJson(doc["file_version"]);
    }
    
    const rapidjson::Value* assetInfoObj = nullptr;
    if (JsonHelper::GetObject(doc, "asset_info", assetInfoObj)) {
        asset_info.ParseFromJson(*assetInfoObj, &unknown_keys["asset_info"]);
    }
    
    // Parse scene (an object holding the instance arrays)
    const rapidjson::Value* sceneObj = nullptr;
    if (JsonHelper::GetObject(doc, "scene", sceneObj)) {
        scene.ParseFromJson(*sceneObj, &unknown_keys["scene"]);
    }
    
    // Parse node_library
    const rapidjson::Value* nodeLib = nullptr;
    if (JsonHelper::GetArray(doc, "node_library", nodeLib)) {
        nodes.reserve(nodeLib->Size());
        for (rapidjson::SizeType i = 0; i < nodeLib->Size(); i++) {
            Node node;
            if (node.ParseFromJson((*nodeLib)[i], &unknown_keys["node_library"])) {
                nodes.push_back(node);
            }
        }
    }
    
    // Parse geometry_library
    const rapidjson::Value* geomLib = nullptr;
    if (JsonHelper::GetArray(doc, "geometry_library", geomLib)) {
        geometries.reserve(geomLib->Size());
        for (rapidjson::SizeType i = 0; i < geomLib->Size(); i++) {
            Geometry geom;
            if (geom.ParseFromJson((*geomLib)[i], &unknown_keys["geometry_library"])) {
                geometries.push_back(geom);
            }
        }
    }
    
    // Parse material_library
    const rapidjson::Value* matLib = nullptr;
    if (JsonHelper::GetArray(doc, "material_library", matLib)) {
        materials.reserve(matLib->Size());
        for (rapidjson::SizeType i = 0; i < matLib->Size(); i++) {
            Material mat;
            if (mat.ParseFromJson((*matLib)[i], &unknown_keys["material_library"])) {
                materials.push_back(mat);
            }
        }
    }
    
    // Parse modifier_library
    const rapidjson::Value* modLib = nullptr;
    if (JsonHelper::GetArray(doc, "modifier_library", modLib)) {
        modifiers.reserve(modLib->Size());
        for (rapidjson::SizeType i = 0; i < modLib->Size(); i++) {
            Modifier mod;
            if (mod.ParseFromJson((*modLib)[i], &unknown_keys["modifier_library"])) {
                modifiers.push_back(mod);
            }
        }
    }
    
    // Parse image_library
    const rapidjson::Value* imgLib = nullptr;
    if (JsonHelper::GetArray(doc, "image_library", imgLib)) {
        images.reserve(imgLib->Size());
        for (rapidjson::SizeType i = 0; i < imgLib->Size(); i++) {
            Image img;
            if (img.ParseFromJson((*imgLib)[i], &unknown_keys["image_library"])) {
                images.push_back(img);
            }
        }
    }
    
    // Parse uv_set_library
    const rapidjson::Value* uvLib = nullptr;
    if (JsonHelper::GetArray(doc, "uv_set_library", uvLib)) {
        uv_sets.reserve(uvLib->Size());
        for (rapidjson::SizeType i = 0; i < uvLib->Size(); i++) {
            UVSet uvset;
            if (uvset.ParseFromJson((*uvLib)[i], &unknown_keys["uv_set_library"])) {
                uv_sets.push_back(uvset);
            }
        }
    }
    
    // Post-parse: resolve image_url → texture_path for every material channel in every collection.
    // Matching order: Image.id → Image.url → Image.map_file.
    auto resolveChannel = [&](MaterialChannel& ch) {
        if (ch.image_url.empty()) return;
        std::string lookupId = ch.image_url;
        if (!lookupId.empty() && lookupId[0] == '#') {
            lookupId = lookupId.substr(1);
        }
        for (const auto& img : images) {
            if (img.id.value == lookupId ||
                img.url.value == ch.image_url ||
                img.map_file.value == ch.image_url) {
                ch.texture_path = img.map_file.value;
                return;
            }
        }
    };

    auto resolveAllChannels = [&](Material& mat) {
        resolveChannel(mat.diffuse);
        resolveChannel(mat.specular);
        resolveChannel(mat.roughness);
        resolveChannel(mat.normal);
        resolveChannel(mat.opacity);
        resolveChannel(mat.subsurface);
        resolveChannel(mat.emission);
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
    
    char* readBuffer = new char[65536];
    rapidjson::FileReadStream is(fp, readBuffer, 65536);
    
    rapidjson::Document doc;
    doc.ParseStream(is);
    fclose(fp);
    delete[] readBuffer;
    
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

bool DsonDocument::LoadFromString(const char* jsonString, std::string& errorMsg) {
    rapidjson::Document doc;
    doc.Parse(jsonString);
    
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