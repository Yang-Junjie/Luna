#include "Core/Log.h"
#include "Renderer/SceneRendererInternal.h"

namespace luna {

void SceneRenderer::resetPipelineState()
{
    m_upload_cache.uploaded_materials.clear();
    m_upload_cache.uploaded_textures.clear();
    m_gpu.geometry_pipeline.reset();
    m_gpu.lighting_pipeline.reset();
    m_gpu.transparent_pipeline.reset();
    m_gpu.geometry_pipeline_layout.reset();
    m_gpu.lighting_pipeline_layout.reset();
    m_gpu.transparent_pipeline_layout.reset();
    m_gpu.material_layout.reset();
    m_gpu.gbuffer_layout.reset();
    m_gpu.scene_layout.reset();
    m_gpu.gbuffer_sampler.reset();
    m_gpu.environment_sampler.reset();
    m_gpu.gbuffer_descriptor_set.reset();
    m_gpu.scene_descriptor_set.reset();
    m_gpu.descriptor_pool.reset();
    m_gpu.scene_params_buffer.reset();
    m_gpu.environment_texture = {};
    m_gpu.geometry_vertex_shader.reset();
    m_gpu.geometry_fragment_shader.reset();
    m_gpu.lighting_vertex_shader.reset();
    m_gpu.lighting_fragment_shader.reset();
    m_gpu.transparent_fragment_shader.reset();
    m_gpu.surface_format = luna::RHI::Format::UNDEFINED;
}

void SceneRenderer::ensurePipelines(const RenderContext& context)
{
    using namespace scene_renderer_detail;

    if (!context.device || !context.compiler) {
        return;
    }

    auto& gpu = m_gpu;
    auto& upload_cache = m_upload_cache;

    if (gpu.device != context.device) {
        upload_cache.uploaded_materials.clear();
        upload_cache.uploaded_textures.clear();
        upload_cache.uploaded_meshes.clear();
        resetPipelineState();
        gpu.device = context.device;
    }

    if (gpu.geometry_pipeline && gpu.lighting_pipeline && gpu.transparent_pipeline &&
        gpu.surface_format == context.color_format && gpu.gbuffer_descriptor_set && gpu.scene_descriptor_set &&
        gpu.scene_params_buffer && gpu.environment_texture.texture) {
        return;
    }

    const ShaderPaths shader_paths = resolveShaderPaths();
    gpu.geometry_vertex_shader = loadShaderModule(gpu.device,
                                                  context.compiler,
                                                  shader_paths.geometry_vertex_path,
                                                  "sceneGeometryVertexMain",
                                                  luna::RHI::ShaderStage::Vertex);
    gpu.geometry_fragment_shader = loadShaderModule(gpu.device,
                                                    context.compiler,
                                                    shader_paths.geometry_fragment_path,
                                                    "sceneGeometryFragmentMain",
                                                    luna::RHI::ShaderStage::Fragment);
    gpu.lighting_vertex_shader = loadShaderModule(gpu.device,
                                                  context.compiler,
                                                  shader_paths.lighting_vertex_path,
                                                  "sceneLightingVertexMain",
                                                  luna::RHI::ShaderStage::Vertex);
    gpu.lighting_fragment_shader = loadShaderModule(gpu.device,
                                                    context.compiler,
                                                    shader_paths.lighting_fragment_path,
                                                    "sceneLightingFragmentMain",
                                                    luna::RHI::ShaderStage::Fragment);
    gpu.transparent_fragment_shader = loadShaderModule(gpu.device,
                                                       context.compiler,
                                                       shader_paths.geometry_fragment_path,
                                                       "sceneTransparentFragmentMain",
                                                       luna::RHI::ShaderStage::Fragment);

    if (!gpu.geometry_vertex_shader || !gpu.geometry_fragment_shader || !gpu.lighting_vertex_shader ||
        !gpu.lighting_fragment_shader || !gpu.transparent_fragment_shader) {
        LUNA_RENDERER_ERROR("Failed to load scene renderer shaders");
        return;
    }

    upload_cache.uploaded_materials.clear();
    upload_cache.uploaded_textures.clear();

    gpu.material_layout = gpu.device->CreateDescriptorSetLayout(
        luna::RHI::DescriptorSetLayoutBuilder()
            .AddBinding(0, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(1, luna::RHI::DescriptorType::Sampler, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(2, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(3, luna::RHI::DescriptorType::Sampler, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(4, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(5, luna::RHI::DescriptorType::Sampler, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(6, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(7, luna::RHI::DescriptorType::Sampler, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(8, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(9, luna::RHI::DescriptorType::Sampler, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(10, luna::RHI::DescriptorType::UniformBuffer, 1, luna::RHI::ShaderStage::Fragment)
            .Build());
    gpu.gbuffer_layout = gpu.device->CreateDescriptorSetLayout(
        luna::RHI::DescriptorSetLayoutBuilder()
            .AddBinding(0, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(1, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(2, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(3, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(4, luna::RHI::DescriptorType::Sampler, 1, luna::RHI::ShaderStage::Fragment)
            .Build());
    gpu.scene_layout = gpu.device->CreateDescriptorSetLayout(
        luna::RHI::DescriptorSetLayoutBuilder()
            .AddBinding(0,
                        luna::RHI::DescriptorType::UniformBuffer,
                        1,
                        luna::RHI::ShaderStage::Vertex | luna::RHI::ShaderStage::Fragment)
            .AddBinding(1, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(2, luna::RHI::DescriptorType::Sampler, 1, luna::RHI::ShaderStage::Fragment)
            .Build());

    gpu.descriptor_pool =
        gpu.device->CreateDescriptorPool(luna::RHI::DescriptorPoolBuilder()
                                             .SetMaxSets(4'096)
                                             .AddPoolSize(luna::RHI::DescriptorType::SampledImage, 24'576)
                                             .AddPoolSize(luna::RHI::DescriptorType::Sampler, 24'576)
                                             .AddPoolSize(luna::RHI::DescriptorType::UniformBuffer, 8'192)
                                             .Build());
    gpu.gbuffer_sampler = gpu.device->CreateSampler(luna::RHI::SamplerBuilder()
                                                        .SetFilter(luna::RHI::Filter::Linear, luna::RHI::Filter::Linear)
                                                        .SetAddressMode(luna::RHI::SamplerAddressMode::ClampToEdge)
                                                        .SetMipmapMode(luna::RHI::SamplerMipmapMode::Linear)
                                                        .SetAnisotropy(false)
                                                        .SetName("SceneGBufferSampler")
                                                        .Build());
    gpu.environment_sampler =
        gpu.device->CreateSampler(luna::RHI::SamplerBuilder()
                                      .SetFilter(luna::RHI::Filter::Linear, luna::RHI::Filter::Linear)
                                      .SetMipmapMode(luna::RHI::SamplerMipmapMode::Linear)
                                      .SetAddressModeU(luna::RHI::SamplerAddressMode::Repeat)
                                      .SetAddressModeV(luna::RHI::SamplerAddressMode::ClampToEdge)
                                      .SetAddressModeW(luna::RHI::SamplerAddressMode::ClampToEdge)
                                      .SetLodRange(0.0f, 16.0f)
                                      .SetAnisotropy(false)
                                      .SetName("SceneEnvironmentSampler")
                                      .Build());

    if (gpu.descriptor_pool && gpu.gbuffer_layout) {
        gpu.gbuffer_descriptor_set = gpu.descriptor_pool->AllocateDescriptorSet(gpu.gbuffer_layout);
    }

    ensureSceneResources();

    gpu.geometry_pipeline_layout = gpu.device->CreatePipelineLayout(
        luna::RHI::PipelineLayoutBuilder()
            .AddSetLayout(gpu.material_layout)
            .AddSetLayout(gpu.scene_layout)
            .AddPushConstant(luna::RHI::ShaderStage::Vertex, 0, sizeof(MeshPushConstants))
            .Build());
    gpu.lighting_pipeline_layout = gpu.device->CreatePipelineLayout(
        luna::RHI::PipelineLayoutBuilder().AddSetLayout(gpu.gbuffer_layout).AddSetLayout(gpu.scene_layout).Build());
    gpu.transparent_pipeline_layout = gpu.device->CreatePipelineLayout(
        luna::RHI::PipelineLayoutBuilder()
            .AddSetLayout(gpu.material_layout)
            .AddSetLayout(gpu.scene_layout)
            .AddPushConstant(luna::RHI::ShaderStage::Vertex, 0, sizeof(MeshPushConstants))
            .Build());

    gpu.geometry_pipeline = gpu.device->CreateGraphicsPipeline(
        luna::RHI::GraphicsPipelineBuilder()
            .SetShaders({gpu.geometry_vertex_shader, gpu.geometry_fragment_shader})
            .AddVertexBinding(0, sizeof(StaticMeshVertex), luna::RHI::VertexInputRate::Vertex)
            .AddVertexAttribute(0, 0, luna::RHI::Format::RGB32_FLOAT, offsetof(StaticMeshVertex, Position), "POSITION")
            .AddVertexAttribute(1, 0, luna::RHI::Format::RG32_FLOAT, offsetof(StaticMeshVertex, TexCoord), "TEXCOORD")
            .AddVertexAttribute(2, 0, luna::RHI::Format::RGB32_FLOAT, offsetof(StaticMeshVertex, Normal), "NORMAL")
            .AddVertexAttribute(3, 0, luna::RHI::Format::RGB32_FLOAT, offsetof(StaticMeshVertex, Tangent), "TANGENT")
            .AddVertexAttribute(4, 0, luna::RHI::Format::RGB32_FLOAT, offsetof(StaticMeshVertex, Bitangent), "BINORMAL")
            .SetTopology(luna::RHI::PrimitiveTopology::TriangleList)
            .SetCullMode(luna::RHI::CullMode::None)
            .SetFrontFace(luna::RHI::FrontFace::CounterClockwise)
            .SetDepthTest(true, true, luna::RHI::CompareOp::Less)
            .AddColorAttachmentDefault(false)
            .AddColorAttachmentDefault(false)
            .AddColorAttachmentDefault(false)
            .AddColorAttachmentDefault(false)
            .AddColorFormat(kGBufferBaseColorFormat)
            .AddColorFormat(kGBufferLightingFormat)
            .AddColorFormat(kGBufferLightingFormat)
            .AddColorFormat(kGBufferLightingFormat)
            .SetDepthStencilFormat(luna::RHI::Format::D32_FLOAT)
            .SetLayout(gpu.geometry_pipeline_layout)
            .Build());

    gpu.lighting_pipeline =
        gpu.device->CreateGraphicsPipeline(luna::RHI::GraphicsPipelineBuilder()
                                               .SetShaders({gpu.lighting_vertex_shader, gpu.lighting_fragment_shader})
                                               .SetTopology(luna::RHI::PrimitiveTopology::TriangleList)
                                               .SetCullMode(luna::RHI::CullMode::None)
                                               .SetFrontFace(luna::RHI::FrontFace::CounterClockwise)
                                               .SetDepthTest(false, false, luna::RHI::CompareOp::Always)
                                               .AddColorAttachmentDefault(false)
                                               .AddColorFormat(context.color_format)
                                               .SetLayout(gpu.lighting_pipeline_layout)
                                               .Build());

    gpu.transparent_pipeline = gpu.device->CreateGraphicsPipeline(
        luna::RHI::GraphicsPipelineBuilder()
            .SetShaders({gpu.geometry_vertex_shader, gpu.transparent_fragment_shader})
            .AddVertexBinding(0, sizeof(StaticMeshVertex), luna::RHI::VertexInputRate::Vertex)
            .AddVertexAttribute(0, 0, luna::RHI::Format::RGB32_FLOAT, offsetof(StaticMeshVertex, Position), "POSITION")
            .AddVertexAttribute(1, 0, luna::RHI::Format::RG32_FLOAT, offsetof(StaticMeshVertex, TexCoord), "TEXCOORD")
            .AddVertexAttribute(2, 0, luna::RHI::Format::RGB32_FLOAT, offsetof(StaticMeshVertex, Normal), "NORMAL")
            .AddVertexAttribute(3, 0, luna::RHI::Format::RGB32_FLOAT, offsetof(StaticMeshVertex, Tangent), "TANGENT")
            .AddVertexAttribute(4, 0, luna::RHI::Format::RGB32_FLOAT, offsetof(StaticMeshVertex, Bitangent), "BINORMAL")
            .SetTopology(luna::RHI::PrimitiveTopology::TriangleList)
            .SetCullMode(luna::RHI::CullMode::None)
            .SetFrontFace(luna::RHI::FrontFace::CounterClockwise)
            .SetDepthTest(true, false, luna::RHI::CompareOp::Less)
            .AddColorAttachment(makeAlphaBlendAttachment())
            .AddColorFormat(context.color_format)
            .SetDepthStencilFormat(luna::RHI::Format::D32_FLOAT)
            .SetLayout(gpu.transparent_pipeline_layout)
            .Build());

    gpu.surface_format = context.color_format;
}

SceneRenderer::ShaderPaths SceneRenderer::resolveShaderPaths() const
{
    ShaderPaths shader_paths = m_shader_paths;
    const ShaderPaths default_paths = getDefaultShaderPaths();

    if (shader_paths.geometry_vertex_path.empty()) {
        shader_paths.geometry_vertex_path = default_paths.geometry_vertex_path;
    }
    if (shader_paths.geometry_fragment_path.empty()) {
        shader_paths.geometry_fragment_path = default_paths.geometry_fragment_path;
    }
    if (shader_paths.lighting_vertex_path.empty()) {
        shader_paths.lighting_vertex_path = default_paths.lighting_vertex_path;
    }
    if (shader_paths.lighting_fragment_path.empty()) {
        shader_paths.lighting_fragment_path = default_paths.lighting_fragment_path;
    }

    return shader_paths;
}

SceneRenderer::UploadedMesh& SceneRenderer::getOrCreateUploadedMesh(const Mesh& mesh)
{
    auto& upload_cache = m_upload_cache;
    auto& gpu = m_gpu;

    const auto it = upload_cache.uploaded_meshes.find(&mesh);
    if (it != upload_cache.uploaded_meshes.end()) {
        return it->second;
    }

    auto [inserted_it, _] = upload_cache.uploaded_meshes.emplace(&mesh, UploadedMesh{});
    auto& uploaded_mesh = inserted_it->second;

    const auto& sub_meshes = mesh.getSubMeshes();
    uploaded_mesh.sub_meshes.resize(sub_meshes.size());

    for (size_t sub_mesh_index = 0; sub_mesh_index < sub_meshes.size(); ++sub_mesh_index) {
        const auto& sub_mesh = sub_meshes[sub_mesh_index];
        auto& uploaded_sub_mesh = uploaded_mesh.sub_meshes[sub_mesh_index];
        uploaded_sub_mesh.sub_mesh_index = static_cast<uint32_t>(sub_mesh_index);

        if (sub_mesh.Vertices.empty() || sub_mesh.Indices.empty()) {
            continue;
        }

        const std::string sub_mesh_name =
            sub_mesh.Name.empty() ? mesh.getName() + "_SubMesh_" + std::to_string(sub_mesh_index) : sub_mesh.Name;

        uploaded_sub_mesh.vertex_buffer =
            gpu.device->CreateBuffer(luna::RHI::BufferBuilder()
                                         .SetSize(sub_mesh.Vertices.size() * sizeof(StaticMeshVertex))
                                         .SetUsage(luna::RHI::BufferUsageFlags::VertexBuffer)
                                         .SetMemoryUsage(luna::RHI::BufferMemoryUsage::CpuToGpu)
                                         .SetName(sub_mesh_name + "_VertexBuffer")
                                         .Build());
        uploaded_sub_mesh.index_buffer =
            gpu.device->CreateBuffer(luna::RHI::BufferBuilder()
                                         .SetSize(sub_mesh.Indices.size() * sizeof(uint32_t))
                                         .SetUsage(luna::RHI::BufferUsageFlags::IndexBuffer)
                                         .SetMemoryUsage(luna::RHI::BufferMemoryUsage::CpuToGpu)
                                         .SetName(sub_mesh_name + "_IndexBuffer")
                                         .Build());
        uploaded_sub_mesh.index_count = static_cast<uint32_t>(sub_mesh.Indices.size());

        if (uploaded_sub_mesh.vertex_buffer) {
            if (void* vertex_memory = uploaded_sub_mesh.vertex_buffer->Map()) {
                std::memcpy(vertex_memory, sub_mesh.Vertices.data(), sub_mesh.Vertices.size() * sizeof(StaticMeshVertex));
                uploaded_sub_mesh.vertex_buffer->Flush();
                uploaded_sub_mesh.vertex_buffer->Unmap();
            }
        }

        if (uploaded_sub_mesh.index_buffer) {
            if (void* index_memory = uploaded_sub_mesh.index_buffer->Map()) {
                std::memcpy(index_memory, sub_mesh.Indices.data(), sub_mesh.Indices.size() * sizeof(uint32_t));
                uploaded_sub_mesh.index_buffer->Flush();
                uploaded_sub_mesh.index_buffer->Unmap();
            }
        }
    }

    return uploaded_mesh;
}

SceneRenderer::UploadedTexture
    SceneRenderer::createUploadedTexture(const rhi::ImageData& image,
                                         const rhi::Texture::SamplerSettings& sampler_settings,
                                         const std::string& debug_name) const
{
    using namespace scene_renderer_detail;

    UploadedTexture uploaded_texture;
    if (!m_gpu.device || !image.isValid()) {
        return uploaded_texture;
    }

    constexpr size_t kTextureDataPlacementAlignment = 512;
    constexpr uint32_t kTextureDataPitchAlignment = 256;

    const uint32_t mip_level_count = 1u + static_cast<uint32_t>(image.MipLevels.size());
    uploaded_texture.texture = m_gpu.device->CreateTexture(
        luna::RHI::TextureBuilder()
            .SetSize(image.Width, image.Height)
            .SetMipLevels(mip_level_count)
            .SetFormat(image.ImageFormat)
            .SetUsage(luna::RHI::TextureUsageFlags::Sampled | luna::RHI::TextureUsageFlags::TransferDst)
            .SetInitialState(luna::RHI::ResourceState::Undefined)
            .SetName(debug_name)
            .Build());

    const float max_lod = sampler_settings.MipFilter == rhi::Texture::MipFilterMode::None
                              ? 0.0f
                              : static_cast<float>((std::max)(mip_level_count, 1u) - 1u);
    uploaded_texture.sampler =
        m_gpu.device->CreateSampler(luna::RHI::SamplerBuilder()
                                        .SetMinFilter(toRhiFilter(sampler_settings.MinFilter))
                                        .SetMagFilter(toRhiFilter(sampler_settings.MagFilter))
                                        .SetMipmapMode(toRhiMipmapMode(sampler_settings.MipFilter))
                                        .SetAddressModeU(toRhiAddressMode(sampler_settings.WrapU))
                                        .SetAddressModeV(toRhiAddressMode(sampler_settings.WrapV))
                                        .SetAddressModeW(toRhiAddressMode(sampler_settings.WrapW))
                                        .SetLodRange(0.0f, max_lod)
                                        .SetAnisotropy(false)
                                        .SetName(debug_name + "_Sampler")
                                        .Build());

    struct PackedMipRegion {
        const std::vector<uint8_t>* source = nullptr;
        size_t offset = 0;
        uint32_t row_pitch_bytes = 0;
        uint32_t row_length_texels = 0;
        uint32_t height = 0;
    };

    std::vector<PackedMipRegion> packed_regions;
    packed_regions.reserve(mip_level_count);

    auto align_up = [](size_t value, size_t alignment) -> size_t {
        return alignment == 0 ? value : ((value + alignment - 1) / alignment) * alignment;
    };

    size_t total_size = 0;

    if (!uploaded_texture.texture || !uploaded_texture.sampler || total_size == 0) {
        // Continue below; total_size is determined from the packed upload layout.
    }

    size_t buffer_offset = 0;
    uint32_t mip_width = image.Width;
    uint32_t mip_height = image.Height;
    uploaded_texture.copy_regions.reserve(mip_level_count);

    auto append_region = [&](const std::vector<uint8_t>& bytes, uint32_t mip_level) {
        const uint32_t safe_width = (std::max)(mip_width, 1u);
        const uint32_t safe_height = (std::max)(mip_height, 1u);
        const uint32_t row_pitch_bytes = safe_height > 0 ? static_cast<uint32_t>(bytes.size() / safe_height) : 0;
        const uint32_t bytes_per_texel = safe_width > 0 ? row_pitch_bytes / safe_width : 0;
        const uint32_t aligned_row_pitch =
            static_cast<uint32_t>(align_up(row_pitch_bytes, kTextureDataPitchAlignment));
        const uint32_t row_length_texels =
            bytes_per_texel > 0 ? aligned_row_pitch / bytes_per_texel : safe_width;

        buffer_offset = align_up(buffer_offset, kTextureDataPlacementAlignment);
        uploaded_texture.copy_regions.push_back(luna::RHI::BufferImageCopy{
            .BufferOffset = buffer_offset,
            .BufferRowLength = row_length_texels,
            .BufferImageHeight = 0,
            .ImageSubresource =
                {
                    .AspectMask = luna::RHI::ImageAspectFlags::Color,
                    .MipLevel = mip_level,
                    .BaseArrayLayer = 0,
                    .LayerCount = 1,
                },
            .ImageOffsetX = 0,
            .ImageOffsetY = 0,
            .ImageOffsetZ = 0,
            .ImageExtentWidth = mip_width,
            .ImageExtentHeight = mip_height,
            .ImageExtentDepth = 1,
        });
        packed_regions.push_back(PackedMipRegion{
            .source = &bytes,
            .offset = buffer_offset,
            .row_pitch_bytes = aligned_row_pitch,
            .row_length_texels = row_length_texels,
            .height = safe_height,
        });
        buffer_offset += static_cast<size_t>(aligned_row_pitch) * safe_height;
    };

    append_region(image.ByteData, 0);
    for (uint32_t mip_level = 1; mip_level < mip_level_count; ++mip_level) {
        mip_width = (std::max) (mip_width / 2, 1u);
        mip_height = (std::max) (mip_height / 2, 1u);
        append_region(image.MipLevels[mip_level - 1], mip_level);
    }

    total_size = buffer_offset;
    if (!uploaded_texture.texture || !uploaded_texture.sampler || total_size == 0) {
        return uploaded_texture;
    }

    uploaded_texture.staging_buffer =
        m_gpu.device->CreateBuffer(luna::RHI::BufferBuilder()
                                       .SetSize(total_size)
                                       .SetUsage(luna::RHI::BufferUsageFlags::TransferSrc)
                                       .SetMemoryUsage(luna::RHI::BufferMemoryUsage::CpuToGpu)
                                       .SetName(debug_name + "_Staging")
                                       .Build());

    if (!uploaded_texture.staging_buffer) {
        return uploaded_texture;
    }

    if (void* mapped = uploaded_texture.staging_buffer->Map()) {
        uint8_t* destination = static_cast<uint8_t*>(mapped);
        for (const auto& packed_region : packed_regions) {
            if (packed_region.source == nullptr || packed_region.height == 0) {
                continue;
            }

            const size_t source_row_pitch = packed_region.source->size() / packed_region.height;
            for (uint32_t row = 0; row < packed_region.height; ++row) {
                std::memcpy(destination + packed_region.offset + static_cast<size_t>(row) * packed_region.row_pitch_bytes,
                            packed_region.source->data() + static_cast<size_t>(row) * source_row_pitch,
                            source_row_pitch);
            }
        }
        uploaded_texture.staging_buffer->Flush();
        uploaded_texture.staging_buffer->Unmap();
    }

    return uploaded_texture;
}

std::shared_ptr<SceneRenderer::UploadedTexture>
    SceneRenderer::getOrCreateUploadedTexture(const std::shared_ptr<rhi::Texture>& texture)
{
    if (texture == nullptr || !texture->isValid()) {
        return {};
    }

    auto& upload_cache = m_upload_cache;
    const auto it = upload_cache.uploaded_textures.find(texture.get());
    if (it != upload_cache.uploaded_textures.end()) {
        return it->second;
    }

    const std::string debug_name = texture->getName().empty() ? std::string("Texture") : texture->getName();
    auto uploaded_texture =
        std::make_shared<UploadedTexture>(createUploadedTexture(texture->getImageData(), texture->getSamplerSettings(), debug_name));
    upload_cache.uploaded_textures.emplace(texture.get(), uploaded_texture);
    return uploaded_texture;
}

SceneRenderer::UploadedMaterial& SceneRenderer::getOrCreateUploadedMaterial(const Material& material)
{
    using namespace scene_renderer_detail;

    auto& upload_cache = m_upload_cache;
    auto& gpu = m_gpu;

    const auto it = upload_cache.uploaded_materials.find(&material);
    if (it != upload_cache.uploaded_materials.end()) {
        return it->second;
    }

    const auto& textures = material.getTextures();
    const auto& surface = material.getSurface();
    const std::string material_name = material.getName().empty() ? "Material" : material.getName();

    auto [inserted_it, _] = upload_cache.uploaded_materials.emplace(&material, UploadedMaterial{});
    auto& uploaded_material = inserted_it->second;
    const rhi::Texture::SamplerSettings default_sampler_settings{};

    const auto create_material_texture =
        [&](const std::shared_ptr<rhi::Texture>& texture,
            const rhi::ImageData& fallback_image,
            std::string_view suffix) -> std::shared_ptr<UploadedTexture> {
        if (texture != nullptr && texture->isValid()) {
            return getOrCreateUploadedTexture(texture);
        }

        const std::string texture_name = material_name + "_" + std::string(suffix);
        return std::make_shared<UploadedTexture>(
            createUploadedTexture(fallback_image, default_sampler_settings, texture_name));
    };

    uploaded_material.base_color_texture =
        create_material_texture(textures.BaseColor, createFallbackImageData(glm::vec4(1.0f)), "BaseColor");
    uploaded_material.normal_texture = create_material_texture(
        textures.Normal, createFallbackImageData(glm::vec4(0.5f, 0.5f, 1.0f, 1.0f)), "Normal");
    uploaded_material.metallic_roughness_texture = create_material_texture(
        textures.MetallicRoughness, createFallbackImageData(glm::vec4(1.0f)), "MetallicRoughness");
    uploaded_material.emissive_texture =
        create_material_texture(textures.Emissive, createFallbackImageData(glm::vec4(1.0f)), "Emissive");
    uploaded_material.occlusion_texture =
        create_material_texture(textures.Occlusion, createFallbackImageData(glm::vec4(1.0f)), "Occlusion");

    uploaded_material.params_buffer =
        gpu.device->CreateBuffer(luna::RHI::BufferBuilder()
                                     .SetSize(sizeof(MaterialGpuParams))
                                     .SetUsage(luna::RHI::BufferUsageFlags::UniformBuffer)
                                     .SetMemoryUsage(luna::RHI::BufferMemoryUsage::CpuToGpu)
                                     .SetName(material_name + "_Params")
                                     .Build());

    if (uploaded_material.params_buffer) {
        if (void* mapped = uploaded_material.params_buffer->Map()) {
            const MaterialGpuParams params{
                .base_color_factor = surface.BaseColorFactor,
                .emissive_factor_normal_scale = glm::vec4(surface.EmissiveFactor, surface.NormalScale),
                .material_factors = glm::vec4(surface.MetallicFactor,
                                              surface.RoughnessFactor,
                                              surface.OcclusionStrength,
                                              surface.AlphaCutoff),
                .material_flags = glm::vec4(materialBlendModeToFloat(material.getBlendMode()),
                                            surface.Unlit ? 1.0f : 0.0f,
                                            surface.DoubleSided ? 1.0f : 0.0f,
                                            0.0f),
            };
            std::memcpy(mapped, &params, sizeof(params));
            uploaded_material.params_buffer->Flush();
            uploaded_material.params_buffer->Unmap();
        }
    }

    if (!gpu.descriptor_pool || !gpu.material_layout ||
        !uploaded_material.base_color_texture || !uploaded_material.normal_texture ||
        !uploaded_material.metallic_roughness_texture || !uploaded_material.emissive_texture ||
        !uploaded_material.occlusion_texture || !uploaded_material.base_color_texture->texture ||
        !uploaded_material.normal_texture->texture || !uploaded_material.metallic_roughness_texture->texture ||
        !uploaded_material.emissive_texture->texture || !uploaded_material.occlusion_texture->texture ||
        !uploaded_material.base_color_texture->sampler || !uploaded_material.normal_texture->sampler ||
        !uploaded_material.metallic_roughness_texture->sampler || !uploaded_material.emissive_texture->sampler ||
        !uploaded_material.occlusion_texture->sampler ||
        !uploaded_material.params_buffer) {
        return uploaded_material;
    }

    uploaded_material.descriptor_set = gpu.descriptor_pool->AllocateDescriptorSet(gpu.material_layout);
    if (!uploaded_material.descriptor_set) {
        return uploaded_material;
    }

    uploaded_material.descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 0,
        .TextureView = uploaded_material.base_color_texture->texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    uploaded_material.descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
        .Binding = 1,
        .Sampler = uploaded_material.base_color_texture->sampler,
    });
    uploaded_material.descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 2,
        .TextureView = uploaded_material.normal_texture->texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    uploaded_material.descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
        .Binding = 3,
        .Sampler = uploaded_material.normal_texture->sampler,
    });
    uploaded_material.descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 4,
        .TextureView = uploaded_material.metallic_roughness_texture->texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    uploaded_material.descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
        .Binding = 5,
        .Sampler = uploaded_material.metallic_roughness_texture->sampler,
    });
    uploaded_material.descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 6,
        .TextureView = uploaded_material.emissive_texture->texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    uploaded_material.descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
        .Binding = 7,
        .Sampler = uploaded_material.emissive_texture->sampler,
    });
    uploaded_material.descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 8,
        .TextureView = uploaded_material.occlusion_texture->texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    uploaded_material.descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
        .Binding = 9,
        .Sampler = uploaded_material.occlusion_texture->sampler,
    });
    uploaded_material.descriptor_set->WriteBuffer(luna::RHI::BufferWriteInfo{
        .Binding = 10,
        .Buffer = uploaded_material.params_buffer,
        .Offset = 0,
        .Stride = sizeof(MaterialGpuParams),
        .Size = sizeof(MaterialGpuParams),
        .Type = luna::RHI::DescriptorType::UniformBuffer,
    });
    uploaded_material.descriptor_set->Update();

    return uploaded_material;
}

void SceneRenderer::uploadTextureIfNeeded(luna::RHI::CommandBufferEncoder& commands, UploadedTexture& uploaded_texture)
{
    if (uploaded_texture.uploaded || !uploaded_texture.texture || !uploaded_texture.staging_buffer ||
        uploaded_texture.copy_regions.empty()) {
        return;
    }

    const luna::RHI::ImageSubresourceRange full_range{
        .BaseMipLevel = 0,
        .LevelCount = uploaded_texture.texture->GetMipLevels(),
        .BaseArrayLayer = 0,
        .LayerCount = uploaded_texture.texture->GetArrayLayers(),
        .AspectMask = luna::RHI::ImageAspectFlags::Color,
    };

    commands.TransitionImage(uploaded_texture.texture, luna::RHI::ImageTransition::UndefinedToTransferDst, full_range);
    commands.CopyBufferToImage(uploaded_texture.staging_buffer,
                               uploaded_texture.texture,
                               luna::RHI::ResourceState::CopyDest,
                               uploaded_texture.copy_regions);
    commands.TransitionImage(uploaded_texture.texture, luna::RHI::ImageTransition::TransferDstToShaderRead, full_range);

    uploaded_texture.uploaded = true;
}

void SceneRenderer::uploadMaterialIfNeeded(luna::RHI::CommandBufferEncoder& commands,
                                           UploadedMaterial& uploaded_material)
{
    if (uploaded_material.base_color_texture) {
        uploadTextureIfNeeded(commands, *uploaded_material.base_color_texture);
    }
    if (uploaded_material.normal_texture) {
        uploadTextureIfNeeded(commands, *uploaded_material.normal_texture);
    }
    if (uploaded_material.metallic_roughness_texture) {
        uploadTextureIfNeeded(commands, *uploaded_material.metallic_roughness_texture);
    }
    if (uploaded_material.emissive_texture) {
        uploadTextureIfNeeded(commands, *uploaded_material.emissive_texture);
    }
    if (uploaded_material.occlusion_texture) {
        uploadTextureIfNeeded(commands, *uploaded_material.occlusion_texture);
    }
}

void SceneRenderer::ensureSceneResources()
{
    using namespace scene_renderer_detail;

    auto& gpu = m_gpu;

    if (!gpu.device || !gpu.descriptor_pool || !gpu.scene_layout || !gpu.environment_sampler) {
        return;
    }

    if (!gpu.scene_params_buffer) {
        gpu.scene_params_buffer = gpu.device->CreateBuffer(luna::RHI::BufferBuilder()
                                                               .SetSize(sizeof(SceneGpuParams))
                                                               .SetUsage(luna::RHI::BufferUsageFlags::UniformBuffer)
                                                               .SetMemoryUsage(luna::RHI::BufferMemoryUsage::CpuToGpu)
                                                               .SetName("SceneParams")
                                                               .Build());
    }

    if (!gpu.environment_texture.texture) {
        std::filesystem::path environment_path = getDefaultEnvironmentPath();
        rhi::ImageData environment_image;

        if (std::filesystem::exists(environment_path)) {
            environment_image = rhi::ImageLoader::LoadImageFromFile(environment_path.string());
            if (!environment_image.isValid()) {
                LUNA_RENDERER_WARN("Failed to load environment map '{}'; falling back to a neutral environment",
                                   environment_path.string());
            }
        } else {
            LUNA_RENDERER_WARN("Environment map '{}' was not found; falling back to a neutral environment",
                               environment_path.string());
        }

        if (!environment_image.isValid() || environment_image.ImageFormat != kEnvironmentFormat) {
            environment_image = createFallbackFloatImageData(
                glm::vec4(kEnvironmentFallbackValue, kEnvironmentFallbackValue, kEnvironmentFallbackValue, 1.0f));
        }

        environment_image = generateEnvironmentMipChain(environment_image);
        gpu.environment_texture =
            createUploadedTexture(environment_image, rhi::Texture::SamplerSettings{}, "SceneEnvironment");
    }

    if (!gpu.scene_descriptor_set) {
        gpu.scene_descriptor_set = gpu.descriptor_pool->AllocateDescriptorSet(gpu.scene_layout);
    }

    if (!gpu.scene_descriptor_set || !gpu.scene_params_buffer || !gpu.environment_texture.texture) {
        return;
    }

    gpu.scene_descriptor_set->WriteBuffer(luna::RHI::BufferWriteInfo{
        .Binding = 0,
        .Buffer = gpu.scene_params_buffer,
        .Offset = 0,
        .Stride = sizeof(SceneGpuParams),
        .Size = sizeof(SceneGpuParams),
        .Type = luna::RHI::DescriptorType::UniformBuffer,
    });
    gpu.scene_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 1,
        .TextureView = gpu.environment_texture.texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    gpu.scene_descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
        .Binding = 2,
        .Sampler = gpu.environment_sampler,
    });
    gpu.scene_descriptor_set->Update();
}

void SceneRenderer::updateSceneParameters(const RenderContext& context)
{
    using namespace scene_renderer_detail;

    auto& gpu = m_gpu;

    if (!gpu.scene_params_buffer || context.framebuffer_width == 0 || context.framebuffer_height == 0) {
        return;
    }

    const float aspect_ratio =
        static_cast<float>(context.framebuffer_width) / static_cast<float>(context.framebuffer_height);
    const glm::mat4 view_projection = buildViewProjection(m_draw_queue.camera, aspect_ratio, context.backend_type);
    const float environment_mip_count =
        gpu.environment_texture.texture != nullptr
            ? static_cast<float>((std::max) (gpu.environment_texture.texture->GetMipLevels(), 1u) - 1u)
            : 0.0f;

    SceneGpuParams params;
    params.view_projection = view_projection;
    params.inverse_view_projection = glm::inverse(view_projection);
    params.camera_position_env_mip = glm::vec4(resolveCameraPosition(m_draw_queue.camera), environment_mip_count);
    params.light_direction_intensity = glm::vec4(glm::normalize(glm::vec3(0.45f, 0.80f, 0.35f)), 4.0f);
    params.light_color_exposure = glm::vec4(1.0f, 0.98f, 0.95f, 1.0f);
    params.ibl_factors = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);

    if (void* mapped = gpu.scene_params_buffer->Map()) {
        std::memcpy(mapped, &params, sizeof(params));
        gpu.scene_params_buffer->Flush();
        gpu.scene_params_buffer->Unmap();
    }
}

} // namespace luna
