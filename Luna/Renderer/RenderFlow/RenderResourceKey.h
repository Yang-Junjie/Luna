#pragma once

#include <string_view>

namespace luna::render_flow {

template <typename ResourceT> struct RenderResourceKey {
    using ValueType = ResourceT;

    std::string_view name;

    [[nodiscard]] constexpr std::string_view value() const noexcept
    {
        return name;
    }
};

} // namespace luna::render_flow
