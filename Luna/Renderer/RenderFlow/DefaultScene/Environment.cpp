#include "Asset/Editor/ImageLoader.h"
#include "Core/Log.h"
#include "Renderer/RenderFlow/DefaultScene/Environment.h"

#include <Builders.h>
#include <DescriptorPool.h>
#include <DescriptorSet.h>
#include <DescriptorSetLayout.h>
#include <Device.h>
#include <Pipeline.h>
#include <PipelineLayout.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace luna::render_flow::default_scene {

namespace {

struct IblDispatchParams {
    uint32_t output_size{1};
    uint32_t sample_count{1};
    float roughness{0.0f};
    uint32_t reserved{0};
};

std::filesystem::path defaultEnvironmentPath()
{
    return render_flow::default_scene_detail::projectRoot() / "SampleProject" / "Assets" / "Texture" /
           "newport_loft.hdr";
}

std::filesystem::path defaultEnvironmentIblShaderPath()
{
    return render_flow::default_scene_detail::projectRoot() / "Luna" / "Renderer" / "Shaders" /
           "EnvironmentIBL.slang";
}

uint32_t divideRoundUp(uint32_t value, uint32_t divisor)
{
    return divisor == 0 ? 0 : (value + divisor - 1u) / divisor;
}

luna::RHI::ImageSubresourceRange fullColorRange(const luna::RHI::Ref<luna::RHI::Texture>& texture)
{
    if (!texture) {
        return {};
    }

    return luna::RHI::ImageSubresourceRange{
        .BaseMipLevel = 0,
        .LevelCount = texture->GetMipLevels(),
        .BaseArrayLayer = 0,
        .LayerCount = texture->GetArrayLayers(),
        .AspectMask = luna::RHI::ImageAspectFlags::Color,
    };
}

void transitionTexture(luna::RHI::CommandBufferEncoder& commands,
                       const luna::RHI::Ref<luna::RHI::Texture>& texture,
                       luna::RHI::ResourceState old_state,
                       luna::RHI::ResourceState new_state)
{
    if (!texture || old_state == new_state) {
        return;
    }

    commands.PipelineBarrier(luna::RHI::SyncScope::AllCommands,
                             luna::RHI::SyncScope::AllCommands,
                             std::array{luna::RHI::TextureBarrier{
                                 .Texture = texture,
                                 .OldState = old_state,
                                 .NewState = new_state,
                                 .SubresourceRange = fullColorRange(texture),
                             }});
}

luna::RHI::TextureViewDesc cubeStorageViewDesc(uint32_t mip_level, luna::RHI::Format format)
{
    return luna::RHI::TextureViewDesc{
        .ViewType = luna::RHI::TextureType::Texture2DArray,
        .FormatOverride = format,
        .BaseMipLevel = mip_level,
        .MipLevelCount = 1,
        .BaseArrayLayer = 0,
        .ArrayLayerCount = 6,
        .Aspect = luna::RHI::AspectMask::Color,
    };
}

luna::RHI::TextureViewDesc texture2DStorageViewDesc(luna::RHI::Format format)
{
    return luna::RHI::TextureViewDesc{
        .ViewType = luna::RHI::TextureType::Texture2D,
        .FormatOverride = format,
        .BaseMipLevel = 0,
        .MipLevelCount = 1,
        .BaseArrayLayer = 0,
        .ArrayLayerCount = 1,
        .Aspect = luna::RHI::AspectMask::Color,
    };
}

luna::RHI::Ref<luna::RHI::Texture>
    createCubeTexture(const luna::RHI::Ref<luna::RHI::Device>& device,
                      uint32_t size,
                      uint32_t mip_levels,
                      std::string_view name)
{
    if (!device) {
        return {};
    }

    return device->CreateTexture(luna::RHI::TextureBuilder()
                                     .SetType(luna::RHI::TextureType::TextureCube)
                                     .SetSize(size, size)
                                     .SetArrayLayers(6)
                                     .SetMipLevels(mip_levels)
                                     .SetFormat(render_flow::default_scene_detail::kEnvironmentIblFormat)
                                     .SetUsage(luna::RHI::TextureUsageFlags::Sampled |
                                               luna::RHI::TextureUsageFlags::Storage)
                                     .SetInitialState(luna::RHI::ResourceState::Undefined)
                                     .SetName(std::string(name))
                                     .Build());
}

luna::RHI::Ref<luna::RHI::Texture> createBrdfLutTexture(const luna::RHI::Ref<luna::RHI::Device>& device)
{
    if (!device) {
        return {};
    }

    return device->CreateTexture(luna::RHI::TextureBuilder()
                                     .SetSize(render_flow::default_scene_detail::kEnvironmentBrdfLutSize,
                                              render_flow::default_scene_detail::kEnvironmentBrdfLutSize)
                                     .SetFormat(render_flow::default_scene_detail::kEnvironmentBrdfLutFormat)
                                     .SetUsage(luna::RHI::TextureUsageFlags::Sampled |
                                               luna::RHI::TextureUsageFlags::Storage)
                                     .SetInitialState(luna::RHI::ResourceState::Undefined)
                                     .SetName("SceneEnvironmentBrdfLut")
                                     .Build());
}

luna::RHI::Ref<luna::RHI::Sampler> createIblSampler(const luna::RHI::Ref<luna::RHI::Device>& device)
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
                                     .SetName("SceneEnvironmentIblSampler")
                                     .Build());
}

luna::RHI::Ref<luna::RHI::PipelineLayout>
    createComputePipelineLayout(const luna::RHI::Ref<luna::RHI::Device>& device,
                                const luna::RHI::Ref<luna::RHI::DescriptorSetLayout>& layout)
{
    if (!device || !layout) {
        return {};
    }

    return device->CreatePipelineLayout(luna::RHI::PipelineLayoutBuilder()
                                            .AddSetLayout(layout)
                                            .AddPushConstant(luna::RHI::ShaderStage::Compute,
                                                             0,
                                                             sizeof(IblDispatchParams))
                                            .Build());
}

luna::RHI::Ref<luna::RHI::ComputePipeline>
    createComputePipeline(const luna::RHI::Ref<luna::RHI::Device>& device,
                          const luna::RHI::Ref<luna::RHI::PipelineLayout>& layout,
                          const luna::RHI::Ref<luna::RHI::ShaderModule>& shader)
{
    if (!device || !layout || !shader) {
        return {};
    }

    return device->CreateComputePipeline(
        luna::RHI::ComputePipelineBuilder().SetShader(shader).SetLayout(layout).Build());
}

} // namespace

void EnvironmentResources::reset()
{
    m_device.reset();
    m_backend_type = luna::RHI::BackendType::Auto;
    m_source_texture = {};
    m_irradiance_sh = {};

    m_environment_cube_texture.reset();
    m_irradiance_texture.reset();
    m_prefiltered_texture.reset();
    m_brdf_lut_texture.reset();
    m_environment_cube_uav.reset();
    m_irradiance_uav.reset();
    m_prefiltered_uavs = {};
    m_brdf_lut_uav.reset();

    m_equirect_to_cube_layout.reset();
    m_cube_filter_layout.reset();
    m_brdf_lut_layout.reset();
    m_equirect_to_cube_pipeline_layout.reset();
    m_cube_filter_pipeline_layout.reset();
    m_brdf_lut_pipeline_layout.reset();
    m_descriptor_pool.reset();
    m_equirect_to_cube_descriptor_set.reset();
    m_irradiance_descriptor_set.reset();
    m_prefilter_descriptor_sets = {};
    m_brdf_lut_descriptor_set.reset();
    m_sampler.reset();

    m_equirect_to_cube_shader.reset();
    m_irradiance_shader.reset();
    m_prefilter_shader.reset();
    m_brdf_lut_shader.reset();
    m_equirect_to_cube_pipeline.reset();
    m_irradiance_pipeline.reset();
    m_prefilter_pipeline.reset();
    m_brdf_lut_pipeline.reset();
    m_precomputed = false;
}

void EnvironmentResources::ensure(const SceneRenderContext& context)
{
    if (!context.device) {
        LUNA_RENDERER_WARN("Cannot ensure scene environment without a device");
        return;
    }

    if (m_device != context.device || m_backend_type != context.backend_type) {
        reset();
        m_device = context.device;
        m_backend_type = context.backend_type;
    }

    if (!m_source_texture.texture) {
        const std::filesystem::path environment_path = defaultEnvironmentPath();
        ImageData environment_image;
        LUNA_RENDERER_INFO("Loading scene environment map from '{}'", environment_path.string());

        if (std::filesystem::exists(environment_path)) {
            environment_image = ImageLoader::LoadImageFromFile(environment_path.string());
            if (!environment_image.isValid()) {
                LUNA_RENDERER_WARN("Failed to load environment map '{}'; falling back to a neutral environment",
                                   environment_path.string());
            }
        } else {
            LUNA_RENDERER_WARN("Environment map '{}' was not found; falling back to a neutral environment",
                               environment_path.string());
        }

        if (!environment_image.isValid() ||
            environment_image.ImageFormat != render_flow::default_scene_detail::kEnvironmentFormat) {
            environment_image = render_flow::default_scene_detail::createFallbackFloatImageData(
                glm::vec4(render_flow::default_scene_detail::kEnvironmentFallbackValue,
                          render_flow::default_scene_detail::kEnvironmentFallbackValue,
                          render_flow::default_scene_detail::kEnvironmentFallbackValue,
                          1.0f));
        }

        m_irradiance_sh = render_flow::default_scene_detail::computeDiffuseIrradianceSH(environment_image);
        environment_image = render_flow::default_scene_detail::generateEnvironmentMipChain(environment_image);
        m_source_texture = render_flow::default_scene_detail::createTextureUpload(
            context.device, environment_image, Texture::SamplerSettings{}, "SceneEnvironmentSource");

        LUNA_RENDERER_INFO("Prepared scene environment source texture ({}x{}, mips={})",
                           environment_image.Width,
                           environment_image.Height,
                           1u + static_cast<uint32_t>(environment_image.MipLevels.size()));
    }

    if (!m_environment_cube_texture) {
        m_environment_cube_texture = createCubeTexture(context.device,
                                                       render_flow::default_scene_detail::kEnvironmentCubeSize,
                                                       1,
                                                       "SceneEnvironmentCube");
        m_irradiance_texture = createCubeTexture(context.device,
                                                 render_flow::default_scene_detail::kEnvironmentIrradianceCubeSize,
                                                 1,
                                                 "SceneEnvironmentIrradiance");
        m_prefiltered_texture = createCubeTexture(context.device,
                                                  render_flow::default_scene_detail::kEnvironmentPrefilterCubeSize,
                                                  render_flow::default_scene_detail::kEnvironmentPrefilterMipLevels,
                                                  "SceneEnvironmentPrefiltered");
    }

    if (!m_brdf_lut_texture) {
        m_brdf_lut_texture = createBrdfLutTexture(context.device);
    }

    if (!m_environment_cube_uav && m_environment_cube_texture) {
        m_environment_cube_uav = m_environment_cube_texture->CreateView(
            cubeStorageViewDesc(0, render_flow::default_scene_detail::kEnvironmentIblFormat));
    }
    if (!m_irradiance_uav && m_irradiance_texture) {
        m_irradiance_uav = m_irradiance_texture->CreateView(
            cubeStorageViewDesc(0, render_flow::default_scene_detail::kEnvironmentIblFormat));
    }
    if (m_prefiltered_texture) {
        for (uint32_t mip_level = 0;
             mip_level < render_flow::default_scene_detail::kEnvironmentPrefilterMipLevels;
             ++mip_level) {
            if (!m_prefiltered_uavs[mip_level]) {
                m_prefiltered_uavs[mip_level] = m_prefiltered_texture->CreateView(
                    cubeStorageViewDesc(mip_level, render_flow::default_scene_detail::kEnvironmentIblFormat));
            }
        }
    }
    if (!m_brdf_lut_uav && m_brdf_lut_texture) {
        m_brdf_lut_uav =
            m_brdf_lut_texture->CreateView(texture2DStorageViewDesc(render_flow::default_scene_detail::kEnvironmentBrdfLutFormat));
    }

    if (!m_sampler) {
        m_sampler = createIblSampler(context.device);
    }

    if (!m_equirect_to_cube_layout) {
        m_equirect_to_cube_layout = context.device->CreateDescriptorSetLayout(
            luna::RHI::DescriptorSetLayoutBuilder()
                .AddBinding(0, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Compute)
                .AddBinding(1, luna::RHI::DescriptorType::Sampler, 1, luna::RHI::ShaderStage::Compute)
                .AddBinding(2, luna::RHI::DescriptorType::StorageImage, 1, luna::RHI::ShaderStage::Compute)
                .Build());
        m_cube_filter_layout = context.device->CreateDescriptorSetLayout(
            luna::RHI::DescriptorSetLayoutBuilder()
                .AddBinding(1, luna::RHI::DescriptorType::Sampler, 1, luna::RHI::ShaderStage::Compute)
                .AddBinding(2, luna::RHI::DescriptorType::StorageImage, 1, luna::RHI::ShaderStage::Compute)
                .AddBinding(3, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Compute)
                .Build());
        m_brdf_lut_layout = context.device->CreateDescriptorSetLayout(
            luna::RHI::DescriptorSetLayoutBuilder()
                .AddBinding(4, luna::RHI::DescriptorType::StorageImage, 1, luna::RHI::ShaderStage::Compute)
                .Build());
    }

    if (!m_descriptor_pool) {
        m_descriptor_pool = context.device->CreateDescriptorPool(luna::RHI::DescriptorPoolBuilder()
                                                                    .SetMaxSets(16)
                                                                    .AddPoolSize(luna::RHI::DescriptorType::SampledImage, 16)
                                                                    .AddPoolSize(luna::RHI::DescriptorType::Sampler, 16)
                                                                    .AddPoolSize(luna::RHI::DescriptorType::StorageImage, 16)
                                                                    .Build());
    }

    if (m_descriptor_pool && !m_equirect_to_cube_descriptor_set) {
        m_equirect_to_cube_descriptor_set = m_descriptor_pool->AllocateDescriptorSet(m_equirect_to_cube_layout);
        m_irradiance_descriptor_set = m_descriptor_pool->AllocateDescriptorSet(m_cube_filter_layout);
        for (auto& descriptor_set : m_prefilter_descriptor_sets) {
            descriptor_set = m_descriptor_pool->AllocateDescriptorSet(m_cube_filter_layout);
        }
        m_brdf_lut_descriptor_set = m_descriptor_pool->AllocateDescriptorSet(m_brdf_lut_layout);
    }

    if (!m_equirect_to_cube_pipeline_layout) {
        m_equirect_to_cube_pipeline_layout = createComputePipelineLayout(context.device, m_equirect_to_cube_layout);
        m_cube_filter_pipeline_layout = createComputePipelineLayout(context.device, m_cube_filter_layout);
        m_brdf_lut_pipeline_layout = createComputePipelineLayout(context.device, m_brdf_lut_layout);
    }

    if (!context.compiler) {
        LUNA_RENDERER_WARN("Scene environment IBL precompute cannot compile shaders because compiler is null");
        return;
    }

    if (!m_equirect_to_cube_shader) {
        const std::filesystem::path shader_path = defaultEnvironmentIblShaderPath();
        m_equirect_to_cube_shader = render_flow::default_scene_detail::loadShaderModule(
            context.device, context.compiler, shader_path, "environmentEquirectToCubeMain", luna::RHI::ShaderStage::Compute);
        m_irradiance_shader = render_flow::default_scene_detail::loadShaderModule(
            context.device, context.compiler, shader_path, "environmentIrradianceMain", luna::RHI::ShaderStage::Compute);
        m_prefilter_shader = render_flow::default_scene_detail::loadShaderModule(
            context.device, context.compiler, shader_path, "environmentPrefilterMain", luna::RHI::ShaderStage::Compute);
        m_brdf_lut_shader = render_flow::default_scene_detail::loadShaderModule(
            context.device, context.compiler, shader_path, "environmentBrdfLutMain", luna::RHI::ShaderStage::Compute);
    }

    if (!m_equirect_to_cube_pipeline) {
        m_equirect_to_cube_pipeline =
            createComputePipeline(context.device, m_equirect_to_cube_pipeline_layout, m_equirect_to_cube_shader);
        m_irradiance_pipeline =
            createComputePipeline(context.device, m_cube_filter_pipeline_layout, m_irradiance_shader);
        m_prefilter_pipeline =
            createComputePipeline(context.device, m_cube_filter_pipeline_layout, m_prefilter_shader);
        m_brdf_lut_pipeline = createComputePipeline(context.device, m_brdf_lut_pipeline_layout, m_brdf_lut_shader);
    }
}

void EnvironmentResources::uploadIfNeeded(luna::RHI::CommandBufferEncoder& commands)
{
    if (!m_source_texture.uploaded) {
        LUNA_RENDERER_DEBUG("Uploading environment source texture '{}'", m_source_texture.debug_name);
    }
    render_flow::default_scene_detail::uploadTextureIfNeeded(
        commands,
        m_source_texture,
        luna::RHI::ResourceState::ShaderRead,
        luna::RHI::SyncScope::ComputeStage | luna::RHI::SyncScope::FragmentStage);
}

void EnvironmentResources::precomputeIfNeeded(luna::RHI::CommandBufferEncoder& commands)
{
    if (m_precomputed) {
        return;
    }

    if (!m_source_texture.texture || !m_environment_cube_texture || !m_irradiance_texture || !m_prefiltered_texture ||
        !m_brdf_lut_texture || !m_environment_cube_uav || !m_irradiance_uav || !m_brdf_lut_uav ||
        !m_equirect_to_cube_pipeline || !m_irradiance_pipeline || !m_prefilter_pipeline || !m_brdf_lut_pipeline ||
        !m_equirect_to_cube_descriptor_set || !m_irradiance_descriptor_set || !m_brdf_lut_descriptor_set ||
        !m_sampler ||
        std::any_of(m_prefilter_descriptor_sets.begin(),
                    m_prefilter_descriptor_sets.end(),
                    [](const auto& descriptor_set) {
                        return descriptor_set == nullptr;
                    })) {
        LUNA_RENDERER_WARN("Skipping environment IBL precompute because GPU resources are incomplete");
        return;
    }

    uploadIfNeeded(commands);
    LUNA_RENDERER_INFO("Precomputing scene environment IBL maps on GPU");

    auto dispatchCube = [&commands](const luna::RHI::Ref<luna::RHI::ComputePipeline>& pipeline,
                                    const luna::RHI::Ref<luna::RHI::DescriptorSet>& descriptor_set,
                                    const IblDispatchParams& params) {
        const std::array<luna::RHI::Ref<luna::RHI::DescriptorSet>, 1> descriptor_sets{descriptor_set};
        commands.BindComputePipeline(pipeline);
        commands.BindComputeDescriptorSets(pipeline, 0, descriptor_sets);
        commands.ComputePushConstants(
            pipeline, luna::RHI::ShaderStage::Compute, 0, sizeof(IblDispatchParams), &params);
        commands.Dispatch(divideRoundUp(params.output_size, 8u), divideRoundUp(params.output_size, 8u), 6);
    };

    auto dispatch2D = [&commands](const luna::RHI::Ref<luna::RHI::ComputePipeline>& pipeline,
                                  const luna::RHI::Ref<luna::RHI::DescriptorSet>& descriptor_set,
                                  const IblDispatchParams& params) {
        const std::array<luna::RHI::Ref<luna::RHI::DescriptorSet>, 1> descriptor_sets{descriptor_set};
        commands.BindComputePipeline(pipeline);
        commands.BindComputeDescriptorSets(pipeline, 0, descriptor_sets);
        commands.ComputePushConstants(
            pipeline, luna::RHI::ShaderStage::Compute, 0, sizeof(IblDispatchParams), &params);
        commands.Dispatch(divideRoundUp(params.output_size, 8u), divideRoundUp(params.output_size, 8u), 1);
    };

    transitionTexture(commands,
                      m_environment_cube_texture,
                      luna::RHI::ResourceState::Undefined,
                      luna::RHI::ResourceState::UnorderedAccess);
    m_equirect_to_cube_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 0,
        .TextureView = m_source_texture.texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_equirect_to_cube_descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{.Binding = 1, .Sampler = m_sampler});
    m_equirect_to_cube_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 2,
        .TextureView = m_environment_cube_uav,
        .Layout = luna::RHI::ResourceState::UnorderedAccess,
        .Type = luna::RHI::DescriptorType::StorageImage,
    });
    m_equirect_to_cube_descriptor_set->Update();
    dispatchCube(m_equirect_to_cube_pipeline,
                 m_equirect_to_cube_descriptor_set,
                 IblDispatchParams{.output_size = render_flow::default_scene_detail::kEnvironmentCubeSize,
                                   .sample_count = 1,
                                   .roughness = 0.0f});
    commands.MemoryBarrierFast(luna::RHI::MemoryTransition::AllWriteToAllRead);
    transitionTexture(commands,
                      m_environment_cube_texture,
                      luna::RHI::ResourceState::UnorderedAccess,
                      luna::RHI::ResourceState::ShaderRead);

    transitionTexture(commands,
                      m_irradiance_texture,
                      luna::RHI::ResourceState::Undefined,
                      luna::RHI::ResourceState::UnorderedAccess);
    m_irradiance_descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{.Binding = 1, .Sampler = m_sampler});
    m_irradiance_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 2,
        .TextureView = m_irradiance_uav,
        .Layout = luna::RHI::ResourceState::UnorderedAccess,
        .Type = luna::RHI::DescriptorType::StorageImage,
    });
    m_irradiance_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 3,
        .TextureView = m_environment_cube_texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_irradiance_descriptor_set->Update();
    dispatchCube(m_irradiance_pipeline,
                 m_irradiance_descriptor_set,
                 IblDispatchParams{
                     .output_size = render_flow::default_scene_detail::kEnvironmentIrradianceCubeSize,
                     .sample_count = render_flow::default_scene_detail::kEnvironmentIrradianceSampleCount,
                     .roughness = 0.0f,
                 });
    commands.MemoryBarrierFast(luna::RHI::MemoryTransition::AllWriteToAllRead);
    transitionTexture(commands,
                      m_irradiance_texture,
                      luna::RHI::ResourceState::UnorderedAccess,
                      luna::RHI::ResourceState::ShaderRead);

    transitionTexture(commands,
                      m_prefiltered_texture,
                      luna::RHI::ResourceState::Undefined,
                      luna::RHI::ResourceState::UnorderedAccess);
    for (uint32_t mip_level = 0; mip_level < render_flow::default_scene_detail::kEnvironmentPrefilterMipLevels;
         ++mip_level) {
        const uint32_t mip_size =
            std::max(render_flow::default_scene_detail::kEnvironmentPrefilterCubeSize >> mip_level, 1u);
        const float roughness =
            render_flow::default_scene_detail::kEnvironmentPrefilterMipLevels > 1
                ? static_cast<float>(mip_level) /
                      static_cast<float>(render_flow::default_scene_detail::kEnvironmentPrefilterMipLevels - 1u)
                : 0.0f;

        const auto& prefilter_descriptor_set = m_prefilter_descriptor_sets[mip_level];
        prefilter_descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{.Binding = 1, .Sampler = m_sampler});
        prefilter_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
            .Binding = 2,
            .TextureView = m_prefiltered_uavs[mip_level],
            .Layout = luna::RHI::ResourceState::UnorderedAccess,
            .Type = luna::RHI::DescriptorType::StorageImage,
        });
        prefilter_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
            .Binding = 3,
            .TextureView = m_environment_cube_texture->GetDefaultView(),
            .Layout = luna::RHI::ResourceState::ShaderRead,
            .Type = luna::RHI::DescriptorType::SampledImage,
        });
        prefilter_descriptor_set->Update();
        dispatchCube(m_prefilter_pipeline,
                     prefilter_descriptor_set,
                     IblDispatchParams{
                         .output_size = mip_size,
                         .sample_count = render_flow::default_scene_detail::kEnvironmentPrefilterSampleCount,
                         .roughness = roughness,
                     });
        commands.MemoryBarrierFast(luna::RHI::MemoryTransition::AllWriteToAllRead);
    }
    transitionTexture(commands,
                      m_prefiltered_texture,
                      luna::RHI::ResourceState::UnorderedAccess,
                      luna::RHI::ResourceState::ShaderRead);

    transitionTexture(commands,
                      m_brdf_lut_texture,
                      luna::RHI::ResourceState::Undefined,
                      luna::RHI::ResourceState::UnorderedAccess);
    m_brdf_lut_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 4,
        .TextureView = m_brdf_lut_uav,
        .Layout = luna::RHI::ResourceState::UnorderedAccess,
        .Type = luna::RHI::DescriptorType::StorageImage,
    });
    m_brdf_lut_descriptor_set->Update();
    dispatch2D(m_brdf_lut_pipeline,
               m_brdf_lut_descriptor_set,
               IblDispatchParams{
                   .output_size = render_flow::default_scene_detail::kEnvironmentBrdfLutSize,
                   .sample_count = render_flow::default_scene_detail::kEnvironmentBrdfSampleCount,
                   .roughness = 0.0f,
               });
    commands.MemoryBarrierFast(luna::RHI::MemoryTransition::AllWriteToAllRead);
    transitionTexture(commands,
                      m_brdf_lut_texture,
                      luna::RHI::ResourceState::UnorderedAccess,
                      luna::RHI::ResourceState::ShaderRead);

    m_precomputed = true;
    LUNA_RENDERER_INFO("Scene environment IBL precompute complete");
}

float EnvironmentResources::prefilteredMaxMipLevel() const noexcept
{
    if (!m_prefiltered_texture || m_prefiltered_texture->GetMipLevels() == 0) {
        return 0.0f;
    }
    return static_cast<float>(m_prefiltered_texture->GetMipLevels() - 1u);
}

} // namespace luna::render_flow::default_scene
