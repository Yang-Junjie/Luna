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
    luna::RHI::Ref<luna::RHI::Device> device;
    luna::RHI::Ref<luna::RHI::Adapter> adapter;
    luna::RHI::Ref<luna::RHI::Surface> surface;
    luna::RHI::Ref<luna::RHI::Queue> graphics_queue;
    luna::RHI::PresentMode present_mode{luna::RHI::PresentMode::Fifo};
    luna::RHI::Extent2D extent{0, 0};
    SwapchainRecreateCallback before_recreate;
};

class SwapchainFactory {
public:
    bool configure(const SwapchainCreateRequest& request);
    [[nodiscard]] bool isConfigured() const noexcept;
    [[nodiscard]] const SwapchainCreateRequest& request() const noexcept;
    void reset() noexcept;

    [[nodiscard]] bool create(luna::RHI::Extent2D requested_extent, SwapchainResources& resources) const;

private:
    SwapchainCreateRequest m_request{};
    bool m_configured{false};
};

} // namespace luna
