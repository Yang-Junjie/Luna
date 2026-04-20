#pragma once

#include "EntityManager.h"

#include <cstdint>

#include <utility>

namespace luna {

class Entity {
public:
    Entity() = default;
    Entity(entt::entity entity_handle, EntityManager* entity_manager);
    Entity(const Entity&) = default;

    template <typename T, typename... Args> T& addComponent(Args&&... args)
    {
        return m_entity_manager->registry().template emplace<T>(m_entity_handle, std::forward<Args>(args)...);
    }

    template <typename T> T& getComponent()
    {
        return m_entity_manager->registry().template get<T>(m_entity_handle);
    }

    template <typename T> const T& getComponent() const
    {
        const auto& registry = m_entity_manager->registry();
        return registry.template get<T>(m_entity_handle);
    }

    template <typename T> bool hasComponent() const
    {
        return m_entity_manager != nullptr && m_entity_handle != entt::null &&
               m_entity_manager->registry().template all_of<T>(m_entity_handle);
    }

    template <typename T> void removeComponent()
    {
        if (hasComponent<T>()) {
            m_entity_manager->registry().template remove<T>(m_entity_handle);
        }
    }

    UUID getUUID() const;
    const std::string& getName() const;
    TransformComponent& transform();
    const TransformComponent& transform() const;
    Entity getParent() const;
    UUID getParentUUID() const;
    bool hasParent() const;
    bool setParent(Entity parent, bool preserve_world_transform = true);
    bool clearParent(bool preserve_world_transform = true);
    const std::vector<UUID>& getChildren() const;
    size_t getChildCount() const;
    bool hasChildren() const;
    bool removeChild(Entity child, bool preserve_world_transform = true);
    EntityManager* getEntityManager() const;

    explicit operator bool() const
    {
        return m_entity_manager != nullptr && m_entity_handle != entt::null;
    }

    operator entt::entity() const
    {
        return m_entity_handle;
    }

    operator uint32_t() const
    {
        return static_cast<uint32_t>(m_entity_handle);
    }

    bool operator==(const Entity& other) const
    {
        return m_entity_handle == other.m_entity_handle && m_entity_manager == other.m_entity_manager;
    }

    bool operator!=(const Entity& other) const
    {
        return !(*this == other);
    }

private:
    entt::entity m_entity_handle{entt::null};
    EntityManager* m_entity_manager = nullptr;

    friend class EntityManager;
};

} // namespace luna
