#pragma once

#include "Components.h"

#include <cstddef>

#include <entt/entity/registry.hpp>
#include <glm/mat4x4.hpp>
#include <string>
#include <unordered_map>

namespace luna {

class Entity;
class Scene;

class EntityManager {
public:
    explicit EntityManager(Scene* scene);

    Entity createEntity(const std::string& name = std::string{});
    Entity createEntityWithUUID(UUID uuid, const std::string& name = std::string{});
    Entity createChildEntity(Entity parent, const std::string& name = std::string{});
    Entity findEntityByUUID(UUID uuid);
    Entity findEntityByUUID(UUID uuid) const;
    bool containsEntity(UUID uuid) const;
    bool setParent(Entity child, Entity parent, bool preserve_world_transform = true);
    void destroyEntity(Entity entity);
    void clear();
    size_t entityCount() const;
    glm::mat4 getWorldSpaceTransformMatrix(Entity entity) const;
    TransformComponent getWorldSpaceTransform(Entity entity) const;
    void setWorldSpaceTransform(Entity entity, const glm::mat4& world_transform);
    void setWorldSpaceTransform(Entity entity, const TransformComponent& world_transform);
    bool convertToWorldSpace(Entity entity);
    bool convertToLocalSpace(Entity entity);

    entt::registry& registry();
    const entt::registry& registry() const;

    Scene* getScene();
    const Scene* getScene() const;

private:
    Scene* m_scene = nullptr;
    std::unordered_map<UUID, entt::entity> m_entity_map;
    entt::registry m_registry;
};

} // namespace luna
