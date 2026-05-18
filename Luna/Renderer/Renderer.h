#pragma once

// Main renderer framwork for the engine runtime.
// Owns device-facing frame state, scene output targets, and the public frame loop,
// while delegating scene-specific drawing to RenderFlow.

#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/FrameResourceRing.h"
#include "Renderer/RenderDeviceContext.h"
#include "Renderer/RenderFlow/RenderFlowBuilder.h"
#include "Renderer/RenderFlow/RenderFlowTypes.h"
#include "Renderer/RenderFlow/RenderFeature.h"
#include "Renderer/RenderViewState.h"
#include "Renderer/RenderWorld/RenderWorld.h"
#include "Renderer/SwapchainImageHistory.h"
#include "Renderer/SwapchainManager.h"

#include <cstdint>
#include <functional>

#include <Barrier.h>
#include <Capabilities.h>
#include <Core.h>
#include <glm/vec4.hpp>
#include <Instance.h>
#include <memory>
#include <optional>
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
    using SceneViewportHandle = uint64_t;
    static constexpr SceneViewportHandle kInvalidSceneViewportHandle{0};

    struct InitializationOptions {
        InitializationOptions()
            : backend(RHI::BackendType::Auto),
              present_mode(RHI::PresentMode::Fifo)
        {}

        InitializationOptions(RHI::BackendType backend_type, RHI::PresentMode mode)
            : backend(backend_type),
              present_mode(mode)
        {}

        RHI::BackendType backend;
        RHI::PresentMode present_mode;
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
    RHI::Extent2D getSceneOutputSize() const;
    const RHI::Ref<RHI::Texture>& getSceneOutputTexture() const;

    [[nodiscard]] SceneViewportHandle getDefaultSceneViewportHandle() const noexcept;
    [[nodiscard]] SceneViewportHandle createSceneViewportHandle();
    void destroySceneViewportHandle(SceneViewportHandle handle);
    [[nodiscard]] bool isSceneViewportHandleValid(SceneViewportHandle handle) const;
    void setSceneViewportOutputMode(SceneViewportHandle handle, SceneOutputMode mode);
    void setSceneViewportOutputSize(SceneViewportHandle handle, uint32_t width, uint32_t height);
    [[nodiscard]] RHI::Extent2D getSceneViewportOutputSize(SceneViewportHandle handle) const;
    [[nodiscard]] const RHI::Ref<RHI::Texture>&
        getSceneViewportOutputTexture(SceneViewportHandle handle) const;
    [[nodiscard]] RenderWorld& getSceneViewportRenderWorld(SceneViewportHandle handle);
    [[nodiscard]] const RenderWorld& getSceneViewportRenderWorld(SceneViewportHandle handle) const;

    void setRenderDebugViewMode(RenderDebugViewMode mode);
    [[nodiscard]] RenderDebugViewMode getRenderDebugViewMode() const;
    void setRenderDebugVelocityScale(float scale);
    [[nodiscard]] float getRenderDebugVelocityScale() const;
    const RHI::Ref<RHI::Texture>& getRenderDebugOutputTexture() const;

    void setScenePickDebugVisualizationEnabled(bool enabled);
    bool isScenePickDebugVisualizationEnabled() const;
    void requestScenePick(uint32_t x, uint32_t y);
    std::optional<uint32_t> consumeScenePickResult();

    GLFWwindow* getNativeWindow() const;

    const RHI::Ref<RHI::Instance>& getInstance() const;
    const RHI::Ref<RHI::Adapter>& getAdapter() const;
    [[nodiscard]] const RHI::RHICapabilities& getCapabilities() const noexcept;
    const RHI::Ref<RHI::Device>& getDevice() const;
    const RHI::Ref<RHI::Queue>& getGraphicsQueue() const;
    const RHI::Ref<RHI::Swapchain>& getSwapchain() const;
    const RHI::Ref<RHI::Synchronization>& getSynchronization() const;

    const RHI::Ref<RHI::ShaderCompiler>& getShaderCompiler() const
    {
        return m_device_context.shaderCompiler();
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
        RHI::Extent2D extent{0, 0};
        RHI::Ref<RHI::Texture> color;
        RHI::Ref<RHI::Texture> depth;
        RHI::Ref<RHI::Texture> pick;
        RHI::Ref<RHI::Texture> debug_color;
        RHI::ResourceState color_state{RHI::ResourceState::Undefined};
        RHI::ResourceState depth_state{RHI::ResourceState::Undefined};
        RHI::ResourceState pick_state{RHI::ResourceState::Undefined};
        RHI::ResourceState debug_color_state{RHI::ResourceState::Undefined};
        RenderDebugViewMode debug_view_mode{RenderDebugViewMode::None};
        float debug_velocity_scale{20.0f};
        bool pick_debug_visualization_enabled{false};
        PickDebugMarker debug_pick_marker{};
        std::optional<PickRequest> queued_pick_request;
        std::optional<uint32_t> completed_pick_id;
        uint64_t generation{0};
    };

    struct RuntimeState {
        InitializationOptions initialization_options{};
        glm::vec4 clear_color{0.10f, 0.10f, 0.12f, 1.0f};
        bool initialized{false};
        bool imgui_enabled{false};
        bool frame_started{false};
        bool render_graph_profiling_enabled{false};
    };

    struct RenderFeatureHistoryState {
        bool has_previous_frame{false};
        RHI::Device* device{nullptr};
        RHI::BackendType backend_type{RHI::BackendType::Auto};
        SceneOutputMode scene_output_mode{SceneOutputMode::Swapchain};
        uint32_t framebuffer_width{0};
        uint32_t framebuffer_height{0};
        uint64_t scene_output_generation{0};
        render_flow::RenderFeatureHistoryInvalidationFlags pending_flags{
            render_flow::RenderFeatureHistoryInvalidationFlags::None};
        bool has_pending_frame{false};
        RHI::Device* pending_device{nullptr};
        RHI::BackendType pending_backend_type{RHI::BackendType::Auto};
        SceneOutputMode pending_scene_output_mode{SceneOutputMode::Swapchain};
        uint32_t pending_framebuffer_width{0};
        uint32_t pending_framebuffer_height{0};
        uint64_t pending_scene_output_generation{0};
    };

    struct SceneViewportState {
        SceneOutputState output{};
        RenderFeatureHistoryState feature_history{};
        RenderViewHistory view_history{};
        RenderWorld world{};
        std::unique_ptr<IRenderFlow> render_flow;
    };

    using SceneViewportId = SceneViewportHandle;
    static constexpr SceneViewportId kInvalidSceneViewportId{0};

    struct SceneViewportSlot {
        SceneViewportId id{kInvalidSceneViewportId};
        std::unique_ptr<SceneViewportState> state;
    };

    struct SceneViewportRenderRequest {
        luna::RenderGraphTextureHandle back_buffer;
        RHI::Extent2D framebuffer_extent{0, 0};
        RHI::Format surface_format{RHI::Format::UNDEFINED};
        RHI::BackendType backend_type{RHI::BackendType::Auto};
        glm::vec4 clear_color{0.0f};
        uint64_t frame_index{0};
        bool pick_readback_supported{false};
        bool pick_readback_slot_available{false};
    };

    struct SceneViewportRenderResult {
        luna::RenderGraphTextureHandle color;
        luna::RenderGraphTextureHandle depth;
        luna::RenderGraphTextureHandle pick;
        luna::RenderGraphTextureHandle debug;
        bool render_to_offscreen{false};
        bool render_to_swapchain{false};
        bool output_valid{false};
        bool issue_pick_readback{false};
    };

    [[nodiscard]] SwapchainCreateRequest makeSwapchainCreateRequest(uint32_t width, uint32_t height);
    void createSwapchain(uint32_t width, uint32_t height);
    void configureSwapchainFrameResources();
    RHI::Extent2D getFramebufferExtent() const;
    void handlePendingResize();
    SceneViewportId createSceneViewport();
    void destroySceneViewport(SceneViewportId id);
    SceneViewportState* findSceneViewport(SceneViewportId id);
    const SceneViewportState* findSceneViewport(SceneViewportId id) const;
    SceneViewportState& defaultSceneViewport();
    const SceneViewportState& defaultSceneViewport() const;
    [[nodiscard]] SceneViewportRenderResult renderSceneViewport(SceneViewportState& viewport,
                                                                luna::RenderGraphBuilder& graph_builder,
                                                                const SceneViewportRenderRequest& request);
    [[nodiscard]] SceneViewportState* findSceneViewportByHandle(SceneViewportHandle handle);
    [[nodiscard]] const SceneViewportState* findSceneViewportByHandle(SceneViewportHandle handle) const;
    void invalidateRenderFeatureHistory(SceneViewportState& viewport,
                                        render_flow::RenderFeatureHistoryInvalidationFlags flags) noexcept;
    [[nodiscard]] render_flow::RenderFeatureFrameContext makeRenderFeatureFrameContext(
        const SceneViewportState& viewport,
        RHI::BackendType backend_type,
        SceneOutputMode scene_output_mode,
        uint64_t frame_index,
        uint32_t framebuffer_width,
        uint32_t framebuffer_height) const;
    void stageRenderFeatureFrameContext(SceneViewportState& viewport,
                                        RHI::BackendType backend_type,
                                        SceneOutputMode scene_output_mode,
                                        uint32_t framebuffer_width,
                                        uint32_t framebuffer_height) noexcept;
    void commitStagedRenderFeatureFrameContext(SceneViewportState& viewport) noexcept;
    bool hasMatchingSceneOutputTargets(const SceneViewportState& viewport, uint32_t width, uint32_t height) const;
    void releaseFrameCommandBuffers();
    void ensureScenePickReadbackBuffers();
    void collectCompletedScenePickResult(SceneViewportState& viewport, uint32_t frame_index);
    void ensureGpuTimingResources();
    void collectCompletedGpuTiming(uint32_t frame_index);
    bool storePendingGpuTimingProfile(uint32_t frame_index, const RenderGraphProfileSnapshot& profile);
    void ensureSceneOutputTargets(SceneViewportState& viewport, uint32_t width, uint32_t height);
    void releaseSceneOutputTargets(SceneViewportState& viewport);

private:
    WindowContext m_window_context{};
    RenderDeviceContext m_device_context{};
    SwapchainManager m_swapchain_manager{};
    SwapchainImageHistory m_swapchain_image_history{};
    std::vector<SceneViewportSlot> m_scene_viewports{};
    SceneViewportId m_default_scene_viewport_id{kInvalidSceneViewportId};
    SceneViewportId m_next_scene_viewport_id{1};
    FrameResourceRing m_frame_resources{};
    uint32_t m_frame_index{0};
    uint32_t m_image_index{0};
    RuntimeState m_runtime{};
    RenderGraphProfileSnapshot m_last_render_graph_profile{};
};

} // namespace luna




