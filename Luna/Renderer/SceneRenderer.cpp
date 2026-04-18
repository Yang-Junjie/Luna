#include "Core/Log.h"
#include "Renderer/SceneRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

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
#include <glm/gtx/norm.hpp>
#include <Pipeline.h>
#include <PipelineLayout.h>
#include <Sampler.h>
#include <ShaderCompiler.h>
#include <ShaderModule.h>
#include <string_view>
#include <Texture.h>

namespace {

constexpr luna::RHI::Format kGBufferBaseColorFormat = luna::RHI::Format::RGBA8_UNORM;
constexpr luna::RHI::Format kGBufferLightingFormat = luna::RHI::Format::RGBA16_FLOAT;
constexpr luna::RHI::Format kEnvironmentFormat = luna::RHI::Format::RGBA32_FLOAT;
constexpr float kDefaultMaterialAlphaCutoff = 0.5f;
constexpr float kEnvironmentFallbackValue = 0.08f;

struct MeshPushConstants {
    glm::mat4 model{1.0f};
};

struct SceneGpuParams {
    glm::mat4 view_projection{1.0f};
    glm::mat4 inverse_view_projection{1.0f};
    glm::vec4 camera_position_env_mip{0.0f, 0.0f, 0.0f, 0.0f};
    glm::vec4 light_direction_intensity{0.45f, 0.80f, 0.35f, 4.0f};
    glm::vec4 light_color_exposure{1.0f, 0.98f, 0.95f, 1.0f};
    glm::vec4 ibl_factors{1.0f, 1.0f, 1.0f, 0.0f};
};

struct MaterialGpuParams {
    glm::vec4 base_color_factor{1.0f};
    glm::vec4 emissive_factor_normal_scale{0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 material_factors{0.0f, 1.0f, 1.0f, kDefaultMaterialAlphaCutoff};
    glm::vec4 material_flags{0.0f};
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

float materialBlendModeToFloat(luna::Material::BlendMode blend_mode)
{
    switch (blend_mode) {
        case luna::Material::BlendMode::Masked:
            return 1.0f;
        case luna::Material::BlendMode::Transparent:
            return 2.0f;
        case luna::Material::BlendMode::Opaque:
        default:
            return 0.0f;
    }
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

float transparentSortDistanceSq(const glm::mat4& transform, const Camera& camera)
{
    const glm::vec3 object_position(transform[3]);
    return glm::length2(object_position - camera.m_position);
}

luna::rhi::ImageData createFallbackFloatImageData(const glm::vec4& value)
{
    std::vector<uint8_t> bytes(sizeof(float) * 4, 0);
    std::memcpy(bytes.data(), &value[0], bytes.size());
    return luna::rhi::ImageData{
        .ByteData = std::move(bytes),
        .ImageFormat = kEnvironmentFormat,
        .Width = 1,
        .Height = 1,
    };
}

luna::rhi::ImageData generateEnvironmentMipChain(const luna::rhi::ImageData& source)
{
    if (!source.isValid() || source.ImageFormat != kEnvironmentFormat || source.ByteData.size() !=
                                                                      static_cast<size_t>(source.Width) *
                                                                          static_cast<size_t>(source.Height) * 4 *
                                                                          sizeof(float)) {
        return source;
    }

    luna::rhi::ImageData result = source;
    result.MipLevels.clear();

    uint32_t previous_width = source.Width;
    uint32_t previous_height = source.Height;
    std::vector<float> previous_level(source.ByteData.size() / sizeof(float), 0.0f);
    std::memcpy(previous_level.data(), source.ByteData.data(), source.ByteData.size());

    while (previous_width > 1 || previous_height > 1) {
        const uint32_t next_width = (std::max)(previous_width / 2, 1u);
        const uint32_t next_height = (std::max)(previous_height / 2, 1u);
        std::vector<float> next_level(static_cast<size_t>(next_width) * static_cast<size_t>(next_height) * 4u, 0.0f);

        for (uint32_t y = 0; y < next_height; ++y) {
            for (uint32_t x = 0; x < next_width; ++x) {
                glm::vec4 sum(0.0f);
                for (uint32_t sample_y = 0; sample_y < 2; ++sample_y) {
                    for (uint32_t sample_x = 0; sample_x < 2; ++sample_x) {
                        const uint32_t source_x = (std::min)(previous_width - 1, x * 2 + sample_x);
                        const uint32_t source_y = (std::min)(previous_height - 1, y * 2 + sample_y);
                        const size_t source_index =
                            (static_cast<size_t>(source_y) * previous_width + source_x) * static_cast<size_t>(4);
                        sum += glm::vec4(previous_level[source_index + 0],
                                         previous_level[source_index + 1],
                                         previous_level[source_index + 2],
                                         previous_level[source_index + 3]);
                    }
                }

                const glm::vec4 averaged = sum * 0.25f;
                const size_t dest_index = (static_cast<size_t>(y) * next_width + x) * static_cast<size_t>(4);
                next_level[dest_index + 0] = averaged.x;
                next_level[dest_index + 1] = averaged.y;
                next_level[dest_index + 2] = averaged.z;
                next_level[dest_index + 3] = averaged.w;
            }
        }

        auto& mip_bytes = result.MipLevels.emplace_back(next_level.size() * sizeof(float), uint8_t{0});
        std::memcpy(mip_bytes.data(), next_level.data(), mip_bytes.size());

        previous_level = std::move(next_level);
        previous_width = next_width;
        previous_height = next_height;
    }

    return result;
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
    resetPipelineState();
}

void SceneRenderer::shutdown()
{
    clearSubmittedMeshes();
    m_uploaded_materials.clear();
    m_uploaded_meshes.clear();
    resetPipelineState();
    m_device.reset();
}

void SceneRenderer::beginScene(const Camera& camera)
{
    m_camera = camera;
    clearSubmittedMeshes();
}

void SceneRenderer::clearSubmittedMeshes()
{
    m_opaque_draw_commands.clear();
    m_transparent_draw_commands.clear();
}

void SceneRenderer::submitStaticMesh(const glm::mat4& transform,
                                     std::shared_ptr<Mesh> mesh,
                                     std::shared_ptr<Material> material)
{
    if (!mesh || !mesh->isValid()) {
        return;
    }

    StaticMeshDrawCommand draw_command{
        .transform = transform,
        .mesh = std::move(mesh),
        .material = std::move(material),
    };

    if (draw_command.material != nullptr && draw_command.material->isTransparent()) {
        m_transparent_draw_commands.push_back(std::move(draw_command));
        return;
    }

    m_opaque_draw_commands.push_back(std::move(draw_command));
}

void SceneRenderer::buildRenderGraph(rhi::RenderGraphBuilder& graph, const RenderContext& context)
{
    if (!context.device || !context.color_target.isValid() || !context.depth_target.isValid() ||
        context.framebuffer_width == 0 || context.framebuffer_height == 0) {
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

    if (!m_transparent_draw_commands.empty()) {
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
    const std::filesystem::path shader_root = projectRoot() / "Luna" / "Renderer" / "Shaders";
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
    return projectRoot() / "Assets" / "hdr" / "newport_loft.hdr";
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

void SceneRenderer::resetPipelineState()
{
    m_uploaded_materials.clear();
    m_geometry_pipeline.reset();
    m_lighting_pipeline.reset();
    m_transparent_pipeline.reset();
    m_geometry_pipeline_layout.reset();
    m_lighting_pipeline_layout.reset();
    m_transparent_pipeline_layout.reset();
    m_material_layout.reset();
    m_gbuffer_layout.reset();
    m_scene_layout.reset();
    m_descriptor_pool.reset();
    m_material_sampler.reset();
    m_gbuffer_sampler.reset();
    m_environment_sampler.reset();
    m_gbuffer_descriptor_set.reset();
    m_scene_descriptor_set.reset();
    m_scene_params_buffer.reset();
    m_environment_texture = {};
    m_geometry_vertex_shader.reset();
    m_geometry_fragment_shader.reset();
    m_lighting_vertex_shader.reset();
    m_lighting_fragment_shader.reset();
    m_transparent_fragment_shader.reset();
    m_surface_format = luna::RHI::Format::UNDEFINED;
}

void SceneRenderer::ensurePipelines(const RenderContext& context)
{
    if (!context.device || !context.compiler) {
        return;
    }

    if (m_device != context.device) {
        m_uploaded_materials.clear();
        m_uploaded_meshes.clear();
        resetPipelineState();
        m_device = context.device;
    }

    if (m_geometry_pipeline && m_lighting_pipeline && m_transparent_pipeline &&
        m_surface_format == context.color_format && m_gbuffer_descriptor_set && m_scene_descriptor_set &&
        m_scene_params_buffer && m_environment_texture.texture) {
        return;
    }

    const ShaderPaths shader_paths = resolveShaderPaths();
    m_geometry_vertex_shader = loadShaderModule(
        m_device, context.compiler, shader_paths.geometry_vertex_path, "sceneGeometryVertexMain", luna::RHI::ShaderStage::Vertex);
    m_geometry_fragment_shader =
        loadShaderModule(m_device,
                         context.compiler,
                         shader_paths.geometry_fragment_path,
                         "sceneGeometryFragmentMain",
                         luna::RHI::ShaderStage::Fragment);
    m_lighting_vertex_shader = loadShaderModule(
        m_device, context.compiler, shader_paths.lighting_vertex_path, "sceneLightingVertexMain", luna::RHI::ShaderStage::Vertex);
    m_lighting_fragment_shader =
        loadShaderModule(m_device,
                         context.compiler,
                         shader_paths.lighting_fragment_path,
                         "sceneLightingFragmentMain",
                         luna::RHI::ShaderStage::Fragment);
    m_transparent_fragment_shader =
        loadShaderModule(m_device,
                         context.compiler,
                         shader_paths.geometry_fragment_path,
                         "sceneTransparentFragmentMain",
                         luna::RHI::ShaderStage::Fragment);

    if (!m_geometry_vertex_shader || !m_geometry_fragment_shader || !m_lighting_vertex_shader ||
        !m_lighting_fragment_shader || !m_transparent_fragment_shader) {
        LUNA_RENDERER_ERROR("Failed to load scene renderer shaders");
        return;
    }

    m_uploaded_materials.clear();

    m_material_layout = m_device->CreateDescriptorSetLayout(
        luna::RHI::DescriptorSetLayoutBuilder()
            .AddBinding(0, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(1, luna::RHI::DescriptorType::Sampler, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(2, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(3, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(4, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(5, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(6, luna::RHI::DescriptorType::UniformBuffer, 1, luna::RHI::ShaderStage::Fragment)
            .Build());
    m_gbuffer_layout = m_device->CreateDescriptorSetLayout(
        luna::RHI::DescriptorSetLayoutBuilder()
            .AddBinding(0, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(1, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(2, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(3, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(4, luna::RHI::DescriptorType::Sampler, 1, luna::RHI::ShaderStage::Fragment)
            .Build());
    m_scene_layout = m_device->CreateDescriptorSetLayout(
        luna::RHI::DescriptorSetLayoutBuilder()
            .AddBinding(0,
                        luna::RHI::DescriptorType::UniformBuffer,
                        1,
                        luna::RHI::ShaderStage::Vertex | luna::RHI::ShaderStage::Fragment)
            .AddBinding(1, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
            .AddBinding(2, luna::RHI::DescriptorType::Sampler, 1, luna::RHI::ShaderStage::Fragment)
            .Build());

    m_descriptor_pool =
        m_device->CreateDescriptorPool(luna::RHI::DescriptorPoolBuilder()
                                           .SetMaxSets(4'096)
                                           .AddPoolSize(luna::RHI::DescriptorType::SampledImage, 16'384)
                                           .AddPoolSize(luna::RHI::DescriptorType::Sampler, 8'192)
                                           .AddPoolSize(luna::RHI::DescriptorType::UniformBuffer, 8'192)
                                           .Build());

    m_material_sampler = m_device->CreateSampler(luna::RHI::SamplerBuilder()
                                                     .SetFilter(luna::RHI::Filter::Linear, luna::RHI::Filter::Linear)
                                                     .SetMipmapMode(luna::RHI::SamplerMipmapMode::Linear)
                                                     .SetAddressMode(luna::RHI::SamplerAddressMode::Repeat)
                                                     .SetAnisotropy(false)
                                                     .SetName("SceneMaterialSampler")
                                                     .Build());
    m_gbuffer_sampler = m_device->CreateSampler(luna::RHI::SamplerBuilder()
                                                    .SetFilter(luna::RHI::Filter::Linear, luna::RHI::Filter::Linear)
                                                    .SetAddressMode(luna::RHI::SamplerAddressMode::ClampToEdge)
                                                    .SetMipmapMode(luna::RHI::SamplerMipmapMode::Linear)
                                                    .SetAnisotropy(false)
                                                    .SetName("SceneGBufferSampler")
                                                    .Build());
    m_environment_sampler = m_device->CreateSampler(luna::RHI::SamplerBuilder()
                                                        .SetFilter(luna::RHI::Filter::Linear, luna::RHI::Filter::Linear)
                                                        .SetMipmapMode(luna::RHI::SamplerMipmapMode::Linear)
                                                        .SetAddressModeU(luna::RHI::SamplerAddressMode::Repeat)
                                                        .SetAddressModeV(luna::RHI::SamplerAddressMode::ClampToEdge)
                                                        .SetAddressModeW(luna::RHI::SamplerAddressMode::ClampToEdge)
                                                        .SetLodRange(0.0f, 16.0f)
                                                        .SetAnisotropy(false)
                                                        .SetName("SceneEnvironmentSampler")
                                                        .Build());

    if (m_descriptor_pool && m_gbuffer_layout) {
        m_gbuffer_descriptor_set = m_descriptor_pool->AllocateDescriptorSet(m_gbuffer_layout);
    }

    ensureSceneResources();

    m_geometry_pipeline_layout =
        m_device->CreatePipelineLayout(luna::RHI::PipelineLayoutBuilder()
                                           .AddSetLayout(m_material_layout)
                                           .AddSetLayout(m_scene_layout)
                                           .AddPushConstant(luna::RHI::ShaderStage::Vertex, 0, sizeof(MeshPushConstants))
                                           .Build());
    m_lighting_pipeline_layout =
        m_device->CreatePipelineLayout(
            luna::RHI::PipelineLayoutBuilder().AddSetLayout(m_gbuffer_layout).AddSetLayout(m_scene_layout).Build());
    m_transparent_pipeline_layout =
        m_device->CreatePipelineLayout(luna::RHI::PipelineLayoutBuilder()
                                           .AddSetLayout(m_material_layout)
                                           .AddSetLayout(m_scene_layout)
                                           .AddPushConstant(luna::RHI::ShaderStage::Vertex, 0, sizeof(MeshPushConstants))
                                           .Build());

    m_geometry_pipeline = m_device->CreateGraphicsPipeline(
        luna::RHI::GraphicsPipelineBuilder()
            .SetShaders({m_geometry_vertex_shader, m_geometry_fragment_shader})
            .AddVertexBinding(0, sizeof(StaticMeshVertex), luna::RHI::VertexInputRate::Vertex)
            .AddVertexAttribute(0, 0, luna::RHI::Format::RGB32_FLOAT, offsetof(StaticMeshVertex, position), "POSITION")
            .AddVertexAttribute(1, 0, luna::RHI::Format::RG32_FLOAT, offsetof(StaticMeshVertex, uv), "TEXCOORD")
            .AddVertexAttribute(2, 0, luna::RHI::Format::RGB32_FLOAT, offsetof(StaticMeshVertex, normal), "NORMAL")
            .AddVertexAttribute(3, 0, luna::RHI::Format::RGB32_FLOAT, offsetof(StaticMeshVertex, tangent), "TANGENT")
            .AddVertexAttribute(
                4, 0, luna::RHI::Format::RGB32_FLOAT, offsetof(StaticMeshVertex, bitangent), "BINORMAL")
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
            .SetLayout(m_geometry_pipeline_layout)
            .Build());

    m_lighting_pipeline = m_device->CreateGraphicsPipeline(
        luna::RHI::GraphicsPipelineBuilder()
            .SetShaders({m_lighting_vertex_shader, m_lighting_fragment_shader})
            .SetTopology(luna::RHI::PrimitiveTopology::TriangleList)
            .SetCullMode(luna::RHI::CullMode::None)
            .SetFrontFace(luna::RHI::FrontFace::CounterClockwise)
            .SetDepthTest(false, false, luna::RHI::CompareOp::Always)
            .AddColorAttachmentDefault(false)
            .AddColorFormat(context.color_format)
            .SetLayout(m_lighting_pipeline_layout)
            .Build());

    m_transparent_pipeline = m_device->CreateGraphicsPipeline(
        luna::RHI::GraphicsPipelineBuilder()
            .SetShaders({m_geometry_vertex_shader, m_transparent_fragment_shader})
            .AddVertexBinding(0, sizeof(StaticMeshVertex), luna::RHI::VertexInputRate::Vertex)
            .AddVertexAttribute(0, 0, luna::RHI::Format::RGB32_FLOAT, offsetof(StaticMeshVertex, position), "POSITION")
            .AddVertexAttribute(1, 0, luna::RHI::Format::RG32_FLOAT, offsetof(StaticMeshVertex, uv), "TEXCOORD")
            .AddVertexAttribute(2, 0, luna::RHI::Format::RGB32_FLOAT, offsetof(StaticMeshVertex, normal), "NORMAL")
            .AddVertexAttribute(3, 0, luna::RHI::Format::RGB32_FLOAT, offsetof(StaticMeshVertex, tangent), "TANGENT")
            .AddVertexAttribute(
                4, 0, luna::RHI::Format::RGB32_FLOAT, offsetof(StaticMeshVertex, bitangent), "BINORMAL")
            .SetTopology(luna::RHI::PrimitiveTopology::TriangleList)
            .SetCullMode(luna::RHI::CullMode::None)
            .SetFrontFace(luna::RHI::FrontFace::CounterClockwise)
            .SetDepthTest(true, false, luna::RHI::CompareOp::Less)
            .AddColorAttachment(makeAlphaBlendAttachment())
            .AddColorFormat(context.color_format)
            .SetDepthStencilFormat(luna::RHI::Format::D32_FLOAT)
            .SetLayout(m_transparent_pipeline_layout)
            .Build());

    m_surface_format = context.color_format;
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

    if (uploaded_mesh.vertex_buffer) {
        if (void* vertex_memory = uploaded_mesh.vertex_buffer->Map()) {
            std::memcpy(vertex_memory, mesh.getVertices().data(), mesh.getVertices().size() * sizeof(StaticMeshVertex));
            uploaded_mesh.vertex_buffer->Flush();
            uploaded_mesh.vertex_buffer->Unmap();
        }
    }

    if (uploaded_mesh.index_buffer) {
        if (void* index_memory = uploaded_mesh.index_buffer->Map()) {
            std::memcpy(index_memory, mesh.getIndices().data(), mesh.getIndices().size() * sizeof(uint32_t));
            uploaded_mesh.index_buffer->Flush();
            uploaded_mesh.index_buffer->Unmap();
        }
    }

    return uploaded_mesh;
}

SceneRenderer::UploadedTexture SceneRenderer::createUploadedTexture(const rhi::ImageData& image,
                                                                   const std::string& debug_name) const
{
    UploadedTexture uploaded_texture;
    if (!m_device || !image.isValid()) {
        return uploaded_texture;
    }

    const uint32_t mip_level_count = 1u + static_cast<uint32_t>(image.MipLevels.size());
    uploaded_texture.texture = m_device->CreateTexture(luna::RHI::TextureBuilder()
                                                           .SetSize(image.Width, image.Height)
                                                           .SetMipLevels(mip_level_count)
                                                           .SetFormat(image.ImageFormat)
                                                           .SetUsage(luna::RHI::TextureUsageFlags::Sampled |
                                                                     luna::RHI::TextureUsageFlags::TransferDst)
                                                           .SetInitialState(luna::RHI::ResourceState::Undefined)
                                                           .SetName(debug_name)
                                                           .Build());

    size_t total_size = image.ByteData.size();
    for (const auto& mip_level : image.MipLevels) {
        total_size += mip_level.size();
    }

    if (!uploaded_texture.texture || total_size == 0) {
        return uploaded_texture;
    }

    uploaded_texture.staging_buffer = m_device->CreateBuffer(luna::RHI::BufferBuilder()
                                                                 .SetSize(total_size)
                                                                 .SetUsage(luna::RHI::BufferUsageFlags::TransferSrc)
                                                                 .SetMemoryUsage(luna::RHI::BufferMemoryUsage::CpuToGpu)
                                                                 .SetName(debug_name + "_Staging")
                                                                 .Build());

    if (!uploaded_texture.staging_buffer) {
        return uploaded_texture;
    }

    size_t buffer_offset = 0;
    uint32_t mip_width = image.Width;
    uint32_t mip_height = image.Height;
    uploaded_texture.copy_regions.reserve(mip_level_count);

    auto append_region = [&](const std::vector<uint8_t>& bytes, uint32_t mip_level) {
        uploaded_texture.copy_regions.push_back(luna::RHI::BufferImageCopy{
            .BufferOffset = buffer_offset,
            .BufferRowLength = 0,
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
        buffer_offset += bytes.size();
    };

    append_region(image.ByteData, 0);
    for (uint32_t mip_level = 1; mip_level < mip_level_count; ++mip_level) {
        mip_width = (std::max)(mip_width / 2, 1u);
        mip_height = (std::max)(mip_height / 2, 1u);
        append_region(image.MipLevels[mip_level - 1], mip_level);
    }

    if (void* mapped = uploaded_texture.staging_buffer->Map()) {
        uint8_t* destination = static_cast<uint8_t*>(mapped);
        size_t write_offset = 0;
        std::memcpy(destination + write_offset, image.ByteData.data(), image.ByteData.size());
        write_offset += image.ByteData.size();
        for (const auto& mip_level : image.MipLevels) {
            std::memcpy(destination + write_offset, mip_level.data(), mip_level.size());
            write_offset += mip_level.size();
        }
        uploaded_texture.staging_buffer->Flush();
        uploaded_texture.staging_buffer->Unmap();
    }

    return uploaded_texture;
}

SceneRenderer::UploadedMaterial& SceneRenderer::getOrCreateUploadedMaterial(const Material& material)
{
    const auto it = m_uploaded_materials.find(&material);
    if (it != m_uploaded_materials.end()) {
        return it->second;
    }

    const rhi::ImageData base_color_image = material.hasBaseColorTexture()
                                                ? material.getBaseColorImageData()
                                                : createFallbackImageData(glm::vec4(1.0f));
    const rhi::ImageData normal_image = material.hasNormalTexture()
                                            ? material.getNormalImageData()
                                            : createFallbackImageData(glm::vec4(0.5f, 0.5f, 1.0f, 1.0f));
    const rhi::ImageData metallic_roughness_image = material.hasMetallicRoughnessTexture()
                                                        ? material.getMetallicRoughnessImageData()
                                                        : createFallbackImageData(glm::vec4(1.0f));
    const rhi::ImageData emissive_image = material.hasEmissiveTexture()
                                              ? material.getEmissiveImageData()
                                              : createFallbackImageData(glm::vec4(1.0f));
    const rhi::ImageData occlusion_image = material.hasOcclusionTexture()
                                               ? material.getOcclusionImageData()
                                               : createFallbackImageData(glm::vec4(1.0f));

    auto [inserted_it, _] = m_uploaded_materials.emplace(&material, UploadedMaterial{});
    auto& uploaded_material = inserted_it->second;

    uploaded_material.base_color_texture = createUploadedTexture(base_color_image, material.getName() + "_BaseColor");
    uploaded_material.normal_texture = createUploadedTexture(normal_image, material.getName() + "_Normal");
    uploaded_material.metallic_roughness_texture =
        createUploadedTexture(metallic_roughness_image, material.getName() + "_MetallicRoughness");
    uploaded_material.emissive_texture = createUploadedTexture(emissive_image, material.getName() + "_Emissive");
    uploaded_material.occlusion_texture = createUploadedTexture(occlusion_image, material.getName() + "_Occlusion");

    uploaded_material.params_buffer = m_device->CreateBuffer(luna::RHI::BufferBuilder()
                                                                 .SetSize(sizeof(MaterialGpuParams))
                                                                 .SetUsage(luna::RHI::BufferUsageFlags::UniformBuffer)
                                                                 .SetMemoryUsage(luna::RHI::BufferMemoryUsage::CpuToGpu)
                                                                 .SetName(material.getName() + "_Params")
                                                                 .Build());

    if (uploaded_material.params_buffer) {
        if (void* mapped = uploaded_material.params_buffer->Map()) {
            const MaterialGpuParams params{
                .base_color_factor = material.getBaseColorFactor(),
                .emissive_factor_normal_scale =
                    glm::vec4(material.getEmissiveFactor(), material.getNormalScale()),
                .material_factors = glm::vec4(material.getMetallicFactor(),
                                              material.getRoughnessFactor(),
                                              material.getOcclusionStrength(),
                                              material.getAlphaCutoff()),
                .material_flags = glm::vec4(materialBlendModeToFloat(material.getBlendMode()),
                                            material.isUnlit() ? 1.0f : 0.0f,
                                            material.isDoubleSided() ? 1.0f : 0.0f,
                                            0.0f),
            };
            std::memcpy(mapped, &params, sizeof(params));
            uploaded_material.params_buffer->Flush();
            uploaded_material.params_buffer->Unmap();
        }
    }

    if (!m_descriptor_pool || !m_material_layout || !m_material_sampler || !uploaded_material.base_color_texture.texture ||
        !uploaded_material.normal_texture.texture || !uploaded_material.metallic_roughness_texture.texture ||
        !uploaded_material.emissive_texture.texture || !uploaded_material.occlusion_texture.texture ||
        !uploaded_material.params_buffer) {
        return uploaded_material;
    }

    uploaded_material.descriptor_set = m_descriptor_pool->AllocateDescriptorSet(m_material_layout);
    if (!uploaded_material.descriptor_set) {
        return uploaded_material;
    }

    uploaded_material.descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 0,
        .TextureView = uploaded_material.base_color_texture.texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    uploaded_material.descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
        .Binding = 1,
        .Sampler = m_material_sampler,
    });
    uploaded_material.descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 2,
        .TextureView = uploaded_material.normal_texture.texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    uploaded_material.descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 3,
        .TextureView = uploaded_material.metallic_roughness_texture.texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    uploaded_material.descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 4,
        .TextureView = uploaded_material.emissive_texture.texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    uploaded_material.descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 5,
        .TextureView = uploaded_material.occlusion_texture.texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    uploaded_material.descriptor_set->WriteBuffer(luna::RHI::BufferWriteInfo{
        .Binding = 6,
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

void SceneRenderer::uploadMaterialIfNeeded(luna::RHI::CommandBufferEncoder& commands, UploadedMaterial& uploaded_material)
{
    uploadTextureIfNeeded(commands, uploaded_material.base_color_texture);
    uploadTextureIfNeeded(commands, uploaded_material.normal_texture);
    uploadTextureIfNeeded(commands, uploaded_material.metallic_roughness_texture);
    uploadTextureIfNeeded(commands, uploaded_material.emissive_texture);
    uploadTextureIfNeeded(commands, uploaded_material.occlusion_texture);
}

void SceneRenderer::ensureSceneResources()
{
    if (!m_device || !m_descriptor_pool || !m_scene_layout || !m_environment_sampler) {
        return;
    }

    if (!m_scene_params_buffer) {
        m_scene_params_buffer = m_device->CreateBuffer(luna::RHI::BufferBuilder()
                                                           .SetSize(sizeof(SceneGpuParams))
                                                           .SetUsage(luna::RHI::BufferUsageFlags::UniformBuffer)
                                                           .SetMemoryUsage(luna::RHI::BufferMemoryUsage::CpuToGpu)
                                                           .SetName("SceneParams")
                                                           .Build());
    }

    if (!m_environment_texture.texture) {
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
        m_environment_texture = createUploadedTexture(environment_image, "SceneEnvironment");
    }

    if (!m_scene_descriptor_set) {
        m_scene_descriptor_set = m_descriptor_pool->AllocateDescriptorSet(m_scene_layout);
    }

    if (!m_scene_descriptor_set || !m_scene_params_buffer || !m_environment_texture.texture) {
        return;
    }

    m_scene_descriptor_set->WriteBuffer(luna::RHI::BufferWriteInfo{
        .Binding = 0,
        .Buffer = m_scene_params_buffer,
        .Offset = 0,
        .Stride = sizeof(SceneGpuParams),
        .Size = sizeof(SceneGpuParams),
        .Type = luna::RHI::DescriptorType::UniformBuffer,
    });
    m_scene_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 1,
        .TextureView = m_environment_texture.texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_scene_descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
        .Binding = 2,
        .Sampler = m_environment_sampler,
    });
    m_scene_descriptor_set->Update();
}

void SceneRenderer::updateSceneParameters(const RenderContext& context)
{
    if (!m_scene_params_buffer || context.framebuffer_width == 0 || context.framebuffer_height == 0) {
        return;
    }

    const float aspect_ratio =
        static_cast<float>(context.framebuffer_width) / static_cast<float>(context.framebuffer_height);
    const glm::mat4 view_projection = buildViewProjection(m_camera, aspect_ratio, context.backend_type);
    const float environment_mip_count = m_environment_texture.texture != nullptr
                                            ? static_cast<float>((std::max)(m_environment_texture.texture->GetMipLevels(), 1u) - 1u)
                                            : 0.0f;

    SceneGpuParams params;
    params.view_projection = view_projection;
    params.inverse_view_projection = glm::inverse(view_projection);
    params.camera_position_env_mip = glm::vec4(m_camera.m_position, environment_mip_count);
    params.light_direction_intensity = glm::vec4(glm::normalize(glm::vec3(0.45f, 0.80f, 0.35f)), 4.0f);
    params.light_color_exposure = glm::vec4(1.0f, 0.98f, 0.95f, 1.0f);
    params.ibl_factors = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);

    if (void* mapped = m_scene_params_buffer->Map()) {
        std::memcpy(mapped, &params, sizeof(params));
        m_scene_params_buffer->Flush();
        m_scene_params_buffer->Unmap();
    }
}

void SceneRenderer::executeGeometryPass(rhi::RenderGraphRasterPassContext& pass_context, const RenderContext& context)
{
    ensurePipelines(context);
    if (!m_geometry_pipeline || !m_scene_descriptor_set) {
        LUNA_RENDERER_ERROR("Scene geometry pass aborted: graphics pipeline is null");
        return;
    }

    auto& commands = pass_context.commandBuffer();
    updateSceneParameters(context);

    getOrCreateUploadedMaterial(m_default_material);

    for (const auto& draw_command : m_opaque_draw_commands) {
        if (!draw_command.mesh || !draw_command.mesh->isValid()) {
            continue;
        }

        (void) getOrCreateUploadedMesh(*draw_command.mesh);
        const Material& material = draw_command.material != nullptr ? *draw_command.material : m_default_material;
        auto& uploaded_material = getOrCreateUploadedMaterial(material);
        uploadMaterialIfNeeded(commands, uploaded_material);
    }

    pass_context.beginRendering();
    commands.BindGraphicsPipeline(m_geometry_pipeline);
    commands.SetViewport({0.0f,
                          0.0f,
                          static_cast<float>(pass_context.framebufferWidth()),
                          static_cast<float>(pass_context.framebufferHeight()),
                          0.0f,
                          1.0f});
    commands.SetScissor({0, 0, pass_context.framebufferWidth(), pass_context.framebufferHeight()});

    for (const auto& draw_command : m_opaque_draw_commands) {
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
        const std::array<luna::RHI::Ref<luna::RHI::DescriptorSet>, 2> descriptor_sets{
            uploaded_material.descriptor_set,
            m_scene_descriptor_set,
        };

        MeshPushConstants push_constants;
        push_constants.model = draw_command.transform;
        commands.BindDescriptorSets(m_geometry_pipeline, 0, descriptor_sets);
        commands.BindVertexBuffer(0, uploaded_mesh.vertex_buffer);
        commands.BindIndexBuffer(uploaded_mesh.index_buffer, 0, luna::RHI::IndexType::UInt32);
        commands.PushConstants(
            m_geometry_pipeline, luna::RHI::ShaderStage::Vertex, 0, sizeof(MeshPushConstants), &push_constants);
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
    if (!m_lighting_pipeline || !m_gbuffer_descriptor_set || !m_scene_descriptor_set || !m_gbuffer_sampler) {
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
    uploadTextureIfNeeded(commands, m_environment_texture);
    updateSceneParameters(context);

    m_gbuffer_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 0,
        .TextureView = gbuffer_base_color->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_gbuffer_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 1,
        .TextureView = gbuffer_normal_metallic->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_gbuffer_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 2,
        .TextureView = gbuffer_world_position_roughness->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_gbuffer_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 3,
        .TextureView = gbuffer_emissive_ao->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_gbuffer_descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
        .Binding = 4,
        .Sampler = m_gbuffer_sampler,
    });
    m_gbuffer_descriptor_set->Update();

    pass_context.beginRendering();

    commands.BindGraphicsPipeline(m_lighting_pipeline);
    commands.SetViewport({0.0f,
                          0.0f,
                          static_cast<float>(pass_context.framebufferWidth()),
                          static_cast<float>(pass_context.framebufferHeight()),
                          0.0f,
                          1.0f});
    commands.SetScissor({0, 0, pass_context.framebufferWidth(), pass_context.framebufferHeight()});

    const std::array<luna::RHI::Ref<luna::RHI::DescriptorSet>, 2> descriptor_sets{
        m_gbuffer_descriptor_set,
        m_scene_descriptor_set,
    };
    commands.BindDescriptorSets(m_lighting_pipeline, 0, descriptor_sets);
    commands.Draw(3, 1, 0, 0);

    pass_context.endRendering();
}

void SceneRenderer::executeTransparentPass(rhi::RenderGraphRasterPassContext& pass_context, const RenderContext& context)
{
    ensurePipelines(context);
    if (!m_transparent_pipeline || !m_scene_descriptor_set || m_transparent_draw_commands.empty()) {
        return;
    }

    auto& commands = pass_context.commandBuffer();
    updateSceneParameters(context);
    uploadTextureIfNeeded(commands, m_environment_texture);

    getOrCreateUploadedMaterial(m_default_material);

    for (const auto& draw_command : m_transparent_draw_commands) {
        if (!draw_command.mesh || !draw_command.mesh->isValid()) {
            continue;
        }

        (void) getOrCreateUploadedMesh(*draw_command.mesh);
        const Material& material = draw_command.material != nullptr ? *draw_command.material : m_default_material;
        auto& uploaded_material = getOrCreateUploadedMaterial(material);
        uploadMaterialIfNeeded(commands, uploaded_material);
    }

    sortTransparentDrawCommands();

    pass_context.beginRendering();
    commands.BindGraphicsPipeline(m_transparent_pipeline);
    commands.SetViewport({0.0f,
                          0.0f,
                          static_cast<float>(pass_context.framebufferWidth()),
                          static_cast<float>(pass_context.framebufferHeight()),
                          0.0f,
                          1.0f});
    commands.SetScissor({0, 0, pass_context.framebufferWidth(), pass_context.framebufferHeight()});

    for (const auto& draw_command : m_transparent_draw_commands) {
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
        const std::array<luna::RHI::Ref<luna::RHI::DescriptorSet>, 2> descriptor_sets{
            uploaded_material.descriptor_set,
            m_scene_descriptor_set,
        };

        MeshPushConstants push_constants;
        push_constants.model = draw_command.transform;
        commands.BindDescriptorSets(m_transparent_pipeline, 0, descriptor_sets);
        commands.BindVertexBuffer(0, uploaded_mesh.vertex_buffer);
        commands.BindIndexBuffer(uploaded_mesh.index_buffer, 0, luna::RHI::IndexType::UInt32);
        commands.PushConstants(
            m_transparent_pipeline, luna::RHI::ShaderStage::Vertex, 0, sizeof(MeshPushConstants), &push_constants);
        commands.DrawIndexed(uploaded_mesh.index_count, 1, 0, 0, 0);
    }

    pass_context.endRendering();
}

void SceneRenderer::sortTransparentDrawCommands()
{
    std::sort(m_transparent_draw_commands.begin(),
              m_transparent_draw_commands.end(),
              [this](const StaticMeshDrawCommand& lhs, const StaticMeshDrawCommand& rhs) {
                  return transparentSortDistanceSq(lhs.transform, m_camera) >
                         transparentSortDistanceSq(rhs.transform, m_camera);
              });
}

} // namespace luna
