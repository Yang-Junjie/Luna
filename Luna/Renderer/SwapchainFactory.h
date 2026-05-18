#pragma once

#include "Renderer/SwapchainResources.h"

#include <Core.h>

#include <functional>

namespace luna::RHI {
class Adapter;
class Device;
class Queue;
class Surface;
} // namespace luna::RHI

namespace luna {

using SwapchainRecreateCallback = std::function<void()>;

struct SwapchainCreateRequest {
    RHI::Ref<RHI::Device> device;
    RHI::Ref<RHI::Adapter> adapter;
    RHI::Ref<RHI::Surface> surface;
    RHI::Ref<RHI::Queue> graphics_queue;
    RHI::PresentMode present_mode{RHI::PresentMode::Fifo};
    RHI::Extent2D extent{0, 0};
    SwapchainRecreateCallback before_recreate;
};

class SwapchainFactory {
public:
    bool configure(const SwapchainCreateRequest& request);
    [[nodiscard]] bool isConfigured() const noexcept;
    [[nodiscard]] const SwapchainCreateRequest& request() const noexcept;
    void reset() noexcept;

    [[nodiscard]] bool create(RHI::Extent2D requested_extent, SwapchainResources& resources) const;

private:
    SwapchainCreateRequest m_request{};
    bool m_configured{false};
};

} // namespace luna
