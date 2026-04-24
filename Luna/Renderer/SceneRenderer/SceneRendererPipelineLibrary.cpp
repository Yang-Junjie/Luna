#include "Renderer/SceneRenderer/SceneRendererPipelineLibrary.h"

#include "Core/Log.h"
#include "Renderer/Mesh.h"
#include "Renderer/RendererUtilities.h"
#include "Renderer/SceneRenderer/SceneRendererSupport.h"

#include <Builders.h>
#include <DescriptorPool.h>
#include <DescriptorSet.h>
#include <DescriptorSetLayout.h>
#include <Device.h>
#include <Pipeline.h>
#include <PipelineLayout.h>
#include <Sampler.h>
#include <ShaderCompiler.h>

#include <array>
#include <cstring>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>
#include <span>

namespace luna::scene_renderer {

namespace {

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
            .AddBinding(5, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
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
                                     .SetName("SceneEnvironmentSourceSampler")
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
        .AddColorAttachmentDefault(false)
        .AddColorFormat(scene_renderer_detail::kGBufferBaseColorFormat)
        .AddColorFormat(scene_renderer_detail::kGBufferLightingFormat)
        .AddColorFormat(scene_renderer_detail::kGBufferLightingFormat)
        .AddColorFormat(scene_renderer_detail::kGBufferLightingFormat)
        .AddColorFormat(scene_renderer_detail::kScenePickingFormat)
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
        .AddColorAttachmentDefault(false)
        .AddColorFormat(color_format)
        .AddColorFormat(scene_renderer_detail::kScenePickingFormat)
        .SetDepthStencilFormat(luna::RHI::Format::D32_FLOAT)
        .SetLayout(layout);

    return device->CreateGraphicsPipeline(builder.Build());
}

} // namespace

void PipelineLibrary::shutdown()
{
    reset();
}

bool PipelineLibrary::hasCompleteState(const SceneRenderer::RenderContext& context) const noexcept
{
    return m_state.device == context.device && m_state.backend_type == context.backend_type &&
           m_state.surface_format == context.color_format && m_state.geometry_pipeline && m_state.lighting_pipeline &&
           m_state.transparent_pipeline && m_state.material_layout && m_state.descriptor_pool &&
           m_state.gbuffer_descriptor_set && m_state.scene_descriptor_set && m_state.scene_params_buffer &&
           m_state.gbuffer_sampler && m_state.environment_source_sampler;
}

void PipelineLibrary::rebuild(const SceneRenderer::RenderContext& context, const SceneRenderer::ShaderPaths& shader_paths)
{
    reset();
    m_state.device = context.device;
    m_state.backend_type = context.backend_type;

    if (!m_state.device || !context.compiler) {
        LUNA_RENDERER_WARN("Cannot rebuild scene renderer pipelines: device={} compiler={}",
                           static_cast<bool>(m_state.device),
                           static_cast<bool>(context.compiler));
        return;
    }

    LUNA_RENDERER_DEBUG("Loading scene renderer shaders: geometry_vs='{}' geometry_fs='{}' lighting_vs='{}' lighting_fs='{}'",
                        shader_paths.geometry_vertex_path.string(),
                        shader_paths.geometry_fragment_path.string(),
                        shader_paths.lighting_vertex_path.string(),
                        shader_paths.lighting_fragment_path.string());

    m_state.geometry_vertex_shader = scene_renderer_detail::loadShaderModule(
        m_state.device, context.compiler, shader_paths.geometry_vertex_path, "sceneGeometryVertexMain", luna::RHI::ShaderStage::Vertex);
    m_state.geometry_fragment_shader = scene_renderer_detail::loadShaderModule(
        m_state.device, context.compiler, shader_paths.geometry_fragment_path, "sceneGeometryFragmentMain", luna::RHI::ShaderStage::Fragment);
    m_state.lighting_vertex_shader = scene_renderer_detail::loadShaderModule(
        m_state.device, context.compiler, shader_paths.lighting_vertex_path, "sceneLightingVertexMain", luna::RHI::ShaderStage::Vertex);
    m_state.lighting_fragment_shader = scene_renderer_detail::loadShaderModule(
        m_state.device, context.compiler, shader_paths.lighting_fragment_path, "sceneLightingFragmentMain", luna::RHI::ShaderStage::Fragment);
    m_state.transparent_fragment_shader = scene_renderer_detail::loadShaderModule(
        m_state.device, context.compiler, shader_paths.geometry_fragment_path, "sceneTransparentFragmentMain", luna::RHI::ShaderStage::Fragment);

    if (!m_state.geometry_vertex_shader || !m_state.geometry_fragment_shader || !m_state.lighting_vertex_shader ||
        !m_state.lighting_fragment_shader || !m_state.transparent_fragment_shader) {
        LUNA_RENDERER_ERROR("Failed to load scene renderer shaders");
        return;
    }

    m_state.material_layout = createMaterialLayout(m_state.device);
    m_state.gbuffer_layout = createGBufferLayout(m_state.device);
    m_state.scene_layout = createSceneLayout(m_state.device);
    m_state.descriptor_pool = createDescriptorPool(m_state.device);
    m_state.gbuffer_sampler = createGBufferSampler(m_state.device);
    m_state.environment_source_sampler = createEnvironmentSampler(m_state.device);

    if (m_state.descriptor_pool && m_state.gbuffer_layout) {
        m_state.gbuffer_descriptor_set = m_state.descriptor_pool->AllocateDescriptorSet(m_state.gbuffer_layout);
    }
    if (m_state.descriptor_pool && m_state.scene_layout) {
        m_state.scene_descriptor_set = m_state.descriptor_pool->AllocateDescriptorSet(m_state.scene_layout);
    }
    if (m_state.device) {
        m_state.scene_params_buffer = m_state.device->CreateBuffer(luna::RHI::BufferBuilder()
                                                                       .SetSize(sizeof(scene_renderer_detail::SceneGpuParams))
                                                                       .SetUsage(luna::RHI::BufferUsageFlags::UniformBuffer)
                                                                       .SetMemoryUsage(luna::RHI::BufferMemoryUsage::CpuToGpu)
                                                                       .SetName("SceneParams")
                                                                       .Build());
    }

    const std::array<luna::RHI::Ref<luna::RHI::DescriptorSetLayout>, 2> geometry_set_layouts{
        m_state.material_layout,
        m_state.scene_layout,
    };
    const std::array<luna::RHI::Ref<luna::RHI::DescriptorSetLayout>, 2> lighting_set_layouts{
        m_state.gbuffer_layout,
        m_state.scene_layout,
    };

    m_state.geometry_pipeline_layout = createPipelineLayout(m_state.device, geometry_set_layouts, true);
    m_state.lighting_pipeline_layout = createPipelineLayout(m_state.device, lighting_set_layouts, false);
    m_state.transparent_pipeline_layout = createPipelineLayout(m_state.device, geometry_set_layouts, true);

    m_state.geometry_pipeline = createGeometryPipeline(
        m_state.device, m_state.geometry_pipeline_layout, m_state.geometry_vertex_shader, m_state.geometry_fragment_shader);
    m_state.lighting_pipeline = createLightingPipeline(
        m_state.device, m_state.lighting_pipeline_layout, context.color_format, m_state.lighting_vertex_shader, m_state.lighting_fragment_shader);
    m_state.transparent_pipeline = createTransparentPipeline(
        m_state.device, m_state.transparent_pipeline_layout, context.color_format, m_state.geometry_vertex_shader, m_state.transparent_fragment_shader);
    m_state.surface_format = context.color_format;

    LUNA_RENDERER_INFO("Created scene renderer graphics pipelines for color format {} ({})",
                       renderer_detail::formatToString(context.color_format),
                       static_cast<int>(context.color_format));
}

void PipelineLibrary::updateSceneBindings(const luna::RHI::Ref<luna::RHI::Texture>& environment_texture)
{
    if (!m_state.scene_descriptor_set || !m_state.scene_params_buffer || !environment_texture || !m_state.environment_source_sampler) {
        LUNA_RENDERER_WARN("Scene resources are incomplete: scene_descriptor_set={} scene_params_buffer={} environment_texture={} environment_sampler={}",
                           static_cast<bool>(m_state.scene_descriptor_set),
                           static_cast<bool>(m_state.scene_params_buffer),
                           static_cast<bool>(environment_texture),
                           static_cast<bool>(m_state.environment_source_sampler));
        return;
    }

    m_state.scene_descriptor_set->WriteBuffer(luna::RHI::BufferWriteInfo{
        .Binding = 0,
        .Buffer = m_state.scene_params_buffer,
        .Offset = 0,
        .Stride = sizeof(scene_renderer_detail::SceneGpuParams),
        .Size = sizeof(scene_renderer_detail::SceneGpuParams),
        .Type = luna::RHI::DescriptorType::UniformBuffer,
    });
    m_state.scene_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 1,
        .TextureView = environment_texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_state.scene_descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
        .Binding = 2,
        .Sampler = m_state.environment_source_sampler,
    });
    m_state.scene_descriptor_set->Update();
}

void PipelineLibrary::updateSceneParameters(const SceneRenderer::RenderContext& context,
                                            const Camera& camera,
                                            const DrawQueue& draw_queue,
                                            float environment_mip_count,
                                            const std::array<glm::vec4, 9>& irradiance_sh)
{
    if (!m_state.scene_params_buffer || context.framebuffer_width == 0 || context.framebuffer_height == 0) {
        LUNA_RENDERER_WARN("Cannot update scene parameters: scene_params_buffer={} framebuffer={}x{}",
                           static_cast<bool>(m_state.scene_params_buffer),
                           context.framebuffer_width,
                           context.framebuffer_height);
        return;
    }

    const float aspect_ratio =
        static_cast<float>(context.framebuffer_width) / static_cast<float>(context.framebuffer_height);
    const glm::mat4 view_projection =
        scene_renderer_detail::buildViewProjection(camera, aspect_ratio, context.backend_type);

    scene_renderer_detail::SceneGpuParams params;
    params.view_projection = view_projection;
    params.inverse_view_projection = glm::inverse(view_projection);
    params.camera_position_env_mip = glm::vec4(scene_renderer_detail::resolveCameraPosition(camera), environment_mip_count);
    if (draw_queue.directionalLight().has_value() && draw_queue.directionalLight()->intensity > 0.0f) {
        const auto& directional_light = *draw_queue.directionalLight();
        params.light_direction_intensity = glm::vec4(glm::normalize(directional_light.direction), directional_light.intensity);
        params.light_color_exposure = glm::vec4(directional_light.color, 1.0f);
    } else {
        params.light_direction_intensity = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
        params.light_color_exposure = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    const uint32_t point_light_count =
        (std::min)(static_cast<uint32_t>(draw_queue.pointLights().size()), scene_renderer_detail::kMaxPointLights);
    const uint32_t spot_light_count =
        (std::min)(static_cast<uint32_t>(draw_queue.spotLights().size()), scene_renderer_detail::kMaxSpotLights);
    params.light_counts = glm::vec4(static_cast<float>(point_light_count), static_cast<float>(spot_light_count), 0.0f, 0.0f);

    for (uint32_t light_index = 0; light_index < point_light_count; ++light_index) {
        const auto& light = draw_queue.pointLights()[light_index];
        params.point_light_position_intensity[light_index] = glm::vec4(light.position, light.intensity);
        params.point_light_color_range[light_index] = glm::vec4(light.color, light.range);
    }

    for (uint32_t light_index = 0; light_index < spot_light_count; ++light_index) {
        const auto& light = draw_queue.spotLights()[light_index];
        params.spot_light_position_intensity[light_index] = glm::vec4(light.position, light.intensity);
        params.spot_light_direction_range[light_index] = glm::vec4(light.direction, light.range);
        params.spot_light_color_cones[light_index] = glm::vec4(light.color, 0.0f);
        params.spot_light_cone_params[light_index] = glm::vec4(light.innerConeCos, light.outerConeCos, 0.0f, 0.0f);
    }
    params.ibl_factors = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
    params.debug_overlay_params = glm::vec4(context.show_pick_debug_visualization ? 1.0f : 0.0f, 0.65f, 0.0f, 0.0f);
    params.debug_pick_marker = glm::vec4(static_cast<float>(context.debug_pick_pixel_x),
                                         static_cast<float>(context.debug_pick_pixel_y),
                                         (context.show_pick_debug_visualization && context.show_pick_debug_marker) ? 1.0f : 0.0f,
                                         1.0f);
    params.irradiance_sh = irradiance_sh;

    if (void* mapped = m_state.scene_params_buffer->Map()) {
        std::memcpy(mapped, &params, sizeof(params));
        m_state.scene_params_buffer->Flush();
        m_state.scene_params_buffer->Unmap();
    } else {
        LUNA_RENDERER_WARN("Failed to map scene parameter buffer");
    }
}

void PipelineLibrary::updateLightingResources(const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_base_color,
                                              const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_normal_metallic,
                                              const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_world_position_roughness,
                                              const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_emissive_ao,
                                              const luna::RHI::Ref<luna::RHI::Texture>& pick_texture)
{
    if (!m_state.gbuffer_descriptor_set || !m_state.gbuffer_sampler || !gbuffer_base_color || !gbuffer_normal_metallic ||
        !gbuffer_world_position_roughness || !gbuffer_emissive_ao || !pick_texture) {
        LUNA_RENDERER_WARN("Cannot update lighting resources: gbuffer_descriptor_set={} gbuffer_sampler={} base={} normal={} position={} emissive={} pick={}",
                           static_cast<bool>(m_state.gbuffer_descriptor_set),
                           static_cast<bool>(m_state.gbuffer_sampler),
                           static_cast<bool>(gbuffer_base_color),
                           static_cast<bool>(gbuffer_normal_metallic),
                           static_cast<bool>(gbuffer_world_position_roughness),
                           static_cast<bool>(gbuffer_emissive_ao),
                           static_cast<bool>(pick_texture));
        return;
    }

    m_state.gbuffer_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 0,
        .TextureView = gbuffer_base_color->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_state.gbuffer_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 1,
        .TextureView = gbuffer_normal_metallic->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_state.gbuffer_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 2,
        .TextureView = gbuffer_world_position_roughness->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_state.gbuffer_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 3,
        .TextureView = gbuffer_emissive_ao->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_state.gbuffer_descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
        .Binding = 4,
        .Sampler = m_state.gbuffer_sampler,
    });
    m_state.gbuffer_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 5,
        .TextureView = pick_texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_state.gbuffer_descriptor_set->Update();
}

const luna::RHI::Ref<luna::RHI::Device>& PipelineLibrary::device() const noexcept
{
    return m_state.device;
}

const luna::RHI::Ref<luna::RHI::DescriptorPool>& PipelineLibrary::descriptorPool() const noexcept
{
    return m_state.descriptor_pool;
}

const luna::RHI::Ref<luna::RHI::DescriptorSetLayout>& PipelineLibrary::materialLayout() const noexcept
{
    return m_state.material_layout;
}

const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& PipelineLibrary::geometryPipeline() const noexcept
{
    return m_state.geometry_pipeline;
}

const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& PipelineLibrary::lightingPipeline() const noexcept
{
    return m_state.lighting_pipeline;
}

const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& PipelineLibrary::transparentPipeline() const noexcept
{
    return m_state.transparent_pipeline;
}

const luna::RHI::Ref<luna::RHI::DescriptorSet>& PipelineLibrary::sceneDescriptorSet() const noexcept
{
    return m_state.scene_descriptor_set;
}

const luna::RHI::Ref<luna::RHI::DescriptorSet>& PipelineLibrary::gbufferDescriptorSet() const noexcept
{
    return m_state.gbuffer_descriptor_set;
}

const luna::RHI::Ref<luna::RHI::Sampler>& PipelineLibrary::gbufferSampler() const noexcept
{
    return m_state.gbuffer_sampler;
}

void PipelineLibrary::reset() noexcept
{
    m_state = {};
}

} // namespace luna::scene_renderer
