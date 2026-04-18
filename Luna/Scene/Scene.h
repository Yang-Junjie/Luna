#pragma once

#include "Components.h"

#include <entt/entity/registry.hpp>
#include <string>

namespace luna {

class Entity;

class Scene {
public:
    Scene() = default;
    ~Scene() = default;

    Entity createEntity(const std::string& name = std::string{});
    Entity createEntityWithUUID(UUID uuid, const std::string& name = std::string{});
    void destroyEntity(Entity entity);
    void onUpdateRuntime();
    entt::registry& registry();
    const entt::registry& registry() const;

private:
    entt::registry m_registry;

    friend class Entity;
};

} // namespace luna
