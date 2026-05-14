#pragma once

#include <cstdint>

namespace luna {

struct ViewportSurfaceState {
    uint32_t width{0};
    uint32_t height{0};
    bool y_flip{false};
    bool presentable{false};
};

class ViewportSurface final {
public:
    void reset() noexcept;
    void setPresentation(uint32_t width, uint32_t height, bool y_flip, bool presentable) noexcept;

    [[nodiscard]] const ViewportSurfaceState& state() const noexcept;

private:
    ViewportSurfaceState m_state;
};

} // namespace luna
