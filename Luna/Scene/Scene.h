#pragma once

#include "Renderer/Camera.h"
#include "EntityManager.h"

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
    void onUpdateRuntime(const Camera& camera);
    void setAssetLoadBehavior(AssetLoadBehavior behavior);
    AssetLoadBehavior getAssetLoadBehavior() const;

    void setName(std::string name);
    const std::string& getName() const;

    EntityManager& entityManager();
    const EntityManager& entityManager() const;

private:
    std::string m_name{"Untitled"};
    AssetLoadBehavior m_asset_load_behavior = AssetLoadBehavior::Blocking;
    EntityManager m_entity_manager;
};

} // namespace luna
