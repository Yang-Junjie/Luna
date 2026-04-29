#include "Core/Log.h"
#include "Renderer/RenderFlow/DefaultScene/PipelineResources.h"

#include <filesystem>

namespace luna::render_flow::default_scene {
namespace {

SceneShaderPaths defaultShaderPaths()
{
    const std::filesystem::path shader_root =
        std::filesystem::path(LUNA_PROJECT_ROOT) / "Luna" / "Renderer" / "Shaders";
    const std::filesystem::path geometry_shader_path = shader_root / "SceneGeometry.slang";
    const std::filesystem::path shadow_shader_path = shader_root / "ShadowMapping.slang";
    const std::filesystem::path lighting_shader_path = shader_root / "SceneLighting.slang";
    return SceneShaderPaths{
        .geometry_vertex_path = geometry_shader_path,
        .geometry_fragment_path = geometry_shader_path,
        .shadow_vertex_path = shadow_shader_path,
        .shadow_fragment_path = shadow_shader_path,
        .lighting_vertex_path = lighting_shader_path,
        .lighting_fragment_path = lighting_shader_path,
    };
}

} // namespace

void PipelineResources::shutdown()
{
    m_pipeline_state.shutdown();
}

bool PipelineResources::hasAnyState() const noexcept
{
    return m_pipeline_state.hasAnyState();
}

PipelineResources::Invalidation PipelineResources::invalidationFor(const SceneRenderContext& context) const noexcept
{
    if (!context.device || !context.compiler) {
        return Invalidation::None;
    }

    if (m_pipeline_state.device() && m_pipeline_state.device() != context.device) {
        return Invalidation::All;
    }

    if (!m_pipeline_state.hasCompleteState(context)) {
        return Invalidation::MaterialsAndTextures;
    }

    return Invalidation::None;
}

void PipelineResources::rebuild(const SceneRenderContext& context)
{
    m_pipeline_state.rebuild(context, defaultShaderPaths());
}

void PipelineResources::updateSceneBindings(const luna::RHI::Ref<luna::RHI::Texture>& environment_texture,
                                            const luna::RHI::Ref<luna::RHI::Texture>& prefiltered_environment_texture,
                                            const luna::RHI::Ref<luna::RHI::Texture>& brdf_lut_texture)
{
    m_pipeline_state.updateSceneBindings(environment_texture, prefiltered_environment_texture, brdf_lut_texture);
}

void PipelineResources::updateSceneParameters(
    const SceneRenderContext& context,
    const RenderWorld& world,
    const RenderFeatureFrameContext& frame_context,
    float environment_mip_count,
    const std::array<glm::vec4, 9>& irradiance_sh,
    const render_flow::default_scene_detail::ShadowRenderParams& shadow_params)
{
    m_pipeline_state.updateSceneParameters(
        context, world, frame_context, environment_mip_count, irradiance_sh, shadow_params);
}

void PipelineResources::updateLightingResources(
    luna::RHI::CommandBufferEncoder& commands,
    const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_base_color,
    const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_normal_metallic,
    const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_world_position_roughness,
    const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_emissive_ao,
    const luna::RHI::Ref<luna::RHI::Texture>& velocity_texture,
    const luna::RHI::Ref<luna::RHI::Texture>& pick_texture,
    const luna::render_flow::LightingExtensionTextureRefs& lighting_extensions)
{
    m_pipeline_state.updateLightingResources(commands,
                                             gbuffer_base_color,
                                             gbuffer_normal_metallic,
                                             gbuffer_world_position_roughness,
                                             gbuffer_emissive_ao,
                                             velocity_texture,
                                             pick_texture,
                                             lighting_extensions);
}

void PipelineResources::updateShadowResources(const luna::RHI::Ref<luna::RHI::Texture>& shadow_map)
{
    m_pipeline_state.updateShadowResources(shadow_map);
}

const luna::RHI::Ref<luna::RHI::Device>& PipelineResources::device() const noexcept
{
    return m_pipeline_state.device();
}

const luna::RHI::Ref<luna::RHI::DescriptorPool>& PipelineResources::descriptorPool() const noexcept
{
    return m_pipeline_state.descriptorPool();
}

const luna::RHI::Ref<luna::RHI::DescriptorSetLayout>& PipelineResources::materialLayout() const noexcept
{
    return m_pipeline_state.materialLayout();
}

DrawPassResources PipelineResources::geometryPassResources() const noexcept
{
    return m_pipeline_state.geometryPassResources();
}

DrawPassResources PipelineResources::shadowPassResources() const noexcept
{
    return m_pipeline_state.shadowPassResources();
}

DrawPassResources PipelineResources::transparentPassResources() const noexcept
{
    return m_pipeline_state.transparentPassResources();
}

LightingPassResources PipelineResources::lightingPassResources() const noexcept
{
    return m_pipeline_state.lightingPassResources();
}

DebugViewPassResources PipelineResources::debugViewPassResources() const noexcept
{
    return m_pipeline_state.debugViewPassResources();
}

} // namespace luna::render_flow::default_scene
