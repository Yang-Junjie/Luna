#pragma once

#include "Renderer/Camera.h"
#include "Renderer/SceneRenderer.h"

#include <Core.h>
#include <glm/vec4.hpp>
#include <memory>
#include <vector>

struct GLFWwindow;

namespace Cacao {
class Adapter;
class CommandBufferEncoder;
class Device;
class Instance;
class Queue;
class Surface;
class Swapchain;
class Synchronization;
} // namespace Cacao

namespace luna {
class Window;

class VulkanRenderer {
public:
    struct InitializationOptions {};

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
    bool isResizeRequested() const;
    void setImGuiEnabled(bool enabled);

    void startFrame();
    void renderFrame();
    void endFrame();

    GLFWwindow* getNativeWindow() const;

    const Cacao::Ref<Cacao::Instance>& getInstance() const;
    const Cacao::Ref<Cacao::Adapter>& getAdapter() const;
    const Cacao::Ref<Cacao::Device>& getDevice() const;
    const Cacao::Ref<Cacao::Queue>& getGraphicsQueue() const;
    const Cacao::Ref<Cacao::Swapchain>& getSwapchain() const;
    const Cacao::Ref<Cacao::Synchronization>& getSynchronization() const;
    uint32_t getFramesInFlight() const;

    Camera& getMainCamera();
    const Camera& getMainCamera() const;
    SceneRenderer& getSceneRenderer();
    const SceneRenderer& getSceneRenderer() const;

    glm::vec4& getClearColor();
    const glm::vec4& getClearColor() const;

private:
    void createSwapchain(uint32_t width, uint32_t height);
    Cacao::Extent2D getFramebufferExtent() const;
    void handlePendingResize();
    void releaseFrameCommandBuffers();

private:
    Window* m_window{nullptr};
    GLFWwindow* m_native_window{nullptr};
    Cacao::Ref<Cacao::Instance> m_instance;
    Cacao::Ref<Cacao::Adapter> m_adapter;
    Cacao::Ref<Cacao::Device> m_device;
    Cacao::Ref<Cacao::Surface> m_surface;
    Cacao::Ref<Cacao::Swapchain> m_swapchain;
    Cacao::Ref<Cacao::Queue> m_graphics_queue;
    Cacao::Ref<Cacao::Synchronization> m_synchronization;
    Cacao::Ref<Cacao::CommandBufferEncoder> m_current_command_buffer;
    std::vector<Cacao::Ref<Cacao::CommandBufferEncoder>> m_frame_command_buffers;
    InitializationOptions m_initialization_options{};
    SceneRenderer m_scene_renderer{};
    Camera m_main_camera{};
    glm::vec4 m_clear_color{0.10f, 0.10f, 0.12f, 1.0f};
    Cacao::Format m_surface_format{Cacao::Format::UNDEFINED};
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
