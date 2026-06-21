#include "pch.h"
#include "DsonHelpers.h"

// Helpers orientation:
// Implementations of the safe RapidJSON accessors declared in DsonHelpers.h
// (JsonHelper::*). These were split out of DsonTypes.cpp so a question about a
// helper no longer requires opening the large parser file. The accessors never
// throw: a missing key or type mismatch yields the default / false, matching the
// parser's permissive contract. Parsing logic that uses them lives in
// DsonTypes.cpp.

namespace Dson {

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

bool JsonHelper::GetNumberOrBool(const rapidjson::Value& v, double& out) {
    if (v.IsNumber()) { out = v.GetDouble(); return true; }
    if (v.IsBool())   { out = v.GetBool() ? 1.0 : 0.0;  return true; }
    return false;
}

double JsonHelper::GetNumberOrBoolOrDefault(const rapidjson::Value& obj, const char* key, double defaultValue) {
    double out = defaultValue;
    if (obj.HasMember(key)) { GetNumberOrBool(obj[key], out); }
    return out;
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

} // namespace Dson
