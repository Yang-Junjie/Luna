#include "Renderer/RenderFlow/DefaultScene/ScenePipelineResources.h"

#include "Core/Log.h"
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

void ScenePipelineResources::shutdown()
{
    m_library.shutdown();
}

bool ScenePipelineResources::hasAnyState() const noexcept
{
    return m_library.hasAnyState();
}

ScenePipelineResources::Invalidation ScenePipelineResources::invalidationFor(const SceneRenderContext& context) const noexcept
{
    if (!context.device || !context.compiler) {
        return Invalidation::None;
    }

    if (m_library.device() && m_library.device() != context.device) {
        return Invalidation::All;
    }

    if (!m_library.hasCompleteState(context)) {
        return Invalidation::MaterialsAndTextures;
    }

    return Invalidation::None;
}

void ScenePipelineResources::rebuild(const SceneRenderContext& context)
{
    m_library.rebuild(context, resolveShaderPaths());
}

void ScenePipelineResources::updateSceneBindings(
    const luna::RHI::Ref<luna::RHI::Texture>& environment_texture,
    const luna::RHI::Ref<luna::RHI::Texture>& prefiltered_environment_texture,
    const luna::RHI::Ref<luna::RHI::Texture>& brdf_lut_texture)
{
    m_library.updateSceneBindings(environment_texture, prefiltered_environment_texture, brdf_lut_texture);
}

void ScenePipelineResources::updateSceneParameters(
    const SceneRenderContext& context,
    const RenderWorld& world,
    float environment_mip_count,
    const std::array<glm::vec4, 9>& irradiance_sh,
    const render_flow::default_scene_detail::ShadowRenderParams& shadow_params)
{
    m_library.updateSceneParameters(context, world, environment_mip_count, irradiance_sh, shadow_params);
}

void ScenePipelineResources::updateLightingResources(
    const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_base_color,
    const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_normal_metallic,
    const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_world_position_roughness,
    const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_emissive_ao,
    const luna::RHI::Ref<luna::RHI::Texture>& pick_texture)
{
    m_library.updateLightingResources(
        gbuffer_base_color, gbuffer_normal_metallic, gbuffer_world_position_roughness, gbuffer_emissive_ao, pick_texture);
}

void ScenePipelineResources::updateShadowResources(const luna::RHI::Ref<luna::RHI::Texture>& shadow_map)
{
    m_library.updateShadowResources(shadow_map);
}

const luna::RHI::Ref<luna::RHI::Device>& ScenePipelineResources::device() const noexcept
{
    return m_library.device();
}

const luna::RHI::Ref<luna::RHI::DescriptorPool>& ScenePipelineResources::descriptorPool() const noexcept
{
    return m_library.descriptorPool();
}

const luna::RHI::Ref<luna::RHI::DescriptorSetLayout>& ScenePipelineResources::materialLayout() const noexcept
{
    return m_library.materialLayout();
}

SceneDrawPassResources ScenePipelineResources::geometryPassResources() const noexcept
{
    return m_library.geometryPassResources();
}

SceneDrawPassResources ScenePipelineResources::shadowPassResources() const noexcept
{
    return m_library.shadowPassResources();
}

SceneDrawPassResources ScenePipelineResources::transparentPassResources() const noexcept
{
    return m_library.transparentPassResources();
}

SceneLightingPassResources ScenePipelineResources::lightingPassResources() const noexcept
{
    return m_library.lightingPassResources();
}

SceneShaderPaths ScenePipelineResources::resolveShaderPaths() const
{
    SceneShaderPaths shader_paths = m_shader_paths;
    const SceneShaderPaths default_paths = defaultShaderPaths();
    if (shader_paths.geometry_vertex_path.empty()) {
        shader_paths.geometry_vertex_path = default_paths.geometry_vertex_path;
    }
    if (shader_paths.geometry_fragment_path.empty()) {
        shader_paths.geometry_fragment_path = default_paths.geometry_fragment_path;
    }
    if (shader_paths.shadow_vertex_path.empty()) {
        shader_paths.shadow_vertex_path = default_paths.shadow_vertex_path;
    }
    if (shader_paths.shadow_fragment_path.empty()) {
        shader_paths.shadow_fragment_path = default_paths.shadow_fragment_path;
    }
    if (shader_paths.lighting_vertex_path.empty()) {
        shader_paths.lighting_vertex_path = default_paths.lighting_vertex_path;
    }
    if (shader_paths.lighting_fragment_path.empty()) {
        shader_paths.lighting_fragment_path = default_paths.lighting_fragment_path;
    }
    return shader_paths;
}

} // namespace luna::render_flow::default_scene
