#pragma once

// Manages environment textures used by scene lighting.
// Loads fallback or project-provided environment data, prepares upload state,
// and exposes irradiance data needed by scene parameter updates.

#include "Renderer/RenderFlow/DefaultScene/Support.h"

#include <array>

namespace luna::render_flow::default_scene {

class EnvironmentResources final {
public:
    void reset();
    void ensure(const luna::RHI::Ref<luna::RHI::Device>& device);
    void uploadIfNeeded(luna::RHI::CommandBufferEncoder& commands);

    [[nodiscard]] const render_flow::default_scene_detail::PendingTextureUpload& sourceTexture() const noexcept
    {
        return m_source_texture;
    }

    [[nodiscard]] const std::array<glm::vec4, 9>& irradianceSH() const noexcept
    {
        return m_irradiance_sh;
    }

private:
    luna::RHI::Ref<luna::RHI::Device> m_device;
    render_flow::default_scene_detail::PendingTextureUpload m_source_texture;
    std::array<glm::vec4, 9> m_irradiance_sh{};
};

} // namespace luna::render_flow::default_scene





