#pragma once
#include <string>
#include <vector>
#include "rapidjson/document.h"

namespace Dson {

// Boolean type
struct Bool {
    bool value = false;
    
    Bool() = default;
    Bool(bool v) : value(v) {}
    operator bool() const { return value; }
    
    bool ParseFromJson(const rapidjson::Value& json);
};

// Integer type
struct Int {
    int value = 0;
    
    Int() = default;
    Int(int v) : value(v) {}
    operator int() const { return value; }
    
    bool ParseFromJson(const rapidjson::Value& json);
};

// Float type
struct Float {
    double value = 0.0;
    
    Float() = default;
    Float(double v) : value(v) {}
    operator double() const { return value; }
    
    bool ParseFromJson(const rapidjson::Value& json);
};

// String type
struct String {
    std::string value;
    
    String() = default;
    String(const std::string& v) : value(v) {}
    String(const char* v) : value(v) {}
    operator std::string() const { return value; }
    const char* c_str() const { return value.c_str(); }
    
    bool ParseFromJson(const rapidjson::Value& json);
};

// 2D Vector
struct Vector2 {
    double x = 0.0;
    double y = 0.0;
    
    Vector2() = default;
    Vector2(double x_, double y_) : x(x_), y(y_) {}
    
    bool ParseFromJson(const rapidjson::Value& json);
};

// 3D Vector
struct Vector3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    
    Vector3() = default;
    Vector3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
    
    bool ParseFromJson(const rapidjson::Value& json);
};

// Color (RGB or RGBA)
struct Color {
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    double a = 1.0; // Alpha defaults to 1.0 (opaque)
    
    Color() = default;
    Color(double r_, double g_, double b_, double a_ = 1.0) 
        : r(r_), g(g_), b(b_), a(a_) {}
    
    bool ParseFromJson(const rapidjson::Value& json);
};

// Integer Array
struct IntArray {
    std::vector<int> values;
    
    IntArray() = default;
    
    size_t size() const { return values.size(); }
    int& operator[](size_t index) { return values[index]; }
    const int& operator[](size_t index) const { return values[index]; }
    
    bool ParseFromJson(const rapidjson::Value& json);
};

// Float Array
struct FloatArray {
    std::vector<double> values;
    
    FloatArray() = default;
    
    size_t size() const { return values.size(); }
    double& operator[](size_t index) { return values[index]; }
    const double& operator[](size_t index) const { return values[index]; }
    
    bool ParseFromJson(const rapidjson::Value& json);
};

// Indexed Array - stores indices and corresponding values
// Used for sparse arrays where not all indices have values
template<typename T>
struct IndexedArray {
    std::vector<int> indices;
    std::vector<T> values;
    
    IndexedArray() = default;
    
    size_t size() const { return values.size(); }
    
    // Get value by array position (not index)
    const T& GetValue(size_t position) const { 
        return values[position]; 
    }
    
    // Get index by array position
    int GetIndex(size_t position) const { 
        return indices[position]; 
    }
    
    bool ParseFromJson(const rapidjson::Value& json);
};

// Specialized indexed arrays
using IndexedIntArray = IndexedArray<int>;
using IndexedFloatArray = IndexedArray<double>;
using IndexedVector3Array = IndexedArray<Vector3>;

// Enum for channel type
enum class ChannelType {
    Unknown,
    Float,
    Int,
    Bool,
    String,
    Color,
    Enum
};

// Channel value - can hold different types
struct ChannelValue {
    ChannelType type = ChannelType::Unknown;
    double float_value = 0.0;
    int int_value = 0;
    bool bool_value = false;
    std::string string_value;
    Color color_value;
    
    ChannelValue() = default;
    
    bool ParseFromJson(const rapidjson::Value& json);
};

// URL reference - can be a string URL or a local ID reference
struct Url {
    std::string value;
    bool is_local_reference = false;
    
    Url() = default;
    Url(const std::string& v) : value(v) {
        is_local_reference = (v.find('#') == 0);
    }
    
    operator std::string() const { return value; }
    const char* c_str() const { return value.c_str(); }
    
    std::string GetId() const {
        if (is_local_reference && value.size() > 1) {
            return value.substr(1); // Remove '#' prefix
        }
        return value;
    }
    
    bool ParseFromJson(const rapidjson::Value& json);
};

} // namespace Dson