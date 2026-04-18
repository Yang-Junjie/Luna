#include "Renderer/Renderer.h"

#include <Builders.h>
#include <Device.h>

namespace luna {

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
}

bool Renderer::isResizeRequested() const
{
    return m_runtime.resize_requested;
}

void Renderer::setImGuiEnabled(bool enabled)
{
    m_runtime.imgui_enabled = enabled;
}

Renderer::SceneOutputMode Renderer::getSceneOutputMode() const
{
    return m_scene_output.mode;
}

void Renderer::setSceneOutputMode(SceneOutputMode mode)
{
    if (m_scene_output.mode == mode) {
        return;
    }

    m_scene_output.mode = mode;

    if (m_scene_output.mode != SceneOutputMode::OffscreenTexture) {
        if (m_scene_output.color || m_scene_output.depth) {
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

    if (!size_changed || m_scene_output.mode != SceneOutputMode::OffscreenTexture || width == 0 || height == 0) {
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
    return m_scene_output.color && m_scene_output.depth && m_scene_output.color->GetWidth() == width &&
           m_scene_output.color->GetHeight() == height && m_scene_output.depth->GetWidth() == width &&
           m_scene_output.depth->GetHeight() == height &&
           m_scene_output.color->GetFormat() == m_device_context.surface_format &&
           m_scene_output.depth->GetFormat() == luna::RHI::Format::D32_FLOAT;
}

void Renderer::ensureSceneOutputTargets(uint32_t width, uint32_t height)
{
    if (!m_device_context.device || width == 0 || height == 0) {
        return;
    }

    if (hasMatchingSceneOutputTargets(width, height)) {
        return;
    }

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

    m_scene_output.depth = m_device_context.device->CreateTexture(luna::RHI::TextureBuilder()
                                                                      .SetSize(width, height)
                                                                      .SetFormat(luna::RHI::Format::D32_FLOAT)
                                                                      .SetUsage(luna::RHI::TextureUsageFlags::DepthStencilAttachment)
                                                                      .SetInitialState(luna::RHI::ResourceState::Undefined)
                                                                      .SetName("SceneOutputDepth")
                                                                      .Build());

    m_scene_output.color_state = luna::RHI::ResourceState::Undefined;
    m_scene_output.depth_state = luna::RHI::ResourceState::Undefined;
}

void Renderer::releaseSceneOutputTargets()
{
    m_scene_output.color.reset();
    m_scene_output.depth.reset();
    m_scene_output.color_state = luna::RHI::ResourceState::Undefined;
    m_scene_output.depth_state = luna::RHI::ResourceState::Undefined;
}

} // namespace luna
