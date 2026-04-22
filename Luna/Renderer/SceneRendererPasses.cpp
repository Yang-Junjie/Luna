#include "Core/Log.h"
#include "Renderer/SceneRendererInternal.h"

#include <array>

namespace luna {

namespace {

void configureViewportAndScissor(luna::RHI::CommandBufferEncoder& commands, uint32_t width, uint32_t height)
{
    commands.SetViewport({0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f});
    commands.SetScissor({0, 0, width, height});
}

void recordDrawCommands(luna::RHI::CommandBufferEncoder& commands,
                        const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& pipeline,
                        const std::vector<scene_renderer::DrawCommand>& draw_commands,
                        const scene_renderer::ResourceManager& resources,
                        const Material& default_material)
{
    using namespace scene_renderer_detail;

    for (const auto& draw_command : draw_commands) {
        const scene_renderer::ResolvedDrawResources draw_resources =
            resources.resolveDrawResources(draw_command, default_material);
        if (!draw_resources.isValid()) {
            continue;
        }

        MeshPushConstants push_constants{
            .model = draw_command.transform,
            .picking_id = draw_command.picking_id,
        };
        const std::array<luna::RHI::Ref<luna::RHI::DescriptorSet>, 2> descriptor_sets{
            draw_resources.material_descriptor_set,
            resources.sceneDescriptorSet(),
        };

        commands.BindDescriptorSets(pipeline, 0, descriptor_sets);
        commands.PushConstants(pipeline, luna::RHI::ShaderStage::Vertex, 0, sizeof(MeshPushConstants), &push_constants);
        commands.BindVertexBuffer(0, draw_resources.vertex_buffer);
        commands.BindIndexBuffer(draw_resources.index_buffer, 0, luna::RHI::IndexType::UInt32);
        commands.DrawIndexed(draw_resources.index_count, 1, 0, 0, 0);
    }
}

} // namespace

void SceneRenderer::Implementation::setShaderPaths(ShaderPaths shader_paths)
{
    m_resources.setShaderPaths(std::move(shader_paths));
}

void SceneRenderer::Implementation::shutdown()
{
    clearSubmittedMeshes();
    m_resources.shutdown();
}

void SceneRenderer::Implementation::beginScene(const Camera& camera)
{
    m_draw_queue.beginScene(camera);
}

void SceneRenderer::Implementation::clearSubmittedMeshes()
{
    m_draw_queue.clear();
}

void SceneRenderer::Implementation::submitStaticMesh(const glm::mat4& transform,
                                                     std::shared_ptr<Mesh> mesh,
                                                     std::shared_ptr<Material> material,
                                                     uint32_t picking_id)
{
    m_draw_queue.submitStaticMesh(transform, std::move(mesh), std::move(material), picking_id);
}

void SceneRenderer::Implementation::submitStaticMesh(
    const glm::mat4& transform,
    std::shared_ptr<Mesh> mesh,
    const std::vector<std::shared_ptr<Material>>& submesh_materials,
    uint32_t picking_id)
{
    m_draw_queue.submitStaticMesh(transform, std::move(mesh), submesh_materials, picking_id);
}

SceneGBufferTextures SceneRenderer::Implementation::createGBufferTextures(rhi::RenderGraphBuilder& graph,
                                                                          const RenderContext& context) const
{
    using namespace scene_renderer_detail;

    return SceneGBufferTextures{
        .base_color = graph.CreateTexture(rhi::RenderGraphTextureDesc{
            .Name = "SceneGBufferBaseColor",
            .Width = context.framebuffer_width,
            .Height = context.framebuffer_height,
            .Format = kGBufferBaseColorFormat,
            .Usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
            .InitialState = luna::RHI::ResourceState::Undefined,
            .SampleCount = luna::RHI::SampleCount::Count1,
        }),
        .normal_metallic = graph.CreateTexture(rhi::RenderGraphTextureDesc{
            .Name = "SceneGBufferNormalMetallic",
            .Width = context.framebuffer_width,
            .Height = context.framebuffer_height,
            .Format = kGBufferLightingFormat,
            .Usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
            .InitialState = luna::RHI::ResourceState::Undefined,
            .SampleCount = luna::RHI::SampleCount::Count1,
        }),
        .world_position_roughness = graph.CreateTexture(rhi::RenderGraphTextureDesc{
            .Name = "SceneGBufferWorldPositionRoughness",
            .Width = context.framebuffer_width,
            .Height = context.framebuffer_height,
            .Format = kGBufferLightingFormat,
            .Usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
            .InitialState = luna::RHI::ResourceState::Undefined,
            .SampleCount = luna::RHI::SampleCount::Count1,
        }),
        .emissive_ao = graph.CreateTexture(rhi::RenderGraphTextureDesc{
            .Name = "SceneGBufferEmissiveAo",
            .Width = context.framebuffer_width,
            .Height = context.framebuffer_height,
            .Format = kGBufferLightingFormat,
            .Usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
            .InitialState = luna::RHI::ResourceState::Undefined,
            .SampleCount = luna::RHI::SampleCount::Count1,
        }),
    };
}

void SceneRenderer::Implementation::buildRenderGraph(rhi::RenderGraphBuilder& graph, const RenderContext& context)
{
    if (!context.isValid()) {
        return;
    }

    const SceneGBufferTextures gbuffer = createGBufferTextures(graph, context);

    graph.AddRasterPass(
        "SceneGeometry",
        [&](rhi::RenderGraphRasterPassBuilder& pass_builder) {
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
        [this, context](rhi::RenderGraphRasterPassContext& pass_context) {
            executeGeometryPass(pass_context, context);
        });

    graph.AddRasterPass(
        "SceneLighting",
        [&](rhi::RenderGraphRasterPassBuilder& pass_builder) {
            pass_builder.ReadTexture(gbuffer.base_color);
            pass_builder.ReadTexture(gbuffer.normal_metallic);
            pass_builder.ReadTexture(gbuffer.world_position_roughness);
            pass_builder.ReadTexture(gbuffer.emissive_ao);
            pass_builder.ReadTexture(context.pick_target);
            pass_builder.WriteColor(
                context.color_target,
                luna::RHI::AttachmentLoadOp::Clear,
                luna::RHI::AttachmentStoreOp::Store,
                luna::RHI::ClearValue::ColorFloat(
                    context.clear_color.r, context.clear_color.g, context.clear_color.b, context.clear_color.a));
        },
        [this, context, gbuffer](rhi::RenderGraphRasterPassContext& pass_context) {
            executeLightingPass(pass_context, context, gbuffer);
        });

    if (!m_draw_queue.hasTransparentDrawCommands()) {
        return;
    }

    graph.AddRasterPass(
        "SceneTransparent",
        [&](rhi::RenderGraphRasterPassBuilder& pass_builder) {
            pass_builder.WriteColor(
                context.color_target, luna::RHI::AttachmentLoadOp::Load, luna::RHI::AttachmentStoreOp::Store);
            pass_builder.WriteColor(
                context.pick_target, luna::RHI::AttachmentLoadOp::Load, luna::RHI::AttachmentStoreOp::Store);
            pass_builder.WriteDepth(
                context.depth_target, luna::RHI::AttachmentLoadOp::Load, luna::RHI::AttachmentStoreOp::Store);
        },
        [this, context](rhi::RenderGraphRasterPassContext& pass_context) {
            executeTransparentPass(pass_context, context);
        });
}

void SceneRenderer::Implementation::executeGeometryPass(rhi::RenderGraphRasterPassContext& pass_context,
                                                        const RenderContext& context)
{
    m_resources.ensurePipelines(context);
    if (!m_resources.geometryPipeline() || !m_resources.sceneDescriptorSet()) {
        LUNA_RENDERER_ERROR("Scene geometry pass aborted: graphics pipeline is null");
        return;
    }

    auto& commands = pass_context.commandBuffer();
    m_resources.updateSceneParameters(context, m_draw_queue.camera());
    m_resources.prepareOpaqueDraws(commands, m_draw_queue, m_default_material);

    pass_context.beginRendering();
    commands.BindGraphicsPipeline(m_resources.geometryPipeline());
    configureViewportAndScissor(commands, pass_context.framebufferWidth(), pass_context.framebufferHeight());
    recordDrawCommands(
        commands, m_resources.geometryPipeline(), m_draw_queue.opaqueDrawCommands(), m_resources, m_default_material);

    pass_context.endRendering();
}

void SceneRenderer::Implementation::executeLightingPass(rhi::RenderGraphRasterPassContext& pass_context,
                                                        const RenderContext& context,
                                                        const SceneGBufferTextures& gbuffer)
{
    m_resources.ensurePipelines(context);
    if (!m_resources.lightingPipeline() || !m_resources.gbufferDescriptorSet() || !m_resources.sceneDescriptorSet() ||
        !m_resources.gbufferSampler()) {
        LUNA_RENDERER_ERROR("Scene lighting pass aborted: deferred lighting resources are incomplete");
        return;
    }

    const auto& gbuffer_base_color = pass_context.getTexture(gbuffer.base_color);
    const auto& gbuffer_normal_metallic = pass_context.getTexture(gbuffer.normal_metallic);
    const auto& gbuffer_world_position_roughness = pass_context.getTexture(gbuffer.world_position_roughness);
    const auto& gbuffer_emissive_ao = pass_context.getTexture(gbuffer.emissive_ao);
    const auto& pick_texture = pass_context.getTexture(context.pick_target);
    if (!gbuffer_base_color || !gbuffer_normal_metallic || !gbuffer_world_position_roughness || !gbuffer_emissive_ao ||
        !pick_texture) {
        return;
    }

    auto& commands = pass_context.commandBuffer();
    m_resources.uploadEnvironmentIfNeeded(commands);
    m_resources.updateSceneParameters(context, m_draw_queue.camera());
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

void SceneRenderer::Implementation::executeTransparentPass(rhi::RenderGraphRasterPassContext& pass_context,
                                                           const RenderContext& context)
{
    m_resources.ensurePipelines(context);
    if (!m_resources.transparentPipeline() || !m_resources.sceneDescriptorSet() ||
        !m_draw_queue.hasTransparentDrawCommands()) {
        return;
    }

    auto& commands = pass_context.commandBuffer();
    m_resources.updateSceneParameters(context, m_draw_queue.camera());
    m_resources.uploadEnvironmentIfNeeded(commands);
    m_resources.prepareTransparentDraws(commands, m_draw_queue, m_default_material);
    m_draw_queue.sortTransparentBackToFront();

    pass_context.beginRendering();
    commands.BindGraphicsPipeline(m_resources.transparentPipeline());
    configureViewportAndScissor(commands, pass_context.framebufferWidth(), pass_context.framebufferHeight());
    recordDrawCommands(commands,
                       m_resources.transparentPipeline(),
                       m_draw_queue.transparentDrawCommands(),
                       m_resources,
                       m_default_material);

    pass_context.endRendering();
}

} // namespace luna
