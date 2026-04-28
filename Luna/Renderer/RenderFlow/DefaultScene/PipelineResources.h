#pragma once

// Feature-facing pipeline resource facade.
// Owns rebuild invalidation policy and default shader path resolution; concrete GPU objects
// live in PipelineState.

#include "Renderer/RenderFlow/RenderFlowTypes.h"
#include "Renderer/RenderFlow/DefaultScene/PipelineState.h"
#include "Renderer/RenderFlow/DefaultScene/GpuTypes.h"
#include "Renderer/RenderFlow/DefaultScene/PassResources.h"
#include "Renderer/RenderFlow/LightingExtensionInputs.h"

#include <array>
#include <cstdint>
#include <glm/vec4.hpp>

namespace luna {
class RenderWorld;
}

namespace luna::RHI {
class CommandBufferEncoder;
class DescriptorPool;
class DescriptorSetLayout;
class Device;
class Texture;
} // namespace luna::RHI

namespace luna::render_flow::default_scene {

class PipelineResources final {
public:
    enum class Invalidation : uint8_t {
        None,
        MaterialsAndTextures,
        All,
    };

    void shutdown();

    [[nodiscard]] bool hasAnyState() const noexcept;
    [[nodiscard]] Invalidation invalidationFor(const SceneRenderContext& context) const noexcept;
    void rebuild(const SceneRenderContext& context);

    void updateSceneBindings(const luna::RHI::Ref<luna::RHI::Texture>& environment_texture,
                             const luna::RHI::Ref<luna::RHI::Texture>& prefiltered_environment_texture,
                             const luna::RHI::Ref<luna::RHI::Texture>& brdf_lut_texture);
    void updateSceneParameters(const SceneRenderContext& context,
                               const RenderWorld& world,
                               const RenderFeatureFrameContext& frame_context,
                               float environment_mip_count,
                               const std::array<glm::vec4, 9>& irradiance_sh,
                               const render_flow::default_scene_detail::ShadowRenderParams& shadow_params);
    void updateLightingResources(luna::RHI::CommandBufferEncoder& commands,
                                 const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_base_color,
                                 const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_normal_metallic,
                                 const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_world_position_roughness,
                                 const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_emissive_ao,
                                 const luna::RHI::Ref<luna::RHI::Texture>& velocity_texture,
                                 const luna::RHI::Ref<luna::RHI::Texture>& pick_texture,
                                 const luna::render_flow::LightingExtensionTextureRefs& lighting_extensions);
    void updateShadowResources(const luna::RHI::Ref<luna::RHI::Texture>& shadow_map);

    [[nodiscard]] const luna::RHI::Ref<luna::RHI::Device>& device() const noexcept;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::DescriptorPool>& descriptorPool() const noexcept;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::DescriptorSetLayout>& materialLayout() const noexcept;
    [[nodiscard]] DrawPassResources geometryPassResources() const noexcept;
    [[nodiscard]] DrawPassResources shadowPassResources() const noexcept;
    [[nodiscard]] DrawPassResources transparentPassResources() const noexcept;
    [[nodiscard]] LightingPassResources lightingPassResources() const noexcept;
    [[nodiscard]] DebugViewPassResources debugViewPassResources() const noexcept;

private:
    PipelineState m_pipeline_state{};
};

} // namespace luna::render_flow::default_scene
