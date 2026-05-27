#include "Core/Log.h"
#include "Renderer/RenderDeviceContext.h"

#include <exception>
#include <Queue.h>

namespace luna {

bool RenderDeviceContext::hasResources() const noexcept
{
    return m_instance || m_adapter || m_device || m_surface || m_graphics_queue || m_shader_compiler;
}

bool RenderDeviceContext::isInitialized() const noexcept
{
    return m_instance && m_adapter && m_device && m_surface && m_graphics_queue;
}

bool RenderDeviceContext::isReady() const noexcept
{
    return m_device && m_graphics_queue;
}

void RenderDeviceContext::reset() noexcept
{
    m_graphics_queue.reset();
    m_shader_compiler.reset();
    m_device.reset();
    m_surface.reset();
    m_adapter.reset();
    m_instance.reset();
    m_capabilities = {};
}

void RenderDeviceContext::waitForIdle() noexcept
{
    if (!m_device) {
        return;
    }

    try {
        if (m_graphics_queue) {
            LUNA_RENDERER_TRACE("Waiting for graphics queue idle");
            m_graphics_queue->WaitIdle();
        }
    } catch (const std::exception& error) {
        LUNA_RENDERER_WARN("Ignoring GPU idle wait failure during renderer teardown: {}", error.what());
    } catch (...) {
        LUNA_RENDERER_WARN("Ignoring unknown GPU idle wait failure during renderer teardown");
    }
}

RHI::Ref<RHI::Instance>& RenderDeviceContext::instance() noexcept
{
    return m_instance;
}

const RHI::Ref<RHI::Instance>& RenderDeviceContext::instance() const noexcept
{
    return m_instance;
}

RHI::Ref<RHI::Adapter>& RenderDeviceContext::adapter() noexcept
{
    return m_adapter;
}

const RHI::Ref<RHI::Adapter>& RenderDeviceContext::adapter() const noexcept
{
    return m_adapter;
}

RHI::Ref<RHI::Device>& RenderDeviceContext::device() noexcept
{
    return m_device;
}

const RHI::Ref<RHI::Device>& RenderDeviceContext::device() const noexcept
{
    return m_device;
}

RHI::Ref<RHI::Surface>& RenderDeviceContext::surface() noexcept
{
    return m_surface;
}

const RHI::Ref<RHI::Surface>& RenderDeviceContext::surface() const noexcept
{
    return m_surface;
}

RHI::Ref<RHI::Queue>& RenderDeviceContext::graphicsQueue() noexcept
{
    return m_graphics_queue;
}

const RHI::Ref<RHI::Queue>& RenderDeviceContext::graphicsQueue() const noexcept
{
    return m_graphics_queue;
}

RHI::Ref<RHI::ShaderCompiler>& RenderDeviceContext::shaderCompiler() noexcept
{
    return m_shader_compiler;
}

const RHI::Ref<RHI::ShaderCompiler>& RenderDeviceContext::shaderCompiler() const noexcept
{
    return m_shader_compiler;
}

RHI::RHICapabilities& RenderDeviceContext::capabilities() noexcept
{
    return m_capabilities;
}

const RHI::RHICapabilities& RenderDeviceContext::capabilities() const noexcept
{
    return m_capabilities;
}

} // namespace luna
