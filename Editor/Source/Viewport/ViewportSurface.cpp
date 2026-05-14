#include "Viewport/ViewportSurface.h"

namespace luna {

void ViewportSurface::reset() noexcept
{
    m_state = {};
}

void ViewportSurface::setPresentation(uint32_t width, uint32_t height, bool y_flip, bool presentable) noexcept
{
    m_state.width = width;
    m_state.height = height;
    m_state.y_flip = y_flip;
    m_state.presentable = presentable;
}

const ViewportSurfaceState& ViewportSurface::state() const noexcept
{
    return m_state;
}

} // namespace luna
