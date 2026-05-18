#pragma once

#include <Core.h>
#include <Swapchain.h>

#include <cstdint>

namespace luna::RHI {
class Synchronization;
class Texture;
} // namespace luna::RHI

namespace luna {

struct SwapchainResources {
    luna::RHI::Ref<luna::RHI::Swapchain> swapchain;
    luna::RHI::Ref<luna::RHI::Synchronization> synchronization;
    luna::RHI::Format surface_format{luna::RHI::Format::UNDEFINED};
    uint32_t frames_in_flight{0};

    [[nodiscard]] bool hasResources() const noexcept;
    [[nodiscard]] bool isReady() const noexcept;
    void reset() noexcept;

    [[nodiscard]] uint32_t imageCount() const noexcept;
    [[nodiscard]] luna::RHI::Extent2D extent() const noexcept;
    [[nodiscard]] luna::RHI::Ref<luna::RHI::Texture> backBuffer(uint32_t image_index) const;
};

} // namespace luna
