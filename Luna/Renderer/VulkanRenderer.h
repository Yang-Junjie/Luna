#pragma once

#include "Renderer/Camera.h"
#include "Renderer/RenderGraph.h"
#include "Renderer/SceneRenderer.h"

#include <functional>
#include <glm/vec4.hpp>
#include <memory>
#include <vulkan/vulkan.hpp>

namespace luna::val {
class VulkanContext;
}

struct GLFWwindow;

namespace luna {
class Window;

class VulkanRenderer {
public:
    struct RenderGraphBuildInfo {
        luna::val::Format m_surface_format{luna::val::Format::UNDEFINED};
        uint32_t m_framebuffer_width{0};
        uint32_t m_framebuffer_height{0};
    };

    using RenderGraphBuilderCallback =
        std::function<std::unique_ptr<luna::val::RenderGraph>(const RenderGraphBuildInfo&)>;

    struct InitializationOptions {
        RenderGraphBuilderCallback m_render_graph_builder;
    };

    VulkanRenderer();
    ~VulkanRenderer();

    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    bool init(Window& window, InitializationOptions options = {});
    void shutdown();

    bool isInitialized() const;
    bool isRenderingEnabled() const;
    bool isImGuiEnabled() const;

    void requestResize();
    void requestRenderGraphRebuild();
    bool isResizeRequested() const;
    void setImGuiEnabled(bool enabled);

    void startFrame();
    void renderFrame();
    void endFrame();

    GLFWwindow* getNativeWindow() const;
    const vk::RenderPass& getImGuiRenderPass() const;

    Camera& getMainCamera();
    const Camera& getMainCamera() const;
    SceneRenderer& getSceneRenderer();
    const SceneRenderer& getSceneRenderer() const;

    glm::vec4& getClearColor();
    const glm::vec4& getClearColor() const;

private:
    void rebuildRenderGraph();
    vk::Extent2D getFramebufferExtent() const;
    void handlePendingResize();

private:
    Window* m_window{nullptr};
    GLFWwindow* m_native_window{nullptr};
    std::unique_ptr<luna::val::VulkanContext> m_context;
    std::unique_ptr<luna::val::RenderGraph> m_render_graph;
    InitializationOptions m_initialization_options{};
    SceneRenderer m_scene_renderer{};
    Camera m_main_camera{};
    glm::vec4 m_clear_color{0.10f, 0.10f, 0.12f, 1.0f};
    bool m_initialized{false};
    bool m_imgui_enabled{false};
    bool m_resize_requested{false};
    bool m_render_graph_rebuild_requested{false};
    bool m_frame_started{false};
};

} // namespace luna
