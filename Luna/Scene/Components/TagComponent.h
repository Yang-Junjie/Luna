#pragma once

#include <string>
#include <utility>

namespace luna {

struct TagComponent {
    std::string tag;

    TagComponent() = default;

    explicit TagComponent(std::string tag)
        : tag(std::move(tag))
    {}
};

} // namespace luna
