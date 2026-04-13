#pragma once

#include "Components.h"
#include "Scene.h"

#include <cstdint>

#include <utility>

namespace luna {

class Entity {
public:
    Entity() = default;
    Entity(entt::entity entity_handle, Scene* scene);
    Entity(const Entity&) = default;

    template <typename T, typename... Args> T& addComponent(Args&&... args)
    {
        return m_scene->m_registry.emplace<T>(m_entity_handle, std::forward<Args>(args)...);
    }

    template <typename T> T& getComponent()
    {
        return m_scene->m_registry.get<T>(m_entity_handle);
    }

    template <typename T> const T& getComponent() const
    {
        const auto& registry = m_scene->m_registry;
        return registry.get<T>(m_entity_handle);
    }

    template <typename T> bool hasComponent() const
    {
        return m_scene != nullptr && m_entity_handle != entt::null && m_scene->m_registry.all_of<T>(m_entity_handle);
    }

    template <typename T> void removeComponent()
    {
        if (hasComponent<T>()) {
            m_scene->m_registry.remove<T>(m_entity_handle);
        }
    }

    UUID getUUID() const;
    const std::string& getName() const;

    explicit operator bool() const
    {
        return m_scene != nullptr && m_entity_handle != entt::null;
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
        return m_entity_handle == other.m_entity_handle && m_scene == other.m_scene;
    }

    bool operator!=(const Entity& other) const
    {
        return !(*this == other);
    }

private:
    entt::entity m_entity_handle{entt::null};
    Scene* m_scene = nullptr;

    friend class Scene;
};

} // namespace luna
