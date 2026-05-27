#pragma once

#include <cstdint>

#include <Core.h>
#include <Swapchain.h>

namespace luna::RHI {
class Synchronization;
class Texture;
} // namespace luna::RHI

namespace luna {

struct SwapchainResources {
    RHI::Ref<RHI::Swapchain> swapchain;
    RHI::Ref<RHI::Synchronization> synchronization;
    RHI::Format surface_format{RHI::Format::UNDEFINED};
    uint32_t frames_in_flight{0};

    [[nodiscard]] bool hasResources() const noexcept;
    [[nodiscard]] bool isReady() const noexcept;
    void reset() noexcept;

    [[nodiscard]] uint32_t imageCount() const noexcept;
    [[nodiscard]] RHI::Extent2D extent() const noexcept;
    [[nodiscard]] RHI::Ref<RHI::Texture> backBuffer(uint32_t image_index) const;
};

} // namespace luna
