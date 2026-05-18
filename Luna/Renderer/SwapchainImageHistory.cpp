#include "Renderer/SwapchainImageHistory.h"

namespace luna {

void SwapchainImageHistory::resize(uint32_t image_count)
{
    m_presented_images.assign(image_count, false);
}

void SwapchainImageHistory::clear()
{
    m_presented_images.clear();
}

bool SwapchainImageHistory::wasPresented(uint32_t image_index) const
{
    return image_index < m_presented_images.size() ? m_presented_images[image_index] : false;
}

void SwapchainImageHistory::markPresented(uint32_t image_index)
{
    if (image_index < m_presented_images.size()) {
        m_presented_images[image_index] = true;
    }
}

} // namespace luna
