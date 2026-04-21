#include "Renderer/SceneRendererResourceManager.h"

#include "Asset/Editor/ImageLoader.h"
#include "Core/Log.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/SceneRendererCommon.h"
#include "Renderer/SceneRendererDrawQueue.h"

#include <Buffer.h>
#include <Builders.h>
#include <CommandBufferEncoder.h>
#include <DescriptorPool.h>
#include <DescriptorSet.h>
#include <DescriptorSetLayout.h>
#include <Device.h>
#include <Pipeline.h>
#include <PipelineLayout.h>
#include <Sampler.h>
#include <ShaderCompiler.h>
#include <ShaderModule.h>
#include <Texture.h>

#include <array>
#include <filesystem>
#include <span>
#include <unordered_map>
#include <utility>

namespace luna::scene_renderer {

namespace {

using ShaderPaths = SceneRenderer::ShaderPaths;
using RenderContext = SceneRenderer::RenderContext;

ShaderPaths defaultShaderPaths()
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

std::filesystem::path defaultEnvironmentPath()
{
    return scene_renderer_detail::projectRoot() / "SampleProject" / "Assets" / "Texture" / "newport_loft.hdr";
}

luna::RHI::Ref<luna::RHI::DescriptorSetLayout> createMaterialLayout(const luna::RHI::Ref<luna::RHI::Device>& device)
{
    if (!device) {
        return {};
    }

    return device->CreateDescriptorSetLayout(
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
}

luna::RHI::Ref<luna::RHI::DescriptorSetLayout> createGBufferLayout(const luna::RHI::Ref<luna::RHI::Device>& device)
{
    if (!device) {
        return {};
    }

    return device->CreateDescriptorSetLayout(
        luna::RHI::DescriptorSetLayoutBuilder()
            .AddBinding(0, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(1, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(2, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(3, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(4, luna::RHI::DescriptorType::Sampler, 1, luna::RHI::ShaderStage::Fragment)
            .Build());
}

luna::RHI::Ref<luna::RHI::DescriptorSetLayout> createSceneLayout(const luna::RHI::Ref<luna::RHI::Device>& device)
{
    if (!device) {
        return {};
    }

    return device->CreateDescriptorSetLayout(
        luna::RHI::DescriptorSetLayoutBuilder()
            .AddBinding(0,
                        luna::RHI::DescriptorType::UniformBuffer,
                        1,
                        luna::RHI::ShaderStage::Vertex | luna::RHI::ShaderStage::Fragment)
            .AddBinding(1, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(2, luna::RHI::DescriptorType::Sampler, 1, luna::RHI::ShaderStage::Fragment)
            .Build());
}

luna::RHI::Ref<luna::RHI::DescriptorPool> createDescriptorPool(const luna::RHI::Ref<luna::RHI::Device>& device)
{
    if (!device) {
        return {};
    }

    return device->CreateDescriptorPool(luna::RHI::DescriptorPoolBuilder()
                                            .SetMaxSets(4'096)
                                            .AddPoolSize(luna::RHI::DescriptorType::SampledImage, 24'576)
                                            .AddPoolSize(luna::RHI::DescriptorType::Sampler, 24'576)
                                            .AddPoolSize(luna::RHI::DescriptorType::UniformBuffer, 8'192)
                                            .Build());
}

luna::RHI::Ref<luna::RHI::Sampler> createGBufferSampler(const luna::RHI::Ref<luna::RHI::Device>& device)
{
    if (!device) {
        return {};
    }

    return device->CreateSampler(luna::RHI::SamplerBuilder()
                                     .SetFilter(luna::RHI::Filter::Linear, luna::RHI::Filter::Linear)
                                     .SetAddressMode(luna::RHI::SamplerAddressMode::ClampToEdge)
                                     .SetMipmapMode(luna::RHI::SamplerMipmapMode::Linear)
                                     .SetAnisotropy(false)
                                     .SetName("SceneGBufferSampler")
                                     .Build());
}

luna::RHI::Ref<luna::RHI::Sampler> createEnvironmentSampler(const luna::RHI::Ref<luna::RHI::Device>& device)
{
    if (!device) {
        return {};
    }

    return device->CreateSampler(luna::RHI::SamplerBuilder()
                                     .SetFilter(luna::RHI::Filter::Linear, luna::RHI::Filter::Linear)
                                     .SetMipmapMode(luna::RHI::SamplerMipmapMode::Linear)
                                     .SetAddressModeU(luna::RHI::SamplerAddressMode::Repeat)
                                     .SetAddressModeV(luna::RHI::SamplerAddressMode::ClampToEdge)
                                     .SetAddressModeW(luna::RHI::SamplerAddressMode::ClampToEdge)
                                     .SetLodRange(0.0f, 16.0f)
                                     .SetAnisotropy(false)
                                     .SetName("SceneEnvironmentSampler")
                                     .Build());
}

void addStaticMeshVertexLayout(luna::RHI::GraphicsPipelineBuilder& builder)
{
    builder.AddVertexBinding(0, sizeof(StaticMeshVertex), luna::RHI::VertexInputRate::Vertex)
        .AddVertexAttribute(0, 0, luna::RHI::Format::RGB32_FLOAT, offsetof(StaticMeshVertex, Position), "POSITION")
        .AddVertexAttribute(1, 0, luna::RHI::Format::RG32_FLOAT, offsetof(StaticMeshVertex, TexCoord), "TEXCOORD")
        .AddVertexAttribute(2, 0, luna::RHI::Format::RGB32_FLOAT, offsetof(StaticMeshVertex, Normal), "NORMAL")
        .AddVertexAttribute(3, 0, luna::RHI::Format::RGB32_FLOAT, offsetof(StaticMeshVertex, Tangent), "TANGENT")
        .AddVertexAttribute(4, 0, luna::RHI::Format::RGB32_FLOAT, offsetof(StaticMeshVertex, Bitangent), "BINORMAL")
        .SetTopology(luna::RHI::PrimitiveTopology::TriangleList)
        .SetCullMode(luna::RHI::CullMode::None)
        .SetFrontFace(luna::RHI::FrontFace::CounterClockwise);
}

luna::RHI::Ref<luna::RHI::PipelineLayout>
    createPipelineLayout(const luna::RHI::Ref<luna::RHI::Device>& device,
                         std::span<const luna::RHI::Ref<luna::RHI::DescriptorSetLayout>> set_layouts,
                         bool include_mesh_push_constants)
{
    if (!device) {
        return {};
    }

    luna::RHI::PipelineLayoutBuilder builder;
    for (const auto& set_layout : set_layouts) {
        builder.AddSetLayout(set_layout);
    }
    if (include_mesh_push_constants) {
        builder.AddPushConstant(luna::RHI::ShaderStage::Vertex, 0, sizeof(scene_renderer_detail::MeshPushConstants));
    }

    return device->CreatePipelineLayout(builder.Build());
}

luna::RHI::Ref<luna::RHI::GraphicsPipeline>
    createGeometryPipeline(const luna::RHI::Ref<luna::RHI::Device>& device,
                           const luna::RHI::Ref<luna::RHI::PipelineLayout>& layout,
                           const luna::RHI::Ref<luna::RHI::ShaderModule>& vertex_shader,
                           const luna::RHI::Ref<luna::RHI::ShaderModule>& fragment_shader)
{
    if (!device || !layout || !vertex_shader || !fragment_shader) {
        return {};
    }

    luna::RHI::GraphicsPipelineBuilder builder;
    builder.SetShaders({vertex_shader, fragment_shader});
    addStaticMeshVertexLayout(builder);
    builder.SetDepthTest(true, true, luna::RHI::CompareOp::Less)
        .AddColorAttachmentDefault(false)
        .AddColorAttachmentDefault(false)
        .AddColorAttachmentDefault(false)
        .AddColorAttachmentDefault(false)
        .AddColorFormat(scene_renderer_detail::kGBufferBaseColorFormat)
        .AddColorFormat(scene_renderer_detail::kGBufferLightingFormat)
        .AddColorFormat(scene_renderer_detail::kGBufferLightingFormat)
        .AddColorFormat(scene_renderer_detail::kGBufferLightingFormat)
        .SetDepthStencilFormat(luna::RHI::Format::D32_FLOAT)
        .SetLayout(layout);

    return device->CreateGraphicsPipeline(builder.Build());
}

luna::RHI::Ref<luna::RHI::GraphicsPipeline>
    createLightingPipeline(const luna::RHI::Ref<luna::RHI::Device>& device,
                           const luna::RHI::Ref<luna::RHI::PipelineLayout>& layout,
                           luna::RHI::Format color_format,
                           const luna::RHI::Ref<luna::RHI::ShaderModule>& vertex_shader,
                           const luna::RHI::Ref<luna::RHI::ShaderModule>& fragment_shader)
{
    if (!device || !layout || !vertex_shader || !fragment_shader) {
        return {};
    }

    return device->CreateGraphicsPipeline(luna::RHI::GraphicsPipelineBuilder()
                                              .SetShaders({vertex_shader, fragment_shader})
                                              .SetTopology(luna::RHI::PrimitiveTopology::TriangleList)
                                              .SetCullMode(luna::RHI::CullMode::None)
                                              .SetFrontFace(luna::RHI::FrontFace::CounterClockwise)
                                              .SetDepthTest(false, false, luna::RHI::CompareOp::Always)
                                              .AddColorAttachmentDefault(false)
                                              .AddColorFormat(color_format)
                                              .SetLayout(layout)
                                              .Build());
}

luna::RHI::Ref<luna::RHI::GraphicsPipeline>
    createTransparentPipeline(const luna::RHI::Ref<luna::RHI::Device>& device,
                              const luna::RHI::Ref<luna::RHI::PipelineLayout>& layout,
                              luna::RHI::Format color_format,
                              const luna::RHI::Ref<luna::RHI::ShaderModule>& vertex_shader,
                              const luna::RHI::Ref<luna::RHI::ShaderModule>& fragment_shader)
{
    if (!device || !layout || !vertex_shader || !fragment_shader) {
        return {};
    }

    luna::RHI::GraphicsPipelineBuilder builder;
    builder.SetShaders({vertex_shader, fragment_shader});
    addStaticMeshVertexLayout(builder);
    builder.SetDepthTest(true, false, luna::RHI::CompareOp::Less)
        .AddColorAttachment(scene_renderer_detail::makeAlphaBlendAttachment())
        .AddColorFormat(color_format)
        .SetDepthStencilFormat(luna::RHI::Format::D32_FLOAT)
        .SetLayout(layout);

    return device->CreateGraphicsPipeline(builder.Build());
}

} // namespace

class ResourceManager::Implementation final {
public:
    void setShaderPaths(ShaderPaths shader_paths)
    {
        m_shader_paths = std::move(shader_paths);
        clearUploadCaches(false);
        resetPipelineState();
    }

    void shutdown()
    {
        clearUploadCaches(true);
        resetPipelineState();
        m_gpu.device.reset();
    }

    void ensurePipelines(const RenderContext& context);
    void prepareOpaqueDraws(luna::RHI::CommandBufferEncoder& commands,
                            const DrawQueue& draw_queue,
                            const Material& default_material)
    {
        prepareDraws(commands, draw_queue.opaqueDrawCommands(), default_material);
    }

    void prepareTransparentDraws(luna::RHI::CommandBufferEncoder& commands,
                                 const DrawQueue& draw_queue,
                                 const Material& default_material)
    {
        prepareDraws(commands, draw_queue.transparentDrawCommands(), default_material);
    }

    void uploadEnvironmentIfNeeded(luna::RHI::CommandBufferEncoder& commands)
    {
        uploadTextureIfNeeded(commands, m_gpu.environment_texture);
    }

    void updateSceneParameters(const RenderContext& context, const Camera& camera);
    void updateLightingResources(const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_base_color,
                                 const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_normal_metallic,
                                 const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_world_position_roughness,
                                 const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_emissive_ao);

    [[nodiscard]] ResolvedDrawResources resolveDrawResources(const DrawCommand& draw_command,
                                                             const Material& default_material) const;

    [[nodiscard]] const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& geometryPipeline() const
    {
        return m_gpu.geometry_pipeline;
    }

    [[nodiscard]] const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& lightingPipeline() const
    {
        return m_gpu.lighting_pipeline;
    }

    [[nodiscard]] const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& transparentPipeline() const
    {
        return m_gpu.transparent_pipeline;
    }

    [[nodiscard]] const luna::RHI::Ref<luna::RHI::DescriptorSet>& sceneDescriptorSet() const
    {
        return m_gpu.scene_descriptor_set;
    }

    [[nodiscard]] const luna::RHI::Ref<luna::RHI::DescriptorSet>& gbufferDescriptorSet() const
    {
        return m_gpu.gbuffer_descriptor_set;
    }

    [[nodiscard]] const luna::RHI::Ref<luna::RHI::Sampler>& gbufferSampler() const
    {
        return m_gpu.gbuffer_sampler;
    }

private:
    struct UploadedSubMesh {
        luna::RHI::Ref<luna::RHI::Buffer> vertex_buffer;
        luna::RHI::Ref<luna::RHI::Buffer> index_buffer;
        uint32_t index_count{0};
    };

    struct UploadedMesh {
        std::vector<UploadedSubMesh> sub_meshes;
    };

    struct UploadedTexture {
        luna::RHI::Ref<luna::RHI::Texture> texture;
        luna::RHI::Ref<luna::RHI::Sampler> sampler;
        luna::RHI::Ref<luna::RHI::Buffer> staging_buffer;
        std::vector<luna::RHI::BufferImageCopy> copy_regions;
        bool uploaded{false};
    };

    struct UploadedMaterial {
        std::shared_ptr<UploadedTexture> base_color_texture;
        std::shared_ptr<UploadedTexture> normal_texture;
        std::shared_ptr<UploadedTexture> metallic_roughness_texture;
        std::shared_ptr<UploadedTexture> emissive_texture;
        std::shared_ptr<UploadedTexture> occlusion_texture;
        luna::RHI::Ref<luna::RHI::Buffer> params_buffer;
        luna::RHI::Ref<luna::RHI::DescriptorSet> descriptor_set;
    };

    struct UploadCacheState {
        std::unordered_map<const Mesh*, UploadedMesh> uploaded_meshes;
        std::unordered_map<const rhi::Texture*, std::shared_ptr<UploadedTexture>> uploaded_textures;
        std::unordered_map<const Material*, UploadedMaterial> uploaded_materials;
    };

    struct GpuState {
        luna::RHI::Ref<luna::RHI::Device> device;

        luna::RHI::Ref<luna::RHI::GraphicsPipeline> geometry_pipeline;
        luna::RHI::Ref<luna::RHI::GraphicsPipeline> lighting_pipeline;
        luna::RHI::Ref<luna::RHI::GraphicsPipeline> transparent_pipeline;

        luna::RHI::Ref<luna::RHI::PipelineLayout> geometry_pipeline_layout;
        luna::RHI::Ref<luna::RHI::PipelineLayout> lighting_pipeline_layout;
        luna::RHI::Ref<luna::RHI::PipelineLayout> transparent_pipeline_layout;

        luna::RHI::Ref<luna::RHI::DescriptorSetLayout> material_layout;
        luna::RHI::Ref<luna::RHI::DescriptorSetLayout> gbuffer_layout;
        luna::RHI::Ref<luna::RHI::DescriptorSetLayout> scene_layout;

        luna::RHI::Ref<luna::RHI::DescriptorPool> descriptor_pool;

        luna::RHI::Ref<luna::RHI::Sampler> gbuffer_sampler;
        luna::RHI::Ref<luna::RHI::Sampler> environment_sampler;

        luna::RHI::Ref<luna::RHI::DescriptorSet> gbuffer_descriptor_set;
        luna::RHI::Ref<luna::RHI::DescriptorSet> scene_descriptor_set;
        luna::RHI::Ref<luna::RHI::Buffer> scene_params_buffer;
        UploadedTexture environment_texture;

        luna::RHI::Ref<luna::RHI::ShaderModule> geometry_vertex_shader;
        luna::RHI::Ref<luna::RHI::ShaderModule> geometry_fragment_shader;
        luna::RHI::Ref<luna::RHI::ShaderModule> lighting_vertex_shader;
        luna::RHI::Ref<luna::RHI::ShaderModule> lighting_fragment_shader;
        luna::RHI::Ref<luna::RHI::ShaderModule> transparent_fragment_shader;

        luna::RHI::Format surface_format{luna::RHI::Format::UNDEFINED};
    };

    void clearUploadCaches(bool include_meshes);
    void resetPipelineState();
    [[nodiscard]] bool hasCompletePipelineState(luna::RHI::Format color_format) const;
    [[nodiscard]] ShaderPaths resolveShaderPaths() const;

    UploadedMesh& getOrCreateUploadedMesh(const Mesh& mesh);
    std::shared_ptr<UploadedTexture> getOrCreateUploadedTexture(const std::shared_ptr<rhi::Texture>& texture);
    UploadedMaterial& getOrCreateUploadedMaterial(const Material& material);
    UploadedTexture createUploadedTexture(const rhi::ImageData& image,
                                          const rhi::Texture::SamplerSettings& sampler_settings,
                                          const std::string& debug_name) const;
    void uploadTextureIfNeeded(luna::RHI::CommandBufferEncoder& commands, UploadedTexture& uploaded_texture);
    void uploadMaterialIfNeeded(luna::RHI::CommandBufferEncoder& commands, UploadedMaterial& uploaded_material);
    void ensureSceneResources();
    void prepareDraws(luna::RHI::CommandBufferEncoder& commands,
                      std::span<const DrawCommand> draw_commands,
                      const Material& default_material);

    [[nodiscard]] static const Material& resolveMaterial(const std::shared_ptr<Material>& material,
                                                         const Material& default_material)
    {
        return material != nullptr ? *material : default_material;
    }

private:
    ShaderPaths m_shader_paths{};
    UploadCacheState m_upload_cache{};
    GpuState m_gpu{};
};

void ResourceManager::Implementation::clearUploadCaches(bool include_meshes)
{
    m_upload_cache.uploaded_materials.clear();
    m_upload_cache.uploaded_textures.clear();
    if (include_meshes) {
        m_upload_cache.uploaded_meshes.clear();
    }
}

void ResourceManager::Implementation::resetPipelineState()
{
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

bool ResourceManager::Implementation::hasCompletePipelineState(luna::RHI::Format color_format) const
{
    return m_gpu.geometry_pipeline && m_gpu.lighting_pipeline && m_gpu.transparent_pipeline &&
           m_gpu.surface_format == color_format && m_gpu.gbuffer_descriptor_set && m_gpu.scene_descriptor_set &&
           m_gpu.scene_params_buffer && m_gpu.environment_texture.texture;
}

ShaderPaths ResourceManager::Implementation::resolveShaderPaths() const
{
    ShaderPaths shader_paths = m_shader_paths;
    const ShaderPaths default_paths = defaultShaderPaths();

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

void ResourceManager::Implementation::ensurePipelines(const RenderContext& context)
{
    using namespace scene_renderer_detail;

    if (!context.device || !context.compiler) {
        return;
    }

    if (m_gpu.device != context.device) {
        clearUploadCaches(true);
        resetPipelineState();
        m_gpu.device = context.device;
    } else if (hasCompletePipelineState(context.color_format)) {
        return;
    } else {
        clearUploadCaches(false);
        resetPipelineState();
    }

    const ShaderPaths shader_paths = resolveShaderPaths();
    m_gpu.geometry_vertex_shader = loadShaderModule(m_gpu.device,
                                                    context.compiler,
                                                    shader_paths.geometry_vertex_path,
                                                    "sceneGeometryVertexMain",
                                                    luna::RHI::ShaderStage::Vertex);
    m_gpu.geometry_fragment_shader = loadShaderModule(m_gpu.device,
                                                      context.compiler,
                                                      shader_paths.geometry_fragment_path,
                                                      "sceneGeometryFragmentMain",
                                                      luna::RHI::ShaderStage::Fragment);
    m_gpu.lighting_vertex_shader = loadShaderModule(m_gpu.device,
                                                    context.compiler,
                                                    shader_paths.lighting_vertex_path,
                                                    "sceneLightingVertexMain",
                                                    luna::RHI::ShaderStage::Vertex);
    m_gpu.lighting_fragment_shader = loadShaderModule(m_gpu.device,
                                                      context.compiler,
                                                      shader_paths.lighting_fragment_path,
                                                      "sceneLightingFragmentMain",
                                                      luna::RHI::ShaderStage::Fragment);
    m_gpu.transparent_fragment_shader = loadShaderModule(m_gpu.device,
                                                         context.compiler,
                                                         shader_paths.geometry_fragment_path,
                                                         "sceneTransparentFragmentMain",
                                                         luna::RHI::ShaderStage::Fragment);

    if (!m_gpu.geometry_vertex_shader || !m_gpu.geometry_fragment_shader || !m_gpu.lighting_vertex_shader ||
        !m_gpu.lighting_fragment_shader || !m_gpu.transparent_fragment_shader) {
        LUNA_RENDERER_ERROR("Failed to load scene renderer shaders");
        return;
    }

    m_gpu.material_layout = createMaterialLayout(m_gpu.device);
    m_gpu.gbuffer_layout = createGBufferLayout(m_gpu.device);
    m_gpu.scene_layout = createSceneLayout(m_gpu.device);
    m_gpu.descriptor_pool = createDescriptorPool(m_gpu.device);
    m_gpu.gbuffer_sampler = createGBufferSampler(m_gpu.device);
    m_gpu.environment_sampler = createEnvironmentSampler(m_gpu.device);

    if (m_gpu.descriptor_pool && m_gpu.gbuffer_layout) {
        m_gpu.gbuffer_descriptor_set = m_gpu.descriptor_pool->AllocateDescriptorSet(m_gpu.gbuffer_layout);
    }

    ensureSceneResources();

    const std::array<luna::RHI::Ref<luna::RHI::DescriptorSetLayout>, 2> geometry_set_layouts{
        m_gpu.material_layout,
        m_gpu.scene_layout,
    };
    const std::array<luna::RHI::Ref<luna::RHI::DescriptorSetLayout>, 2> lighting_set_layouts{
        m_gpu.gbuffer_layout,
        m_gpu.scene_layout,
    };

    m_gpu.geometry_pipeline_layout = createPipelineLayout(m_gpu.device, geometry_set_layouts, true);
    m_gpu.lighting_pipeline_layout = createPipelineLayout(m_gpu.device, lighting_set_layouts, false);
    m_gpu.transparent_pipeline_layout = createPipelineLayout(m_gpu.device, geometry_set_layouts, true);

    m_gpu.geometry_pipeline = createGeometryPipeline(
        m_gpu.device, m_gpu.geometry_pipeline_layout, m_gpu.geometry_vertex_shader, m_gpu.geometry_fragment_shader);
    m_gpu.lighting_pipeline = createLightingPipeline(
        m_gpu.device, m_gpu.lighting_pipeline_layout, context.color_format, m_gpu.lighting_vertex_shader, m_gpu.lighting_fragment_shader);
    m_gpu.transparent_pipeline = createTransparentPipeline(
        m_gpu.device, m_gpu.transparent_pipeline_layout, context.color_format, m_gpu.geometry_vertex_shader, m_gpu.transparent_fragment_shader);

    m_gpu.surface_format = context.color_format;
}

ResourceManager::Implementation::UploadedMesh& ResourceManager::Implementation::getOrCreateUploadedMesh(const Mesh& mesh)
{
    const auto it = m_upload_cache.uploaded_meshes.find(&mesh);
    if (it != m_upload_cache.uploaded_meshes.end()) {
        return it->second;
    }

    auto [inserted_it, _] = m_upload_cache.uploaded_meshes.emplace(&mesh, UploadedMesh{});
    auto& uploaded_mesh = inserted_it->second;
    const auto& sub_meshes = mesh.getSubMeshes();
    uploaded_mesh.sub_meshes.resize(sub_meshes.size());

    for (size_t sub_mesh_index = 0; sub_mesh_index < sub_meshes.size(); ++sub_mesh_index) {
        const auto& sub_mesh = sub_meshes[sub_mesh_index];
        auto& uploaded_sub_mesh = uploaded_mesh.sub_meshes[sub_mesh_index];

        if (sub_mesh.Vertices.empty() || sub_mesh.Indices.empty()) {
            continue;
        }

        const std::string sub_mesh_name =
            sub_mesh.Name.empty() ? mesh.getName() + "_SubMesh_" + std::to_string(sub_mesh_index) : sub_mesh.Name;

        uploaded_sub_mesh.vertex_buffer =
            m_gpu.device->CreateBuffer(luna::RHI::BufferBuilder()
                                           .SetSize(sub_mesh.Vertices.size() * sizeof(StaticMeshVertex))
                                           .SetUsage(luna::RHI::BufferUsageFlags::VertexBuffer)
                                           .SetMemoryUsage(luna::RHI::BufferMemoryUsage::CpuToGpu)
                                           .SetName(sub_mesh_name + "_VertexBuffer")
                                           .Build());
        uploaded_sub_mesh.index_buffer =
            m_gpu.device->CreateBuffer(luna::RHI::BufferBuilder()
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

ResourceManager::Implementation::UploadedTexture
    ResourceManager::Implementation::createUploadedTexture(const rhi::ImageData& image,
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
    uploaded_texture.texture = m_gpu.device->CreateTexture(luna::RHI::TextureBuilder()
                                                               .SetSize(image.Width, image.Height)
                                                               .SetMipLevels(mip_level_count)
                                                               .SetFormat(image.ImageFormat)
                                                               .SetUsage(luna::RHI::TextureUsageFlags::Sampled |
                                                                         luna::RHI::TextureUsageFlags::TransferDst)
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
        uint32_t height = 0;
    };

    auto align_up = [](size_t value, size_t alignment) -> size_t {
        return alignment == 0 ? value : ((value + alignment - 1) / alignment) * alignment;
    };

    std::vector<PackedMipRegion> packed_regions;
    packed_regions.reserve(mip_level_count);
    uploaded_texture.copy_regions.reserve(mip_level_count);

    size_t buffer_offset = 0;
    uint32_t mip_width = image.Width;
    uint32_t mip_height = image.Height;

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
            .height = safe_height,
        });
        buffer_offset += static_cast<size_t>(aligned_row_pitch) * safe_height;
    };

    append_region(image.ByteData, 0);
    for (uint32_t mip_level = 1; mip_level < mip_level_count; ++mip_level) {
        mip_width = (std::max)(mip_width / 2, 1u);
        mip_height = (std::max)(mip_height / 2, 1u);
        append_region(image.MipLevels[mip_level - 1], mip_level);
    }

    if (!uploaded_texture.texture || !uploaded_texture.sampler || buffer_offset == 0) {
        return uploaded_texture;
    }

    uploaded_texture.staging_buffer =
        m_gpu.device->CreateBuffer(luna::RHI::BufferBuilder()
                                       .SetSize(buffer_offset)
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

std::shared_ptr<ResourceManager::Implementation::UploadedTexture>
    ResourceManager::Implementation::getOrCreateUploadedTexture(const std::shared_ptr<rhi::Texture>& texture)
{
    if (texture == nullptr || !texture->isValid()) {
        return {};
    }

    const auto it = m_upload_cache.uploaded_textures.find(texture.get());
    if (it != m_upload_cache.uploaded_textures.end()) {
        return it->second;
    }

    const std::string debug_name = texture->getName().empty() ? std::string("Texture") : texture->getName();
    auto uploaded_texture =
        std::make_shared<UploadedTexture>(createUploadedTexture(texture->getImageData(), texture->getSamplerSettings(), debug_name));
    m_upload_cache.uploaded_textures.emplace(texture.get(), uploaded_texture);
    return uploaded_texture;
}

ResourceManager::Implementation::UploadedMaterial&
    ResourceManager::Implementation::getOrCreateUploadedMaterial(const Material& material)
{
    using namespace scene_renderer_detail;

    const auto it = m_upload_cache.uploaded_materials.find(&material);
    if (it != m_upload_cache.uploaded_materials.end()) {
        return it->second;
    }

    const auto& textures = material.getTextures();
    const auto& surface = material.getSurface();
    const std::string material_name = material.getName().empty() ? "Material" : material.getName();
    const rhi::Texture::SamplerSettings default_sampler_settings{};

    auto [inserted_it, _] = m_upload_cache.uploaded_materials.emplace(&material, UploadedMaterial{});
    auto& uploaded_material = inserted_it->second;

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
        create_material_texture(textures.BaseColor, createFallbackColorImageData(glm::vec4(1.0f)), "BaseColor");
    uploaded_material.normal_texture = create_material_texture(
        textures.Normal, createFallbackColorImageData(glm::vec4(0.5f, 0.5f, 1.0f, 1.0f)), "Normal");
    uploaded_material.metallic_roughness_texture = create_material_texture(
        textures.MetallicRoughness, createFallbackColorImageData(glm::vec4(1.0f)), "MetallicRoughness");
    uploaded_material.emissive_texture =
        create_material_texture(textures.Emissive, createFallbackColorImageData(glm::vec4(1.0f)), "Emissive");
    uploaded_material.occlusion_texture =
        create_material_texture(textures.Occlusion, createFallbackColorImageData(glm::vec4(1.0f)), "Occlusion");

    uploaded_material.params_buffer =
        m_gpu.device->CreateBuffer(luna::RHI::BufferBuilder()
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

    if (!m_gpu.descriptor_pool || !m_gpu.material_layout ||
        !uploaded_material.base_color_texture || !uploaded_material.normal_texture ||
        !uploaded_material.metallic_roughness_texture || !uploaded_material.emissive_texture ||
        !uploaded_material.occlusion_texture || !uploaded_material.base_color_texture->texture ||
        !uploaded_material.normal_texture->texture || !uploaded_material.metallic_roughness_texture->texture ||
        !uploaded_material.emissive_texture->texture || !uploaded_material.occlusion_texture->texture ||
        !uploaded_material.base_color_texture->sampler || !uploaded_material.normal_texture->sampler ||
        !uploaded_material.metallic_roughness_texture->sampler || !uploaded_material.emissive_texture->sampler ||
        !uploaded_material.occlusion_texture->sampler || !uploaded_material.params_buffer) {
        return uploaded_material;
    }

    uploaded_material.descriptor_set = m_gpu.descriptor_pool->AllocateDescriptorSet(m_gpu.material_layout);
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

void ResourceManager::Implementation::uploadTextureIfNeeded(luna::RHI::CommandBufferEncoder& commands,
                                                            UploadedTexture& uploaded_texture)
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

void ResourceManager::Implementation::uploadMaterialIfNeeded(luna::RHI::CommandBufferEncoder& commands,
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

void ResourceManager::Implementation::ensureSceneResources()
{
    using namespace scene_renderer_detail;

    if (!m_gpu.device || !m_gpu.descriptor_pool || !m_gpu.scene_layout || !m_gpu.environment_sampler) {
        return;
    }

    if (!m_gpu.scene_params_buffer) {
        m_gpu.scene_params_buffer = m_gpu.device->CreateBuffer(luna::RHI::BufferBuilder()
                                                                   .SetSize(sizeof(SceneGpuParams))
                                                                   .SetUsage(luna::RHI::BufferUsageFlags::UniformBuffer)
                                                                   .SetMemoryUsage(luna::RHI::BufferMemoryUsage::CpuToGpu)
                                                                   .SetName("SceneParams")
                                                                   .Build());
    }

    if (!m_gpu.environment_texture.texture) {
        std::filesystem::path environment_path = defaultEnvironmentPath();
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
        m_gpu.environment_texture =
            createUploadedTexture(environment_image, rhi::Texture::SamplerSettings{}, "SceneEnvironment");
    }

    if (!m_gpu.scene_descriptor_set) {
        m_gpu.scene_descriptor_set = m_gpu.descriptor_pool->AllocateDescriptorSet(m_gpu.scene_layout);
    }

    if (!m_gpu.scene_descriptor_set || !m_gpu.scene_params_buffer || !m_gpu.environment_texture.texture) {
        return;
    }

    m_gpu.scene_descriptor_set->WriteBuffer(luna::RHI::BufferWriteInfo{
        .Binding = 0,
        .Buffer = m_gpu.scene_params_buffer,
        .Offset = 0,
        .Stride = sizeof(SceneGpuParams),
        .Size = sizeof(SceneGpuParams),
        .Type = luna::RHI::DescriptorType::UniformBuffer,
    });
    m_gpu.scene_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 1,
        .TextureView = m_gpu.environment_texture.texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_gpu.scene_descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
        .Binding = 2,
        .Sampler = m_gpu.environment_sampler,
    });
    m_gpu.scene_descriptor_set->Update();
}

void ResourceManager::Implementation::prepareDraws(luna::RHI::CommandBufferEncoder& commands,
                                                   std::span<const DrawCommand> draw_commands,
                                                   const Material& default_material)
{
    (void) getOrCreateUploadedMaterial(default_material);

    for (const auto& draw_command : draw_commands) {
        if (!draw_command.mesh || !draw_command.mesh->isValid()) {
            continue;
        }

        (void) getOrCreateUploadedMesh(*draw_command.mesh);
        auto& uploaded_material = getOrCreateUploadedMaterial(resolveMaterial(draw_command.material, default_material));
        uploadMaterialIfNeeded(commands, uploaded_material);
    }
}

void ResourceManager::Implementation::updateSceneParameters(const RenderContext& context, const Camera& camera)
{
    using namespace scene_renderer_detail;

    if (!m_gpu.scene_params_buffer || context.framebuffer_width == 0 || context.framebuffer_height == 0) {
        return;
    }

    const float aspect_ratio =
        static_cast<float>(context.framebuffer_width) / static_cast<float>(context.framebuffer_height);
    const glm::mat4 view_projection = buildViewProjection(camera, aspect_ratio, context.backend_type);
    const float environment_mip_count =
        m_gpu.environment_texture.texture != nullptr
            ? static_cast<float>((std::max)(m_gpu.environment_texture.texture->GetMipLevels(), 1u) - 1u)
            : 0.0f;

    SceneGpuParams params;
    params.view_projection = view_projection;
    params.inverse_view_projection = glm::inverse(view_projection);
    params.camera_position_env_mip = glm::vec4(resolveCameraPosition(camera), environment_mip_count);
    params.light_direction_intensity = glm::vec4(glm::normalize(glm::vec3(0.45f, 0.80f, 0.35f)), 4.0f);
    params.light_color_exposure = glm::vec4(1.0f, 0.98f, 0.95f, 1.0f);
    params.ibl_factors = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);

    if (void* mapped = m_gpu.scene_params_buffer->Map()) {
        std::memcpy(mapped, &params, sizeof(params));
        m_gpu.scene_params_buffer->Flush();
        m_gpu.scene_params_buffer->Unmap();
    }
}

void ResourceManager::Implementation::updateLightingResources(
    const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_base_color,
    const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_normal_metallic,
    const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_world_position_roughness,
    const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_emissive_ao)
{
    if (!m_gpu.gbuffer_descriptor_set || !m_gpu.gbuffer_sampler || !gbuffer_base_color || !gbuffer_normal_metallic ||
        !gbuffer_world_position_roughness || !gbuffer_emissive_ao) {
        return;
    }

    m_gpu.gbuffer_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 0,
        .TextureView = gbuffer_base_color->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_gpu.gbuffer_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 1,
        .TextureView = gbuffer_normal_metallic->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_gpu.gbuffer_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 2,
        .TextureView = gbuffer_world_position_roughness->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_gpu.gbuffer_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 3,
        .TextureView = gbuffer_emissive_ao->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_gpu.gbuffer_descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
        .Binding = 4,
        .Sampler = m_gpu.gbuffer_sampler,
    });
    m_gpu.gbuffer_descriptor_set->Update();
}

ResolvedDrawResources ResourceManager::Implementation::resolveDrawResources(const DrawCommand& draw_command,
                                                                           const Material& default_material) const
{
    ResolvedDrawResources resolved;
    if (!draw_command.mesh || !draw_command.mesh->isValid()) {
        return resolved;
    }

    const auto uploaded_mesh_it = m_upload_cache.uploaded_meshes.find(draw_command.mesh.get());
    if (uploaded_mesh_it == m_upload_cache.uploaded_meshes.end()) {
        return resolved;
    }

    const Material& material = resolveMaterial(draw_command.material, default_material);
    const auto uploaded_material_it = m_upload_cache.uploaded_materials.find(&material);
    if (uploaded_material_it == m_upload_cache.uploaded_materials.end() || !uploaded_material_it->second.descriptor_set) {
        return resolved;
    }

    const auto& uploaded_mesh = uploaded_mesh_it->second;
    if (draw_command.sub_mesh_index >= uploaded_mesh.sub_meshes.size()) {
        return resolved;
    }

    const auto& uploaded_sub_mesh = uploaded_mesh.sub_meshes[draw_command.sub_mesh_index];
    resolved.vertex_buffer = uploaded_sub_mesh.vertex_buffer;
    resolved.index_buffer = uploaded_sub_mesh.index_buffer;
    resolved.material_descriptor_set = uploaded_material_it->second.descriptor_set;
    resolved.index_count = uploaded_sub_mesh.index_count;
    return resolved;
}

ResourceManager::ResourceManager()
    : m_impl(std::make_unique<Implementation>())
{}

ResourceManager::~ResourceManager()
{
    shutdown();
}

void ResourceManager::setShaderPaths(SceneRenderer::ShaderPaths shader_paths)
{
    m_impl->setShaderPaths(std::move(shader_paths));
}

void ResourceManager::shutdown()
{
    m_impl->shutdown();
}

void ResourceManager::ensurePipelines(const SceneRenderer::RenderContext& context)
{
    m_impl->ensurePipelines(context);
}

void ResourceManager::prepareOpaqueDraws(luna::RHI::CommandBufferEncoder& commands,
                                         const DrawQueue& draw_queue,
                                         const Material& default_material)
{
    m_impl->prepareOpaqueDraws(commands, draw_queue, default_material);
}

void ResourceManager::prepareTransparentDraws(luna::RHI::CommandBufferEncoder& commands,
                                              const DrawQueue& draw_queue,
                                              const Material& default_material)
{
    m_impl->prepareTransparentDraws(commands, draw_queue, default_material);
}

void ResourceManager::uploadEnvironmentIfNeeded(luna::RHI::CommandBufferEncoder& commands)
{
    m_impl->uploadEnvironmentIfNeeded(commands);
}

void ResourceManager::updateSceneParameters(const SceneRenderer::RenderContext& context, const Camera& camera)
{
    m_impl->updateSceneParameters(context, camera);
}

void ResourceManager::updateLightingResources(const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_base_color,
                                              const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_normal_metallic,
                                              const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_world_position_roughness,
                                              const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_emissive_ao)
{
    m_impl->updateLightingResources(
        gbuffer_base_color, gbuffer_normal_metallic, gbuffer_world_position_roughness, gbuffer_emissive_ao);
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

const luna::RHI::Ref<luna::RHI::DescriptorSet>& ResourceManager::gbufferDescriptorSet() const
{
    return m_impl->gbufferDescriptorSet();
}

const luna::RHI::Ref<luna::RHI::Sampler>& ResourceManager::gbufferSampler() const
{
    return m_impl->gbufferSampler();
}

} // namespace luna::scene_renderer
