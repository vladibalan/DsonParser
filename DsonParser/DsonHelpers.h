#pragma once
#include <string>
#include <vector>
#include "rapidjson/document.h"

// Helpers orientation:
// Safe RapidJSON accessor API used by the parser to read keys without throwing:
// typed GetXOrDefault lookups, out-param GetX existence-checked variants, and
// array/object/member checks. Declarations only — implementations live in
// DsonHelpers.cpp.

namespace Dson {

// Helper functions for safe JSON parsing
class JsonHelper {
public:
    // String helpers
    static std::string GetStringOrDefault(const rapidjson::Value& obj, const char* key, const std::string& defaultValue = "");
    static bool GetString(const rapidjson::Value& obj, const char* key, std::string& out);
    
    // Numeric helpers
    static double GetDoubleOrDefault(const rapidjson::Value& obj, const char* key, double defaultValue = 0.0);
    static int GetIntOrDefault(const rapidjson::Value& obj, const char* key, int defaultValue = 0);
    static bool GetDouble(const rapidjson::Value& obj, const char* key, double& out);
    static bool GetInt(const rapidjson::Value& obj, const char* key, int& out);
    
    // Boolean helper
    static bool GetBoolOrDefault(const rapidjson::Value& obj, const char* key, bool defaultValue = false);
    
    // Array helpers
    static bool GetArray(const rapidjson::Value& obj, const char* key, const rapidjson::Value*& out);
    static bool GetObject(const rapidjson::Value& obj, const char* key, const rapidjson::Value*& out);
    
    // Existence check
    static bool HasMember(const rapidjson::Value& obj, const char* key);
};

} // namespace Dson