#include "Renderer/SceneRendererInternal.h"

namespace luna {

SceneRenderer::~SceneRenderer()
{
    shutdown();
}

void SceneRenderer::setShaderPaths(ShaderPaths shader_paths)
{
    m_shader_paths = std::move(shader_paths);
    resetPipelineState();
}

void SceneRenderer::shutdown()
{
    clearSubmittedMeshes();
    m_upload_cache.uploaded_materials.clear();
    m_upload_cache.uploaded_textures.clear();
    m_upload_cache.uploaded_meshes.clear();
    resetPipelineState();
    m_gpu.device.reset();
}

void SceneRenderer::beginScene(const Camera& camera)
{
    m_draw_queue.camera = camera;
    clearSubmittedMeshes();
}

void SceneRenderer::clearSubmittedMeshes()
{
    m_draw_queue.opaque_draw_commands.clear();
    m_draw_queue.transparent_draw_commands.clear();
}

void SceneRenderer::submitStaticMesh(const glm::mat4& transform,
                                     std::shared_ptr<Mesh> mesh,
                                     std::shared_ptr<Material> material)
{
    if (!mesh || !mesh->isValid()) {
        return;
    }

    const size_t sub_mesh_count = mesh->getSubMeshes().size();
    std::vector<std::shared_ptr<Material>> submesh_materials(sub_mesh_count, std::move(material));
    submitStaticMesh(transform, std::move(mesh), submesh_materials);
}

void SceneRenderer::submitStaticMesh(const glm::mat4& transform,
                                     std::shared_ptr<Mesh> mesh,
                                     const std::vector<std::shared_ptr<Material>>& submesh_materials)
{
    if (!mesh || !mesh->isValid()) {
        return;
    }

    const auto& mesh_sub_meshes = mesh->getSubMeshes();
    for (size_t sub_mesh_index = 0; sub_mesh_index < mesh_sub_meshes.size(); ++sub_mesh_index) {
        const auto& sub_mesh = mesh_sub_meshes[sub_mesh_index];
        if (sub_mesh.Vertices.empty() || sub_mesh.Indices.empty()) {
            continue;
        }

        std::shared_ptr<Material> material;
        if (sub_mesh_index < submesh_materials.size()) {
            material = submesh_materials[sub_mesh_index];
        }

        StaticMeshDrawCommand draw_command{
            .transform = transform,
            .mesh = mesh,
            .material = std::move(material),
            .sub_mesh_index = static_cast<uint32_t>(sub_mesh_index),
        };

        if (draw_command.material != nullptr && draw_command.material->isTransparent()) {
            m_draw_queue.transparent_draw_commands.push_back(std::move(draw_command));
            continue;
        }

        m_draw_queue.opaque_draw_commands.push_back(std::move(draw_command));
    }
}

void SceneRenderer::buildRenderGraph(rhi::RenderGraphBuilder& graph, const RenderContext& context)
{
    using namespace scene_renderer_detail;

    if (!context.isValid()) {
        return;
    }

    const auto gbuffer_base_color_handle = graph.CreateTexture(rhi::RenderGraphTextureDesc{
        .Name = "SceneGBufferBaseColor",
        .Width = context.framebuffer_width,
        .Height = context.framebuffer_height,
        .Format = kGBufferBaseColorFormat,
        .Usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
        .InitialState = luna::RHI::ResourceState::Undefined,
        .SampleCount = luna::RHI::SampleCount::Count1,
    });

    const auto gbuffer_normal_metallic_handle = graph.CreateTexture(rhi::RenderGraphTextureDesc{
        .Name = "SceneGBufferNormalMetallic",
        .Width = context.framebuffer_width,
        .Height = context.framebuffer_height,
        .Format = kGBufferLightingFormat,
        .Usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
        .InitialState = luna::RHI::ResourceState::Undefined,
        .SampleCount = luna::RHI::SampleCount::Count1,
    });

    const auto gbuffer_world_position_roughness_handle = graph.CreateTexture(rhi::RenderGraphTextureDesc{
        .Name = "SceneGBufferWorldPositionRoughness",
        .Width = context.framebuffer_width,
        .Height = context.framebuffer_height,
        .Format = kGBufferLightingFormat,
        .Usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
        .InitialState = luna::RHI::ResourceState::Undefined,
        .SampleCount = luna::RHI::SampleCount::Count1,
    });

    const auto gbuffer_emissive_ao_handle = graph.CreateTexture(rhi::RenderGraphTextureDesc{
        .Name = "SceneGBufferEmissiveAo",
        .Width = context.framebuffer_width,
        .Height = context.framebuffer_height,
        .Format = kGBufferLightingFormat,
        .Usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
        .InitialState = luna::RHI::ResourceState::Undefined,
        .SampleCount = luna::RHI::SampleCount::Count1,
    });

    graph.AddRasterPass(
        "SceneGeometry",
        [&](rhi::RenderGraphRasterPassBuilder& pass_builder) {
            pass_builder.WriteColor(gbuffer_base_color_handle,
                                    luna::RHI::AttachmentLoadOp::Clear,
                                    luna::RHI::AttachmentStoreOp::Store,
                                    luna::RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 0.0f));
            pass_builder.WriteColor(gbuffer_normal_metallic_handle,
                                    luna::RHI::AttachmentLoadOp::Clear,
                                    luna::RHI::AttachmentStoreOp::Store,
                                    luna::RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 0.0f));
            pass_builder.WriteColor(gbuffer_world_position_roughness_handle,
                                    luna::RHI::AttachmentLoadOp::Clear,
                                    luna::RHI::AttachmentStoreOp::Store,
                                    luna::RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 0.0f));
            pass_builder.WriteColor(gbuffer_emissive_ao_handle,
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
        [=](rhi::RenderGraphRasterPassBuilder& pass_builder) {
            pass_builder.ReadTexture(gbuffer_base_color_handle);
            pass_builder.ReadTexture(gbuffer_normal_metallic_handle);
            pass_builder.ReadTexture(gbuffer_world_position_roughness_handle);
            pass_builder.ReadTexture(gbuffer_emissive_ao_handle);
            pass_builder.WriteColor(
                context.color_target,
                luna::RHI::AttachmentLoadOp::Clear,
                luna::RHI::AttachmentStoreOp::Store,
                luna::RHI::ClearValue::ColorFloat(
                    context.clear_color.r, context.clear_color.g, context.clear_color.b, context.clear_color.a));
        },
        [this,
         context,
         gbuffer_base_color_handle,
         gbuffer_normal_metallic_handle,
         gbuffer_world_position_roughness_handle,
         gbuffer_emissive_ao_handle](rhi::RenderGraphRasterPassContext& pass_context) {
            executeLightingPass(pass_context,
                                context,
                                gbuffer_base_color_handle,
                                gbuffer_normal_metallic_handle,
                                gbuffer_world_position_roughness_handle,
                                gbuffer_emissive_ao_handle);
        });

    if (!m_draw_queue.transparent_draw_commands.empty()) {
        graph.AddRasterPass(
            "SceneTransparent",
            [&](rhi::RenderGraphRasterPassBuilder& pass_builder) {
                pass_builder.WriteColor(
                    context.color_target, luna::RHI::AttachmentLoadOp::Load, luna::RHI::AttachmentStoreOp::Store);
                pass_builder.WriteDepth(
                    context.depth_target, luna::RHI::AttachmentLoadOp::Load, luna::RHI::AttachmentStoreOp::Store);
            },
            [this, context](rhi::RenderGraphRasterPassContext& pass_context) {
                executeTransparentPass(pass_context, context);
            });
    }
}

SceneRenderer::ShaderPaths SceneRenderer::getDefaultShaderPaths()
{
    const std::filesystem::path shader_root = scene_renderer_detail::projectRoot() / "Luna" / "Renderer" / "Shaders";
    const std::filesystem::path geometry_shader_path = shader_root / "SceneGeometry.slang";
    const std::filesystem::path lighting_shader_path = shader_root / "SceneLighting.slang";
    return ShaderPaths{
        .geometry_vertex_path = geometry_shader_path,
        .geometry_fragment_path = geometry_shader_path,
        .lighting_vertex_path = lighting_shader_path,
        .lighting_fragment_path = lighting_shader_path,
    };
}

std::filesystem::path SceneRenderer::getDefaultEnvironmentPath()
{
    return scene_renderer_detail::projectRoot() / "SampleProject" / "Assets" / "Texture" / "newport_loft.hdr";
}

rhi::ImageData SceneRenderer::createFallbackImageData(const glm::vec4& color)
{
    const glm::vec4 clamped_color = glm::clamp(color, glm::vec4(0.0f), glm::vec4(1.0f));
    auto to_byte = [](float channel) {
        return static_cast<uint8_t>(std::lround(channel * 255.0f));
    };

    return rhi::ImageData{
        .ByteData = {to_byte(clamped_color.r),
                     to_byte(clamped_color.g),
                     to_byte(clamped_color.b),
                     to_byte(clamped_color.a)},
        .ImageFormat = luna::RHI::Format::RGBA8_UNORM,
        .Width = 1,
        .Height = 1,
    };
}

const Material& SceneRenderer::resolveMaterial(const std::shared_ptr<Material>& material) const
{
    return material != nullptr ? *material : m_default_material;
}

} // namespace luna
