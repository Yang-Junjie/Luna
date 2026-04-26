#pragma once

#include "Renderer/RenderFlow/RenderFlowTypes.h"
#include "Renderer/RenderFlow/DefaultScene/PipelineLibrary.h"
#include "Renderer/RenderFlow/DefaultScene/SceneGpuTypes.h"
#include "Renderer/RenderFlow/DefaultScene/ScenePassResources.h"

#include <array>
#include <cstdint>
#include <glm/vec4.hpp>

namespace luna {
class RenderWorld;
}

namespace luna::RHI {
class DescriptorPool;
class DescriptorSetLayout;
class Device;
class Texture;
} // namespace luna::RHI

namespace luna::render_flow::default_scene {

class ScenePipelineResources final {
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
                               float environment_mip_count,
                               const std::array<glm::vec4, 9>& irradiance_sh,
                               const render_flow::default_scene_detail::ShadowRenderParams& shadow_params);
    void updateLightingResources(const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_base_color,
                                 const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_normal_metallic,
                                 const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_world_position_roughness,
                                 const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_emissive_ao,
                                 const luna::RHI::Ref<luna::RHI::Texture>& pick_texture);
    void updateShadowResources(const luna::RHI::Ref<luna::RHI::Texture>& shadow_map);

    [[nodiscard]] const luna::RHI::Ref<luna::RHI::Device>& device() const noexcept;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::DescriptorPool>& descriptorPool() const noexcept;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::DescriptorSetLayout>& materialLayout() const noexcept;
    [[nodiscard]] SceneDrawPassResources geometryPassResources() const noexcept;
    [[nodiscard]] SceneDrawPassResources shadowPassResources() const noexcept;
    [[nodiscard]] SceneDrawPassResources transparentPassResources() const noexcept;
    [[nodiscard]] SceneLightingPassResources lightingPassResources() const noexcept;

private:
    [[nodiscard]] SceneShaderPaths resolveShaderPaths() const;

private:
    SceneShaderPaths m_shader_paths{};
    PipelineLibrary m_library{};
};

} // namespace luna::render_flow::default_scene
