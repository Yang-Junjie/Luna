#pragma once

#include "Asset/Asset.h"
#include "Authoring/AuthoringTypes.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

#include <filesystem>
#include <string>
#include <vector>

namespace luna::authoring {

struct SceneBootstrapResult {
    Entity camera{};
    Entity directional_light{};
};

class AuthoringSession {
public:
    AuthoringSession() = default;
    explicit AuthoringSession(Scene& scene);

    void bindScene(Scene& scene);
    [[nodiscard]] bool hasScene() const noexcept;

    [[nodiscard]] Scene& scene();
    [[nodiscard]] const Scene& scene() const;

    void setSceneFilePath(std::filesystem::path scene_file_path);
    [[nodiscard]] const std::filesystem::path& sceneFilePath() const noexcept;

    [[nodiscard]] bool isSceneDirty() const noexcept;
    void markSceneDirty();
    void clearSceneDirty();

    [[nodiscard]] std::vector<AuthoringEvent> consumeEvents();

    void resetScene();
    [[nodiscard]] SceneBootstrapResult createScene();

    [[nodiscard]] bool openScene(const std::filesystem::path& scene_file_path);
    [[nodiscard]] bool saveScene();
    [[nodiscard]] bool saveSceneAs(const std::filesystem::path& scene_file_path);

    [[nodiscard]] Entity createEntity(const std::string& name = std::string{}, Entity parent = {});
    [[nodiscard]] Entity createCameraEntity(Entity parent = {});
    [[nodiscard]] Entity createDirectionalLightEntity(Entity parent = {});
    [[nodiscard]] Entity createPointLightEntity(Entity parent = {});
    [[nodiscard]] Entity createSpotLightEntity(Entity parent = {});
    [[nodiscard]] bool destroyEntity(Entity entity);
    [[nodiscard]] bool reparentEntity(Entity entity, Entity parent, bool preserve_world_transform = true);

    [[nodiscard]] Entity createEntityFromModelAsset(AssetHandle model_handle, Entity parent = {});
    [[nodiscard]] Entity createEntityFromMeshAsset(AssetHandle mesh_handle, Entity parent = {});
    [[nodiscard]] Entity createPrimitiveEntity(AssetHandle mesh_handle, Entity parent = {});
    [[nodiscard]] bool applyMeshAssetToEntity(Entity entity, AssetHandle mesh_handle);
    [[nodiscard]] bool setSceneEnvironmentSettings(const SceneEnvironmentSettings& settings);
    [[nodiscard]] bool setSceneShadowSettings(const SceneShadowSettings& settings);

private:
    void queueEvent(AuthoringEvent event);
    [[nodiscard]] bool hasBoundScene() const noexcept;

private:
    Scene* m_scene{nullptr};
    std::filesystem::path m_scene_file_path;
    bool m_scene_dirty{false};
    std::vector<AuthoringEvent> m_events;
};

} // namespace luna::authoring
