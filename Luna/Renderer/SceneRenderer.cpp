#include "Core/Log.h"
#include "Renderer/SceneRenderer.h"

#include <cmath>
#include <cstring>

#include <array>
#include <Barrier.h>
#include <Buffer.h>
#include <Builders.h>
#include <CommandBufferEncoder.h>
#include <DescriptorPool.h>
#include <DescriptorSet.h>
#include <DescriptorSetLayout.h>
#include <Device.h>
#include <filesystem>
#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <Pipeline.h>
#include <PipelineLayout.h>
#include <Sampler.h>
#include <ShaderCompiler.h>
#include <ShaderModule.h>
#include <string_view>
#include <Texture.h>

namespace {

struct MeshPushConstants {
    glm::mat4 model{1.0f};
    glm::mat4 view_projection{1.0f};
};

std::filesystem::path projectRoot()
{
    return std::filesystem::path(LUNA_PROJECT_ROOT);
}

luna::RHI::Ref<luna::RHI::ShaderModule> loadShaderModule(const luna::RHI::Ref<luna::RHI::Device>& device,
                                                 const luna::RHI::Ref<luna::RHI::ShaderCompiler>& compiler,
                                                 const std::filesystem::path& path,
                                                 std::string_view entry_point,
                                                 luna::RHI::ShaderStage stage)
{
    if (!device || !compiler) {
        return {};
    }

    luna::RHI::ShaderCreateInfo create_info;
    create_info.SourcePath = path.string();
    create_info.EntryPoint = std::string(entry_point);
    create_info.Stage = stage;
    return compiler->CompileOrLoad(device, create_info);
}

glm::mat4 buildViewProjection(const Camera& camera, float aspect_ratio, luna::RHI::BackendType backend_type)
{
    const float clamped_aspect_ratio = std::max(aspect_ratio, 0.001f);
    glm::mat4 projection = glm::perspectiveRH_ZO(glm::radians(50.0f), clamped_aspect_ratio, 0.05f, 200.0f);
    if (backend_type == luna::RHI::BackendType::Vulkan) {
        projection[1][1] *= -1.0f;
    }
    return projection * camera.getViewMatrix();
}

} // namespace

namespace luna {

SceneRenderer::~SceneRenderer()
{
    shutdown();
}

void SceneRenderer::setShaderPaths(ShaderPaths shader_paths)
{
    m_shader_paths = std::move(shader_paths);
    m_pipeline.reset();
    m_pipeline_layout.reset();
    m_vertex_shader.reset();
    m_fragment_shader.reset();
    m_surface_format = luna::RHI::Format::UNDEFINED;
}

void SceneRenderer::shutdown()
{
    clearSubmittedMeshes();
    m_uploaded_materials.clear();
    m_uploaded_meshes.clear();
    m_pipeline.reset();
    m_pipeline_layout.reset();
    m_material_layout.reset();
    m_descriptor_pool.reset();
    m_material_sampler.reset();
    m_vertex_shader.reset();
    m_fragment_shader.reset();
    m_device.reset();
    m_surface_format = luna::RHI::Format::UNDEFINED;
}

void SceneRenderer::beginScene(const Camera& camera)
{
    m_camera = camera;
    m_submitted_meshes.clear();
}

void SceneRenderer::clearSubmittedMeshes()
{
    m_submitted_meshes.clear();
}

void SceneRenderer::submitStaticMesh(const glm::mat4& transform,
                                     std::shared_ptr<Mesh> mesh,
                                     std::shared_ptr<Material> material)
{
    if (!mesh || !mesh->isValid()) {
        return;
    }

    m_submitted_meshes.push_back(StaticMeshDrawCommand{
        .transform = transform,
        .mesh = std::move(mesh),
        .material = std::move(material),
    });
}

void SceneRenderer::buildRenderGraph(rhi::RenderGraphBuilder& graph, const RenderContext& context)
{
    if (!context.device || !context.color_target.isValid() || !context.depth_target.isValid() ||
        context.framebuffer_width == 0 || context.framebuffer_height == 0) {
        return;
    }

    graph.AddRasterPass(
        "OpaqueScene",
        [&](rhi::RenderGraphRasterPassBuilder& pass_builder) {
            pass_builder.WriteColor(
                context.color_target,
                luna::RHI::AttachmentLoadOp::Clear,
                luna::RHI::AttachmentStoreOp::Store,
                luna::RHI::ClearValue::ColorFloat(
                    context.clear_color.r, context.clear_color.g, context.clear_color.b, context.clear_color.a));
            pass_builder.WriteDepth(context.depth_target,
                                    luna::RHI::AttachmentLoadOp::Clear,
                                    luna::RHI::AttachmentStoreOp::Store,
                                    {1.0f, 0});
        },
        [this, context](rhi::RenderGraphRasterPassContext& pass_context) {
            executeOpaquePass(pass_context, context);
        });
}

SceneRenderer::ShaderPaths SceneRenderer::getDefaultShaderPaths()
{
    const std::filesystem::path shader_root = projectRoot() / "Luna" / "Renderer" / "Shaders";
    const std::filesystem::path shader_path = shader_root / "SceneGeometry.slang";
    return ShaderPaths{
        .geometry_vertex_path = shader_path,
        .geometry_fragment_path = shader_path,
        .lighting_vertex_path = {},
        .lighting_fragment_path = {},
    };
}

rhi::ImageData SceneRenderer::createFallbackImageData(const glm::vec4& albedo_color)
{
    const glm::vec4 clamped_color = glm::clamp(albedo_color, glm::vec4(0.0f), glm::vec4(1.0f));
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

void SceneRenderer::ensurePipeline(const RenderContext& context)
{
    if (!context.device || !context.compiler) {
        return;
    }

    if (m_device != context.device) {
        m_uploaded_materials.clear();
        m_uploaded_meshes.clear();
        m_pipeline.reset();
        m_pipeline_layout.reset();
        m_material_layout.reset();
        m_descriptor_pool.reset();
        m_material_sampler.reset();
        m_vertex_shader.reset();
        m_fragment_shader.reset();
        m_device = context.device;
        m_surface_format = luna::RHI::Format::UNDEFINED;
    }

    if (m_pipeline && m_surface_format == context.color_format) {
        return;
    }

    const ShaderPaths shader_paths = resolveShaderPaths();
    m_vertex_shader = loadShaderModule(
        m_device, context.compiler, shader_paths.geometry_vertex_path, "sceneVertexMain", luna::RHI::ShaderStage::Vertex);
    m_fragment_shader = loadShaderModule(m_device,
                                         context.compiler,
                                         shader_paths.geometry_fragment_path,
                                         "sceneFragmentMain",
                                         luna::RHI::ShaderStage::Fragment);
    if (!m_vertex_shader || !m_fragment_shader) {
        LUNA_CORE_ERROR("Failed to load scene renderer shaders: '{}' '{}'",
                        shader_paths.geometry_vertex_path.string(),
                        shader_paths.geometry_fragment_path.string());
        return;
    }

    m_material_layout = m_device->CreateDescriptorSetLayout(
        luna::RHI::DescriptorSetLayoutBuilder()
            .AddBinding(0, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(1, luna::RHI::DescriptorType::Sampler, 1, luna::RHI::ShaderStage::Fragment)
            .Build());

    m_descriptor_pool =
        m_device->CreateDescriptorPool(luna::RHI::DescriptorPoolBuilder()
                                           .SetMaxSets(1'024)
                                           .AddPoolSize(luna::RHI::DescriptorType::SampledImage, 1'024)
                                           .AddPoolSize(luna::RHI::DescriptorType::Sampler, 1'024)
                                           .Build());

    m_material_sampler = m_device->CreateSampler(luna::RHI::SamplerBuilder()
                                                     .SetFilter(luna::RHI::Filter::Linear, luna::RHI::Filter::Linear)
                                                     .SetMipmapMode(luna::RHI::SamplerMipmapMode::Linear)
                                                     .SetAddressMode(luna::RHI::SamplerAddressMode::Repeat)
                                                     .SetAnisotropy(false)
                                                     .SetName("SceneMaterialSampler")
                                                     .Build());

    m_pipeline_layout =
        m_device->CreatePipelineLayout(luna::RHI::PipelineLayoutBuilder()
                                           .AddSetLayout(m_material_layout)
                                           .AddPushConstant(luna::RHI::ShaderStage::Vertex, 0, sizeof(MeshPushConstants))
                                           .Build());

    m_pipeline = m_device->CreateGraphicsPipeline(
        luna::RHI::GraphicsPipelineBuilder()
            .SetShaders({m_vertex_shader, m_fragment_shader})
            .AddVertexBinding(0, sizeof(StaticMeshVertex), luna::RHI::VertexInputRate::Vertex)
            .AddVertexAttribute(0, 0, luna::RHI::Format::RGB32_FLOAT, offsetof(StaticMeshVertex, position), "POSITION")
            .AddVertexAttribute(1, 0, luna::RHI::Format::RG32_FLOAT, offsetof(StaticMeshVertex, uv), "TEXCOORD")
            .AddVertexAttribute(2, 0, luna::RHI::Format::RGB32_FLOAT, offsetof(StaticMeshVertex, normal), "NORMAL")
            .SetTopology(luna::RHI::PrimitiveTopology::TriangleList)
            .SetCullMode(luna::RHI::CullMode::None)
            .SetFrontFace(luna::RHI::FrontFace::CounterClockwise)
            .SetDepthTest(true, true, luna::RHI::CompareOp::Less)
            .AddColorAttachmentDefault(false)
            .AddColorFormat(context.color_format)
            .SetDepthStencilFormat(luna::RHI::Format::D32_FLOAT)
            .SetLayout(m_pipeline_layout)
            .Build());

    m_surface_format = context.color_format;
}

SceneRenderer::ShaderPaths SceneRenderer::resolveShaderPaths() const
{
    if (m_shader_paths.isComplete()) {
        return m_shader_paths;
    }

    return getDefaultShaderPaths();
}

SceneRenderer::UploadedMesh& SceneRenderer::getOrCreateUploadedMesh(const Mesh& mesh)
{
    const auto it = m_uploaded_meshes.find(&mesh);
    if (it != m_uploaded_meshes.end()) {
        return it->second;
    }

    auto [inserted_it, _] = m_uploaded_meshes.emplace(&mesh, UploadedMesh{});
    auto& uploaded_mesh = inserted_it->second;

    uploaded_mesh.vertex_buffer =
        m_device->CreateBuffer(luna::RHI::BufferBuilder()
                                   .SetSize(mesh.getVertices().size() * sizeof(StaticMeshVertex))
                                   .SetUsage(luna::RHI::BufferUsageFlags::VertexBuffer)
                                   .SetMemoryUsage(luna::RHI::BufferMemoryUsage::CpuToGpu)
                                   .SetName(mesh.getName() + "_VertexBuffer")
                                   .Build());
    uploaded_mesh.index_buffer = m_device->CreateBuffer(luna::RHI::BufferBuilder()
                                                            .SetSize(mesh.getIndices().size() * sizeof(uint32_t))
                                                            .SetUsage(luna::RHI::BufferUsageFlags::IndexBuffer)
                                                            .SetMemoryUsage(luna::RHI::BufferMemoryUsage::CpuToGpu)
                                                            .SetName(mesh.getName() + "_IndexBuffer")
                                                            .Build());
    uploaded_mesh.index_count = static_cast<uint32_t>(mesh.getIndices().size());

    if (void* vertex_memory = uploaded_mesh.vertex_buffer->Map()) {
        std::memcpy(vertex_memory, mesh.getVertices().data(), mesh.getVertices().size() * sizeof(StaticMeshVertex));
        uploaded_mesh.vertex_buffer->Flush();
        uploaded_mesh.vertex_buffer->Unmap();
    }

    if (void* index_memory = uploaded_mesh.index_buffer->Map()) {
        std::memcpy(index_memory, mesh.getIndices().data(), mesh.getIndices().size() * sizeof(uint32_t));
        uploaded_mesh.index_buffer->Flush();
        uploaded_mesh.index_buffer->Unmap();
    }

    return uploaded_mesh;
}

SceneRenderer::UploadedMaterial& SceneRenderer::getOrCreateUploadedMaterial(const Material& material)
{
    const auto it = m_uploaded_materials.find(&material);
    if (it != m_uploaded_materials.end()) {
        return it->second;
    }

    const rhi::ImageData source_image = material.hasAlbedoTexture()
                                            ? material.getAlbedoImageData()
                                            : createFallbackImageData(material.getAlbedoColor());

    auto [inserted_it, _] = m_uploaded_materials.emplace(&material, UploadedMaterial{});
    auto& uploaded_material = inserted_it->second;

    uploaded_material.albedo_texture =
        m_device->CreateTexture(luna::RHI::TextureBuilder()
                                    .SetSize(source_image.Width, source_image.Height)
                                    .SetFormat(source_image.ImageFormat)
                                    .SetUsage(luna::RHI::TextureUsageFlags::Sampled | luna::RHI::TextureUsageFlags::TransferDst)
                                    .SetInitialState(luna::RHI::ResourceState::Undefined)
                                    .SetName(material.getName() + "_Albedo")
                                    .Build());

    uploaded_material.staging_buffer = m_device->CreateBuffer(luna::RHI::BufferBuilder()
                                                                  .SetSize(source_image.ByteData.size())
                                                                  .SetUsage(luna::RHI::BufferUsageFlags::TransferSrc)
                                                                  .SetMemoryUsage(luna::RHI::BufferMemoryUsage::CpuToGpu)
                                                                  .SetName(material.getName() + "_AlbedoStaging")
                                                                  .Build());

    if (void* mapped = uploaded_material.staging_buffer->Map()) {
        std::memcpy(mapped, source_image.ByteData.data(), source_image.ByteData.size());
        uploaded_material.staging_buffer->Flush();
        uploaded_material.staging_buffer->Unmap();
    }

    uploaded_material.descriptor_set = m_descriptor_pool->AllocateDescriptorSet(m_material_layout);
    uploaded_material.descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 0,
        .TextureView = uploaded_material.albedo_texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    uploaded_material.descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
        .Binding = 1,
        .Sampler = m_material_sampler,
    });
    uploaded_material.descriptor_set->Update();

    return uploaded_material;
}

void SceneRenderer::uploadMaterialIfNeeded(luna::RHI::CommandBufferEncoder& commands, UploadedMaterial& uploaded_material)
{
    if (uploaded_material.uploaded || !uploaded_material.albedo_texture || !uploaded_material.staging_buffer) {
        return;
    }

    commands.TransitionImage(uploaded_material.albedo_texture, luna::RHI::ImageTransition::UndefinedToTransferDst);

    const luna::RHI::BufferImageCopy copy_region{
        .BufferOffset = 0,
        .BufferRowLength = 0,
        .BufferImageHeight = 0,
        .ImageSubresource =
            {
                .AspectMask = luna::RHI::ImageAspectFlags::Color,
                .MipLevel = 0,
                .BaseArrayLayer = 0,
                .LayerCount = 1,
            },
        .ImageOffsetX = 0,
        .ImageOffsetY = 0,
        .ImageOffsetZ = 0,
        .ImageExtentWidth = uploaded_material.albedo_texture->GetWidth(),
        .ImageExtentHeight = uploaded_material.albedo_texture->GetHeight(),
        .ImageExtentDepth = 1,
    };

    const std::array<luna::RHI::BufferImageCopy, 1> copy_regions{copy_region};
    commands.CopyBufferToImage(uploaded_material.staging_buffer,
                               uploaded_material.albedo_texture,
                               luna::RHI::ResourceState::CopyDest,
                               copy_regions);
    commands.TransitionImage(uploaded_material.albedo_texture, luna::RHI::ImageTransition::TransferDstToShaderRead);

    uploaded_material.uploaded = true;
}

void SceneRenderer::executeOpaquePass(rhi::RenderGraphRasterPassContext& pass_context, const RenderContext& context)
{
    ensurePipeline(context);
    if (!m_pipeline) {
        LUNA_CORE_ERROR("SceneRenderer::executeOpaquePass aborted: graphics pipeline is null");
        return;
    }

    auto& commands = pass_context.commandBuffer();

    getOrCreateUploadedMaterial(m_default_material);

    for (const auto& draw_command : m_submitted_meshes) {
        if (!draw_command.mesh || !draw_command.mesh->isValid()) {
            continue;
        }

        (void) getOrCreateUploadedMesh(*draw_command.mesh);
        const Material& material = draw_command.material != nullptr ? *draw_command.material : m_default_material;
        auto& uploaded_material = getOrCreateUploadedMaterial(material);
        uploadMaterialIfNeeded(commands, uploaded_material);
    }

    pass_context.beginRendering();
    commands.BindGraphicsPipeline(m_pipeline);
    commands.SetViewport({0.0f,
                          0.0f,
                          static_cast<float>(pass_context.framebufferWidth()),
                          static_cast<float>(pass_context.framebufferHeight()),
                          0.0f,
                          1.0f});
    commands.SetScissor({0, 0, pass_context.framebufferWidth(), pass_context.framebufferHeight()});

    const float aspect_ratio =
        static_cast<float>(pass_context.framebufferWidth()) / static_cast<float>(pass_context.framebufferHeight());
    const glm::mat4 view_projection = buildViewProjection(m_camera, aspect_ratio, context.backend_type);

    for (const auto& draw_command : m_submitted_meshes) {
        if (!draw_command.mesh || !draw_command.mesh->isValid()) {
            continue;
        }

        const auto uploaded_mesh_it = m_uploaded_meshes.find(draw_command.mesh.get());
        if (uploaded_mesh_it == m_uploaded_meshes.end()) {
            continue;
        }

        const Material& material = draw_command.material != nullptr ? *draw_command.material : m_default_material;
        const auto uploaded_material_it = m_uploaded_materials.find(&material);
        if (uploaded_material_it == m_uploaded_materials.end() || !uploaded_material_it->second.descriptor_set) {
            continue;
        }

        const auto& uploaded_mesh = uploaded_mesh_it->second;
        const auto& uploaded_material = uploaded_material_it->second;
        const std::array<luna::RHI::Ref<luna::RHI::DescriptorSet>, 1> descriptor_sets{uploaded_material.descriptor_set};

        MeshPushConstants push_constants;
        push_constants.model = draw_command.transform;
        push_constants.view_projection = view_projection;

        commands.BindDescriptorSets(m_pipeline, 0, descriptor_sets);
        commands.BindVertexBuffer(0, uploaded_mesh.vertex_buffer);
        commands.BindIndexBuffer(uploaded_mesh.index_buffer, 0, luna::RHI::IndexType::UInt32);
        commands.PushConstants(m_pipeline, luna::RHI::ShaderStage::Vertex, 0, sizeof(MeshPushConstants), &push_constants);
        commands.DrawIndexed(uploaded_mesh.index_count, 1, 0, 0, 0);
    }

    pass_context.endRendering();
}

} // namespace luna
