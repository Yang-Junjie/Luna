#pragma once

#include "EditorApi/EditorTypes.h"
#include "Scene/Scene.h"

#include <cstddef>
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

class SceneService {
public:
    virtual ~SceneService() = default;

    virtual std::string sceneLabel() const = 0;
    virtual size_t entityCount() const = 0;
    virtual bool canEditScene() const noexcept = 0;
    virtual std::vector<SceneEntityInfo> entityHierarchy() const = 0;
    virtual bool entityExists(EntityId entity_id) const noexcept = 0;
    virtual bool isEntityDescendantOf(EntityId entity_id, EntityId potential_ancestor_id) const = 0;
    virtual SceneEnvironmentSettings sceneEnvironmentSettings() const = 0;
    virtual SceneShadowSettings sceneShadowSettings() const = 0;
    virtual bool setSceneEnvironmentSettings(const SceneEnvironmentSettings& settings) = 0;
    virtual bool setSceneShadowSettings(const SceneShadowSettings& settings) = 0;

    virtual EntityId createEntity(std::string name) = 0;
    virtual EntityId createEntity(const SceneEntityCreateRequest& request) = 0;
    virtual bool destroyEntity(EntityId entity_id) = 0;
    virtual bool reparentEntity(EntityId entity_id, EntityId new_parent_id, bool preserve_world_transform = true) = 0;
    virtual bool applyMeshAssetToEntity(EntityId entity_id, AssetHandle mesh_handle) = 0;
};

} // namespace luna::editor
