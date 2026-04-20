#include "Entity.h"

namespace luna {

Entity::Entity(entt::entity entity_handle, EntityManager* entity_manager)
    : m_entity_handle(entity_handle),
      m_entity_manager(entity_manager)
{}

UUID Entity::getUUID() const
{
    return getComponent<IDComponent>().id;
}

const std::string& Entity::getName() const
{
    return getComponent<TagComponent>().tag;
}

TransformComponent& Entity::transform()
{
    return getComponent<TransformComponent>();
}

const TransformComponent& Entity::transform() const
{
    return getComponent<TransformComponent>();
}

Entity Entity::getParent() const
{
    if (m_entity_manager == nullptr) {
        return {};
    }

    return m_entity_manager->findEntityByUUID(getParentUUID());
}

UUID Entity::getParentUUID() const
{
    if (!hasComponent<RelationshipComponent>()) {
        return UUID(0);
    }

    return getComponent<RelationshipComponent>().parentHandle;
}

bool Entity::hasParent() const
{
    return getParentUUID().isValid() && static_cast<bool>(getParent());
}

bool Entity::setParent(Entity parent, bool preserve_world_transform)
{
    if (m_entity_manager == nullptr) {
        return false;
    }

    return m_entity_manager->setParent(*this, parent, preserve_world_transform);
}

bool Entity::clearParent(bool preserve_world_transform)
{
    return setParent({}, preserve_world_transform);
}

const std::vector<UUID>& Entity::getChildren() const
{
    static const std::vector<UUID> empty_children;
    if (!hasComponent<RelationshipComponent>()) {
        return empty_children;
    }

    return getComponent<RelationshipComponent>().children;
}

size_t Entity::getChildCount() const
{
    return getChildren().size();
}

bool Entity::hasChildren() const
{
    return !getChildren().empty();
}

bool Entity::removeChild(Entity child, bool preserve_world_transform)
{
    if (!child || !(child.getParentUUID() == getUUID())) {
        return false;
    }

    return child.clearParent(preserve_world_transform);
}

EntityManager* Entity::getEntityManager() const
{
    return m_entity_manager;
}

bool Entity::isValid() const
{
    return m_entity_manager != nullptr && m_entity_handle != entt::null &&
           m_entity_manager->registry().valid(m_entity_handle);
}

} // namespace luna
