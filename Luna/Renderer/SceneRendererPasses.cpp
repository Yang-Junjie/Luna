#include "Core/Log.h"
#include "Renderer/SceneRendererInternal.h"

namespace luna {

void SceneRenderer::executeGeometryPass(rhi::RenderGraphRasterPassContext& pass_context, const RenderContext& context)
{
    using namespace scene_renderer_detail;

    ensurePipelines(context);
    auto& gpu = m_gpu;
    auto& draw_queue = m_draw_queue;
    auto& upload_cache = m_upload_cache;

    if (!gpu.geometry_pipeline || !gpu.scene_descriptor_set) {
        LUNA_RENDERER_ERROR("Scene geometry pass aborted: graphics pipeline is null");
        return;
    }

    auto& commands = pass_context.commandBuffer();
    updateSceneParameters(context);

    getOrCreateUploadedMaterial(m_default_material);

    for (const auto& draw_command : draw_queue.opaque_draw_commands) {
        if (!draw_command.mesh || !draw_command.mesh->isValid()) {
            continue;
        }

        (void) getOrCreateUploadedMesh(*draw_command.mesh);
        auto& uploaded_material = getOrCreateUploadedMaterial(resolveMaterial(draw_command.material));
        uploadMaterialIfNeeded(commands, uploaded_material);
    }

    pass_context.beginRendering();
    commands.BindGraphicsPipeline(gpu.geometry_pipeline);
    commands.SetViewport({0.0f,
                          0.0f,
                          static_cast<float>(pass_context.framebufferWidth()),
                          static_cast<float>(pass_context.framebufferHeight()),
                          0.0f,
                          1.0f});
    commands.SetScissor({0, 0, pass_context.framebufferWidth(), pass_context.framebufferHeight()});

    for (const auto& draw_command : draw_queue.opaque_draw_commands) {
        if (!draw_command.mesh || !draw_command.mesh->isValid()) {
            continue;
        }

        const auto uploaded_mesh_it = upload_cache.uploaded_meshes.find(draw_command.mesh.get());
        if (uploaded_mesh_it == upload_cache.uploaded_meshes.end()) {
            continue;
        }

        const Material& material = resolveMaterial(draw_command.material);
        const auto uploaded_material_it = upload_cache.uploaded_materials.find(&material);
        if (uploaded_material_it == upload_cache.uploaded_materials.end() || !uploaded_material_it->second.descriptor_set) {
            continue;
        }

        const auto& uploaded_mesh = uploaded_mesh_it->second;
        const auto& uploaded_material = uploaded_material_it->second;
        const std::array<luna::RHI::Ref<luna::RHI::DescriptorSet>, 2> descriptor_sets{
            uploaded_material.descriptor_set,
            gpu.scene_descriptor_set,
        };

        MeshPushConstants push_constants;
        push_constants.model = draw_command.transform;
        commands.BindDescriptorSets(gpu.geometry_pipeline, 0, descriptor_sets);
        commands.BindVertexBuffer(0, uploaded_mesh.vertex_buffer);
        commands.BindIndexBuffer(uploaded_mesh.index_buffer, 0, luna::RHI::IndexType::UInt32);
        commands.PushConstants(
            gpu.geometry_pipeline, luna::RHI::ShaderStage::Vertex, 0, sizeof(MeshPushConstants), &push_constants);
        commands.DrawIndexed(uploaded_mesh.index_count, 1, 0, 0, 0);
    }

    pass_context.endRendering();
}

void SceneRenderer::executeLightingPass(rhi::RenderGraphRasterPassContext& pass_context,
                                        const RenderContext& context,
                                        rhi::RenderGraphTextureHandle gbuffer_base_color_handle,
                                        rhi::RenderGraphTextureHandle gbuffer_normal_metallic_handle,
                                        rhi::RenderGraphTextureHandle gbuffer_world_position_roughness_handle,
                                        rhi::RenderGraphTextureHandle gbuffer_emissive_ao_handle)
{
    ensurePipelines(context);
    auto& gpu = m_gpu;

    if (!gpu.lighting_pipeline || !gpu.gbuffer_descriptor_set || !gpu.scene_descriptor_set || !gpu.gbuffer_sampler) {
        LUNA_RENDERER_ERROR("Scene lighting pass aborted: deferred lighting resources are incomplete");
        return;
    }

    const auto& gbuffer_base_color = pass_context.getTexture(gbuffer_base_color_handle);
    const auto& gbuffer_normal_metallic = pass_context.getTexture(gbuffer_normal_metallic_handle);
    const auto& gbuffer_world_position_roughness = pass_context.getTexture(gbuffer_world_position_roughness_handle);
    const auto& gbuffer_emissive_ao = pass_context.getTexture(gbuffer_emissive_ao_handle);
    if (!gbuffer_base_color || !gbuffer_normal_metallic || !gbuffer_world_position_roughness || !gbuffer_emissive_ao) {
        return;
    }

    auto& commands = pass_context.commandBuffer();
    uploadTextureIfNeeded(commands, gpu.environment_texture);
    updateSceneParameters(context);

    gpu.gbuffer_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 0,
        .TextureView = gbuffer_base_color->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    gpu.gbuffer_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 1,
        .TextureView = gbuffer_normal_metallic->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    gpu.gbuffer_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 2,
        .TextureView = gbuffer_world_position_roughness->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    gpu.gbuffer_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 3,
        .TextureView = gbuffer_emissive_ao->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    gpu.gbuffer_descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
        .Binding = 4,
        .Sampler = gpu.gbuffer_sampler,
    });
    gpu.gbuffer_descriptor_set->Update();

    pass_context.beginRendering();
    commands.BindGraphicsPipeline(gpu.lighting_pipeline);
    commands.SetViewport({0.0f,
                          0.0f,
                          static_cast<float>(pass_context.framebufferWidth()),
                          static_cast<float>(pass_context.framebufferHeight()),
                          0.0f,
                          1.0f});
    commands.SetScissor({0, 0, pass_context.framebufferWidth(), pass_context.framebufferHeight()});

    const std::array<luna::RHI::Ref<luna::RHI::DescriptorSet>, 2> descriptor_sets{
        gpu.gbuffer_descriptor_set,
        gpu.scene_descriptor_set,
    };
    commands.BindDescriptorSets(gpu.lighting_pipeline, 0, descriptor_sets);
    commands.Draw(3, 1, 0, 0);

    pass_context.endRendering();
}

void SceneRenderer::executeTransparentPass(rhi::RenderGraphRasterPassContext& pass_context, const RenderContext& context)
{
    using namespace scene_renderer_detail;

    ensurePipelines(context);
    auto& gpu = m_gpu;
    auto& draw_queue = m_draw_queue;
    auto& upload_cache = m_upload_cache;

    if (!gpu.transparent_pipeline || !gpu.scene_descriptor_set || draw_queue.transparent_draw_commands.empty()) {
        return;
    }

    auto& commands = pass_context.commandBuffer();
    updateSceneParameters(context);
    uploadTextureIfNeeded(commands, gpu.environment_texture);

    getOrCreateUploadedMaterial(m_default_material);

    for (const auto& draw_command : draw_queue.transparent_draw_commands) {
        if (!draw_command.mesh || !draw_command.mesh->isValid()) {
            continue;
        }

        (void) getOrCreateUploadedMesh(*draw_command.mesh);
        auto& uploaded_material = getOrCreateUploadedMaterial(resolveMaterial(draw_command.material));
        uploadMaterialIfNeeded(commands, uploaded_material);
    }

    sortTransparentDrawCommands();

    pass_context.beginRendering();
    commands.BindGraphicsPipeline(gpu.transparent_pipeline);
    commands.SetViewport({0.0f,
                          0.0f,
                          static_cast<float>(pass_context.framebufferWidth()),
                          static_cast<float>(pass_context.framebufferHeight()),
                          0.0f,
                          1.0f});
    commands.SetScissor({0, 0, pass_context.framebufferWidth(), pass_context.framebufferHeight()});

    for (const auto& draw_command : draw_queue.transparent_draw_commands) {
        if (!draw_command.mesh || !draw_command.mesh->isValid()) {
            continue;
        }

        const auto uploaded_mesh_it = upload_cache.uploaded_meshes.find(draw_command.mesh.get());
        if (uploaded_mesh_it == upload_cache.uploaded_meshes.end()) {
            continue;
        }

        const Material& material = resolveMaterial(draw_command.material);
        const auto uploaded_material_it = upload_cache.uploaded_materials.find(&material);
        if (uploaded_material_it == upload_cache.uploaded_materials.end() || !uploaded_material_it->second.descriptor_set) {
            continue;
        }

        const auto& uploaded_mesh = uploaded_mesh_it->second;
        const auto& uploaded_material = uploaded_material_it->second;
        const std::array<luna::RHI::Ref<luna::RHI::DescriptorSet>, 2> descriptor_sets{
            uploaded_material.descriptor_set,
            gpu.scene_descriptor_set,
        };

        MeshPushConstants push_constants;
        push_constants.model = draw_command.transform;
        commands.BindDescriptorSets(gpu.transparent_pipeline, 0, descriptor_sets);
        commands.BindVertexBuffer(0, uploaded_mesh.vertex_buffer);
        commands.BindIndexBuffer(uploaded_mesh.index_buffer, 0, luna::RHI::IndexType::UInt32);
        commands.PushConstants(
            gpu.transparent_pipeline, luna::RHI::ShaderStage::Vertex, 0, sizeof(MeshPushConstants), &push_constants);
        commands.DrawIndexed(uploaded_mesh.index_count, 1, 0, 0, 0);
    }

    pass_context.endRendering();
}

void SceneRenderer::sortTransparentDrawCommands()
{
    using namespace scene_renderer_detail;

    std::sort(m_draw_queue.transparent_draw_commands.begin(),
              m_draw_queue.transparent_draw_commands.end(),
              [this](const StaticMeshDrawCommand& lhs, const StaticMeshDrawCommand& rhs) {
                  return transparentSortDistanceSq(lhs.transform, m_draw_queue.camera) >
                         transparentSortDistanceSq(rhs.transform, m_draw_queue.camera);
              });
}

} // namespace luna
