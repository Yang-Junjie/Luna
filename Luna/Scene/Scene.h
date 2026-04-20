#pragma once

#include "Components.h"

#include <cstddef>
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
    void clear();
    void onUpdateRuntime();
    void setName(std::string name);
    const std::string& getName() const;
    size_t entityCount() const;
    entt::registry& registry();
    const entt::registry& registry() const;

private:
    std::string m_name{"Untitled"};
    entt::registry m_registry;

    friend class Entity;
};

} // namespace luna
