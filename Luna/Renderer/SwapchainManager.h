#pragma once

#include "Renderer/SwapchainFactory.h"

#include <cstdint>
#include <functional>

namespace luna::RHI {
class Queue;
class Texture;
} // namespace luna::RHI

namespace luna {

using FrameSyncHandle = uint32_t;
using FramebufferExtentProvider = std::function<luna::RHI::Extent2D()>;

struct AcquireResult {
    luna::RHI::Result result{luna::RHI::Result::Error};
    int image_index{-1};

    [[nodiscard]] bool acquired() const noexcept;
    [[nodiscard]] bool requiresRecreate() const noexcept;
};

struct PresentResult {
    luna::RHI::Result result{luna::RHI::Result::Error};

    [[nodiscard]] bool presented() const noexcept;
    [[nodiscard]] bool requiresRecreate() const noexcept;
};

class SwapchainManager {
public:
    bool create(const SwapchainCreateRequest& request);
    AcquireResult acquireNextImage(FrameSyncHandle frame);
    PresentResult present(FrameSyncHandle frame);
    bool recreateIfRequested(const FramebufferExtentProvider& extent_provider);
    void requestResize();

    [[nodiscard]] bool hasResources() const noexcept;
    [[nodiscard]] bool isReady() const noexcept;
    [[nodiscard]] bool isResizeRequested() const noexcept;
    void reset() noexcept;
    void waitForFrame(FrameSyncHandle frame);

    [[nodiscard]] const luna::RHI::Ref<luna::RHI::Swapchain>& swapchain() const noexcept;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::Synchronization>& synchronization() const noexcept;
    [[nodiscard]] luna::RHI::Format surfaceFormat() const noexcept;
    [[nodiscard]] uint32_t framesInFlight() const noexcept;
    [[nodiscard]] uint32_t imageCount() const noexcept;
    [[nodiscard]] luna::RHI::Extent2D extent() const noexcept;
    [[nodiscard]] luna::RHI::Ref<luna::RHI::Texture> backBuffer(uint32_t image_index) const;

private:
    void releaseSwapchain() noexcept;

    SwapchainFactory m_factory;
    SwapchainResources m_resources;
    bool m_resize_requested{false};
};

} // namespace luna
