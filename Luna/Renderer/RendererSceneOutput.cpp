#include "Core/Log.h"
#include "Renderer/Renderer.h"
#include "Renderer/RendererInternal.h"

#include <Builders.h>
#include <Device.h>

namespace luna {

namespace {

const char* sceneOutputModeToString(Renderer::SceneOutputMode mode)
{
    switch (mode) {
        case Renderer::SceneOutputMode::Swapchain:
            return "Swapchain";
        case Renderer::SceneOutputMode::OffscreenTexture:
            return "OffscreenTexture";
        default:
            return "Unknown";
    }
}

} // namespace

bool Renderer::isInitialized() const
{
    return m_runtime.initialized && m_device_context.device && m_device_context.swapchain &&
           m_device_context.graphics_queue && m_device_context.synchronization;
}

bool Renderer::isRenderingEnabled() const
{
    const auto extent = getFramebufferExtent();
    return isInitialized() && extent.width > 0 && extent.height > 0;
}

bool Renderer::isImGuiEnabled() const
{
    return m_runtime.imgui_enabled;
}

void Renderer::requestResize()
{
    m_runtime.resize_requested = true;
    LUNA_RENDERER_DEBUG("Renderer resize requested");
}

bool Renderer::isResizeRequested() const
{
    return m_runtime.resize_requested;
}

void Renderer::setImGuiEnabled(bool enabled)
{
    if (m_runtime.imgui_enabled == enabled) {
        LUNA_RENDERER_TRACE("Ignoring redundant ImGui enabled state change: {}", enabled);
        return;
    }

    m_runtime.imgui_enabled = enabled;
    LUNA_RENDERER_INFO("Renderer ImGui rendering {}", enabled ? "enabled" : "disabled");
}

Renderer::SceneOutputMode Renderer::getSceneOutputMode() const
{
    return m_scene_output.mode;
}

void Renderer::setSceneOutputMode(SceneOutputMode mode)
{
    if (m_scene_output.mode == mode) {
        LUNA_RENDERER_TRACE("Ignoring redundant scene output mode change: {}", sceneOutputModeToString(mode));
        return;
    }

    LUNA_RENDERER_INFO("Changing scene output mode from '{}' to '{}'",
                       sceneOutputModeToString(m_scene_output.mode),
                       sceneOutputModeToString(mode));
    m_scene_output.mode = mode;

    if (m_scene_output.mode != SceneOutputMode::OffscreenTexture) {
        if (m_scene_output.color || m_scene_output.depth || m_scene_output.pick) {
            LUNA_RENDERER_DEBUG("Waiting for GPU before releasing offscreen scene output targets");
            waitForGpuIdle();
        }
        releaseSceneOutputTargets();
        return;
    }

    if (m_scene_output.extent.width > 0 && m_scene_output.extent.height > 0 &&
        !hasMatchingSceneOutputTargets(m_scene_output.extent.width, m_scene_output.extent.height)) {
        ensureSceneOutputTargets(m_scene_output.extent.width, m_scene_output.extent.height);
    }
}

void Renderer::setSceneOutputSize(uint32_t width, uint32_t height)
{
    const bool size_changed = m_scene_output.extent.width != width || m_scene_output.extent.height != height;
    m_scene_output.extent = {width, height};
    if (size_changed) {
        LUNA_RENDERER_INFO("Scene output size set to {}x{}", width, height);
    }

    if (!size_changed || m_scene_output.mode != SceneOutputMode::OffscreenTexture || width == 0 || height == 0) {
        if (size_changed && (width == 0 || height == 0)) {
            LUNA_RENDERER_DEBUG("Scene output target creation delayed because requested size is zero");
        }
        return;
    }

    ensureSceneOutputTargets(width, height);
}

luna::RHI::Extent2D Renderer::getSceneOutputSize() const
{
    return m_scene_output.extent;
}

const luna::RHI::Ref<luna::RHI::Texture>& Renderer::getSceneOutputTexture() const
{
    return m_scene_output.color;
}

void Renderer::setScenePickDebugVisualizationEnabled(bool enabled)
{
    if (m_scene_output.pick_debug_visualization_enabled != enabled) {
        LUNA_RENDERER_INFO("Scene pick debug visualization {}", enabled ? "enabled" : "disabled");
    }
    m_scene_output.pick_debug_visualization_enabled = enabled;
    if (!enabled) {
        m_scene_output.debug_pick_marker = {};
    }
}

bool Renderer::isScenePickDebugVisualizationEnabled() const
{
    return m_scene_output.pick_debug_visualization_enabled;
}

void Renderer::requestScenePick(uint32_t x, uint32_t y)
{
    if (m_scene_output.mode != SceneOutputMode::OffscreenTexture || !m_scene_output.pick ||
        m_scene_output.extent.width == 0 || m_scene_output.extent.height == 0) {
        LUNA_RENDERER_WARN(
            "Ignoring scene pick request at ({}, {}) because offscreen pick output is unavailable", x, y);
        return;
    }

    const uint32_t clamped_x = (std::min) (x, m_scene_output.extent.width - 1);
    const uint32_t clamped_y = (std::min) (y, m_scene_output.extent.height - 1);
    if (clamped_x != x || clamped_y != y) {
        LUNA_RENDERER_DEBUG("Clamped scene pick request from ({}, {}) to ({}, {}) within {}x{}",
                            x,
                            y,
                            clamped_x,
                            clamped_y,
                            m_scene_output.extent.width,
                            m_scene_output.extent.height);
    } else {
        LUNA_RENDERER_DEBUG("Queued scene pick request at ({}, {})", clamped_x, clamped_y);
    }
    m_scene_output.debug_pick_marker = SceneOutputState::PickDebugMarker{
        .x = clamped_x,
        .y = clamped_y,
        .valid = m_scene_output.pick_debug_visualization_enabled,
    };
    m_scene_output.queued_pick_request = SceneOutputState::PickRequest{
        .x = clamped_x,
        .y = clamped_y,
    };
}

std::optional<uint32_t> Renderer::consumeScenePickResult()
{
    if (!m_scene_output.completed_pick_id.has_value()) {
        return std::nullopt;
    }

    const uint32_t picked_id = *m_scene_output.completed_pick_id;
    m_scene_output.completed_pick_id.reset();
    LUNA_RENDERER_DEBUG("Consumed scene pick result: entity id {}", picked_id);
    return picked_id;
}

GLFWwindow* Renderer::getNativeWindow() const
{
    return m_window_context.native_window;
}

const luna::RHI::Ref<luna::RHI::Instance>& Renderer::getInstance() const
{
    return m_device_context.instance;
}

const luna::RHI::Ref<luna::RHI::Adapter>& Renderer::getAdapter() const
{
    return m_device_context.adapter;
}

const luna::RHI::Ref<luna::RHI::Device>& Renderer::getDevice() const
{
    return m_device_context.device;
}

const luna::RHI::Ref<luna::RHI::Queue>& Renderer::getGraphicsQueue() const
{
    return m_device_context.graphics_queue;
}

const luna::RHI::Ref<luna::RHI::Swapchain>& Renderer::getSwapchain() const
{
    return m_device_context.swapchain;
}

const luna::RHI::Ref<luna::RHI::Synchronization>& Renderer::getSynchronization() const
{
    return m_device_context.synchronization;
}

uint32_t Renderer::getFramesInFlight() const
{
    return m_frame_resources.frames_in_flight;
}

Camera& Renderer::getMainCamera()
{
    return m_runtime.main_camera;
}

const Camera& Renderer::getMainCamera() const
{
    return m_runtime.main_camera;
}

SceneRenderer& Renderer::getSceneRenderer()
{
    return m_scene_renderer;
}

const SceneRenderer& Renderer::getSceneRenderer() const
{
    return m_scene_renderer;
}

glm::vec4& Renderer::getClearColor()
{
    return m_runtime.clear_color;
}

const glm::vec4& Renderer::getClearColor() const
{
    return m_runtime.clear_color;
}

bool Renderer::hasMatchingSceneOutputTargets(uint32_t width, uint32_t height) const
{
    return m_scene_output.color && m_scene_output.depth && m_scene_output.pick &&
           m_scene_output.color->GetWidth() == width && m_scene_output.color->GetHeight() == height &&
           m_scene_output.depth->GetWidth() == width && m_scene_output.depth->GetHeight() == height &&
           m_scene_output.pick->GetWidth() == width && m_scene_output.pick->GetHeight() == height &&
           m_scene_output.color->GetFormat() == m_device_context.surface_format &&
           m_scene_output.depth->GetFormat() == luna::RHI::Format::D32_FLOAT &&
           m_scene_output.pick->GetFormat() == luna::RHI::Format::R32_UINT;
}

void Renderer::ensureSceneOutputTargets(uint32_t width, uint32_t height)
{
    if (!m_device_context.device || width == 0 || height == 0) {
        LUNA_RENDERER_WARN("Cannot create scene output targets: device_available={} size={}x{}",
                           static_cast<bool>(m_device_context.device),
                           width,
                           height);
        return;
    }

    if (hasMatchingSceneOutputTargets(width, height)) {
        LUNA_RENDERER_TRACE("Scene output targets already match requested size {}x{}", width, height);
        return;
    }

    LUNA_RENDERER_INFO("Creating scene output targets {}x{} using color format {} ({})",
                       width,
                       height,
                       renderer_detail::formatToString(m_device_context.surface_format),
                       static_cast<int>(m_device_context.surface_format));
    waitForGpuIdle();
    releaseSceneOutputTargets();

    m_scene_output.color = m_device_context.device->CreateTexture(
        luna::RHI::TextureBuilder()
            .SetSize(width, height)
            .SetFormat(m_device_context.surface_format)
            .SetUsage(luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled)
            .SetInitialState(luna::RHI::ResourceState::Undefined)
            .SetName("SceneOutputColor")
            .Build());

    m_scene_output.depth =
        m_device_context.device->CreateTexture(luna::RHI::TextureBuilder()
                                                   .SetSize(width, height)
                                                   .SetFormat(luna::RHI::Format::D32_FLOAT)
                                                   .SetUsage(luna::RHI::TextureUsageFlags::DepthStencilAttachment)
                                                   .SetInitialState(luna::RHI::ResourceState::Undefined)
                                                   .SetName("SceneOutputDepth")
                                                   .Build());

    m_scene_output.pick = m_device_context.device->CreateTexture(
        luna::RHI::TextureBuilder()
            .SetSize(width, height)
            .SetFormat(luna::RHI::Format::R32_UINT)
            .SetUsage(luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled |
                      luna::RHI::TextureUsageFlags::TransferSrc)
            .SetInitialState(luna::RHI::ResourceState::Undefined)
            .SetName("SceneOutputPick")
            .Build());

    m_scene_output.color_state = luna::RHI::ResourceState::Undefined;
    m_scene_output.depth_state = luna::RHI::ResourceState::Undefined;
    m_scene_output.pick_state = luna::RHI::ResourceState::Undefined;

    if (!m_scene_output.color || !m_scene_output.depth || !m_scene_output.pick) {
        LUNA_RENDERER_ERROR("Failed to create complete scene output target set: color={} depth={} pick={}",
                            static_cast<bool>(m_scene_output.color),
                            static_cast<bool>(m_scene_output.depth),
                            static_cast<bool>(m_scene_output.pick));
    } else {
        LUNA_RENDERER_INFO("Created scene output targets {}x{}", width, height);
    }
}

void Renderer::releaseSceneOutputTargets()
{
    const bool had_targets = m_scene_output.color || m_scene_output.depth || m_scene_output.pick;
    m_scene_output.color.reset();
    m_scene_output.depth.reset();
    m_scene_output.pick.reset();
    m_scene_output.color_state = luna::RHI::ResourceState::Undefined;
    m_scene_output.depth_state = luna::RHI::ResourceState::Undefined;
    m_scene_output.pick_state = luna::RHI::ResourceState::Undefined;
    m_scene_output.queued_pick_request.reset();
    m_scene_output.completed_pick_id.reset();
    m_scene_output.debug_pick_marker = {};
    if (had_targets) {
        LUNA_RENDERER_INFO("Released scene output targets");
    }
}

} // namespace luna
