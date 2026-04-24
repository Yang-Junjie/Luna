#include "Renderer/SceneRenderer/SceneRenderer.h"

#include "Core/Log.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/RendererUtilities.h"
#include "Renderer/SceneRenderer/SceneRendererDrawQueue.h"
#include "Renderer/SceneRenderer/SceneRendererResourceManager.h"
#include "Renderer/SceneRenderer/SceneRendererSupport.h"

#include <array>
#include <utility>

namespace luna {

namespace {

struct SceneGBufferTextures {
    RenderGraphTextureHandle base_color;
    RenderGraphTextureHandle normal_metallic;
    RenderGraphTextureHandle world_position_roughness;
    RenderGraphTextureHandle emissive_ao;
};

void configureViewportAndScissor(luna::RHI::CommandBufferEncoder& commands, uint32_t width, uint32_t height)
{
    commands.SetViewport({0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f});
    commands.SetScissor({0, 0, width, height});
}

size_t recordDrawCommands(luna::RHI::CommandBufferEncoder& commands,
                          const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& pipeline,
                          const std::vector<scene_renderer::DrawCommand>& draw_commands,
                          const scene_renderer::ResourceManager& resources,
                          const Material& default_material)
{
    size_t recorded_count = 0;
    for (const auto& draw_command : draw_commands) {
        const scene_renderer::ResolvedDrawResources draw_resources =
            resources.resolveDrawResources(draw_command, default_material);
        if (!draw_resources.isValid()) {
            LUNA_RENDERER_FRAME_TRACE(
                "Skipping draw command because GPU resources are incomplete: mesh='{}' submesh={} picking_id={}",
                draw_command.mesh ? draw_command.mesh->getName() : "<null>",
                draw_command.sub_mesh_index,
                draw_command.picking_id);
            continue;
        }

        scene_renderer_detail::MeshPushConstants push_constants{
            .model = draw_command.transform,
            .picking_id = draw_command.picking_id,
        };
        const std::array<luna::RHI::Ref<luna::RHI::DescriptorSet>, 2> descriptor_sets{
            draw_resources.material_descriptor_set,
            resources.sceneDescriptorSet(),
        };

        commands.BindDescriptorSets(pipeline, 0, descriptor_sets);
        commands.PushConstants(
            pipeline, luna::RHI::ShaderStage::Vertex, 0, sizeof(scene_renderer_detail::MeshPushConstants), &push_constants);
        commands.BindVertexBuffer(0, draw_resources.vertex_buffer);
        commands.BindIndexBuffer(draw_resources.index_buffer, 0, luna::RHI::IndexType::UInt32);
        commands.DrawIndexed(draw_resources.index_count, 1, 0, 0, 0);
        ++recorded_count;
    }
    return recorded_count;
}

SceneGBufferTextures createGBufferTextures(RenderGraphBuilder& graph, const SceneRenderer::RenderContext& context)
{
    return SceneGBufferTextures{
        .base_color = graph.CreateTexture(RenderGraphTextureDesc{
            .Name = "SceneGBufferBaseColor",
            .Width = context.framebuffer_width,
            .Height = context.framebuffer_height,
            .Format = scene_renderer_detail::kGBufferBaseColorFormat,
            .Usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
            .InitialState = luna::RHI::ResourceState::Undefined,
            .SampleCount = luna::RHI::SampleCount::Count1,
        }),
        .normal_metallic = graph.CreateTexture(RenderGraphTextureDesc{
            .Name = "SceneGBufferNormalMetallic",
            .Width = context.framebuffer_width,
            .Height = context.framebuffer_height,
            .Format = scene_renderer_detail::kGBufferLightingFormat,
            .Usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
            .InitialState = luna::RHI::ResourceState::Undefined,
            .SampleCount = luna::RHI::SampleCount::Count1,
        }),
        .world_position_roughness = graph.CreateTexture(RenderGraphTextureDesc{
            .Name = "SceneGBufferWorldPositionRoughness",
            .Width = context.framebuffer_width,
            .Height = context.framebuffer_height,
            .Format = scene_renderer_detail::kGBufferLightingFormat,
            .Usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
            .InitialState = luna::RHI::ResourceState::Undefined,
            .SampleCount = luna::RHI::SampleCount::Count1,
        }),
        .emissive_ao = graph.CreateTexture(RenderGraphTextureDesc{
            .Name = "SceneGBufferEmissiveAo",
            .Width = context.framebuffer_width,
            .Height = context.framebuffer_height,
            .Format = scene_renderer_detail::kGBufferLightingFormat,
            .Usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
            .InitialState = luna::RHI::ResourceState::Undefined,
            .SampleCount = luna::RHI::SampleCount::Count1,
        }),
    };
}

} // namespace

class SceneRenderer::Implementation final {
public:
    void setShaderPaths(ShaderPaths shader_paths)
    {
        m_resources.setShaderPaths(std::move(shader_paths));
    }

    void shutdown()
    {
        clearSubmittedMeshes();
        m_resources.shutdown();
    }

    void beginScene(const Camera& camera)
    {
        m_draw_queue.beginScene(camera);
    }

    void submitDirectionalLight(const DirectionalLight& light)
    {
        m_draw_queue.submitDirectionalLight(scene_renderer::DirectionalLightSubmission{
            .direction = light.direction,
            .intensity = light.intensity,
            .color = light.color,
        });
    }

    void submitPointLight(const PointLight& light)
    {
        m_draw_queue.submitPointLight(scene_renderer::PointLightSubmission{
            .position = light.position,
            .intensity = light.intensity,
            .color = light.color,
            .range = light.range,
        });
    }

    void submitSpotLight(const SpotLight& light)
    {
        m_draw_queue.submitSpotLight(scene_renderer::SpotLightSubmission{
            .position = light.position,
            .intensity = light.intensity,
            .direction = light.direction,
            .range = light.range,
            .color = light.color,
            .innerConeCos = light.innerConeCos,
            .outerConeCos = light.outerConeCos,
        });
    }

    void clearSubmittedMeshes()
    {
        m_draw_queue.clear();
    }

    void submitStaticMesh(const glm::mat4& transform,
                          std::shared_ptr<Mesh> mesh,
                          std::shared_ptr<Material> material,
                          uint32_t picking_id)
    {
        m_draw_queue.submitStaticMesh(transform, std::move(mesh), std::move(material), picking_id);
    }

    void submitStaticMesh(const glm::mat4& transform,
                          std::shared_ptr<Mesh> mesh,
                          const std::vector<std::shared_ptr<Material>>& submesh_materials,
                          uint32_t picking_id)
    {
        m_draw_queue.submitStaticMesh(transform, std::move(mesh), submesh_materials, picking_id);
    }

    void buildRenderGraph(RenderGraphBuilder& graph, const RenderContext& context)
    {
        if (!context.isValid()) {
            LUNA_RENDERER_WARN("Scene render graph build skipped because context is invalid: device={} color={} depth={} size={}x{}",
                               static_cast<bool>(context.device),
                               context.color_target.isValid(),
                               context.depth_target.isValid(),
                               context.framebuffer_width,
                               context.framebuffer_height);
            return;
        }

        LUNA_RENDERER_FRAME_DEBUG(
            "Building scene render graph: size={}x{} backend={} color_format={} ({}) opaque_draws={} transparent_draws={} pick_debug={}",
            context.framebuffer_width,
            context.framebuffer_height,
            renderer_detail::backendTypeToString(context.backend_type),
            renderer_detail::formatToString(context.color_format),
            static_cast<int>(context.color_format),
            m_draw_queue.opaqueDrawCommands().size(),
            m_draw_queue.transparentDrawCommands().size(),
            context.show_pick_debug_visualization);

        const SceneGBufferTextures gbuffer = createGBufferTextures(graph, context);

        graph.AddRasterPass(
            "SceneGeometry",
            [&](RenderGraphRasterPassBuilder& pass_builder) {
                pass_builder.WriteColor(gbuffer.base_color,
                                        luna::RHI::AttachmentLoadOp::Clear,
                                        luna::RHI::AttachmentStoreOp::Store,
                                        luna::RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 0.0f));
                pass_builder.WriteColor(gbuffer.normal_metallic,
                                        luna::RHI::AttachmentLoadOp::Clear,
                                        luna::RHI::AttachmentStoreOp::Store,
                                        luna::RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 0.0f));
                pass_builder.WriteColor(gbuffer.world_position_roughness,
                                        luna::RHI::AttachmentLoadOp::Clear,
                                        luna::RHI::AttachmentStoreOp::Store,
                                        luna::RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 0.0f));
                pass_builder.WriteColor(gbuffer.emissive_ao,
                                        luna::RHI::AttachmentLoadOp::Clear,
                                        luna::RHI::AttachmentStoreOp::Store,
                                        luna::RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 0.0f));
                pass_builder.WriteColor(context.pick_target,
                                        luna::RHI::AttachmentLoadOp::Clear,
                                        luna::RHI::AttachmentStoreOp::Store,
                                        luna::RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 0.0f));
                pass_builder.WriteDepth(context.depth_target,
                                        luna::RHI::AttachmentLoadOp::Clear,
                                        luna::RHI::AttachmentStoreOp::Store,
                                        {1.0f, 0});
            },
            [this, context](RenderGraphRasterPassContext& pass_context) {
                executeGeometryPass(pass_context, context);
            });

        graph.AddRasterPass(
            "SceneLighting",
            [&](RenderGraphRasterPassBuilder& pass_builder) {
                pass_builder.ReadTexture(gbuffer.base_color);
                pass_builder.ReadTexture(gbuffer.normal_metallic);
                pass_builder.ReadTexture(gbuffer.world_position_roughness);
                pass_builder.ReadTexture(gbuffer.emissive_ao);
                pass_builder.ReadTexture(context.pick_target);
                pass_builder.WriteColor(context.color_target,
                                        luna::RHI::AttachmentLoadOp::Clear,
                                        luna::RHI::AttachmentStoreOp::Store,
                                        luna::RHI::ClearValue::ColorFloat(
                                            context.clear_color.r, context.clear_color.g, context.clear_color.b, context.clear_color.a));
            },
            [this, context, gbuffer](RenderGraphRasterPassContext& pass_context) {
                executeLightingPass(pass_context, context, gbuffer);
            });

        if (!m_draw_queue.hasTransparentDrawCommands()) {
            return;
        }

        graph.AddRasterPass(
            "SceneTransparent",
            [&](RenderGraphRasterPassBuilder& pass_builder) {
                pass_builder.WriteColor(
                    context.color_target, luna::RHI::AttachmentLoadOp::Load, luna::RHI::AttachmentStoreOp::Store);
                pass_builder.WriteColor(
                    context.pick_target, luna::RHI::AttachmentLoadOp::Load, luna::RHI::AttachmentStoreOp::Store);
                pass_builder.WriteDepth(
                    context.depth_target, luna::RHI::AttachmentLoadOp::Load, luna::RHI::AttachmentStoreOp::Store);
            },
            [this, context](RenderGraphRasterPassContext& pass_context) {
                executeTransparentPass(pass_context, context);
            });
    }

private:
    void executeGeometryPass(RenderGraphRasterPassContext& pass_context, const RenderContext& context)
    {
        LUNA_RENDERER_FRAME_DEBUG("Executing scene geometry pass with {} opaque draw command(s)",
                                  m_draw_queue.opaqueDrawCommands().size());

        m_resources.ensurePipelines(context);
        if (!m_resources.geometryPipeline() || !m_resources.sceneDescriptorSet()) {
            LUNA_RENDERER_ERROR("Scene geometry pass aborted: graphics_pipeline={} scene_descriptor_set={}",
                                static_cast<bool>(m_resources.geometryPipeline()),
                                static_cast<bool>(m_resources.sceneDescriptorSet()));
            return;
        }

        auto& commands = pass_context.commandBuffer();
        m_resources.updateSceneParameters(context, m_draw_queue.camera(), m_draw_queue);
        m_resources.prepareOpaqueDraws(commands, m_draw_queue, m_default_material);

        pass_context.beginRendering();
        commands.BindGraphicsPipeline(m_resources.geometryPipeline());
        configureViewportAndScissor(commands, pass_context.framebufferWidth(), pass_context.framebufferHeight());
        const size_t recorded_draw_count = recordDrawCommands(
            commands, m_resources.geometryPipeline(), m_draw_queue.opaqueDrawCommands(), m_resources, m_default_material);
        LUNA_RENDERER_FRAME_DEBUG("Scene geometry pass recorded {}/{} draw command(s)",
                                  recorded_draw_count,
                                  m_draw_queue.opaqueDrawCommands().size());
        pass_context.endRendering();
    }

    void executeLightingPass(RenderGraphRasterPassContext& pass_context,
                             const RenderContext& context,
                             const SceneGBufferTextures& gbuffer)
    {
        LUNA_RENDERER_FRAME_DEBUG("Executing scene lighting pass");

        m_resources.ensurePipelines(context);
        if (!m_resources.lightingPipeline() || !m_resources.gbufferDescriptorSet() || !m_resources.sceneDescriptorSet() ||
            !m_resources.gbufferSampler()) {
            LUNA_RENDERER_ERROR(
                "Scene lighting pass aborted: lighting_pipeline={} gbuffer_descriptor_set={} scene_descriptor_set={} gbuffer_sampler={}",
                static_cast<bool>(m_resources.lightingPipeline()),
                static_cast<bool>(m_resources.gbufferDescriptorSet()),
                static_cast<bool>(m_resources.sceneDescriptorSet()),
                static_cast<bool>(m_resources.gbufferSampler()));
            return;
        }

        const auto& gbuffer_base_color = pass_context.getTexture(gbuffer.base_color);
        const auto& gbuffer_normal_metallic = pass_context.getTexture(gbuffer.normal_metallic);
        const auto& gbuffer_world_position_roughness = pass_context.getTexture(gbuffer.world_position_roughness);
        const auto& gbuffer_emissive_ao = pass_context.getTexture(gbuffer.emissive_ao);
        const auto& pick_texture = pass_context.getTexture(context.pick_target);
        if (!gbuffer_base_color || !gbuffer_normal_metallic || !gbuffer_world_position_roughness || !gbuffer_emissive_ao ||
            !pick_texture) {
            LUNA_RENDERER_WARN("Scene lighting pass aborted because one or more GBuffer textures are missing: base={} normal={} position={} emissive={} pick={}",
                               static_cast<bool>(gbuffer_base_color),
                               static_cast<bool>(gbuffer_normal_metallic),
                               static_cast<bool>(gbuffer_world_position_roughness),
                               static_cast<bool>(gbuffer_emissive_ao),
                               static_cast<bool>(pick_texture));
            return;
        }

        auto& commands = pass_context.commandBuffer();
        m_resources.uploadEnvironmentIfNeeded(commands);
        m_resources.updateSceneParameters(context, m_draw_queue.camera(), m_draw_queue);
        m_resources.updateLightingResources(
            gbuffer_base_color, gbuffer_normal_metallic, gbuffer_world_position_roughness, gbuffer_emissive_ao, pick_texture);

        pass_context.beginRendering();
        commands.BindGraphicsPipeline(m_resources.lightingPipeline());
        configureViewportAndScissor(commands, pass_context.framebufferWidth(), pass_context.framebufferHeight());
        const std::array<luna::RHI::Ref<luna::RHI::DescriptorSet>, 2> descriptor_sets{
            m_resources.gbufferDescriptorSet(),
            m_resources.sceneDescriptorSet(),
        };
        commands.BindDescriptorSets(m_resources.lightingPipeline(), 0, descriptor_sets);
        commands.Draw(3, 1, 0, 0);
        pass_context.endRendering();
    }

    void executeTransparentPass(RenderGraphRasterPassContext& pass_context, const RenderContext& context)
    {
        LUNA_RENDERER_FRAME_DEBUG("Executing scene transparent pass with {} draw command(s)",
                                  m_draw_queue.transparentDrawCommands().size());

        m_resources.ensurePipelines(context);
        if (!m_resources.transparentPipeline() || !m_resources.sceneDescriptorSet() ||
            !m_draw_queue.hasTransparentDrawCommands()) {
            LUNA_RENDERER_FRAME_DEBUG("Scene transparent pass skipped: transparent_pipeline={} scene_descriptor_set={} has_draws={}",
                                      static_cast<bool>(m_resources.transparentPipeline()),
                                      static_cast<bool>(m_resources.sceneDescriptorSet()),
                                      m_draw_queue.hasTransparentDrawCommands());
            return;
        }

        m_draw_queue.sortTransparentBackToFront();

        auto& commands = pass_context.commandBuffer();
        m_resources.updateSceneParameters(context, m_draw_queue.camera(), m_draw_queue);
        m_resources.uploadEnvironmentIfNeeded(commands);
        m_resources.prepareTransparentDraws(commands, m_draw_queue, m_default_material);

        pass_context.beginRendering();
        commands.BindGraphicsPipeline(m_resources.transparentPipeline());
        configureViewportAndScissor(commands, pass_context.framebufferWidth(), pass_context.framebufferHeight());
        const size_t recorded_draw_count = recordDrawCommands(commands,
                                                              m_resources.transparentPipeline(),
                                                              m_draw_queue.transparentDrawCommands(),
                                                              m_resources,
                                                              m_default_material);
        LUNA_RENDERER_FRAME_DEBUG("Scene transparent pass recorded {}/{} draw command(s)",
                                  recorded_draw_count,
                                  m_draw_queue.transparentDrawCommands().size());
        pass_context.endRendering();
    }

private:
    scene_renderer::DrawQueue m_draw_queue{};
    scene_renderer::ResourceManager m_resources{};
    Material m_default_material{};
};

SceneRenderer::SceneRenderer()
    : m_impl(std::make_unique<Implementation>())
{}

SceneRenderer::~SceneRenderer()
{
    shutdown();
}

void SceneRenderer::setShaderPaths(ShaderPaths shader_paths)
{
    m_impl->setShaderPaths(std::move(shader_paths));
}

void SceneRenderer::shutdown()
{
    m_impl->shutdown();
}

void SceneRenderer::beginScene(const Camera& camera)
{
    m_impl->beginScene(camera);
}

void SceneRenderer::submitDirectionalLight(const DirectionalLight& light)
{
    m_impl->submitDirectionalLight(light);
}

void SceneRenderer::submitPointLight(const PointLight& light)
{
    m_impl->submitPointLight(light);
}

void SceneRenderer::submitSpotLight(const SpotLight& light)
{
    m_impl->submitSpotLight(light);
}

void SceneRenderer::clearSubmittedMeshes()
{
    m_impl->clearSubmittedMeshes();
}

void SceneRenderer::submitStaticMesh(const glm::mat4& transform,
                                     std::shared_ptr<Mesh> mesh,
                                     std::shared_ptr<Material> material,
                                     uint32_t picking_id)
{
    m_impl->submitStaticMesh(transform, std::move(mesh), std::move(material), picking_id);
}

void SceneRenderer::submitStaticMesh(const glm::mat4& transform,
                                     std::shared_ptr<Mesh> mesh,
                                     const std::vector<std::shared_ptr<Material>>& submesh_materials,
                                     uint32_t picking_id)
{
    m_impl->submitStaticMesh(transform, std::move(mesh), submesh_materials, picking_id);
}

void SceneRenderer::buildRenderGraph(RenderGraphBuilder& graph, const RenderContext& context)
{
    m_impl->buildRenderGraph(graph, context);
}

} // namespace luna
