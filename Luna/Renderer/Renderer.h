#pragma once

#include "Renderer/Camera.h"
#include "Renderer/SceneRenderer.h"

#include <Core.h>
#include <glm/vec4.hpp>
#include <Instance.h>
#include <memory>
#include <Surface.h>
#include <vector>

struct GLFWwindow;

namespace luna::RHI {
class Adapter;
class CommandBufferEncoder;
class Device;
class Instance;
class Queue;
class ShaderCompiler;
class Surface;
class Swapchain;
class Synchronization;
} // namespace luna::RHI

namespace luna::rhi {
class RenderGraph;
}

namespace luna {
class Window;

class Renderer {
public:
    struct InitializationOptions {
        InitializationOptions()
            : backend(luna::RHI::BackendType::Vulkan),
              present_mode(luna::RHI::PresentMode::Fifo)
        {}

        InitializationOptions(luna::RHI::BackendType backend_type, luna::RHI::PresentMode mode)
            : backend(backend_type),
              present_mode(mode)
        {}

        luna::RHI::BackendType backend;
        luna::RHI::PresentMode present_mode;
    };

    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool init(Window& window, InitializationOptions options = {});
    void shutdown();

    bool isInitialized() const;
    bool isRenderingEnabled() const;
    bool isImGuiEnabled() const;

    void requestResize();
    bool isResizeRequested() const;
    void setImGuiEnabled(bool enabled);

    void startFrame();
    void renderFrame();
    void endFrame();

    GLFWwindow* getNativeWindow() const;

    const luna::RHI::Ref<luna::RHI::Instance>& getInstance() const;
    const luna::RHI::Ref<luna::RHI::Adapter>& getAdapter() const;
    const luna::RHI::Ref<luna::RHI::Device>& getDevice() const;
    const luna::RHI::Ref<luna::RHI::Queue>& getGraphicsQueue() const;
    const luna::RHI::Ref<luna::RHI::Swapchain>& getSwapchain() const;
    const luna::RHI::Ref<luna::RHI::Synchronization>& getSynchronization() const;
    const luna::RHI::Ref<luna::RHI::ShaderCompiler>& getShaderCompiler() const
    {
        return m_shader_compiler;
    }
    uint32_t getFramesInFlight() const;

    Camera& getMainCamera();
    const Camera& getMainCamera() const;
    SceneRenderer& getSceneRenderer();
    const SceneRenderer& getSceneRenderer() const;

    glm::vec4& getClearColor();
    const glm::vec4& getClearColor() const;

private:
    void createSwapchain(uint32_t width, uint32_t height);
    luna::RHI::Extent2D getFramebufferExtent() const;
    void handlePendingResize();
    void releaseFrameCommandBuffers();

private:
    Window* m_window{nullptr};
    GLFWwindow* m_native_window{nullptr};

    luna::RHI::Ref<luna::RHI::Instance> m_instance;
    luna::RHI::Ref<luna::RHI::Adapter> m_adapter;
    luna::RHI::Ref<luna::RHI::Device> m_device;
    luna::RHI::Ref<luna::RHI::Surface> m_surface;
    luna::RHI::Ref<luna::RHI::Swapchain> m_swapchain;
    luna::RHI::Ref<luna::RHI::Queue> m_graphics_queue;
    luna::RHI::Ref<luna::RHI::ShaderCompiler> m_shader_compiler;
    luna::RHI::Ref<luna::RHI::Synchronization> m_synchronization;
    luna::RHI::Ref<luna::RHI::CommandBufferEncoder> m_current_command_buffer;
    std::vector<luna::RHI::Ref<luna::RHI::CommandBufferEncoder>> m_frame_command_buffers;
    std::vector<std::unique_ptr<luna::rhi::RenderGraph>> m_frame_render_graphs;

    InitializationOptions m_initialization_options{};

    SceneRenderer m_scene_renderer{};

    Camera m_main_camera{};

    glm::vec4 m_clear_color{0.10f, 0.10f, 0.12f, 1.0f};
    luna::RHI::Format m_surface_format{luna::RHI::Format::UNDEFINED};

    uint32_t m_frames_in_flight{0};
    uint32_t m_frame_index{0};
    uint32_t m_image_index{0};
    std::vector<bool> m_swapchain_images_presented;

    bool m_initialized{false};
    bool m_imgui_enabled{false};
    bool m_resize_requested{false};
    bool m_frame_started{false};
};

} // namespace luna
