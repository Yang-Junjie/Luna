#pragma once

#include <cstdint>
#include <vector>

namespace luna {

class SwapchainImageHistory {
public:
    void resize(uint32_t image_count);
    void clear();

    [[nodiscard]] bool wasPresented(uint32_t image_index) const;
    void markPresented(uint32_t image_index);

private:
    std::vector<bool> m_presented_images;
};

} // namespace luna
