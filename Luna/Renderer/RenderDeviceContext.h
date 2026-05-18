#pragma once

#include <Capabilities.h>
#include <Core.h>

namespace luna::RHI {
class Adapter;
class Device;
class Instance;
class Queue;
class ShaderCompiler;
class Surface;
} // namespace luna::RHI

namespace luna {

class RenderDeviceContext {
public:
    [[nodiscard]] bool hasResources() const noexcept;
    [[nodiscard]] bool isInitialized() const noexcept;
    [[nodiscard]] bool isReady() const noexcept;

    void reset() noexcept;
    void waitForIdle() noexcept;

    [[nodiscard]] luna::RHI::Ref<luna::RHI::Instance>& instance() noexcept;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::Instance>& instance() const noexcept;
    [[nodiscard]] luna::RHI::Ref<luna::RHI::Adapter>& adapter() noexcept;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::Adapter>& adapter() const noexcept;
    [[nodiscard]] luna::RHI::Ref<luna::RHI::Device>& device() noexcept;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::Device>& device() const noexcept;
    [[nodiscard]] luna::RHI::Ref<luna::RHI::Surface>& surface() noexcept;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::Surface>& surface() const noexcept;
    [[nodiscard]] luna::RHI::Ref<luna::RHI::Queue>& graphicsQueue() noexcept;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::Queue>& graphicsQueue() const noexcept;
    [[nodiscard]] luna::RHI::Ref<luna::RHI::ShaderCompiler>& shaderCompiler() noexcept;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::ShaderCompiler>& shaderCompiler() const noexcept;
    [[nodiscard]] luna::RHI::RHICapabilities& capabilities() noexcept;
    [[nodiscard]] const luna::RHI::RHICapabilities& capabilities() const noexcept;

private:
    luna::RHI::Ref<luna::RHI::Instance> m_instance;
    luna::RHI::Ref<luna::RHI::Adapter> m_adapter;
    luna::RHI::Ref<luna::RHI::Device> m_device;
    luna::RHI::Ref<luna::RHI::Surface> m_surface;
    luna::RHI::Ref<luna::RHI::Queue> m_graphics_queue;
    luna::RHI::Ref<luna::RHI::ShaderCompiler> m_shader_compiler;
    luna::RHI::RHICapabilities m_capabilities{};
};

} // namespace luna
