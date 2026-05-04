#pragma once

#include "Asset/Asset.h"
#include "Renderer/Camera.h"
#include "EntityManager.h"

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <cstdint>
#include <memory>
#include <string>

namespace luna {

enum class SceneBackgroundMode : uint8_t {
    SolidColor,
    ProceduralSky,
    EnvironmentMap,
};

struct SceneEnvironmentSettings {
    SceneBackgroundMode backgroundMode = SceneBackgroundMode::ProceduralSky;
    glm::vec3 backgroundColor{0.10f, 0.10f, 0.12f};
    // Legacy serialized visibility flag. New code should use backgroundMode.
    bool enabled = true;
    bool iblEnabled = true;
    AssetHandle environmentMapHandle = AssetHandle(0);
    float intensity = 1.0f;
    float skyIntensity = 1.0f;
    float diffuseIntensity = 1.0f;
    float specularIntensity = 1.0f;

    glm::vec3 proceduralSunDirection = glm::normalize(glm::vec3(0.4f, 0.6f, 0.3f));
    float proceduralSunIntensity = 20.0f;
    float proceduralSunAngularRadius = 0.02f;
    glm::vec3 proceduralSkyColorZenith{0.15f, 0.30f, 0.60f};
    glm::vec3 proceduralSkyColorHorizon{0.60f, 0.50f, 0.40f};
    glm::vec3 proceduralGroundColor{0.10f, 0.08f, 0.06f};
    float proceduralSkyExposure = 1.5f;
};

class Scene {
public:
    enum class AssetLoadBehavior {
        Blocking,
        NonBlocking,
    };

    Scene();
    ~Scene() = default;

    [[nodiscard]] std::unique_ptr<Scene> clone() const;

    void renderFromRuntimeCamera();
    void renderFromEditorCamera(const Camera& camera);
    void setAssetLoadBehavior(AssetLoadBehavior behavior);
    AssetLoadBehavior getAssetLoadBehavior() const;

    void setName(std::string name);
    const std::string& getName() const;
    void setIblEnabled(bool enabled);
    bool isIblEnabled() const;
    SceneEnvironmentSettings& environmentSettings();
    const SceneEnvironmentSettings& environmentSettings() const;

    EntityManager& entityManager();
    const EntityManager& entityManager() const;

private:
    void submitScene(const Camera& camera);
    bool findPrimaryRuntimeCamera(Camera& camera) const;

private:
    std::string m_name{"Untitled"};
    AssetLoadBehavior m_asset_load_behavior = AssetLoadBehavior::Blocking;
    SceneEnvironmentSettings m_environment_settings{};
    EntityManager m_entity_manager;
};

} // namespace luna
