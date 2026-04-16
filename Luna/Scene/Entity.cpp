#include "Entity.h"

namespace luna {

Entity::Entity(entt::entity entity_handle, Scene* scene)
    : m_entity_handle(entity_handle),
      m_scene(scene)
{}

UUID Entity::getUUID() const
{
    return getComponent<IDComponent>().id;
}

const std::string& Entity::getName() const
{
    return getComponent<TagComponent>().tag;
}

} // namespace luna
