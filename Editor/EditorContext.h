#pragma once

#include "Asset/Asset.h"
#include "Core/UUID.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Script/ScriptPluginManifest.h"

#include <cstddef>

#include <filesystem>
#include <string>
#include <vector>

namespace luna {

class EditorContext {
public:
    virtual ~EditorContext() = default;

    virtual Scene& getScene() = 0;
    virtual Scene& getInspectionScene() = 0;
    virtual bool isRuntimeViewportEnabled() const noexcept = 0;
    virtual Entity getSelectedEntity() = 0;
    virtual void setSelectedEntity(Entity entity) = 0;
    virtual void markSceneDirty() = 0;
    virtual void patchRuntimeScriptProperty(UUID entity_id, size_t script_index, size_t property_index) = 0;

    virtual bool openSceneFile(const std::filesystem::path& scene_file_path) = 0;
    virtual Entity createEntity(const std::string& name = std::string{}, Entity parent = {}) = 0;
    virtual Entity createEntityFromModelAsset(AssetHandle model_handle, Entity parent = {}) = 0;
    virtual Entity createEntityFromMeshAsset(AssetHandle mesh_handle, Entity parent = {}) = 0;
    virtual Entity createPrimitiveEntity(AssetHandle mesh_handle, Entity parent = {}) = 0;
    virtual Entity createCameraEntity(Entity parent = {}) = 0;
    virtual Entity createDirectionalLightEntity(Entity parent = {}) = 0;
    virtual Entity createPointLightEntity(Entity parent = {}) = 0;
    virtual Entity createSpotLightEntity(Entity parent = {}) = 0;
    virtual bool destroyEntity(Entity entity) = 0;
    virtual bool reparentEntity(Entity entity, Entity parent, bool preserve_world_transform = true) = 0;
    virtual void applyMeshAssetToEntity(Entity entity, AssetHandle mesh_handle) = 0;
    virtual bool setSceneEnvironmentSettings(const SceneEnvironmentSettings& settings) = 0;
    virtual bool setSceneShadowSettings(const SceneShadowSettings& settings) = 0;
    virtual void openBuiltinMaterialsPanel(AssetHandle material_handle = AssetHandle(0)) = 0;

    virtual bool hasProjectLoaded() const = 0;
    virtual void refreshProjectScriptPlugins() = 0;
    virtual const std::vector<ScriptPluginCandidate>& getDiscoveredScriptPlugins() const = 0;
    virtual const std::string& getScriptPluginStatus() const = 0;
    virtual const ScriptPluginCandidate* getSelectedScriptPluginCandidate() const = 0;
    virtual bool selectScriptPlugin(const ScriptPluginCandidate* candidate) = 0;
};

} // namespace luna
