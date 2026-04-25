#pragma once

// Main renderer framwork for the engine runtime.
// Owns device-facing frame state, scene output targets, and the public frame loop,
// while delegating scene-specific drawing to RenderFlow.

#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/RenderFlow/RenderFlowBuilder.h"
#include "Renderer/RenderWorld/RenderWorld.h"

#include <cstdint>
#include <functional>

#include <Barrier.h>
#include <Core.h>
#include <glm/vec4.hpp>
#include <Instance.h>
#include <memory>
#include <optional>
#include <Surface.h>
#include <vector>

struct GLFWwindow;

namespace luna::RHI {
class Adapter;
class Buffer;
class CommandBufferEncoder;
class Device;
class Instance;
class Queue;
class ShaderCompiler;
class Surface;
class Swapchain;
class Synchronization;
class Texture;
} // namespace luna::RHI

namespace luna {
class RenderGraph;
} // namespace luna

namespace luna {
class IRenderFlow;
class Window;

class Renderer {
public:
    using DefaultRenderFlowConfigureFunction = std::function<void(render_flow::RenderFlowBuilder&)>;

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

    enum class SceneOutputMode : uint8_t {
        Swapchain,
        OffscreenTexture,
    };

    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool init(Window& window, InitializationOptions options = {});
    void shutdown();
    void startFrame();
    void renderFrame();
    void endFrame();

    bool isInitialized() const;
    bool isRenderingEnabled() const;
    bool isImGuiEnabled() const;

    void requestResize();
    bool isResizeRequested() const;

    void setImGuiEnabled(bool enabled);

    SceneOutputMode getSceneOutputMode() const;
    void setSceneOutputMode(SceneOutputMode mode);
    void setSceneOutputSize(uint32_t width, uint32_t height);
    luna::RHI::Extent2D getSceneOutputSize() const;
    const luna::RHI::Ref<luna::RHI::Texture>& getSceneOutputTexture() const;

    void setScenePickDebugVisualizationEnabled(bool enabled);
    bool isScenePickDebugVisualizationEnabled() const;
    void requestScenePick(uint32_t x, uint32_t y);
    std::optional<uint32_t> consumeScenePickResult();

    GLFWwindow* getNativeWindow() const;

    const luna::RHI::Ref<luna::RHI::Instance>& getInstance() const;
    const luna::RHI::Ref<luna::RHI::Adapter>& getAdapter() const;
    const luna::RHI::Ref<luna::RHI::Device>& getDevice() const;
    const luna::RHI::Ref<luna::RHI::Queue>& getGraphicsQueue() const;
    const luna::RHI::Ref<luna::RHI::Swapchain>& getSwapchain() const;
    const luna::RHI::Ref<luna::RHI::Synchronization>& getSynchronization() const;

    const luna::RHI::Ref<luna::RHI::ShaderCompiler>& getShaderCompiler() const
    {
        return m_device_context.shader_compiler;
    }

    uint32_t getFramesInFlight() const;

    RenderWorld& getRenderWorld();
    const RenderWorld& getRenderWorld() const;
    bool configureDefaultRenderFlow(const DefaultRenderFlowConfigureFunction& configure_function);

    glm::vec4& getClearColor();
    const glm::vec4& getClearColor() const;

private:
    struct WindowContext {
        Window* window{nullptr};
        GLFWwindow* native_window{nullptr};
    };

    struct DeviceContext {
        luna::RHI::Ref<luna::RHI::Instance> instance;
        luna::RHI::Ref<luna::RHI::Adapter> adapter;
        luna::RHI::Ref<luna::RHI::Device> device;
        luna::RHI::Ref<luna::RHI::Surface> surface;
        luna::RHI::Ref<luna::RHI::Swapchain> swapchain;
        luna::RHI::Ref<luna::RHI::Queue> graphics_queue;
        luna::RHI::Ref<luna::RHI::ShaderCompiler> shader_compiler;
        luna::RHI::Ref<luna::RHI::Synchronization> synchronization;
        luna::RHI::Format surface_format{luna::RHI::Format::UNDEFINED};
    };

    struct SceneOutputState {
        struct PickRequest {
            uint32_t x{0};
            uint32_t y{0};
        };

        struct PickDebugMarker {
            uint32_t x{0};
            uint32_t y{0};
            bool valid{false};
        };

        SceneOutputMode mode{SceneOutputMode::Swapchain};
        luna::RHI::Extent2D extent{0, 0};
        luna::RHI::Ref<luna::RHI::Texture> color;
        luna::RHI::Ref<luna::RHI::Texture> depth;
        luna::RHI::Ref<luna::RHI::Texture> pick;
        luna::RHI::ResourceState color_state{luna::RHI::ResourceState::Undefined};
        luna::RHI::ResourceState depth_state{luna::RHI::ResourceState::Undefined};
        luna::RHI::ResourceState pick_state{luna::RHI::ResourceState::Undefined};
        bool pick_debug_visualization_enabled{false};
        PickDebugMarker debug_pick_marker{};
        std::optional<PickRequest> queued_pick_request;
        std::optional<uint32_t> completed_pick_id;
    };

    struct FrameResources {
        struct ScenePickReadbackSlot {
            luna::RHI::Ref<luna::RHI::Buffer> buffer;
            bool pending{false};
        };

        luna::RHI::Ref<luna::RHI::CommandBufferEncoder> current_command_buffer;
        std::vector<luna::RHI::Ref<luna::RHI::CommandBufferEncoder>> command_buffers;
        std::vector<std::unique_ptr<luna::RenderGraph>> render_graphs;
        std::vector<luna::RenderGraphTransientTextureCache> transient_texture_caches;
        std::vector<ScenePickReadbackSlot> scene_pick_readback_slots;
        uint32_t frames_in_flight{0};
        uint32_t frame_index{0};
        uint32_t image_index{0};
        std::vector<bool> swapchain_images_presented;
    };

    struct RuntimeState {
        InitializationOptions initialization_options{};
        glm::vec4 clear_color{0.10f, 0.10f, 0.12f, 1.0f};
        bool initialized{false};
        bool imgui_enabled{false};
        bool resize_requested{false};
        bool frame_started{false};
    };

    void createSwapchain(uint32_t width, uint32_t height);
    luna::RHI::Extent2D getFramebufferExtent() const;
    void handlePendingResize();
    bool hasMatchingSceneOutputTargets(uint32_t width, uint32_t height) const;
    void releaseFrameCommandBuffers();
    void ensureScenePickReadbackBuffers();
    void collectCompletedScenePickResult(uint32_t frame_index);
    void ensureSceneOutputTargets(uint32_t width, uint32_t height);
    void releaseSceneOutputTargets();
    void waitForGpuIdle() noexcept;

private:
    WindowContext m_window_context{};
    DeviceContext m_device_context{};
    SceneOutputState m_scene_output{};
    FrameResources m_frame_resources{};
    RuntimeState m_runtime{};
    RenderWorld m_render_world{};
    std::unique_ptr<IRenderFlow> m_render_flow;
};

} // namespace luna




