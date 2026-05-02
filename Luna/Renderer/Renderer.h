#pragma once

// Main renderer framwork for the engine runtime.
// Owns device-facing frame state, scene output targets, and the public frame loop,
// while delegating scene-specific drawing to RenderFlow.

#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/RenderFlow/RenderFlowBuilder.h"
#include "Renderer/RenderFlow/RenderFlowTypes.h"
#include "Renderer/RenderFlow/RenderFeature.h"
#include "Renderer/RenderViewState.h"
#include "Renderer/RenderWorld/RenderWorld.h"

#include <cstdint>
#include <functional>

#include <Barrier.h>
#include <Capabilities.h>
#include <Core.h>
#include <glm/vec4.hpp>
#include <Instance.h>
#include <memory>
#include <optional>
#include <Surface.h>
#include <string_view>
#include <vector>

struct GLFWwindow;

namespace luna::RHI {
class Adapter;
class Buffer;
class CommandBufferEncoder;
class Device;
class Instance;
class QueryPool;
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

namespace luna::render_flow {
class IRenderFeature;
} // namespace luna::render_flow

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
    void waitForGpuIdle() noexcept;
    void startFrame();
    void renderFrame();
    void endFrame();

    bool isInitialized() const;
    bool isRenderingEnabled() const;
    bool isImGuiEnabled() const;

    void requestResize();
    bool isResizeRequested() const;
    void notifyCameraCut();

    void setImGuiEnabled(bool enabled);

    SceneOutputMode getSceneOutputMode() const;
    void setSceneOutputMode(SceneOutputMode mode);
    void setSceneOutputSize(uint32_t width, uint32_t height);
    luna::RHI::Extent2D getSceneOutputSize() const;
    const luna::RHI::Ref<luna::RHI::Texture>& getSceneOutputTexture() const;

    void setRenderDebugViewMode(RenderDebugViewMode mode);
    [[nodiscard]] RenderDebugViewMode getRenderDebugViewMode() const;
    void setRenderDebugVelocityScale(float scale);
    [[nodiscard]] float getRenderDebugVelocityScale() const;
    const luna::RHI::Ref<luna::RHI::Texture>& getRenderDebugOutputTexture() const;

    void setScenePickDebugVisualizationEnabled(bool enabled);
    bool isScenePickDebugVisualizationEnabled() const;
    void requestScenePick(uint32_t x, uint32_t y);
    std::optional<uint32_t> consumeScenePickResult();

    GLFWwindow* getNativeWindow() const;

    const luna::RHI::Ref<luna::RHI::Instance>& getInstance() const;
    const luna::RHI::Ref<luna::RHI::Adapter>& getAdapter() const;
    [[nodiscard]] const luna::RHI::RHICapabilities& getCapabilities() const noexcept;
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
    [[nodiscard]] const RenderGraphProfileSnapshot& getLastRenderGraphProfile() const;
    void setRenderGraphProfilingEnabled(bool enabled);
    [[nodiscard]] bool isRenderGraphProfilingEnabled() const;
    bool addDefaultRenderFeature(std::unique_ptr<render_flow::IRenderFeature> feature);
    [[nodiscard]] std::vector<render_flow::RenderFeatureInfo> getDefaultRenderFeatureInfos() const;
    bool setDefaultRenderFeatureEnabled(std::string_view name, bool enabled);
    [[nodiscard]] std::vector<render_flow::RenderFeatureParameterInfo>
        getDefaultRenderFeatureParameters(std::string_view name) const;
    bool setDefaultRenderFeatureParameter(std::string_view feature_name,
                                          std::string_view parameter_name,
                                          const render_flow::RenderFeatureParameterValue& value);
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
        luna::RHI::RHICapabilities capabilities{};
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
        luna::RHI::Ref<luna::RHI::Texture> debug_color;
        luna::RHI::ResourceState color_state{luna::RHI::ResourceState::Undefined};
        luna::RHI::ResourceState depth_state{luna::RHI::ResourceState::Undefined};
        luna::RHI::ResourceState pick_state{luna::RHI::ResourceState::Undefined};
        luna::RHI::ResourceState debug_color_state{luna::RHI::ResourceState::Undefined};
        RenderDebugViewMode debug_view_mode{RenderDebugViewMode::None};
        float debug_velocity_scale{20.0f};
        bool pick_debug_visualization_enabled{false};
        PickDebugMarker debug_pick_marker{};
        std::optional<PickRequest> queued_pick_request;
        std::optional<uint32_t> completed_pick_id;
        uint64_t generation{0};
    };

    struct FrameResources {
        struct ScenePickReadbackSlot {
            luna::RHI::Ref<luna::RHI::Buffer> buffer;
            bool pending{false};
        };

        struct GpuTimingSlot {
            luna::RHI::Ref<luna::RHI::QueryPool> query_pool;
            luna::RHI::Ref<luna::RHI::QueryPool> disjoint_query_pool;
            RenderGraphProfileSnapshot profile;
            uint32_t query_count{0};
            bool pending{false};
            bool uses_disjoint_timestamps{false};
        };

        luna::RHI::Ref<luna::RHI::CommandBufferEncoder> current_command_buffer;
        std::vector<luna::RHI::Ref<luna::RHI::CommandBufferEncoder>> command_buffers;
        std::vector<std::unique_ptr<luna::RenderGraph>> render_graphs;
        std::vector<luna::RenderGraphTransientTextureCache> transient_texture_caches;
        std::vector<ScenePickReadbackSlot> scene_pick_readback_slots;
        std::vector<GpuTimingSlot> gpu_timing_slots;
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
        bool render_graph_profiling_enabled{false};
    };

    struct RenderFeatureHistoryState {
        bool has_previous_frame{false};
        luna::RHI::Device* device{nullptr};
        luna::RHI::BackendType backend_type{luna::RHI::BackendType::Auto};
        SceneOutputMode scene_output_mode{SceneOutputMode::Swapchain};
        uint32_t framebuffer_width{0};
        uint32_t framebuffer_height{0};
        uint64_t scene_output_generation{0};
        render_flow::RenderFeatureHistoryInvalidationFlags pending_flags{
            render_flow::RenderFeatureHistoryInvalidationFlags::None};
        bool has_pending_frame{false};
        luna::RHI::Device* pending_device{nullptr};
        luna::RHI::BackendType pending_backend_type{luna::RHI::BackendType::Auto};
        SceneOutputMode pending_scene_output_mode{SceneOutputMode::Swapchain};
        uint32_t pending_framebuffer_width{0};
        uint32_t pending_framebuffer_height{0};
        uint64_t pending_scene_output_generation{0};
    };

    void createSwapchain(uint32_t width, uint32_t height);
    luna::RHI::Extent2D getFramebufferExtent() const;
    void handlePendingResize();
    void invalidateRenderFeatureHistory(render_flow::RenderFeatureHistoryInvalidationFlags flags) noexcept;
    [[nodiscard]] render_flow::RenderFeatureFrameContext makeRenderFeatureFrameContext(
        luna::RHI::BackendType backend_type,
        SceneOutputMode scene_output_mode,
        uint64_t frame_index,
        uint32_t framebuffer_width,
        uint32_t framebuffer_height) const;
    void stageRenderFeatureFrameContext(luna::RHI::BackendType backend_type,
                                        SceneOutputMode scene_output_mode,
                                        uint32_t framebuffer_width,
                                        uint32_t framebuffer_height) noexcept;
    void commitStagedRenderFeatureFrameContext() noexcept;
    bool hasMatchingSceneOutputTargets(uint32_t width, uint32_t height) const;
    void releaseFrameCommandBuffers();
    void ensureScenePickReadbackBuffers();
    void collectCompletedScenePickResult(uint32_t frame_index);
    void ensureGpuTimingResources();
    void collectCompletedGpuTiming(uint32_t frame_index);
    bool storePendingGpuTimingProfile(uint32_t frame_index, const RenderGraphProfileSnapshot& profile);
    void ensureSceneOutputTargets(uint32_t width, uint32_t height);
    void releaseSceneOutputTargets();

private:
    WindowContext m_window_context{};
    DeviceContext m_device_context{};
    SceneOutputState m_scene_output{};
    FrameResources m_frame_resources{};
    RuntimeState m_runtime{};
    RenderFeatureHistoryState m_render_feature_history{};
    RenderViewHistory m_render_view_history{};
    RenderWorld m_render_world{};
    std::unique_ptr<IRenderFlow> m_render_flow;
    RenderGraphProfileSnapshot m_last_render_graph_profile{};
};

} // namespace luna




