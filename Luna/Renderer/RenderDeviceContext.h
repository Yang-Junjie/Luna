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

    [[nodiscard]] RHI::Ref<RHI::Instance>& instance() noexcept;
    [[nodiscard]] const RHI::Ref<RHI::Instance>& instance() const noexcept;
    [[nodiscard]] RHI::Ref<RHI::Adapter>& adapter() noexcept;
    [[nodiscard]] const RHI::Ref<RHI::Adapter>& adapter() const noexcept;
    [[nodiscard]] RHI::Ref<RHI::Device>& device() noexcept;
    [[nodiscard]] const RHI::Ref<RHI::Device>& device() const noexcept;
    [[nodiscard]] RHI::Ref<RHI::Surface>& surface() noexcept;
    [[nodiscard]] const RHI::Ref<RHI::Surface>& surface() const noexcept;
    [[nodiscard]] RHI::Ref<RHI::Queue>& graphicsQueue() noexcept;
    [[nodiscard]] const RHI::Ref<RHI::Queue>& graphicsQueue() const noexcept;
    [[nodiscard]] RHI::Ref<RHI::ShaderCompiler>& shaderCompiler() noexcept;
    [[nodiscard]] const RHI::Ref<RHI::ShaderCompiler>& shaderCompiler() const noexcept;
    [[nodiscard]] RHI::RHICapabilities& capabilities() noexcept;
    [[nodiscard]] const RHI::RHICapabilities& capabilities() const noexcept;

private:
    RHI::Ref<RHI::Instance> m_instance;
    RHI::Ref<RHI::Adapter> m_adapter;
    RHI::Ref<RHI::Device> m_device;
    RHI::Ref<RHI::Surface> m_surface;
    RHI::Ref<RHI::Queue> m_graphics_queue;
    RHI::Ref<RHI::ShaderCompiler> m_shader_compiler;
    RHI::RHICapabilities m_capabilities{};
};

} // namespace luna
