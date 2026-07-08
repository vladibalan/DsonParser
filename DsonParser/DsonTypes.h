#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include <map>
#include <set>
#include "DsonDataTypes.h"

// Types orientation:
// The typed DSON model: section structs (AssetInfo, Node, Geometry, Material,
// Modifier, Image, UVSet, scene instances) and the root DsonDocument that owns
// them. These compose the primitive wrappers from DsonDataTypes.h. Parsing
// logic lives in DsonTypes.cpp; Node and Geometry retain their own authored
// material-to-UV assignments, while Geometry also includes raw graft and
// rigidity data.
// The C ABI over this model is in DsonParserAPI.
//
// Internal header — NOT part of the public surface. Consumers use the C ABI in
// DsonParserAPI.h and must not include this header; the RapidJSON it references
// is an internal implementation detail and never reaches a consumer.

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

// One authored material_uvs pair. Both names are retained verbatim; resolving
// either name to another object or file belongs to the consumer.
struct MaterialUVAssignment {
    std::string material_group;
    std::string uv_set_name;
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
    unsigned int translation_presence = 0; // authored numeric components: X=1, Y=2, Z=4
    Vector3 rotation;
    unsigned int rotation_presence = 0;    // authored numeric components: X=1, Y=2, Z=4
    Vector3 scale;
    unsigned int scale_presence = 0;       // authored numeric components: X=1, Y=2, Z=4
    double general_scale = 1.0;
    bool has_general_scale = false;
    Vector3 center_point; // joint origin (DSF rigs store the real position here)
    unsigned int center_point_presence = 0; // authored numeric components: X=1, Y=2, Z=4
    // Rigid-follow rigidity group: a studio/node/rigid_follow entry in extra[]
    // carrying an inline rigidity_group: a fixed reference-vertex patch on a
    // followed mesh this node rides rigidly. Raw, unevaluated passthrough (R6.4).
    bool has_rigid_follow = false;                     // marker WITH a rigidity_group is present
    std::string rigid_follow_rotation_mode;            // rigidity_group.rotation_mode; "" if absent
    std::vector<std::string> rigid_follow_scale_modes; // rigidity_group.scale_modes, per-axis
    std::vector<int> rigid_follow_reference_vertices;  // raw DSON indices into the followed geometry
    Vector3 end_point;
    Vector3 orientation;                  // local axis alignment in rest pose (XYZ Euler degrees); default {0,0,0}
    unsigned int orientation_presence = 0; // authored numeric components: X=1, Y=2, Z=4
    bool inherits_scale = false;          // raw authored value; meaningful when has_inherits_scale
    bool has_inherits_scale = false;
    std::string rotation_order = "YXZ";  // Euler rotation order; default matches DAZ Genesis 9
    bool has_rotation_order = false;
    std::vector<NodeGeometryRef> geometries; // only populated on scene figure nodes
    // Rows from studio/node/shell extra entries, in authored extra/row order.
    std::vector<MaterialUVAssignment> shell_material_uv_assignments;
    std::string presentation_type;   // presentation.type  (DAZ "Content Type"; "" if absent)
    std::string presentation_label;  // presentation.label (declared display name; "" if absent)

    bool ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys = nullptr);
};

// One geograft weld pair, raw DSON file-local indices (no remap).
struct GraftVertexPair {
    int graft_vertex = -1; // pair[0]: graft-local vertex index
    int base_vertex  = -1; // pair[1]: base-figure vertex index
};

// One sparse geometry.rigidity weight row, in the authored geometry's own
// vertex-index space. No remapping or normalization is performed.
struct GeometryRigidityWeight {
    int vertex_index = -1;
    double weight = 0.0;
};

// One source-order geometry.rigidity group. Strings and vertex/node references
// are retained verbatim; interpreting rotation/scale behavior belongs to the
// consumer.
struct GeometryRigidityGroup {
    std::string id;
    std::string rotation_mode;
    std::vector<std::string> scale_modes;
    std::vector<int> reference_vertices;
    std::vector<int> mask_vertices;
    std::string reference;
    std::vector<std::string> transform_nodes;
    bool use_transform_bones_for_scale = false;

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
    std::vector<MaterialUVAssignment> material_uv_assignments; // authored source-order pairs
    bool is_graft = false; // true iff a populated graft (vertex_pairs) is present;
                           // an empty "graft": {} (base figures, G9 eyes/eyelashes) stays false.
    // Geograft weld correspondence — raw DSON, file-local index space, only
    // meaningful when is_graft. Faithful passthrough (R6.4): no remap/weld.
    std::vector<GraftVertexPair> graft_vertex_pairs; // [graft-local, base-figure] per pair
    std::vector<int> graft_hidden_polys;             // base-figure polys hidden on weld (may be empty)
    Int graft_base_vertex_count;                     // graft.vertex_count: base target resolution; 0 if no graft
    Int graft_base_poly_count;                       // graft.poly_count; 0 if no graft
    bool has_rigidity = false; // true for an authored object, even if its arrays are empty
    std::vector<GeometryRigidityWeight> rigidity_weights;
    std::vector<GeometryRigidityGroup> rigidity_groups;

    bool ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys = nullptr);
};

// One image map layer: layer 0 is the base texture; higher indexes are LIE overlays.
// Per-layer compositing metadata is parsed faithfully from the DAZ map-array element
// as raw values verbatim; no compositing is performed here.
struct ImageLayer {
    std::string url;          // map element "url"; layer 0 == base
    std::string label;        // LIE layer label ("" if absent)
    // Per-layer LIE compositing metadata — raw DAZ map-element values, verbatim.
    std::string blend_op;     // "operation" (e.g. "blend_source_over"); "" if absent
    double opacity   = 1.0;   // "transparency" (1 = opaque, 0 = transparent)
    bool   active    = true;  // "active"
    bool   invert    = false; // "invert"
    Vector3 color    = {};    // "color" [r,g,b] tint (default 0,0,0)
    double rotation  = 0.0;   // "rotation" (degrees)
    double scale_x   = 1.0;   // "xscale"
    double scale_y   = 1.0;   // "yscale"
    double offset_x  = 0.0;   // "xoffset"
    double offset_y  = 0.0;   // "yoffset"
    bool   mirror_x  = false; // "xmirror"
    bool   mirror_y  = false; // "ymirror"
};

// One PBR material channel: scalar/color value plus optional texture reference.
// LIE layers are populated only when the channel resolves by image identity.
struct MaterialChannel {
    std::string type;        // DAZ raw type string ("float", "float_color", "bool", "string", etc.)
    double value = 0.0;      // scalar (roughness, opacity strength, normal strength, etc.)
    Vector3 color = {};      // RGB color; meaningful only when has_color == true
    bool has_color = false;  // true for float_color channels (diffuse, emission, subsurface, etc.)

    std::string image_url;     // raw image reference from JSON (e.g. "#img-0" or a path); empty = no texture
    std::string texture_path;  // resolved absolute file path (filled by the post-parse linkage pass)
    std::vector<ImageLayer> layers; // LIE layer paths/labels; empty for non-LIE and bare-path matches
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

// One operation in a formula's RPN expression.
struct FormulaOperation {
    std::string op;    // "push","mult","div","add","sub","pow","spline_tcb"
    double val = 0.0;  // for op=="push" with a constant SCALAR operand
    std::string url;   // for op=="push" with a channel-reference operand
    std::vector<double> val_array;  // for a push whose "val" is a JSON ARRAY:
                                    // spline_tcb knot [input,output,tension,
                                    // continuity,bias]. Empty unless "val" was
                                    // an array. Raw, unevaluated; scalar `val`
                                    // stays 0.0 in that case.

    bool ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys = nullptr);
};

// A single formula: an RPN expression driving an output channel.
struct Formula {
    std::string output;                        // target channel URL
    std::vector<FormulaOperation> operations;  // evaluated left-to-right
    std::string stage;                         // "sum"/"mult" (optional)

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
    // Stored dial state only; formulas are still exposed as data and never evaluated here.
    double channel_value = 0.0;   // resolved dial: current_value -> value -> 0.0
    double channel_min = 0.0;
    double channel_max = 1.0;
    bool channel_clamped = false;

    // Geometry-shell "Mesh Offset" push modifier (studio/modifier/push nested
    // in extra[]). The push marker and the offset channel are NOT at the
    // modifier top level, so they are parsed separately from the top-level
    // channel above. Faithful, unevaluated (R6.4).
    bool is_push = false;             // extra[].type == "studio/modifier/push" present
    double push_offset_value = 0.0;   // effective offset: current_value -> value -> 0.0 (only when is_push)

    // For morph modifiers - indexed deltas
    bool has_morph = false;
    IndexedVector3Array morph_deltas;
    IndexedVector3Array normal_deltas;

    // For skin_binding modifiers - the "skin" payload
    bool has_skin = false;
    SkinBinding skin;

    // Formula-driven modifiers: JCM/FHM correctives and character control morphs.
    std::vector<Formula> formulas;
    std::string presentation_type;   // presentation.type  (DAZ "Content Type"; "" if absent)
    std::string presentation_label;  // presentation.label (declared display name; "" if absent)
    std::string region;              // modifier-level "region" (DAZ Parameter Settings "Region", e.g. "Chest"; "" if absent)
    std::string group;               // modifier-level "group"  (DAZ Parameter Settings "Path",   e.g. "/Feminine"; "" if absent)
    std::string presentation_icon;   // presentation.icon_large (control thumbnail path, raw/verbatim; "" if absent)

    bool ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys = nullptr);
};

// Image/Texture data. map_file is the base map path; layers retain LIE map-array
// entries (base plus overlays) with full per-layer compositing metadata (blend op,
// opacity, active/invert, color tint, 2D transform) as raw parsed values.
struct Image {
    String id;
    String name;
    Url url;
    String map_file;
    Int map_width;
    Int map_height;
    std::vector<ImageLayer> layers;
    
    bool ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys = nullptr);
};

// UV Set data
struct UVOverride {
    int face = 0;
    int corner = 0;
    int uv_index = 0;
};

struct UVSet {
    String id;
    String name;
    Url url;
    FloatArray uvs;
    int vertex_count = 0;                   // parsed from JSON "vertex_count"; identity-default basis for consumers expanding overrides
    std::vector<UVOverride> uv_overrides;   // parsed from "polygon_vertex_indices" when JSON elements are [face, corner, uv_index] triplets

    bool ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys = nullptr);
};

// One scene.animations keyframe channel, stored faithfully: the verbatim DSON
// property pointer plus the first key's typed value. Per R6.4 the parser never
// applies this onto scene.materials — the consumer resolves the pointer and
// decides. v1 reads the first key only.
struct SceneAnimation {
    // ValueKind: 0 null · 1 number · 2 bool · 3 string · 4 color
    enum ValueKind { KindNull = 0, KindNumber = 1, KindBool = 2, KindString = 3, KindColor = 4 };
    std::string url;        // raw pointer, verbatim (no decode, no decompose)
    int kind = KindNull;    // ValueKind of keys[0][1]
    double number = 0.0;    // kind == KindNumber
    bool boolean = false;   // kind == KindBool
    std::string str;        // kind == KindString (e.g. an image_file path)
    Vector3 color = {};     // kind == KindColor (first 3 of a numeric array)

    bool ParseFromJson(const rapidjson::Value& json, std::set<std::string>* unknownKeys = nullptr);
};

// Post-load addon from a scene "Character Addon Loader" manifest
// (scene.extra[].settings.PostLoadAddons): a companion figure a character preset
// pulls in but does not list in scene.nodes (e.g. Genesis 9 eyes/mouth/eyelashes/
// tears). One entry per addon slot.
struct ScenePostLoadAddon {
    std::string slot;        // PostLoadAddons.value member name (e.g. "Follower/Attachment/Head/Face/Eyes")
    std::string asset_name;  // <slot>.value.AssetName
    std::string asset_file;  // <slot>.value.AssetFile (the loader .duf)
    std::string mat_preset;  // <slot>.value.Presets.value.Mat.value.PresetFile (MAT preset .duf; may be empty)
};

// A DAZ post-load script reference from scene.extra (a
// "scene_post_load_script" entry, or any extra entry that names a script).
// The static parser does NOT execute it; this surfaces the reference so a
// consumer can warn that the script's runtime effects (e.g. texture
// assignment, node dedup) are not captured by a static import. The same
// scene.extra entry may ALSO carry a PostLoadAddons manifest (see
// ScenePostLoadAddon); the two are modeled separately.
struct ScenePostLoadScript {
    std::string type;    // entry "type" (e.g. "scene_post_load_script"); may be ""
    std::string name;    // entry "name" (e.g. "DuplicateNodeRemover"); may be ""
    std::string script;  // entry "script": content-relative .dse/.dsa path; may be ""
};

// Scene - the DSON "scene" object: a collection of instances hooked into the scene.
// Distinct from the *_library definitions on DsonDocument below.
struct Scene {
    std::vector<Node> nodes;
    std::vector<Modifier> modifiers;
    std::vector<Material> materials;
    std::vector<UVSet> uvs;
    std::vector<ScenePostLoadAddon> post_load_addons; // from scene.extra "Character Addon Loader" manifests
    std::vector<ScenePostLoadScript> post_load_scripts; // from scene.extra scene_post_load_script entries (DAZ Scripts; not executed)
    std::vector<SceneAnimation> animations;           // scene.animations: parsed faithfully, never applied onto scene.materials (R6.4)
    // Recognized by the parser but not (fully) read into typed fields:
    //   presentation, current_camera, extra
    //   (extra: its PostLoadAddons "Character Addon Loader" manifest and its
    //    scene_post_load_script references are modeled; the rest is not)

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
    bool LoadFromBuffer(const char* data, size_t size, std::string& errorMsg);
    
    // Legacy overloads without error messages (for backwards compatibility)
    bool LoadFromFile(const char* filepath);
    bool LoadFromString(const char* jsonString);
    
    // Get unknown keys by context
    std::vector<std::string> GetUnknownKeys(const std::string& context) const;
    std::vector<std::string> GetAllContextsWithUnknownKeys() const;
    
    void Clear();
};

} // namespace Dson
