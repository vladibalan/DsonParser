#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include "DsonDataTypes.h"

namespace Dson {

// Forward declarations
struct AssetInfo;
struct Node;
struct Geometry;
struct Material;
struct Modifier;
struct Image;
struct UVSet;
struct Scene;

// Asset metadata
struct AssetInfo {
    String id;
    String type;
    String contributor_name;
    String contributor_email;
    String revision;
    String modified;
    double unit_scale = 1.0;

    bool ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys = nullptr);
};

// Geometry reference stored on scene figure nodes (the "geometries" array)
struct NodeGeometryRef {
    std::string id;
    std::string url;
};

// Node in scene hierarchy
struct Node {
    String id;
    String name;
    String label;
    String type;
    String parent;
    Url url;
    Vector3 translation;
    Vector3 rotation;
    Vector3 scale;
    double general_scale = 1.0;
    Vector3 center_point; // joint origin (DSF rigs store the real position here)
    Vector3 end_point;
    Vector3 orientation;                  // local axis alignment in rest pose (XYZ Euler degrees); default {0,0,0}
    std::string rotation_order = "YXZ";  // Euler rotation order; default matches DAZ Genesis 9
    std::vector<NodeGeometryRef> geometries; // only populated on scene figure nodes

    bool ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys = nullptr);
};

// Geometry data
struct Geometry {
    String id;
    String name;
    String type;
    Url url;
    FloatArray vertices;           // flattened x,y,z,x,y,z,...
    IntArray polygons;
    IntArray polylist;             // flattened face data (incl. leading group/material indices)
    std::vector<int> polylist_face_offsets; // start index in polylist.values for each face
    Int vertex_count;
    Int polygon_count;
    std::vector<std::string> polygon_groups;          // face group names
    std::vector<std::string> polygon_material_groups; // material group names
    std::string default_uv_set_id;                    // primary UV channel URL (e.g. "/data/.../Base.dsf#Base Multi UDIM")

    bool ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys = nullptr);
};

// One PBR material channel: scalar/color value plus optional texture reference
struct MaterialChannel {
    std::string type;        // DAZ raw type string ("float", "float_color", "bool", "string", etc.)
    double value = 0.0;      // scalar (roughness, opacity strength, normal strength, etc.)
    Vector3 color = {};      // RGB color; meaningful only when has_color == true
    bool has_color = false;  // true for float_color channels (diffuse, emission, subsurface, etc.)

    std::string image_url;     // raw image reference from JSON (e.g. "#img-0" or a path); empty = no texture
    std::string texture_path;  // resolved absolute file path (filled by the post-parse linkage pass)
};

// Material data
struct Material {
    String id;
    String name;
    String type;
    Url url;
    String geometry;
    String uv_set_id;
    std::string shader_type; // "studio/material/<name>" from extra[]; empty if not present
    std::vector<std::string> groups; // surface zone names (populated on scene material instances)

    // All channels in source-file order. first = DAZ channel id, second = parsed data.
    std::vector<std::pair<std::string, MaterialChannel>> channels;

    bool ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys = nullptr);
};

// One bone's influence in a skin binding: which vertices it weights and by how much
struct SkinJoint {
    String id;                          // joint id (e.g. "l_index3")
    Url node;                           // referenced node (e.g. "#l_index3")
    Int weight_count;                   // number of weighted vertices
    std::vector<int> weight_indices;    // vertex indices
    std::vector<double> weights;        // corresponding weights (parallel to weight_indices)

    bool ParseFromJson(const rapidjson::Value& json);
};

// Skin binding payload (the "skin" property of a skin_binding modifier)
struct SkinBinding {
    Url node;          // skeleton root node
    Url geometry;      // bound geometry
    Int vertex_count;
    std::vector<SkinJoint> joints;

    bool ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys = nullptr);
};

// Modifier data (morphs, skins, formulas, etc.)
struct Modifier {
    String id;
    String name;
    String type;
    Url url;
    String parent;
    String skin_binding;
    String channel;
    std::string channel_label;

    // For morph modifiers - indexed deltas
    IndexedVector3Array morph_deltas;
    IndexedVector3Array normal_deltas;

    // For skin_binding modifiers - the "skin" payload
    bool has_skin = false;
    SkinBinding skin;

    bool ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys = nullptr);
};

// Image/Texture data
struct Image {
    String id;
    String name;
    Url url;
    String map_file;
    Int map_width;
    Int map_height;
    
    bool ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys = nullptr);
};

// UV Set data
struct UVSet {
    String id;
    String name;
    Url url;
    FloatArray uvs;
    IntArray polygon_vertex_indices;
    
    bool ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys = nullptr);
};

// Scene - the DSON "scene" object: a collection of instances hooked into the scene.
// Distinct from the *_library definitions on DsonDocument below.
struct Scene {
    std::vector<Node> nodes;
    std::vector<Modifier> modifiers;
    std::vector<Material> materials;
    std::vector<UVSet> uvs;
    // Recognized by the parser but not yet read into typed fields:
    //   presentation, animations, current_camera, extra

    bool ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys = nullptr);
};

// Root DSON document structure
struct DsonDocument {
    String file_version;
    AssetInfo asset_info;

    // Scene data
    Scene scene;

    // Libraries
    std::vector<Node> nodes;
    std::vector<Geometry> geometries;
    std::vector<Material> materials;
    std::vector<Modifier> modifiers;
    std::vector<Image> images;
    
    std::vector<UVSet> uv_sets;
    
    // Track unrecognized keys
    std::map<std::string, std::set<std::string>> unknown_keys; // context -> set of keys
    
    bool ParseFromJson(const rapidjson::Document& doc);
    bool LoadFromFile(const char* filepath, std::string& errorMsg);
    bool LoadFromString(const char* jsonString, std::string& errorMsg);
    
    // Legacy overloads without error messages (for backwards compatibility)
    bool LoadFromFile(const char* filepath);
    bool LoadFromString(const char* jsonString);
    
    // Get unknown keys by context
    std::vector<std::string> GetUnknownKeys(const std::string& context) const;
    std::vector<std::string> GetAllContextsWithUnknownKeys() const;
    
    void Clear();
};

} // namespace Dson