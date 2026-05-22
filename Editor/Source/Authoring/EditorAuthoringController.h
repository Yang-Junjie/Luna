#pragma once

#include "EditorDocumentContext.h"
#include "EditorRuntime.h"
#include "Scene/Entity.h"

#include <filesystem>
#include <string>

namespace luna {

class EditorAuthoringController final {
public:
    EditorAuthoringController();

    [[nodiscard]] EditorDocumentContext& documentContext() noexcept;
    [[nodiscard]] const EditorDocumentContext& documentContext() const noexcept;

    [[nodiscard]] Scene& scene() noexcept;
    [[nodiscard]] const Scene& scene() const noexcept;
    [[nodiscard]] Scene& documentScene() noexcept;
    [[nodiscard]] const std::string& assetLabel() const noexcept;

    void reset();
    [[nodiscard]] authoring::SceneBootstrapResult createScene();
    [[nodiscard]] bool openScene(const std::filesystem::path& scene_file_path);
    [[nodiscard]] bool saveSceneAs(const std::filesystem::path& scene_file_path);
    void setSceneFilePath(std::filesystem::path scene_file_path);
    [[nodiscard]] const std::filesystem::path& sceneFilePath() const noexcept;

    [[nodiscard]] UUID selectedEntityId() const noexcept;
    [[nodiscard]] Entity selectedEntity(Scene& inspection_scene) const;
    void setSelectedEntity(Entity entity) noexcept;
    void setSelectedEntityId(UUID entity_id) noexcept;
    void clearSelection() noexcept;

    void markSceneDirty();
    [[nodiscard]] bool isSceneDirty() const noexcept;
    [[nodiscard]] bool hasOpenTransaction() const noexcept;
    [[nodiscard]] bool beginTransaction(std::string name);
    [[nodiscard]] bool commitTransaction();
    [[nodiscard]] bool rollbackTransaction();
    [[nodiscard]] bool undo();
    [[nodiscard]] bool redo();
    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;

    [[nodiscard]] Entity createEntity(const std::string& name = std::string{}, Entity parent = {});
    [[nodiscard]] Entity createEntityFromModelAsset(AssetHandle model_handle, Entity parent = {});
    [[nodiscard]] Entity createEntityFromMeshAsset(AssetHandle mesh_handle, Entity parent = {});
    [[nodiscard]] Entity createPrimitiveEntity(AssetHandle mesh_handle, Entity parent = {});
    [[nodiscard]] Entity createCameraEntity(Entity parent = {});
    [[nodiscard]] Entity createDirectionalLightEntity(Entity parent = {});
    [[nodiscard]] Entity createPointLightEntity(Entity parent = {});
    [[nodiscard]] Entity createSpotLightEntity(Entity parent = {});
    [[nodiscard]] bool destroyEntity(Entity entity);
    [[nodiscard]] bool reparentEntity(Entity entity, Entity parent, bool preserve_world_transform = true);
    [[nodiscard]] bool addComponent(Entity entity, authoring::AuthoringComponentKind component_kind);
    [[nodiscard]] bool removeComponent(Entity entity, authoring::AuthoringComponentKind component_kind);
    [[nodiscard]] bool applyMeshAssetToEntity(Entity entity, AssetHandle mesh_handle);
    [[nodiscard]] bool setEntityName(Entity entity, std::string name);
    [[nodiscard]] bool setEntityTransform(Entity entity, const TransformComponent& transform);
    [[nodiscard]] bool setCameraComponent(Entity entity, const CameraComponent& camera_component);
    [[nodiscard]] bool setLightComponent(Entity entity, const LightComponent& light_component);
    [[nodiscard]] bool setMeshComponent(Entity entity, const MeshComponent& mesh_component);
    [[nodiscard]] bool setScriptComponent(Entity entity, const ScriptComponent& script_component);
    [[nodiscard]] bool setSceneEnvironmentSettings(const SceneEnvironmentSettings& settings);
    [[nodiscard]] bool setSceneShadowSettings(const SceneShadowSettings& settings);

    void processEvents();
    void updateAssetLabel();

private:
    [[nodiscard]] static std::filesystem::path relativeScenePathToProject(
        const std::filesystem::path& scene_file_path);
    [[nodiscard]] static std::string makeAssetLabel(const EditorRuntime& runtime);

    EditorRuntime m_runtime;
    EditorDocumentContext m_document_context{"luna.document.authoring.scene",
                                             EditorDocumentKind::AuthoringScene,
                                             true};
    std::string m_asset_label{"No scene loaded"};
};

} // namespace luna
