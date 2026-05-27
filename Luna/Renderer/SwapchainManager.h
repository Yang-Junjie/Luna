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
using FramebufferExtentProvider = std::function<RHI::Extent2D()>;

struct AcquireResult {
    RHI::Result result{RHI::Result::Error};
    int image_index{-1};

    [[nodiscard]] bool acquired() const noexcept;
    [[nodiscard]] bool requiresRecreate() const noexcept;
};

struct PresentResult {
    RHI::Result result{RHI::Result::Error};

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

    [[nodiscard]] const RHI::Ref<RHI::Swapchain>& swapchain() const noexcept;
    [[nodiscard]] const RHI::Ref<RHI::Synchronization>& synchronization() const noexcept;
    [[nodiscard]] RHI::Format surfaceFormat() const noexcept;
    [[nodiscard]] uint32_t framesInFlight() const noexcept;
    [[nodiscard]] uint32_t imageCount() const noexcept;
    [[nodiscard]] RHI::Extent2D extent() const noexcept;
    [[nodiscard]] RHI::Ref<RHI::Texture> backBuffer(uint32_t image_index) const;

private:
    void releaseSwapchain() noexcept;

    SwapchainFactory m_factory;
    SwapchainResources m_resources;
    bool m_resize_requested{false};
};

} // namespace luna
