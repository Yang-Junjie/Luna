#pragma once

#include "Core/UUID.h"

#include <vector>

namespace luna {

struct RelationshipComponent {
    UUID parentHandle = UUID(0);
    std::vector<UUID> children;

    RelationshipComponent() = default;
    RelationshipComponent(const RelationshipComponent&) = default;

    explicit RelationshipComponent(UUID parent)
        : parentHandle(parent)
    {}
};

} // namespace luna
