#pragma once

#include "Renderer/Camera.h"
#include "Renderer/RenderGraph.h"

#include <glm/vec4.hpp>
#include <vulkan/vulkan.hpp>

#include <functional>
#include <memory>

namespace VulkanAbstractionLayer {
class VulkanContext;
}

struct GLFWwindow;

namespace luna {
class Window;

class VulkanRenderer {
public:
    struct RenderGraphBuildInfo {
        VulkanAbstractionLayer::Format m_surface_format{VulkanAbstractionLayer::Format::UNDEFINED};
        uint32_t m_framebuffer_width{0};
        uint32_t m_framebuffer_height{0};
    };

    using RenderGraphBuilderCallback =
        std::function<std::unique_ptr<VulkanAbstractionLayer::RenderGraph>(const RenderGraphBuildInfo&)>;

    VulkanRenderer();
    ~VulkanRenderer();

    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    bool init(Window& window, RenderGraphBuilderCallback render_graph_builder = {});
    void shutdown();

    bool isInitialized() const;
    bool isRenderingEnabled() const;

    void requestResize();
    bool isResizeRequested() const;

    void startFrame();
    void renderFrame();
    void endFrame();

    GLFWwindow* getNativeWindow() const;
    const vk::RenderPass& getImGuiRenderPass() const;

    Camera& getMainCamera();
    const Camera& getMainCamera() const;

    glm::vec4& getClearColor();
    const glm::vec4& getClearColor() const;

private:
    void rebuildRenderGraph();
    vk::Extent2D getFramebufferExtent() const;
    void handlePendingResize();

private:
    Window* m_window{nullptr};
    GLFWwindow* m_native_window{nullptr};
    std::unique_ptr<VulkanAbstractionLayer::VulkanContext> m_context;
    std::unique_ptr<VulkanAbstractionLayer::RenderGraph> m_render_graph;
    RenderGraphBuilderCallback m_render_graph_builder;
    Camera m_main_camera{};
    glm::vec4 m_clear_color{0.10f, 0.10f, 0.12f, 1.0f};
    bool m_initialized{false};
    bool m_resize_requested{false};
    bool m_frame_started{false};
};

} // namespace luna
