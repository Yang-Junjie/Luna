#include "Core/Log.h"
#include "Math/Math.h"
#include "Renderer/Image/ImageDataUtils.h"
#include "Renderer/Mesh.h"
#include "Renderer/RendererUtilities.h"
#include "Renderer/RenderFlow/DefaultScene/BindingSchema.h"
#include "Renderer/RenderFlow/DefaultScene/Constants.h"
#include "Renderer/RenderFlow/DefaultScene/PipelineState.h"
#include "Renderer/RenderWorld/RenderWorld.h"
#include "Renderer/Resources/ShaderModuleLoader.h"
#include "Renderer/Resources/TextureUpload.h"

#include <cstring>

#include <array>
#include <Builders.h>
#include <DescriptorPool.h>
#include <DescriptorSet.h>
#include <DescriptorSetLayout.h>
#include <Device.h>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>
#include <Pipeline.h>
#include <PipelineLayout.h>
#include <Sampler.h>
#include <ShaderCompiler.h>
#include <span>
#include <string_view>
#include <vector>

namespace luna::render_flow::default_scene {

namespace {

luna::RHI::Ref<luna::RHI::DescriptorSetLayout> createMaterialLayout(const luna::RHI::Ref<luna::RHI::Device>& device)
{
    return createDescriptorSetLayoutFromSchema(device, materialDescriptorSetSchema());
}

luna::RHI::Ref<luna::RHI::DescriptorSetLayout> createGBufferLayout(const luna::RHI::Ref<luna::RHI::Device>& device)
{
    return createDescriptorSetLayoutFromSchema(device, gbufferDescriptorSetSchema());
}

luna::RHI::Ref<luna::RHI::DescriptorSetLayout> createSceneLayout(const luna::RHI::Ref<luna::RHI::Device>& device)
{
    return createDescriptorSetLayoutFromSchema(device, sceneDescriptorSetSchema());
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
                                     .SetName("SceneEnvironmentSourceSampler")
                                     .Build());
}

luna::RHI::Ref<luna::RHI::Sampler> createShadowSampler(const luna::RHI::Ref<luna::RHI::Device>& device)
{
    if (!device) {
        return {};
    }

    return device->CreateSampler(luna::RHI::SamplerBuilder()
                                     .SetFilter(luna::RHI::Filter::Nearest, luna::RHI::Filter::Nearest)
                                     .SetMipmapMode(luna::RHI::SamplerMipmapMode::Nearest)
                                     .SetAddressMode(luna::RHI::SamplerAddressMode::ClampToBorder)
                                     .SetBorderColor(luna::RHI::BorderColor::FloatOpaqueWhite)
                                     .SetAnisotropy(false)
                                     .SetName("SceneShadowMapSampler")
                                     .Build());
}

renderer_detail::PendingTextureUpload createDefaultLightingInputTexture(const luna::RHI::Ref<luna::RHI::Device>& device,
                                                                        const glm::vec4& value,
                                                                        std::string_view name)
{
    luna::Texture::SamplerSettings sampler_settings;
    sampler_settings.MipFilter = luna::Texture::MipFilterMode::None;
    sampler_settings.WrapU = luna::Texture::WrapMode::ClampToEdge;
    sampler_settings.WrapV = luna::Texture::WrapMode::ClampToEdge;
    sampler_settings.WrapW = luna::Texture::WrapMode::ClampToEdge;
    return renderer_detail::createTextureUpload(
        device, renderer_detail::createFallbackColorImageData(value), sampler_settings, name);
}

const luna::RHI::Ref<luna::RHI::Texture>& textureOrFallback(const luna::RHI::Ref<luna::RHI::Texture>& texture,
                                                            const renderer_detail::PendingTextureUpload& fallback)
{
    return texture ? texture : fallback.texture;
}

glm::mat4 adjustProjectionForBackend(glm::mat4 projection, luna::RHI::BackendType backend_type)
{
    return backend_type == luna::RHI::BackendType::Vulkan ? luna::flipProjectionY(projection) : projection;
}

RenderViewMatrices buildViewMatrices(const Camera& camera, float aspect_ratio, luna::RHI::BackendType backend_type)
{
    RenderViewMatrices matrices{};
    matrices.view = camera.getViewMatrix();
    matrices.projection = adjustProjectionForBackend(camera.getProjectionMatrix(aspect_ratio), backend_type);
    matrices.view_projection = matrices.projection * matrices.view;
    matrices.inverse_view = glm::inverse(matrices.view);
    matrices.inverse_projection = glm::inverse(matrices.projection);
    matrices.inverse_view_projection = glm::inverse(matrices.view_projection);
    return matrices;
}

bool hasValidViewState(const RenderViewFrameState& view_state)
{
    return view_state.viewport_size.x > 0 && view_state.viewport_size.y > 0;
}

glm::vec2 reciprocalViewportSize(glm::uvec2 viewport_size)
{
    return glm::vec2(viewport_size.x > 0 ? 1.0f / static_cast<float>(viewport_size.x) : 0.0f,
                     viewport_size.y > 0 ? 1.0f / static_cast<float>(viewport_size.y) : 0.0f);
}

glm::vec4 packViewportSize(glm::uvec2 viewport_size)
{
    const glm::vec2 reciprocal_size = reciprocalViewportSize(viewport_size);
    return glm::vec4(
        static_cast<float>(viewport_size.x), static_cast<float>(viewport_size.y), reciprocal_size.x, reciprocal_size.y);
}

uint32_t low32(uint64_t value)
{
    return static_cast<uint32_t>(value & 0xff'ff'ff'ffull);
}

RenderViewFrameState resolveViewState(const SceneRenderContext& context,
                                      const Camera& camera,
                                      const RenderFeatureFrameContext& frame_context)
{
    if (hasValidViewState(frame_context.view)) {
        return frame_context.view;
    }

    const float aspect_ratio =
        static_cast<float>(context.framebuffer_width) / static_cast<float>(context.framebuffer_height);
    const RenderViewMatrices matrices = buildViewMatrices(camera, aspect_ratio, context.backend_type);
    return RenderViewFrameState{
        .current = matrices,
        .previous = matrices,
        .current_jittered = matrices,
        .previous_jittered = matrices,
        .viewport_size = glm::uvec2(context.framebuffer_width, context.framebuffer_height),
        .previous_viewport_size = glm::uvec2(context.framebuffer_width, context.framebuffer_height),
        .frame_index = frame_context.frame_index,
        .previous_frame_index = frame_context.frame_index,
        .temporal_frame_index = frame_context.frame_index,
        .previous_temporal_frame_index = frame_context.frame_index,
        .history_valid = false,
    };
}

luna::RHI::ColorBlendAttachmentState makeAlphaBlendAttachment()
{
    luna::RHI::ColorBlendAttachmentState blend_attachment{};
    blend_attachment.BlendEnable = true;
    blend_attachment.SrcColorBlendFactor = luna::RHI::BlendFactor::SrcAlpha;
    blend_attachment.DstColorBlendFactor = luna::RHI::BlendFactor::OneMinusSrcAlpha;
    blend_attachment.ColorBlendOp = luna::RHI::BlendOp::Add;
    blend_attachment.SrcAlphaBlendFactor = luna::RHI::BlendFactor::One;
    blend_attachment.DstAlphaBlendFactor = luna::RHI::BlendFactor::OneMinusSrcAlpha;
    blend_attachment.AlphaBlendOp = luna::RHI::BlendOp::Add;
    blend_attachment.ColorWriteMask = luna::RHI::ColorComponentFlags::All;
    return blend_attachment;
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

luna::RHI::Ref<luna::RHI::GraphicsPipeline>
    createShadowPipeline(const luna::RHI::Ref<luna::RHI::Device>& device,
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
        .SetDepthBias(true, 1.25f, 0.0f, 1.75f)
        .AddColorAttachmentDefault(false)
        .AddColorFormat(render_flow::default_scene_detail::kShadowMapFormat)
        .SetDepthStencilFormat(luna::RHI::Format::D32_FLOAT)
        .SetLayout(layout);

    return device->CreateGraphicsPipeline(builder.Build());
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
        .AddColorAttachmentDefault(false)
        .AddColorAttachmentDefault(false)
        .AddColorFormat(render_flow::default_scene_detail::kGBufferBaseColorFormat)
        .AddColorFormat(render_flow::default_scene_detail::kGBufferLightingFormat)
        .AddColorFormat(render_flow::default_scene_detail::kGBufferLightingFormat)
        .AddColorFormat(render_flow::default_scene_detail::kGBufferLightingFormat)
        .AddColorFormat(render_flow::default_scene_detail::kVelocityFormat)
        .AddColorFormat(render_flow::default_scene_detail::kScenePickingFormat)
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
        .AddColorAttachment(makeAlphaBlendAttachment())
        .AddColorAttachmentDefault(false)
        .AddColorFormat(color_format)
        .AddColorFormat(render_flow::default_scene_detail::kScenePickingFormat)
        .SetDepthStencilFormat(luna::RHI::Format::D32_FLOAT)
        .SetLayout(layout);

    return device->CreateGraphicsPipeline(builder.Build());
}

} // namespace

void PipelineState::shutdown()
{
    reset();
}

bool PipelineState::hasAnyState() const noexcept
{
    return m_state.device || m_state.geometry_pipeline || m_state.scene_descriptor_set;
}

bool PipelineState::hasCompleteState(const SceneRenderContext& context) const noexcept
{
    return m_state.device == context.device && m_state.backend_type == context.backend_type &&
           m_state.surface_format == context.color_format && m_state.geometry_pipeline && m_state.shadow_pipeline &&
           m_state.lighting_pipeline && m_state.debug_view_pipeline && m_state.transparent_pipeline &&
           m_state.material_layout && m_state.descriptor_pool && m_state.gbuffer_descriptor_set &&
           m_state.scene_descriptor_set && m_state.lighting_scene_descriptor_set && m_state.scene_params_buffer &&
           m_state.gbuffer_sampler && m_state.environment_source_sampler && m_state.shadow_sampler;
}

void PipelineState::rebuild(const SceneRenderContext& context, const SceneShaderPaths& shader_paths)
{
    reset();
    m_state.device = context.device;
    m_state.backend_type = context.backend_type;

    if (!m_state.device || !context.compiler) {
        LUNA_RENDERER_WARN("Cannot rebuild scene render flow pipelines: device={} compiler={}",
                           static_cast<bool>(m_state.device),
                           static_cast<bool>(context.compiler));
        return;
    }

    LUNA_RENDERER_DEBUG("Loading scene render flow shaders: geometry_vs='{}' geometry_fs='{}' shadow_vs='{}' "
                        "shadow_fs='{}' lighting_vs='{}' lighting_fs='{}'",
                        shader_paths.geometry_vertex_path.string(),
                        shader_paths.geometry_fragment_path.string(),
                        shader_paths.shadow_vertex_path.string(),
                        shader_paths.shadow_fragment_path.string(),
                        shader_paths.lighting_vertex_path.string(),
                        shader_paths.lighting_fragment_path.string());

    m_state.geometry_vertex_shader = renderer_detail::loadShaderModule(m_state.device,
                                                                       context.compiler,
                                                                       shader_paths.geometry_vertex_path,
                                                                       "sceneGeometryVertexMain",
                                                                       luna::RHI::ShaderStage::Vertex);
    m_state.geometry_fragment_shader = renderer_detail::loadShaderModule(m_state.device,
                                                                         context.compiler,
                                                                         shader_paths.geometry_fragment_path,
                                                                         "sceneGeometryFragmentMain",
                                                                         luna::RHI::ShaderStage::Fragment);
    m_state.shadow_vertex_shader = renderer_detail::loadShaderModule(m_state.device,
                                                                     context.compiler,
                                                                     shader_paths.shadow_vertex_path,
                                                                     "sceneShadowVertexMain",
                                                                     luna::RHI::ShaderStage::Vertex);
    m_state.shadow_fragment_shader = renderer_detail::loadShaderModule(m_state.device,
                                                                       context.compiler,
                                                                       shader_paths.shadow_fragment_path,
                                                                       "sceneShadowFragmentMain",
                                                                       luna::RHI::ShaderStage::Fragment);
    m_state.lighting_vertex_shader = renderer_detail::loadShaderModule(m_state.device,
                                                                       context.compiler,
                                                                       shader_paths.lighting_vertex_path,
                                                                       "sceneLightingVertexMain",
                                                                       luna::RHI::ShaderStage::Vertex);
    m_state.lighting_fragment_shader = renderer_detail::loadShaderModule(m_state.device,
                                                                         context.compiler,
                                                                         shader_paths.lighting_fragment_path,
                                                                         "sceneLightingFragmentMain",
                                                                         luna::RHI::ShaderStage::Fragment);
    m_state.debug_view_fragment_shader = renderer_detail::loadShaderModule(m_state.device,
                                                                           context.compiler,
                                                                           shader_paths.lighting_fragment_path,
                                                                           "sceneDebugFragmentMain",
                                                                           luna::RHI::ShaderStage::Fragment);
    m_state.transparent_fragment_shader = renderer_detail::loadShaderModule(m_state.device,
                                                                            context.compiler,
                                                                            shader_paths.geometry_fragment_path,
                                                                            "sceneTransparentFragmentMain",
                                                                            luna::RHI::ShaderStage::Fragment);

    if (!m_state.geometry_vertex_shader || !m_state.geometry_fragment_shader || !m_state.shadow_vertex_shader ||
        !m_state.shadow_fragment_shader || !m_state.lighting_vertex_shader || !m_state.lighting_fragment_shader ||
        !m_state.debug_view_fragment_shader || !m_state.transparent_fragment_shader) {
        LUNA_RENDERER_ERROR("Failed to load scene render flow shaders");
        return;
    }

    m_state.material_layout = createMaterialLayout(m_state.device);
    m_state.gbuffer_layout = createGBufferLayout(m_state.device);
    m_state.scene_layout = createSceneLayout(m_state.device);
    m_state.descriptor_pool = createDescriptorPool(m_state.device);
    m_state.gbuffer_sampler = createGBufferSampler(m_state.device);
    m_state.environment_source_sampler = createEnvironmentSampler(m_state.device);
    m_state.shadow_sampler = createShadowSampler(m_state.device);
    m_state.default_ambient_occlusion_texture =
        createDefaultLightingInputTexture(m_state.device, glm::vec4(1.0f), "DefaultAmbientOcclusion");
    m_state.default_reflection_texture =
        createDefaultLightingInputTexture(m_state.device, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), "DefaultReflection");
    m_state.default_indirect_diffuse_texture =
        createDefaultLightingInputTexture(m_state.device, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), "DefaultIndirectDiffuse");
    m_state.default_indirect_specular_texture =
        createDefaultLightingInputTexture(m_state.device, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), "DefaultIndirectSpecular");

    if (m_state.descriptor_pool && m_state.gbuffer_layout) {
        m_state.gbuffer_descriptor_set = m_state.descriptor_pool->AllocateDescriptorSet(m_state.gbuffer_layout);
    }
    if (m_state.descriptor_pool && m_state.scene_layout) {
        m_state.scene_descriptor_set = m_state.descriptor_pool->AllocateDescriptorSet(m_state.scene_layout);
        m_state.lighting_scene_descriptor_set = m_state.descriptor_pool->AllocateDescriptorSet(m_state.scene_layout);
    }
    if (m_state.device) {
        m_state.scene_params_buffer =
            m_state.device->CreateBuffer(luna::RHI::BufferBuilder()
                                             .SetSize(sizeof(render_flow::default_scene_detail::SceneGpuParams))
                                             .SetUsage(luna::RHI::BufferUsageFlags::UniformBuffer)
                                             .SetMemoryUsage(luna::RHI::BufferMemoryUsage::CpuToGpu)
                                             .SetName("SceneParams")
                                             .Build());
    }

    const DescriptorSetLayoutRefs descriptor_set_layouts{
        .material = m_state.material_layout,
        .gbuffer = m_state.gbuffer_layout,
        .scene = m_state.scene_layout,
    };

    m_state.geometry_pipeline_layout =
        createPipelineLayoutFromSchema(m_state.device, geometryPipelineLayoutSchema(), descriptor_set_layouts);
    m_state.shadow_pipeline_layout =
        createPipelineLayoutFromSchema(m_state.device, shadowPipelineLayoutSchema(), descriptor_set_layouts);
    m_state.lighting_pipeline_layout =
        createPipelineLayoutFromSchema(m_state.device, lightingPipelineLayoutSchema(), descriptor_set_layouts);
    m_state.transparent_pipeline_layout =
        createPipelineLayoutFromSchema(m_state.device, transparentPipelineLayoutSchema(), descriptor_set_layouts);

    m_state.geometry_pipeline = createGeometryPipeline(m_state.device,
                                                       m_state.geometry_pipeline_layout,
                                                       m_state.geometry_vertex_shader,
                                                       m_state.geometry_fragment_shader);
    m_state.shadow_pipeline = createShadowPipeline(
        m_state.device, m_state.shadow_pipeline_layout, m_state.shadow_vertex_shader, m_state.shadow_fragment_shader);
    m_state.lighting_pipeline = createLightingPipeline(m_state.device,
                                                       m_state.lighting_pipeline_layout,
                                                       context.color_format,
                                                       m_state.lighting_vertex_shader,
                                                       m_state.lighting_fragment_shader);
    m_state.debug_view_pipeline = createLightingPipeline(m_state.device,
                                                         m_state.lighting_pipeline_layout,
                                                         context.color_format,
                                                         m_state.lighting_vertex_shader,
                                                         m_state.debug_view_fragment_shader);
    m_state.transparent_pipeline = createTransparentPipeline(m_state.device,
                                                             m_state.transparent_pipeline_layout,
                                                             context.color_format,
                                                             m_state.geometry_vertex_shader,
                                                             m_state.transparent_fragment_shader);
    m_state.surface_format = context.color_format;

    LUNA_RENDERER_INFO("Created scene render flow graphics pipelines for color format {} ({})",
                       renderer_detail::formatToString(context.color_format),
                       static_cast<int>(context.color_format));
}

void PipelineState::updateSceneBindings(const luna::RHI::Ref<luna::RHI::Texture>& environment_texture,
                                        const luna::RHI::Ref<luna::RHI::Texture>& prefiltered_environment_texture,
                                        const luna::RHI::Ref<luna::RHI::Texture>& brdf_lut_texture)
{
    if (!m_state.scene_descriptor_set || !m_state.lighting_scene_descriptor_set || !m_state.scene_params_buffer ||
        !environment_texture || !prefiltered_environment_texture || !brdf_lut_texture ||
        !m_state.environment_source_sampler) {
        LUNA_RENDERER_WARN("Scene resources are incomplete: scene_descriptor_set={} lighting_scene_descriptor_set={} "
                           "scene_params_buffer={} environment_texture={} prefiltered_environment={} brdf_lut={} "
                           "environment_sampler={}",
                           static_cast<bool>(m_state.scene_descriptor_set),
                           static_cast<bool>(m_state.lighting_scene_descriptor_set),
                           static_cast<bool>(m_state.scene_params_buffer),
                           static_cast<bool>(environment_texture),
                           static_cast<bool>(prefiltered_environment_texture),
                           static_cast<bool>(brdf_lut_texture),
                           static_cast<bool>(m_state.environment_source_sampler));
        return;
    }

    if (m_state.scene_bindings_valid && m_state.bound_environment_texture == environment_texture &&
        m_state.bound_prefiltered_environment_texture == prefiltered_environment_texture &&
        m_state.bound_brdf_lut_texture == brdf_lut_texture) {
        return;
    }

    auto write_scene_bindings = [this, &environment_texture, &prefiltered_environment_texture, &brdf_lut_texture](
                                    const luna::RHI::Ref<luna::RHI::DescriptorSet>& descriptor_set) {
        descriptor_set->WriteBuffer(luna::RHI::BufferWriteInfo{
            .Binding = scene_binding::SceneParams,
            .Buffer = m_state.scene_params_buffer,
            .Offset = 0,
            .Stride = sizeof(render_flow::default_scene_detail::SceneGpuParams),
            .Size = sizeof(render_flow::default_scene_detail::SceneGpuParams),
            .Type = luna::RHI::DescriptorType::UniformBuffer,
        });
        descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
            .Binding = scene_binding::EnvironmentTexture,
            .TextureView = environment_texture->GetDefaultView(),
            .Layout = luna::RHI::ResourceState::ShaderRead,
            .Type = luna::RHI::DescriptorType::SampledImage,
        });
        descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
            .Binding = scene_binding::EnvironmentSampler,
            .Sampler = m_state.environment_source_sampler,
        });
        descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
            .Binding = scene_binding::EnvironmentPrefilterTexture,
            .TextureView = prefiltered_environment_texture->GetDefaultView(),
            .Layout = luna::RHI::ResourceState::ShaderRead,
            .Type = luna::RHI::DescriptorType::SampledImage,
        });
        descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
            .Binding = scene_binding::EnvironmentBrdfLut,
            .TextureView = brdf_lut_texture->GetDefaultView(),
            .Layout = luna::RHI::ResourceState::ShaderRead,
            .Type = luna::RHI::DescriptorType::SampledImage,
        });
        descriptor_set->Update();
    };

    write_scene_bindings(m_state.scene_descriptor_set);
    write_scene_bindings(m_state.lighting_scene_descriptor_set);

    m_state.bound_environment_texture = environment_texture;
    m_state.bound_prefiltered_environment_texture = prefiltered_environment_texture;
    m_state.bound_brdf_lut_texture = brdf_lut_texture;
    m_state.scene_bindings_valid = true;
}

namespace {

struct SceneLightView {
    const RenderDirectionalLight* directional_light{nullptr};
    std::span<const RenderPointLight> point_lights;
    std::span<const RenderSpotLight> spot_lights;
};

void updateSceneParameterBuffer(const SceneRenderContext& context,
                                const Camera& camera,
                                const RenderFeatureFrameContext& frame_context,
                                const SceneLightView& lights,
                                float environment_mip_count,
                                const std::array<glm::vec4, 9>& irradiance_sh,
                                const render_flow::default_scene_detail::ShadowRenderParams& shadow_params,
                                const luna::RHI::Ref<luna::RHI::Buffer>& scene_params_buffer)
{
    if (!scene_params_buffer || context.framebuffer_width == 0 || context.framebuffer_height == 0) {
        LUNA_RENDERER_WARN("Cannot update scene parameters: scene_params_buffer={} framebuffer={}x{}",
                           static_cast<bool>(scene_params_buffer),
                           context.framebuffer_width,
                           context.framebuffer_height);
        return;
    }

    const RenderViewFrameState view_state = resolveViewState(context, camera, frame_context);

    render_flow::default_scene_detail::SceneGpuParams params;
    params.view_projection = view_state.current.view_projection;
    params.inverse_view_projection = view_state.current.inverse_view_projection;
    params.view = view_state.current.view;
    params.projection = view_state.current.projection;
    params.inverse_view = view_state.current.inverse_view;
    params.inverse_projection = view_state.current.inverse_projection;
    params.jittered_view_projection = view_state.current_jittered.view_projection;
    params.inverse_jittered_view_projection = view_state.current_jittered.inverse_view_projection;
    params.previous_view_projection = view_state.previous.view_projection;
    params.previous_inverse_view_projection = view_state.previous.inverse_view_projection;
    params.previous_jittered_view_projection = view_state.previous_jittered.view_projection;
    params.previous_inverse_jittered_view_projection = view_state.previous_jittered.inverse_view_projection;
    params.camera_position_env_mip = glm::vec4(camera.getPosition(), environment_mip_count);
    params.viewport_size = packViewportSize(view_state.viewport_size);
    params.previous_viewport_size = packViewportSize(view_state.previous_viewport_size);
    params.jitter_ndc = glm::vec4(view_state.jitter_ndc, view_state.previous_jitter_ndc);
    params.jitter_pixels = glm::vec4(view_state.jitter_pixels, view_state.previous_jitter_pixels);
    params.frame_indices = glm::uvec4(low32(view_state.frame_index),
                                      low32(view_state.previous_frame_index),
                                      low32(view_state.temporal_frame_index),
                                      low32(view_state.previous_temporal_frame_index));
    params.view_flags = glm::uvec4(view_state.history_valid ? 1u : 0u,
                                   view_state.jitter_sample_index,
                                   view_state.previous_jitter_sample_index,
                                   0u);
    if (lights.directional_light && lights.directional_light->intensity > 0.0f) {
        const auto& directional_light = *lights.directional_light;
        params.light_direction_intensity =
            glm::vec4(glm::normalize(directional_light.direction), directional_light.intensity);
        params.light_color_exposure = glm::vec4(directional_light.color, 1.0f);
    } else {
        params.light_direction_intensity = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
        params.light_color_exposure = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    const uint32_t point_light_count = (std::min) (static_cast<uint32_t>(lights.point_lights.size()),
                                                   render_flow::default_scene_detail::kMaxPointLights);
    const uint32_t spot_light_count = (std::min) (static_cast<uint32_t>(lights.spot_lights.size()),
                                                  render_flow::default_scene_detail::kMaxSpotLights);
    params.light_counts =
        glm::vec4(static_cast<float>(point_light_count), static_cast<float>(spot_light_count), 0.0f, 0.0f);

    for (uint32_t light_index = 0; light_index < point_light_count; ++light_index) {
        const auto& light = lights.point_lights[light_index];
        params.point_light_position_intensity[light_index] = glm::vec4(light.position, light.intensity);
        params.point_light_color_range[light_index] = glm::vec4(light.color, light.range);
    }

    for (uint32_t light_index = 0; light_index < spot_light_count; ++light_index) {
        const auto& light = lights.spot_lights[light_index];
        params.spot_light_position_intensity[light_index] = glm::vec4(light.position, light.intensity);
        params.spot_light_direction_range[light_index] = glm::vec4(light.direction, light.range);
        params.spot_light_color_cones[light_index] = glm::vec4(light.color, 0.0f);
        params.spot_light_cone_params[light_index] = glm::vec4(light.innerConeCos, light.outerConeCos, 0.0f, 0.0f);
    }
    params.ibl_factors = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
    params.debug_overlay_params = glm::vec4(context.show_pick_debug_visualization ? 1.0f : 0.0f,
                                            0.65f,
                                            static_cast<float>(context.debug_view_mode),
                                            context.debug_velocity_scale);
    params.debug_pick_marker =
        glm::vec4(static_cast<float>(context.debug_pick_pixel_x),
                  static_cast<float>(context.debug_pick_pixel_y),
                  (context.show_pick_debug_visualization && context.show_pick_debug_marker) ? 1.0f : 0.0f,
                  1.0f);
    params.shadow_view_projection = shadow_params.view_projection;
    params.shadow_params = shadow_params.params;
    params.irradiance_sh = irradiance_sh;

    if (void* mapped = scene_params_buffer->Map()) {
        std::memcpy(mapped, &params, sizeof(params));
        scene_params_buffer->Flush();
        scene_params_buffer->Unmap();
    } else {
        LUNA_RENDERER_WARN("Failed to map scene parameter buffer");
    }
}

} // namespace

void PipelineState::updateSceneParameters(const SceneRenderContext& context,
                                          const RenderWorld& world,
                                          const RenderFeatureFrameContext& frame_context,
                                          float environment_mip_count,
                                          const std::array<glm::vec4, 9>& irradiance_sh,
                                          const render_flow::default_scene_detail::ShadowRenderParams& shadow_params)
{
    const RenderDirectionalLight* directional_light =
        world.directionalLights().empty() ? nullptr : &world.directionalLights().front();
    updateSceneParameterBuffer(context,
                               world.camera(),
                               frame_context,
                               SceneLightView{
                                   .directional_light = directional_light,
                                   .point_lights = world.pointLights(),
                                   .spot_lights = world.spotLights(),
                               },
                               environment_mip_count,
                               irradiance_sh,
                               shadow_params,
                               m_state.scene_params_buffer);
}

void PipelineState::updateLightingResources(luna::RHI::CommandBufferEncoder& commands,
                                            const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_base_color,
                                            const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_normal_metallic,
                                            const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_world_position_roughness,
                                            const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_emissive_ao,
                                            const luna::RHI::Ref<luna::RHI::Texture>& velocity_texture,
                                            const luna::RHI::Ref<luna::RHI::Texture>& pick_texture,
                                            const luna::render_flow::LightingExtensionTextureRefs& lighting_extensions)
{
    renderer_detail::uploadTextureIfNeeded(commands, m_state.default_ambient_occlusion_texture);
    renderer_detail::uploadTextureIfNeeded(commands, m_state.default_reflection_texture);
    renderer_detail::uploadTextureIfNeeded(commands, m_state.default_indirect_diffuse_texture);
    renderer_detail::uploadTextureIfNeeded(commands, m_state.default_indirect_specular_texture);

    const auto& ambient_occlusion_texture =
        textureOrFallback(lighting_extensions.ambient_occlusion, m_state.default_ambient_occlusion_texture);
    const auto& reflection_texture =
        textureOrFallback(lighting_extensions.reflection, m_state.default_reflection_texture);
    const auto& indirect_diffuse_texture =
        textureOrFallback(lighting_extensions.indirect_diffuse, m_state.default_indirect_diffuse_texture);
    const auto& indirect_specular_texture =
        textureOrFallback(lighting_extensions.indirect_specular, m_state.default_indirect_specular_texture);

    if (!m_state.gbuffer_descriptor_set || !m_state.gbuffer_sampler || !gbuffer_base_color ||
        !gbuffer_normal_metallic || !gbuffer_world_position_roughness || !gbuffer_emissive_ao || !velocity_texture ||
        !pick_texture || !ambient_occlusion_texture || !reflection_texture || !indirect_diffuse_texture ||
        !indirect_specular_texture) {
        LUNA_RENDERER_WARN(
            "Cannot update lighting resources: gbuffer_descriptor_set={} gbuffer_sampler={} base={} normal={} "
            "position={} emissive={} velocity={} pick={} ao={} reflection={} indirect_diffuse={} indirect_specular={}",
            static_cast<bool>(m_state.gbuffer_descriptor_set),
            static_cast<bool>(m_state.gbuffer_sampler),
            static_cast<bool>(gbuffer_base_color),
            static_cast<bool>(gbuffer_normal_metallic),
            static_cast<bool>(gbuffer_world_position_roughness),
            static_cast<bool>(gbuffer_emissive_ao),
            static_cast<bool>(velocity_texture),
            static_cast<bool>(pick_texture),
            static_cast<bool>(ambient_occlusion_texture),
            static_cast<bool>(reflection_texture),
            static_cast<bool>(indirect_diffuse_texture),
            static_cast<bool>(indirect_specular_texture));
        return;
    }

    m_state.gbuffer_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = gbuffer_binding::BaseColor,
        .TextureView = gbuffer_base_color->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_state.gbuffer_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = gbuffer_binding::NormalMetallic,
        .TextureView = gbuffer_normal_metallic->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_state.gbuffer_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = gbuffer_binding::WorldPositionRoughness,
        .TextureView = gbuffer_world_position_roughness->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_state.gbuffer_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = gbuffer_binding::EmissiveAo,
        .TextureView = gbuffer_emissive_ao->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_state.gbuffer_descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
        .Binding = gbuffer_binding::Sampler,
        .Sampler = m_state.gbuffer_sampler,
    });
    m_state.gbuffer_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = gbuffer_binding::Pick,
        .TextureView = pick_texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_state.gbuffer_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = gbuffer_binding::AmbientOcclusion,
        .TextureView = ambient_occlusion_texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_state.gbuffer_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = gbuffer_binding::Reflection,
        .TextureView = reflection_texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_state.gbuffer_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = gbuffer_binding::IndirectDiffuse,
        .TextureView = indirect_diffuse_texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_state.gbuffer_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = gbuffer_binding::IndirectSpecular,
        .TextureView = indirect_specular_texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_state.gbuffer_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = gbuffer_binding::Velocity,
        .TextureView = velocity_texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_state.gbuffer_descriptor_set->Update();
}

void PipelineState::updateShadowResources(const luna::RHI::Ref<luna::RHI::Texture>& shadow_map)
{
    if (!m_state.lighting_scene_descriptor_set || !m_state.scene_bindings_valid || !shadow_map ||
        !m_state.shadow_sampler) {
        LUNA_RENDERER_WARN("Cannot update shadow resources: lighting_scene_descriptor_set={} scene_bindings_valid={} "
                           "shadow_map={} shadow_sampler={}",
                           static_cast<bool>(m_state.lighting_scene_descriptor_set),
                           m_state.scene_bindings_valid,
                           static_cast<bool>(shadow_map),
                           static_cast<bool>(m_state.shadow_sampler));
        return;
    }

    if (m_state.shadow_bindings_valid && m_state.bound_shadow_map_texture == shadow_map) {
        return;
    }

    m_state.lighting_scene_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = scene_binding::ShadowMap,
        .TextureView = shadow_map->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_state.lighting_scene_descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
        .Binding = scene_binding::ShadowSampler,
        .Sampler = m_state.shadow_sampler,
    });
    m_state.lighting_scene_descriptor_set->Update();

    m_state.bound_shadow_map_texture = shadow_map;
    m_state.shadow_bindings_valid = true;
}

const luna::RHI::Ref<luna::RHI::Device>& PipelineState::device() const noexcept
{
    return m_state.device;
}

const luna::RHI::Ref<luna::RHI::DescriptorPool>& PipelineState::descriptorPool() const noexcept
{
    return m_state.descriptor_pool;
}

const luna::RHI::Ref<luna::RHI::DescriptorSetLayout>& PipelineState::materialLayout() const noexcept
{
    return m_state.material_layout;
}

DrawPassResources PipelineState::geometryPassResources() const noexcept
{
    return DrawPassResources{
        .pipeline = m_state.geometry_pipeline,
        .scene_descriptor_set = m_state.scene_descriptor_set,
    };
}

DrawPassResources PipelineState::shadowPassResources() const noexcept
{
    return DrawPassResources{
        .pipeline = m_state.shadow_pipeline,
        .scene_descriptor_set = m_state.scene_descriptor_set,
    };
}

DrawPassResources PipelineState::transparentPassResources() const noexcept
{
    return DrawPassResources{
        .pipeline = m_state.transparent_pipeline,
        .scene_descriptor_set = m_state.scene_descriptor_set,
    };
}

LightingPassResources PipelineState::lightingPassResources() const noexcept
{
    return LightingPassResources{
        .pipeline = m_state.lighting_pipeline,
        .gbuffer_descriptor_set = m_state.gbuffer_descriptor_set,
        .scene_descriptor_set = m_state.lighting_scene_descriptor_set,
        .gbuffer_sampler = m_state.gbuffer_sampler,
    };
}

DebugViewPassResources PipelineState::debugViewPassResources() const noexcept
{
    return DebugViewPassResources{
        .pipeline = m_state.debug_view_pipeline,
        .gbuffer_descriptor_set = m_state.gbuffer_descriptor_set,
        .scene_descriptor_set = m_state.lighting_scene_descriptor_set,
        .gbuffer_sampler = m_state.gbuffer_sampler,
    };
}

void PipelineState::reset() noexcept
{
    m_state = {};
}

} // namespace luna::render_flow::default_scene
