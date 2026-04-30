#pragma once

#include "Core/UUID.h"

namespace luna {

struct IDComponent {
    UUID id;

    IDComponent() = default;

    explicit IDComponent(UUID id)
        : id(id)
    {}
};

} // namespace luna
