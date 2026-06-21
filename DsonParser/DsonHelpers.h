#pragma once
#include <string>
#include <vector>
#include "rapidjson/fwd.h"

// Helpers orientation:
// Safe RapidJSON accessor API used by the parser to read keys without throwing:
// typed GetXOrDefault lookups, out-param GetX existence-checked variants, and
// array/object/member checks. Declarations only — implementations live in
// DsonHelpers.cpp.
//
// Internal header — NOT part of the public surface. Consumers use the C ABI in
// DsonParserAPI.h and must not include this header; the RapidJSON it references
// is an internal implementation detail and never reaches a consumer.

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

    // Coerce a JSON number OR boolean to double (true->1.0, false->0.0). Any
    // other type leaves the default. DSON channel values are number-typed, but a
    // bool-typed channel (e.g. a "JCMs On" gate) stores its value as a JSON
    // boolean -- faithfully 1.0/0.0, not the 0.0 a number-only read drops it to.
    static bool   GetNumberOrBool(const rapidjson::Value& v, double& out);
    static double GetNumberOrBoolOrDefault(const rapidjson::Value& obj, const char* key, double defaultValue = 0.0);
    
    // Boolean helper
    static bool GetBoolOrDefault(const rapidjson::Value& obj, const char* key, bool defaultValue = false);
    
    // Array helpers
    static bool GetArray(const rapidjson::Value& obj, const char* key, const rapidjson::Value*& out);
    static bool GetObject(const rapidjson::Value& obj, const char* key, const rapidjson::Value*& out);
    
    // Existence check
    static bool HasMember(const rapidjson::Value& obj, const char* key);

    // Returns a human-readable JSON type name for the value: "number", "bool",
    // "string", "array", "object", or "null". Used by the channel type-mismatch
    // audit trail to name the unrepresentable type in the decorated entry.
    static const char* JsonTypeName(const rapidjson::Value& v);
};

} // namespace Dson