#pragma once

#include "EntityManager.h"

#include <string>

namespace luna {

class Scene {
public:
    Scene();
    ~Scene() = default;

    void onUpdateRuntime();

    void setName(std::string name);
    const std::string& getName() const;

    EntityManager& entityManager();
    const EntityManager& entityManager() const;

private:
    std::string m_name{"Untitled"};
    EntityManager m_entity_manager;
};

} // namespace luna
