#include "pch.h"
#include "DsonDataTypes.h"

// Data types orientation:
// ParseFromJson implementations for the primitive value wrappers declared in
// DsonDataTypes.h. Each is permissive: a type mismatch returns false and leaves
// the wrapper at its default rather than throwing.

namespace Dson {

// Bool implementation
bool Bool::ParseFromJson(const rapidjson::Value& json) {
    if (json.IsBool()) {
        value = json.GetBool();
        return true;
    }
    return false;
}

// Int implementation
bool Int::ParseFromJson(const rapidjson::Value& json) {
    if (json.IsInt()) {
        value = json.GetInt();
        return true;
    }
    return false;
}

// Float implementation
bool Float::ParseFromJson(const rapidjson::Value& json) {
    if (json.IsNumber()) {
        value = json.GetDouble();
        return true;
    }
    return false;
}

// String implementation
bool String::ParseFromJson(const rapidjson::Value& json) {
    if (json.IsString()) {
        value = json.GetString();
        return true;
    }
    return false;
}

// Vector2 implementation
bool Vector2::ParseFromJson(const rapidjson::Value& json) {
    if (json.IsArray() && json.Size() >= 2) {
        if (json[0].IsNumber() && json[1].IsNumber()) {
            x = json[0].GetDouble();
            y = json[1].GetDouble();
            return true;
        }
    }
    return false;
}

// Vector3 implementation
bool Vector3::ParseFromJson(const rapidjson::Value& json) {
    if (json.IsArray() && json.Size() >= 3) {
        if (json[0].IsNumber() && json[1].IsNumber() && json[2].IsNumber()) {
            x = json[0].GetDouble();
            y = json[1].GetDouble();
            z = json[2].GetDouble();
            return true;
        }
    }
    return false;
}

// Color implementation
bool Color::ParseFromJson(const rapidjson::Value& json) {
    if (json.IsArray()) {
        if (json.Size() >= 3) {
            if (json[0].IsNumber() && json[1].IsNumber() && json[2].IsNumber()) {
                r = json[0].GetDouble();
                g = json[1].GetDouble();
                b = json[2].GetDouble();
                
                // Check for alpha channel
                if (json.Size() >= 4 && json[3].IsNumber()) {
                    a = json[3].GetDouble();
                }
                return true;
            }
        }
    }
    return false;
}

// IntArray implementation
bool IntArray::ParseFromJson(const rapidjson::Value& json) {
    if (json.IsArray()) {
        values.reserve(json.Size());
        for (rapidjson::SizeType i = 0; i < json.Size(); i++) {
            if (json[i].IsInt()) {
                values.push_back(json[i].GetInt());
            }
        }
        return true;
    }
    return false;
}

// FloatArray implementation
bool FloatArray::ParseFromJson(const rapidjson::Value& json) {
    if (json.IsArray()) {
        values.reserve(json.Size());
        for (rapidjson::SizeType i = 0; i < json.Size(); i++) {
            if (json[i].IsNumber()) {
                values.push_back(json[i].GetDouble());
            }
        }
        return true;
    }
    return false;
}

// Template specializations for IndexedArray
template<>
bool IndexedArray<int>::ParseFromJson(const rapidjson::Value& json) {
    if (!json.IsObject()) {
        return false;
    }
    
    // Parse indices array
    if (json.HasMember("indices") && json["indices"].IsArray()) {
        const auto& indicesArray = json["indices"];
        indices.reserve(indicesArray.Size());
        for (rapidjson::SizeType i = 0; i < indicesArray.Size(); i++) {
            if (indicesArray[i].IsInt()) {
                indices.push_back(indicesArray[i].GetInt());
            }
        }
    }
    
    // Parse values array
    if (json.HasMember("values") && json["values"].IsArray()) {
        const auto& valuesArray = json["values"];
        values.reserve(valuesArray.Size());
        for (rapidjson::SizeType i = 0; i < valuesArray.Size(); i++) {
            if (valuesArray[i].IsInt()) {
                values.push_back(valuesArray[i].GetInt());
            }
        }
    }
    
    return indices.size() == values.size();
}

template<>
bool IndexedArray<double>::ParseFromJson(const rapidjson::Value& json) {
    if (!json.IsObject()) {
        return false;
    }
    
    // Parse indices array
    if (json.HasMember("indices") && json["indices"].IsArray()) {
        const auto& indicesArray = json["indices"];
        indices.reserve(indicesArray.Size());
        for (rapidjson::SizeType i = 0; i < indicesArray.Size(); i++) {
            if (indicesArray[i].IsInt()) {
                indices.push_back(indicesArray[i].GetInt());
            }
        }
    }
    
    // Parse values array
    if (json.HasMember("values") && json["values"].IsArray()) {
        const auto& valuesArray = json["values"];
        values.reserve(valuesArray.Size());
        for (rapidjson::SizeType i = 0; i < valuesArray.Size(); i++) {
            if (valuesArray[i].IsNumber()) {
                values.push_back(valuesArray[i].GetDouble());
            }
        }
    }
    
    return indices.size() == values.size();
}

template<>
bool IndexedArray<Vector3>::ParseFromJson(const rapidjson::Value& json) {
    if (!json.IsObject()) {
        return false;
    }

    // DSON morph delta format: { "count": N, "values": [[vertex_idx, dx, dy, dz], ...] }
    // Element [0] of each sub-array is the vertex index; [1..3] are the XYZ displacement.
    if (!json.HasMember("values") || !json["values"].IsArray()) {
        return false;
    }
    const auto& valuesArray = json["values"];

    indices.reserve(valuesArray.Size());
    values.reserve(valuesArray.Size());

    for (rapidjson::SizeType i = 0; i < valuesArray.Size(); i++) {
        const rapidjson::Value& elem = valuesArray[i];
        if (!elem.IsArray() || elem.Size() < 4) {
            continue;
        }
        if (!elem[0].IsInt() || !elem[1].IsNumber() || !elem[2].IsNumber() || !elem[3].IsNumber()) {
            continue;
        }
        indices.push_back(elem[0].GetInt());
        values.push_back(Vector3(elem[1].GetDouble(), elem[2].GetDouble(), elem[3].GetDouble()));
    }

    return indices.size() == values.size();
}

// ChannelValue implementation
bool ChannelValue::ParseFromJson(const rapidjson::Value& json) {
    if (json.IsNumber()) {
        type = ChannelType::Float;
        float_value = json.GetDouble();
        return true;
    } else if (json.IsInt()) {
        type = ChannelType::Int;
        int_value = json.GetInt();
        return true;
    } else if (json.IsBool()) {
        type = ChannelType::Bool;
        bool_value = json.GetBool();
        return true;
    } else if (json.IsString()) {
        type = ChannelType::String;
        string_value = json.GetString();
        return true;
    } else if (json.IsArray()) {
        // Try to parse as color
        if (color_value.ParseFromJson(json)) {
            type = ChannelType::Color;
            return true;
        }
    }
    
    type = ChannelType::Unknown;
    return false;
}

// Url implementation
bool Url::ParseFromJson(const rapidjson::Value& json) {
    if (json.IsString()) {
        value = json.GetString();
        is_local_reference = (value.find('#') == 0);
        return true;
    }
    return false;
}

} // namespace Dson