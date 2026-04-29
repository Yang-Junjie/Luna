#pragma once

#include "EntityManager.h"
#include "Renderer/Camera.h"

#include <string>

namespace luna {

class Scene {
public:
    enum class AssetLoadBehavior {
        Blocking,
        NonBlocking,
    };

    Scene();
    ~Scene() = default;

    void onUpdateRuntime();
    void onUpdateEditor(const Camera& camera);
    void setAssetLoadBehavior(AssetLoadBehavior behavior);
    AssetLoadBehavior getAssetLoadBehavior() const;

    void setName(std::string name);
    const std::string& getName() const;

    EntityManager& entityManager();
    const EntityManager& entityManager() const;

private:
    void submitScene(const Camera& camera);
    bool findPrimaryRuntimeCamera(Camera& camera) const;

private:
    std::string m_name{"Untitled"};
    AssetLoadBehavior m_asset_load_behavior = AssetLoadBehavior::Blocking;
    EntityManager m_entity_manager;
};

} // namespace luna
