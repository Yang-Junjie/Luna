#pragma once

#include "Asset/Asset.h"
#include "Authoring/AuthoringHost.h"
#include "Authoring/AuthoringProtocol.h"
#include "Authoring/AuthoringSession.h"
#include "Authoring/AuthoringTypes.h"
#include "Core/UUID.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace luna {

class EditorRuntime final {
public:
    EditorRuntime();

    [[nodiscard]] Scene& scene() noexcept;
    [[nodiscard]] const Scene& scene() const noexcept;

    [[nodiscard]] authoring::AuthoringSession& authoringSession() noexcept;
    [[nodiscard]] const authoring::AuthoringSession& authoringSession() const noexcept;

    [[nodiscard]] UUID selectedEntityId() const noexcept;
    [[nodiscard]] Entity selectedEntity();
    [[nodiscard]] Entity selectedEntity(Scene& inspection_scene) const;
    void setSelectedEntity(Entity entity) noexcept;
    void setSelectedEntityId(UUID entity_id) noexcept;
    void clearSelection() noexcept;
    void validateSelection();

    [[nodiscard]] bool isSceneDirty() const noexcept;
    void markSceneDirty();
    void clearSceneDirty();
    void resetScene();
    [[nodiscard]] authoring::SceneBootstrapResult createScene();

    void setSceneFilePath(std::filesystem::path scene_file_path);
    [[nodiscard]] const std::filesystem::path& sceneFilePath() const noexcept;
    [[nodiscard]] bool openScene(const std::filesystem::path& scene_file_path);
    [[nodiscard]] bool saveScene();
    [[nodiscard]] bool saveSceneAs(const std::filesystem::path& scene_file_path);

    [[nodiscard]] bool beginTransaction(std::string name);
    [[nodiscard]] bool commitTransaction();
    [[nodiscard]] bool rollbackTransaction();
    [[nodiscard]] bool hasOpenTransaction() const noexcept;
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

    [[nodiscard]] bool executeAuthoringPlan(const authoring::AuthoringPlan& plan,
                                            authoring::AuthoringReport& report);
    [[nodiscard]] authoring::AuthoringSceneSnapshot captureSceneSnapshot() const;
    [[nodiscard]] std::vector<authoring::AuthoringEvent> consumeAuthoringEvents();

private:
    std::unique_ptr<Scene> m_scene;
    authoring::AuthoringSession m_authoring_session;
    authoring::AuthoringHost m_authoring_host;
    UUID m_selected_entity_id{0};
};

} // namespace luna
