#include "Renderer/RenderFlow/DefaultScene/ResourceManager.h"

#include "Core/Log.h"
#include "Renderer/RenderWorld/RenderWorld.h"
#include "Renderer/RendererUtilities.h"
#include "Renderer/RenderFlow/DefaultScene/AssetCache.h"
#include "Renderer/RenderFlow/DefaultScene/Environment.h"
#include "Renderer/RenderFlow/DefaultScene/PipelineLibrary.h"
#include "Renderer/RenderFlow/DefaultScene/Support.h"

#include <algorithm>
#include <utility>

namespace luna::render_flow::default_scene {

namespace {

using ShaderPaths = SceneShaderPaths;
using RenderContext = SceneRenderContext;

ShaderPaths defaultShaderPaths()
{
    const std::filesystem::path shader_root = render_flow::default_scene_detail::projectRoot() / "Luna" / "Renderer" / "Shaders";
    const std::filesystem::path geometry_shader_path = shader_root / "SceneGeometry.slang";
    const std::filesystem::path shadow_shader_path = shader_root / "ShadowMapping.slang";
    const std::filesystem::path lighting_shader_path = shader_root / "SceneLighting.slang";
    return ShaderPaths{
        .geometry_vertex_path = geometry_shader_path,
        .geometry_fragment_path = geometry_shader_path,
        .shadow_vertex_path = shadow_shader_path,
        .shadow_fragment_path = shadow_shader_path,
        .lighting_vertex_path = lighting_shader_path,
        .lighting_fragment_path = lighting_shader_path,
    };
}

} // namespace

class ResourceManager::Implementation final {
public:
    void setShaderPaths(ShaderPaths shader_paths)
    {
        LUNA_RENDERER_INFO(
            "Scene render flow shader paths updated: geometry_vs='{}' geometry_fs='{}' shadow_vs='{}' shadow_fs='{}' lighting_vs='{}' lighting_fs='{}'",
                           shader_paths.geometry_vertex_path.string(),
                           shader_paths.geometry_fragment_path.string(),
                           shader_paths.shadow_vertex_path.string(),
                           shader_paths.shadow_fragment_path.string(),
                           shader_paths.lighting_vertex_path.string(),
                           shader_paths.lighting_fragment_path.string());
        m_shader_paths = std::move(shader_paths);
        m_assets.clear(AssetCache::ClearMode::MaterialsAndTextures);
        m_environment.reset();
        m_pipelines.shutdown();
    }

    void shutdown()
    {
        const bool had_state = m_pipelines.device() || m_pipelines.geometryPipeline() || m_pipelines.sceneDescriptorSet() ||
                               m_environment.sourceTexture().texture;
        if (had_state) {
            LUNA_RENDERER_INFO("Shutting down scene render flow resource manager");
        }

        m_assets.clear(AssetCache::ClearMode::All);
        m_environment.reset();
        m_pipelines.shutdown();

        if (had_state) {
            LUNA_RENDERER_INFO("Scene render flow resource manager shutdown complete");
        }
    }

    void ensurePipelines(const RenderContext& context)
    {
        if (!context.device || !context.compiler) {
            LUNA_RENDERER_WARN("Cannot ensure scene render flow pipelines: device={} compiler={}",
                               static_cast<bool>(context.device),
                               static_cast<bool>(context.compiler));
            return;
        }

        const bool device_changed = m_pipelines.device() && m_pipelines.device() != context.device;
        const bool needs_rebuild = !m_pipelines.hasCompleteState(context);
        if (device_changed) {
            LUNA_RENDERER_INFO("Scene render flow device changed; rebuilding GPU resources for backend '{}'",
                               renderer_detail::backendTypeToString(context.backend_type));
            m_assets.clear(AssetCache::ClearMode::All);
            m_environment.reset();
            m_pipelines.shutdown();
        } else if (needs_rebuild) {
            LUNA_RENDERER_INFO("Rebuilding scene render flow pipeline state for backend '{}' and color format {} ({})",
                               renderer_detail::backendTypeToString(context.backend_type),
                               renderer_detail::formatToString(context.color_format),
                               static_cast<int>(context.color_format));
            m_assets.clear(AssetCache::ClearMode::MaterialsAndTextures);
            m_environment.reset();
            m_pipelines.shutdown();
        }

        if (needs_rebuild) {
            m_pipelines.rebuild(context, resolveShaderPaths());
            m_environment.ensure(context);
            if (m_environment.sourceTexture().texture && m_environment.prefilteredTexture() &&
                m_environment.brdfLutTexture()) {
                // Scene descriptors are bound during the geometry pass, so only rewrite them
                // when the pipeline state has been rebuilt.
                m_pipelines.updateSceneBindings(m_environment.sourceTexture().texture,
                                                m_environment.prefilteredTexture(),
                                                m_environment.brdfLutTexture());
            }
        }
    }

    void prepareDraws(luna::RHI::CommandBufferEncoder& commands,
                      std::span<const DrawCommand> draw_commands,
                      const Material& default_material)
    {
        m_assets.prepareDraws(commands, draw_commands, default_material, makeBindings());
    }

    void prepareEnvironmentIfNeeded(luna::RHI::CommandBufferEncoder& commands, const RenderContext& context)
    {
        m_environment.ensure(context);
        m_environment.uploadIfNeeded(commands);
        m_environment.precomputeIfNeeded(commands);
        if (m_environment.sourceTexture().texture && m_environment.prefilteredTexture() &&
            m_environment.brdfLutTexture()) {
            m_pipelines.updateSceneBindings(m_environment.sourceTexture().texture,
                                            m_environment.prefilteredTexture(),
                                            m_environment.brdfLutTexture());
        }
    }

    void updateSceneParameters(const RenderContext& context,
                               const RenderWorld& world,
                               const render_flow::default_scene_detail::ShadowRenderParams& shadow_params)
    {
        const float environment_mip_count = m_environment.prefilteredMaxMipLevel();
        m_pipelines.updateSceneParameters(
            context, world, environment_mip_count, m_environment.irradianceSH(), shadow_params);
    }

    void updateLightingResources(const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_base_color,
                                 const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_normal_metallic,
                                 const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_world_position_roughness,
                                 const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_emissive_ao,
                                 const luna::RHI::Ref<luna::RHI::Texture>& pick_texture)
    {
        m_pipelines.updateLightingResources(gbuffer_base_color,
                                            gbuffer_normal_metallic,
                                            gbuffer_world_position_roughness,
                                            gbuffer_emissive_ao,
                                            pick_texture);
    }

    void updateShadowResources(const luna::RHI::Ref<luna::RHI::Texture>& shadow_map)
    {
        m_pipelines.updateShadowResources(shadow_map);
    }

    [[nodiscard]] ResolvedDrawResources resolveDrawResources(const DrawCommand& draw_command,
                                                             const Material& default_material) const
    {
        const AssetCache::DrawResources draw_resources = m_assets.resolveDrawResources(draw_command, default_material);
        return ResolvedDrawResources{
            .vertex_buffer = draw_resources.vertex_buffer,
            .index_buffer = draw_resources.index_buffer,
            .material_descriptor_set = draw_resources.material_descriptor_set,
            .index_count = draw_resources.index_count,
        };
    }

    [[nodiscard]] const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& geometryPipeline() const
    {
        return m_pipelines.geometryPipeline();
    }

    [[nodiscard]] const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& shadowPipeline() const
    {
        return m_pipelines.shadowPipeline();
    }

    [[nodiscard]] const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& lightingPipeline() const
    {
        return m_pipelines.lightingPipeline();
    }

    [[nodiscard]] const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& transparentPipeline() const
    {
        return m_pipelines.transparentPipeline();
    }

    [[nodiscard]] const luna::RHI::Ref<luna::RHI::DescriptorSet>& sceneDescriptorSet() const
    {
        return m_pipelines.sceneDescriptorSet();
    }

    [[nodiscard]] const luna::RHI::Ref<luna::RHI::DescriptorSet>& lightingSceneDescriptorSet() const
    {
        return m_pipelines.lightingSceneDescriptorSet();
    }

    [[nodiscard]] const luna::RHI::Ref<luna::RHI::DescriptorSet>& gbufferDescriptorSet() const
    {
        return m_pipelines.gbufferDescriptorSet();
    }

    [[nodiscard]] const luna::RHI::Ref<luna::RHI::Sampler>& gbufferSampler() const
    {
        return m_pipelines.gbufferSampler();
    }

private:
    [[nodiscard]] ShaderPaths resolveShaderPaths() const
    {
        ShaderPaths shader_paths = m_shader_paths;
        const ShaderPaths default_paths = defaultShaderPaths();
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

    [[nodiscard]] AssetCache::Bindings makeBindings() const
    {
        return AssetCache::Bindings{
            .device = m_pipelines.device(),
            .descriptor_pool = m_pipelines.descriptorPool(),
            .material_layout = m_pipelines.materialLayout(),
        };
    }

private:
    ShaderPaths m_shader_paths{};
    AssetCache m_assets{};
    EnvironmentResources m_environment{};
    PipelineLibrary m_pipelines{};
};

ResourceManager::ResourceManager()
    : m_impl(std::make_unique<Implementation>())
{}

ResourceManager::~ResourceManager()
{
    shutdown();
}

void ResourceManager::setShaderPaths(SceneShaderPaths shader_paths)
{
    m_impl->setShaderPaths(std::move(shader_paths));
}

void ResourceManager::shutdown()
{
    m_impl->shutdown();
}

void ResourceManager::ensurePipelines(const SceneRenderContext& context)
{
    m_impl->ensurePipelines(context);
}

void ResourceManager::prepareDraws(luna::RHI::CommandBufferEncoder& commands,
                                   std::span<const DrawCommand> draw_commands,
                                   const Material& default_material)
{
    m_impl->prepareDraws(commands, draw_commands, default_material);
}

void ResourceManager::prepareEnvironmentIfNeeded(luna::RHI::CommandBufferEncoder& commands,
                                                 const SceneRenderContext& context)
{
    m_impl->prepareEnvironmentIfNeeded(commands, context);
}

void ResourceManager::updateSceneParameters(
    const SceneRenderContext& context,
    const RenderWorld& world,
    const render_flow::default_scene_detail::ShadowRenderParams& shadow_params)
{
    m_impl->updateSceneParameters(context, world, shadow_params);
}

void ResourceManager::updateLightingResources(const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_base_color,
                                              const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_normal_metallic,
                                              const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_world_position_roughness,
                                              const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_emissive_ao,
                                              const luna::RHI::Ref<luna::RHI::Texture>& pick_texture)
{
    m_impl->updateLightingResources(
        gbuffer_base_color, gbuffer_normal_metallic, gbuffer_world_position_roughness, gbuffer_emissive_ao, pick_texture);
}

void ResourceManager::updateShadowResources(const luna::RHI::Ref<luna::RHI::Texture>& shadow_map)
{
    m_impl->updateShadowResources(shadow_map);
}

ResolvedDrawResources ResourceManager::resolveDrawResources(const DrawCommand& draw_command,
                                                            const Material& default_material) const
{
    return m_impl->resolveDrawResources(draw_command, default_material);
}

const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& ResourceManager::geometryPipeline() const
{
    return m_impl->geometryPipeline();
}

const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& ResourceManager::shadowPipeline() const
{
    return m_impl->shadowPipeline();
}

const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& ResourceManager::lightingPipeline() const
{
    return m_impl->lightingPipeline();
}

const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& ResourceManager::transparentPipeline() const
{
    return m_impl->transparentPipeline();
}

const luna::RHI::Ref<luna::RHI::DescriptorSet>& ResourceManager::sceneDescriptorSet() const
{
    return m_impl->sceneDescriptorSet();
}

const luna::RHI::Ref<luna::RHI::DescriptorSet>& ResourceManager::lightingSceneDescriptorSet() const
{
    return m_impl->lightingSceneDescriptorSet();
}

const luna::RHI::Ref<luna::RHI::DescriptorSet>& ResourceManager::gbufferDescriptorSet() const
{
    return m_impl->gbufferDescriptorSet();
}

const luna::RHI::Ref<luna::RHI::Sampler>& ResourceManager::gbufferSampler() const
{
    return m_impl->gbufferSampler();
}

} // namespace luna::render_flow::default_scene






