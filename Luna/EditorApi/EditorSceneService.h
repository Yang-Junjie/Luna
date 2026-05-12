#pragma once

#include "EditorApi/EditorTypes.h"
#include "Scene/Scene.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace luna::editor {

enum class SceneEntityCreateKind : uint8_t {
    Empty,
    Camera,
    DirectionalLight,
    PointLight,
    SpotLight,
    PrimitiveMesh,
    MeshAsset,
    ModelAsset,
};

enum class SceneComponentKind : uint8_t {
    Transform,
    Camera,
    Light,
    Mesh,
    Script,
};

enum class SceneCameraProjection : uint8_t {
    Perspective,
    Orthographic,
};

enum class SceneLightType : uint8_t {
    Directional,
    Point,
    Spot,
};

enum class SceneScriptPropertyType : uint8_t {
    Bool,
    Int,
    Float,
    String,
    Vec3,
    Entity,
    Asset,
};

struct SceneTransform {
    Vec3 translation{};
    Vec3 rotation_degrees{};
    Vec3 scale{.x = 1.0f, .y = 1.0f, .z = 1.0f};
};

struct SceneCameraComponent {
    bool primary{true};
    bool fixed_aspect_ratio{false};
    SceneCameraProjection projection{SceneCameraProjection::Perspective};
    float perspective_vertical_fov_degrees{50.0f};
    float perspective_near{0.05f};
    float perspective_far{500.0f};
    float orthographic_size{10.0f};
    float orthographic_near{-100.0f};
    float orthographic_far{100.0f};
};

struct SceneLightComponent {
    SceneLightType type{SceneLightType::Directional};
    bool enabled{true};
    Vec3 color{.x = 1.0f, .y = 1.0f, .z = 1.0f};
    float intensity{4.0f};
    float range{10.0f};
    float inner_cone_angle_degrees{20.0f};
    float outer_cone_angle_degrees{35.0f};
};

struct SceneMeshComponent {
    static constexpr uint32_t AllSubmeshes = UINT32_MAX;

    AssetHandle mesh_handle{0};
    uint32_t first_submesh{0};
    uint32_t submesh_count{AllSubmeshes};
    std::vector<AssetHandle> submesh_materials;
};

struct SceneScriptPropertyOption {
    std::string label;
    int int_value{0};
    std::string string_value;
};

struct SceneScriptPropertyMetadata {
    std::string display_name;
    std::string description;
    std::string category;
    bool has_min_value{false};
    bool has_max_value{false};
    bool has_step_value{false};
    float min_value{0.0f};
    float max_value{0.0f};
    float step_value{0.0f};
    std::string asset_type;
    std::string entity_filter;
    std::vector<SceneScriptPropertyOption> options;
};

struct SceneScriptProperty {
    std::string name;
    SceneScriptPropertyType type{SceneScriptPropertyType::Float};
    bool bool_value{false};
    int int_value{0};
    float float_value{0.0f};
    std::string string_value;
    Vec3 vec3_value{};
    EntityId entity_value{0};
    AssetHandle asset_value{0};
    SceneScriptPropertyMetadata metadata;
};

struct SceneScriptEntry {
    ScriptEntryId id{0};
    bool enabled{true};
    AssetHandle script_asset{0};
    std::string type_name;
    int execution_order{0};
    std::vector<SceneScriptProperty> properties;
};

struct SceneScriptComponent {
    bool enabled{true};
    std::vector<SceneScriptEntry> scripts;
};

struct SceneEntityReference {
    EntityId id{0};
    std::string name;
};

struct SceneEntityComponents {
    bool transform{false};
    bool camera{false};
    bool light{false};
    bool mesh{false};
    bool script{false};
};

struct SceneEntityInfo {
    EntityId id{0};
    EntityId parent_id{0};
    std::string name;
    std::vector<EntityId> child_ids;
};

struct SceneEntityCreateRequest {
    SceneEntityCreateKind kind{SceneEntityCreateKind::Empty};
    std::string name;
    EntityId parent_id{0};
    AssetHandle asset_handle{0};
};

struct SceneEntityDetails {
    EntityId id{0};
    EntityId parent_id{0};
    std::string name;
    std::string parent_name;
    std::vector<SceneEntityReference> children;
    SceneEntityComponents components{};
    SceneTransform transform{};
    std::optional<SceneCameraComponent> camera;
    std::optional<SceneLightComponent> light;
    std::optional<SceneMeshComponent> mesh;
    std::optional<SceneScriptComponent> script;
};

class SceneService {
public:
    virtual ~SceneService() = default;

    virtual std::string sceneLabel() const = 0;
    virtual size_t entityCount() const = 0;
    virtual bool canEditScene() const noexcept = 0;
    virtual std::vector<SceneEntityInfo> entityHierarchy() const = 0;
    virtual bool entityExists(EntityId entity_id) const noexcept = 0;
    virtual std::optional<SceneEntityDetails> entityDetails(EntityId entity_id) const = 0;
    virtual bool isEntityDescendantOf(EntityId entity_id, EntityId potential_ancestor_id) const = 0;
    virtual SceneEnvironmentSettings sceneEnvironmentSettings() const = 0;
    virtual SceneShadowSettings sceneShadowSettings() const = 0;
    virtual bool setSceneEnvironmentSettings(const SceneEnvironmentSettings& settings) = 0;
    virtual bool setSceneShadowSettings(const SceneShadowSettings& settings) = 0;

    virtual EntityId createEntity(std::string name) = 0;
    virtual EntityId createEntity(const SceneEntityCreateRequest& request) = 0;
    virtual bool destroyEntity(EntityId entity_id) = 0;
    virtual bool reparentEntity(EntityId entity_id, EntityId new_parent_id, bool preserve_world_transform = true) = 0;
    virtual bool setEntityName(EntityId entity_id, std::string name) = 0;
    virtual bool setEntityTransform(EntityId entity_id, const SceneTransform& transform) = 0;
    virtual bool setCameraComponent(EntityId entity_id, const SceneCameraComponent& camera_component) = 0;
    virtual bool setLightComponent(EntityId entity_id, const SceneLightComponent& light_component) = 0;
    virtual bool setMeshComponent(EntityId entity_id, const SceneMeshComponent& mesh_component) = 0;
    virtual bool setScriptComponent(EntityId entity_id, const SceneScriptComponent& script_component) = 0;
    virtual bool setScriptProperty(EntityId entity_id,
                                   std::size_t script_index,
                                   std::size_t property_index,
                                   const SceneScriptProperty& property) = 0;
    virtual bool addComponent(EntityId entity_id, SceneComponentKind component_kind) = 0;
    virtual bool removeComponent(EntityId entity_id, SceneComponentKind component_kind) = 0;
    virtual bool applyMeshAssetToEntity(EntityId entity_id, AssetHandle mesh_handle) = 0;
};

} // namespace luna::editor
