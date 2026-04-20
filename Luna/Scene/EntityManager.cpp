#include "EntityManager.h"

#include "Core/Log.h"
#include "Entity.h"

#include <algorithm>
#include <glm/matrix.hpp>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

bool isOwnedBy(const luna::Entity& entity, const luna::EntityManager* entity_manager)
{
    return entity && entity.getEntityManager() == entity_manager;
}

void removeChildUUID(luna::RelationshipComponent& relationship, luna::UUID child_uuid)
{
    auto& children = relationship.children;
    children.erase(std::remove(children.begin(), children.end(), child_uuid), children.end());
}

void addChildUUID(luna::RelationshipComponent& relationship, luna::UUID child_uuid)
{
    auto& children = relationship.children;
    if (std::find(children.begin(), children.end(), child_uuid) == children.end()) {
        children.push_back(child_uuid);
    }
}

} // namespace

namespace luna {

EntityManager::EntityManager(Scene* scene)
    : m_scene(scene)
{}

Entity EntityManager::createEntity(const std::string& name)
{
    return createEntityWithUUID(UUID{}, name);
}

Entity EntityManager::createEntityWithUUID(UUID uuid, const std::string& name)
{
    UUID resolved_uuid = uuid;
    if (!resolved_uuid.isValid() || m_entity_map.contains(resolved_uuid)) {
        if (resolved_uuid.isValid()) {
            LUNA_CORE_WARN("Duplicate entity UUID '{}' detected. Generating a new UUID.", resolved_uuid.toString());
        }

        do {
            resolved_uuid = UUID{};
        } while (m_entity_map.contains(resolved_uuid));
    }

    const entt::entity entity_handle = m_registry.create();
    Entity entity(entity_handle, this);

    entity.addComponent<IDComponent>(resolved_uuid);
    entity.addComponent<TagComponent>(name.empty() ? "Entity" : name);
    entity.addComponent<TransformComponent>();
    entity.addComponent<RelationshipComponent>();
    m_entity_map[resolved_uuid] = entity_handle;

    return entity;
}

Entity EntityManager::createChildEntity(Entity parent, const std::string& name)
{
    if (!isOwnedBy(parent, this)) {
        LUNA_CORE_WARN("Cannot create child entity because the parent is invalid or belongs to another EntityManager");
        return {};
    }

    Entity child = createEntity(name);
    if (!setParent(child, parent, false)) {
        destroyEntity(child);
        return {};
    }

    return child;
}

Entity EntityManager::findEntityByUUID(UUID uuid)
{
    if (!uuid.isValid()) {
        return {};
    }

    const auto it = m_entity_map.find(uuid);
    if (it == m_entity_map.end()) {
        return {};
    }

    if (!m_registry.valid(it->second)) {
        m_entity_map.erase(it);
        return {};
    }

    return Entity(it->second, this);
}

Entity EntityManager::findEntityByUUID(UUID uuid) const
{
    return const_cast<EntityManager*>(this)->findEntityByUUID(uuid);
}

bool EntityManager::containsEntity(UUID uuid) const
{
    if (!uuid.isValid()) {
        return false;
    }

    const auto it = m_entity_map.find(uuid);
    return it != m_entity_map.end() && m_registry.valid(it->second);
}

bool EntityManager::setParent(Entity child, Entity parent, bool preserve_world_transform)
{
    if (!isOwnedBy(child, this)) {
        LUNA_CORE_WARN("Cannot set entity parent because the child is invalid or belongs to another EntityManager");
        return false;
    }

    if (parent && !isOwnedBy(parent, this)) {
        LUNA_CORE_WARN("Cannot set entity parent because the parent belongs to another EntityManager");
        return false;
    }

    if (parent && child == parent) {
        LUNA_CORE_WARN("Cannot parent entity '{}' to itself", child.getUUID().toString());
        return false;
    }

    if (parent) {
        for (Entity current = parent; current; current = current.getParent()) {
            if (current == child) {
                LUNA_CORE_WARN("Cannot create cyclic entity hierarchy for entity '{}'", child.getUUID().toString());
                return false;
            }
        }
    }

    const UUID previous_parent_uuid = child.getParentUUID();
    const UUID next_parent_uuid = parent ? parent.getUUID() : UUID(0);
    if (previous_parent_uuid == next_parent_uuid) {
        return true;
    }

    const glm::mat4 child_world_transform =
        preserve_world_transform ? getWorldSpaceTransformMatrix(child) : child.transform().getTransform();

    if (Entity previous_parent = findEntityByUUID(previous_parent_uuid); previous_parent) {
        removeChildUUID(previous_parent.getComponent<RelationshipComponent>(), child.getUUID());
    }

    auto& relationship = child.getComponent<RelationshipComponent>();
    relationship.parentHandle = next_parent_uuid;

    if (parent) {
        addChildUUID(parent.getComponent<RelationshipComponent>(), child.getUUID());
    }

    if (preserve_world_transform) {
        setWorldSpaceTransform(child, child_world_transform);
    }

    return true;
}

void EntityManager::destroyEntity(Entity entity)
{
    if (!isOwnedBy(entity, this) || !m_registry.valid(static_cast<entt::entity>(entity))) {
        return;
    }

    std::vector<Entity> destroy_order;
    std::vector<Entity> stack{entity};
    std::unordered_set<UUID> visited;
    destroy_order.reserve(8);

    while (!stack.empty()) {
        Entity current = stack.back();
        stack.pop_back();

        if (!isOwnedBy(current, this) || !m_registry.valid(static_cast<entt::entity>(current))) {
            continue;
        }

        if (!visited.insert(current.getUUID()).second) {
            continue;
        }

        destroy_order.push_back(current);
        for (const UUID child_uuid : current.getChildren()) {
            if (Entity child = findEntityByUUID(child_uuid); child) {
                stack.push_back(child);
            }
        }
    }

    for (auto it = destroy_order.rbegin(); it != destroy_order.rend(); ++it) {
        Entity current = *it;
        if (!isOwnedBy(current, this) || !m_registry.valid(static_cast<entt::entity>(current))) {
            continue;
        }

        if (Entity parent = current.getParent(); parent) {
            removeChildUUID(parent.getComponent<RelationshipComponent>(), current.getUUID());
        }

        m_entity_map.erase(current.getUUID());
        m_registry.destroy(static_cast<entt::entity>(current));
    }
}

void EntityManager::clear()
{
    m_registry.clear();
    m_entity_map.clear();
}

size_t EntityManager::entityCount() const
{
    return m_entity_map.size();
}

glm::mat4 EntityManager::getWorldSpaceTransformMatrix(Entity entity) const
{
    if (!isOwnedBy(entity, this)) {
        return glm::mat4(1.0f);
    }

    std::vector<Entity> hierarchy_chain;
    std::unordered_set<UUID> visited;
    hierarchy_chain.reserve(8);

    for (Entity current = entity; current; current = current.getParent()) {
        if (!visited.insert(current.getUUID()).second) {
            LUNA_CORE_WARN("Cycle detected while computing world transform for entity '{}'",
                           entity.getUUID().toString());
            break;
        }

        hierarchy_chain.push_back(current);
    }

    glm::mat4 world_transform(1.0f);
    for (auto it = hierarchy_chain.rbegin(); it != hierarchy_chain.rend(); ++it) {
        world_transform *= it->transform().getTransform();
    }

    return world_transform;
}

TransformComponent EntityManager::getWorldSpaceTransform(Entity entity) const
{
    TransformComponent world_transform;
    world_transform.setTransform(getWorldSpaceTransformMatrix(entity));
    return world_transform;
}

void EntityManager::setWorldSpaceTransform(Entity entity, const glm::mat4& world_transform)
{
    if (!isOwnedBy(entity, this)) {
        return;
    }

    glm::mat4 local_transform = world_transform;
    if (Entity parent = entity.getParent(); parent) {
        const glm::mat4 parent_world_transform = getWorldSpaceTransformMatrix(parent);
        local_transform = glm::inverse(parent_world_transform) * world_transform;
    }

    entity.transform().setTransform(local_transform);
}

void EntityManager::setWorldSpaceTransform(Entity entity, const TransformComponent& world_transform)
{
    setWorldSpaceTransform(entity, world_transform.getTransform());
}

bool EntityManager::convertToWorldSpace(Entity entity)
{
    if (!isOwnedBy(entity, this) || !entity.hasParent()) {
        return false;
    }

    return setParent(entity, {}, true);
}

bool EntityManager::convertToLocalSpace(Entity entity)
{
    if (!isOwnedBy(entity, this)) {
        return false;
    }

    Entity parent = entity.getParent();
    if (!parent) {
        return false;
    }

    const glm::mat4 parent_world_transform = getWorldSpaceTransformMatrix(parent);
    const glm::mat4 local_transform = glm::inverse(parent_world_transform) * entity.transform().getTransform();
    entity.transform().setTransform(local_transform);
    return true;
}

entt::registry& EntityManager::registry()
{
    return m_registry;
}

const entt::registry& EntityManager::registry() const
{
    return m_registry;
}

Scene* EntityManager::getScene()
{
    return m_scene;
}

const Scene* EntityManager::getScene() const
{
    return m_scene;
}

} // namespace luna
