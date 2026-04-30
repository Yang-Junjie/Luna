#include "Renderer/RenderFlow/DefaultScene/Environment.h"

#include "Asset/AssetManager.h"
#include "Core/Log.h"
#include "Renderer/Image/ImageDataUtils.h"
#include "Renderer/Resources/ShaderModuleLoader.h"
#include "Renderer/Texture.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include <Builders.h>
#include <DescriptorPool.h>
#include <DescriptorSet.h>
#include <DescriptorSetLayout.h>
#include <Device.h>
#include <Pipeline.h>
#include <PipelineLayout.h>

namespace luna::render_flow::default_scene {

namespace {

constexpr uint32_t kProceduralSkyWidth = 1024;
constexpr uint32_t kProceduralSkyHeight = 512;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

struct IblDispatchParams {
    uint32_t output_size{1};
    uint32_t sample_count{1};
    float roughness{0.0f};
    uint32_t reserved{0};
};

struct ProceduralSkyParams {
    glm::vec4 sun_direction_intensity{0.51214755f, 0.76822126f, 0.38411063f, 20.0f};
    glm::vec4 sky_color_zenith_exposure{0.15f, 0.30f, 0.60f, 1.5f};
    glm::vec4 sky_color_horizon_sun_radius{0.60f, 0.50f, 0.40f, 0.02f};
    glm::vec4 ground_color{0.10f, 0.08f, 0.06f, 0.0f};
    glm::uvec4 output_size{kProceduralSkyWidth, kProceduralSkyHeight, 0u, 0u};
};

namespace environment_binding {
constexpr uint32_t SourceTexture = 0;
constexpr uint32_t Sampler = 1;
constexpr uint32_t OutputTexture = 2;
constexpr uint32_t SourceCubeTexture = 3;
constexpr uint32_t BrdfLutOutputTexture = 4;
} // namespace environment_binding

namespace procedural_sky_binding {
constexpr uint32_t OutputTexture = 0;
} // namespace procedural_sky_binding

uint32_t divideRoundUp(uint32_t value, uint32_t divisor)
{
    return divisor == 0 ? 0 : (value + divisor - 1u) / divisor;
}

float saturate(float value)
{
    return (std::clamp)(value, 0.0f, 1.0f);
}

float smoothStep(float edge0, float edge1, float value)
{
    const float t = saturate((value - edge0) / (std::max)(edge1 - edge0, 0.00001f));
    return t * t * (3.0f - 2.0f * t);
}

glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback)
{
    const float length_squared = glm::dot(value, value);
    if (length_squared <= 0.000001f) {
        return fallback;
    }
    return glm::normalize(value);
}

bool sameVec3(const glm::vec3& lhs, const glm::vec3& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

bool sameSourceSignature(const EnvironmentResources::SourceSignature& lhs,
                         const EnvironmentResources::SourceSignature& rhs)
{
    if (lhs.kind != rhs.kind) {
        return false;
    }

    if (lhs.kind == EnvironmentResources::SourceKind::TextureAsset) {
        return lhs.texture_handle == rhs.texture_handle;
    }

    return sameVec3(lhs.procedural_sun_direction, rhs.procedural_sun_direction) &&
           lhs.procedural_sun_intensity == rhs.procedural_sun_intensity &&
           lhs.procedural_sun_angular_radius == rhs.procedural_sun_angular_radius &&
           sameVec3(lhs.procedural_sky_color_zenith, rhs.procedural_sky_color_zenith) &&
           sameVec3(lhs.procedural_sky_color_horizon, rhs.procedural_sky_color_horizon) &&
           sameVec3(lhs.procedural_ground_color, rhs.procedural_ground_color) &&
           lhs.procedural_sky_exposure == rhs.procedural_sky_exposure;
}

EnvironmentResources::SourceSignature defaultSkySignature(const RenderEnvironment* environment)
{
    EnvironmentResources::SourceSignature signature{};
    signature.kind = EnvironmentResources::SourceKind::DefaultSky;
    if (environment != nullptr) {
        signature.procedural_sun_direction =
            safeNormalize(environment->procedural_sun_direction, signature.procedural_sun_direction);
        signature.procedural_sun_intensity = (std::max)(environment->procedural_sun_intensity, 0.0f);
        signature.procedural_sun_angular_radius = (std::max)(environment->procedural_sun_angular_radius, 0.0f);
        signature.procedural_sky_color_zenith = environment->procedural_sky_color_zenith;
        signature.procedural_sky_color_horizon = environment->procedural_sky_color_horizon;
        signature.procedural_ground_color = environment->procedural_ground_color;
        signature.procedural_sky_exposure = (std::max)(environment->procedural_sky_exposure, 0.0f);
    }
    return signature;
}

EnvironmentResources::SourceSignature requestedSourceSignature(const RenderEnvironment* environment)
{
    if (environment == nullptr || !environment->enabled || !environment->environment_map_handle.isValid()) {
        return defaultSkySignature(environment);
    }

    EnvironmentResources::SourceSignature signature{};
    signature.kind = EnvironmentResources::SourceKind::TextureAsset;
    signature.texture_handle = environment->environment_map_handle;
    return signature;
}

bool isUsableEnvironmentTexture(const std::shared_ptr<Texture>& texture)
{
    if (!texture || !texture->isValid()) {
        return false;
    }

    const ImageData& image = texture->getImageData();
    return image.ImageFormat == render_flow::default_scene_detail::kEnvironmentFormat;
}

glm::vec3 computeRayleighScattering(const glm::vec3& direction,
                                    const glm::vec3& zenith_color,
                                    const glm::vec3& horizon_color)
{
    const float height = (std::max)(direction.y, 0.0f);
    const float zenith_factor = std::pow(height, 0.5f);
    glm::vec3 sky_color = glm::mix(horizon_color, zenith_color, zenith_factor);
    const float horizon_glow = std::exp(-height * 4.0f) * 0.3f;
    sky_color += horizon_color * horizon_glow;
    return sky_color;
}

glm::vec3 computeMieScattering(const glm::vec3& direction, const glm::vec3& sun_dir)
{
    const float cos_theta = glm::dot(direction, sun_dir);
    constexpr float g = 0.76f;
    constexpr float g2 = g * g;
    const float phase =
        (1.0f - g2) / (4.0f * kPi * std::pow((std::max)(1.0f + g2 - 2.0f * g * cos_theta, 0.00001f), 1.5f));
    return glm::vec3(1.0f, 0.9f, 0.7f) * phase * 0.15f;
}

glm::vec3 computeSunDisc(const glm::vec3& direction,
                         const glm::vec3& sun_dir,
                         float sun_angular_radius,
                         float sun_intensity)
{
    const float radius = (std::max)(sun_angular_radius, 0.0001f);
    const float cos_theta = glm::dot(direction, sun_dir);
    const float sun_cos_angle = std::cos(radius);
    const float edge_softness = radius * 0.3f;
    const float sun_disc = smoothStep(std::cos(radius + edge_softness), sun_cos_angle, cos_theta);
    return glm::vec3(1.0f, 0.95f, 0.9f) * (std::max)(sun_intensity, 0.0f) * sun_disc;
}

glm::vec3 proceduralSkyColor(const EnvironmentResources::SourceSignature& signature, const glm::vec3& direction)
{
    const glm::vec3 dir = safeNormalize(direction, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::vec3 sun_dir =
        safeNormalize(signature.procedural_sun_direction, glm::vec3(0.51214755f, 0.76822126f, 0.38411063f));
    const float exposure = (std::max)(signature.procedural_sky_exposure, 0.0f);
    const glm::vec3 zenith_color = glm::max(signature.procedural_sky_color_zenith, glm::vec3(0.0f));
    const glm::vec3 horizon_color = glm::max(signature.procedural_sky_color_horizon, glm::vec3(0.0f));
    const glm::vec3 ground_color = glm::max(signature.procedural_ground_color, glm::vec3(0.0f));

    if (dir.y > -0.02f) {
        glm::vec3 sky_color = computeRayleighScattering(dir, zenith_color, horizon_color);
        sky_color += computeMieScattering(dir, sun_dir);
        if (sun_dir.y > 0.0f) {
            sky_color += computeSunDisc(
                dir, sun_dir, signature.procedural_sun_angular_radius, signature.procedural_sun_intensity);
        }

        const float horizon_blend = smoothStep(-0.02f, 0.05f, dir.y);
        const glm::vec3 sky = sky_color * exposure;
        return glm::max(glm::mix(ground_color * exposure * 0.5f, sky, horizon_blend), glm::vec3(0.0f));
    }

    const float ground_fade = smoothStep(-1.0f, -0.1f, dir.y);
    return glm::max(ground_color * exposure * 0.5f * ground_fade, glm::vec3(0.0f));
}

ImageData createProceduralSkyImageData(const EnvironmentResources::SourceSignature& signature)
{
    std::vector<float> pixels(static_cast<size_t>(kProceduralSkyWidth) * static_cast<size_t>(kProceduralSkyHeight) * 4u,
                              1.0f);

    for (uint32_t y = 0; y < kProceduralSkyHeight; ++y) {
        const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(kProceduralSkyHeight);
        const float theta = v * kPi;
        const float sin_theta = std::sin(theta);
        const float cos_theta = std::cos(theta);

        for (uint32_t x = 0; x < kProceduralSkyWidth; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(kProceduralSkyWidth);
            const float phi = (u - 0.5f) * kTwoPi;
            const glm::vec3 direction(std::cos(phi) * sin_theta, cos_theta, std::sin(phi) * sin_theta);
            const glm::vec3 color = proceduralSkyColor(signature, direction);
            const size_t pixel_index =
                (static_cast<size_t>(y) * static_cast<size_t>(kProceduralSkyWidth) + static_cast<size_t>(x)) * 4u;
            pixels[pixel_index + 0] = color.r;
            pixels[pixel_index + 1] = color.g;
            pixels[pixel_index + 2] = color.b;
            pixels[pixel_index + 3] = 1.0f;
        }
    }

    std::vector<uint8_t> bytes(pixels.size() * sizeof(float));
    std::memcpy(bytes.data(), pixels.data(), bytes.size());
    return ImageData{
        .ByteData = std::move(bytes),
        .ImageFormat = render_flow::default_scene_detail::kEnvironmentFormat,
        .Width = kProceduralSkyWidth,
        .Height = kProceduralSkyHeight,
    };
}

ProceduralSkyParams proceduralSkyParams(const EnvironmentResources::SourceSignature& signature)
{
    return ProceduralSkyParams{
        .sun_direction_intensity =
            glm::vec4(safeNormalize(signature.procedural_sun_direction, glm::vec3(0.51214755f, 0.76822126f, 0.38411063f)),
                      signature.procedural_sun_intensity),
        .sky_color_zenith_exposure =
            glm::vec4(signature.procedural_sky_color_zenith, signature.procedural_sky_exposure),
        .sky_color_horizon_sun_radius =
            glm::vec4(signature.procedural_sky_color_horizon, signature.procedural_sun_angular_radius),
        .ground_color = glm::vec4(signature.procedural_ground_color, 0.0f),
        .output_size = glm::uvec4(kProceduralSkyWidth, kProceduralSkyHeight, 0u, 0u),
    };
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

luna::RHI::Ref<luna::RHI::Texture> createProceduralSourceTexture(const luna::RHI::Ref<luna::RHI::Device>& device)
{
    if (!device) {
        return {};
    }

    return device->CreateTexture(
        luna::RHI::TextureBuilder()
            .SetSize(kProceduralSkyWidth, kProceduralSkyHeight)
            .SetFormat(render_flow::default_scene_detail::kEnvironmentFormat)
            .SetUsage(luna::RHI::TextureUsageFlags::Sampled | luna::RHI::TextureUsageFlags::Storage)
            .SetInitialState(luna::RHI::ResourceState::Undefined)
            .SetName("SceneEnvironmentProceduralSkySource")
            .Build());
}

luna::RHI::Ref<luna::RHI::Texture> createCubeTexture(const luna::RHI::Ref<luna::RHI::Device>& device,
                                                     uint32_t size,
                                                     uint32_t mip_levels,
                                                     std::string_view name)
{
    if (!device) {
        return {};
    }

    return device->CreateTexture(
        luna::RHI::TextureBuilder()
            .SetType(luna::RHI::TextureType::TextureCube)
            .SetSize(size, size)
            .SetArrayLayers(6)
            .SetMipLevels(mip_levels)
            .SetFormat(render_flow::default_scene_detail::kEnvironmentIblFormat)
            .SetUsage(luna::RHI::TextureUsageFlags::Sampled | luna::RHI::TextureUsageFlags::Storage)
            .SetInitialState(luna::RHI::ResourceState::Undefined)
            .SetName(std::string(name))
            .Build());
}

luna::RHI::Ref<luna::RHI::Texture> createBrdfLutTexture(const luna::RHI::Ref<luna::RHI::Device>& device)
{
    if (!device) {
        return {};
    }

    return device->CreateTexture(
        luna::RHI::TextureBuilder()
            .SetSize(render_flow::default_scene_detail::kEnvironmentBrdfLutSize,
                     render_flow::default_scene_detail::kEnvironmentBrdfLutSize)
            .SetFormat(render_flow::default_scene_detail::kEnvironmentBrdfLutFormat)
            .SetUsage(luna::RHI::TextureUsageFlags::Sampled | luna::RHI::TextureUsageFlags::Storage)
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
                                const luna::RHI::Ref<luna::RHI::DescriptorSetLayout>& layout,
                                uint32_t push_constant_size)
{
    if (!device || !layout) {
        return {};
    }

    return device->CreatePipelineLayout(luna::RHI::PipelineLayoutBuilder()
                                            .AddSetLayout(layout)
                                            .AddPushConstant(luna::RHI::ShaderStage::Compute, 0, push_constant_size)
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

    return device->CreateComputePipeline(luna::RHI::ComputePipelineBuilder().SetShader(shader).SetLayout(layout).Build());
}

} // namespace

void EnvironmentResources::reset()
{
    m_device.reset();
    m_backend_type = luna::RHI::BackendType::Auto;
    m_source_signature = {};
    m_has_source_signature = false;
    m_source_is_gpu_generated = false;
    m_procedural_source_generated = false;
    m_source_texture = {};
    m_irradiance_sh = {};

    m_environment_cube_texture.reset();
    m_irradiance_texture.reset();
    m_prefiltered_texture.reset();
    m_brdf_lut_texture.reset();
    m_procedural_source_uav.reset();
    m_environment_cube_uav.reset();
    m_irradiance_uav.reset();
    m_prefiltered_uavs = {};
    m_brdf_lut_uav.reset();

    m_procedural_sky_layout.reset();
    m_equirect_to_cube_layout.reset();
    m_cube_filter_layout.reset();
    m_brdf_lut_layout.reset();
    m_procedural_sky_pipeline_layout.reset();
    m_equirect_to_cube_pipeline_layout.reset();
    m_cube_filter_pipeline_layout.reset();
    m_brdf_lut_pipeline_layout.reset();
    m_descriptor_pool.reset();
    m_procedural_sky_descriptor_set.reset();
    m_equirect_to_cube_descriptor_set.reset();
    m_irradiance_descriptor_set.reset();
    m_prefilter_descriptor_sets = {};
    m_brdf_lut_descriptor_set.reset();
    m_sampler.reset();

    m_procedural_sky_shader.reset();
    m_equirect_to_cube_shader.reset();
    m_irradiance_shader.reset();
    m_prefilter_shader.reset();
    m_brdf_lut_shader.reset();
    m_procedural_sky_pipeline.reset();
    m_equirect_to_cube_pipeline.reset();
    m_irradiance_pipeline.reset();
    m_prefilter_pipeline.reset();
    m_brdf_lut_pipeline.reset();
    m_precomputed = false;
}

void EnvironmentResources::ensure(const SceneRenderContext& context,
                                  const RenderEnvironment* environment,
                                  const SceneShaderPaths& shader_paths)
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

    const SourceSignature requested_signature = requestedSourceSignature(environment);
    SourceSignature source_signature_to_prepare = requested_signature;
    bool requested_texture_is_available = false;
    std::shared_ptr<Texture> requested_texture;
    const bool source_matches_request =
        m_has_source_signature && m_source_texture.texture && sameSourceSignature(m_source_signature, requested_signature);

    if (!source_matches_request && requested_signature.kind == SourceKind::TextureAsset) {
        requested_texture =
            environment != nullptr && environment->allow_async_load
                ? AssetManager::get().requestAssetAs<Texture>(requested_signature.texture_handle)
                : AssetManager::get().loadAssetAs<Texture>(requested_signature.texture_handle);
        requested_texture_is_available = isUsableEnvironmentTexture(requested_texture);

        if (!requested_texture_is_available) {
            if (requested_texture) {
                LUNA_RENDERER_WARN(
                    "Environment texture asset '{}' is not a valid RGBA32_FLOAT equirect map; using procedural sky",
                    requested_signature.texture_handle.toString());
            }

            const bool texture_load_is_pending =
                requested_texture == nullptr && environment != nullptr && environment->allow_async_load &&
                AssetManager::get().isAssetLoading(requested_signature.texture_handle);
            source_signature_to_prepare =
                texture_load_is_pending && m_source_texture.texture ? m_source_signature : defaultSkySignature(environment);
        }
    }

    if (m_has_source_signature && !sameSourceSignature(m_source_signature, source_signature_to_prepare)) {
        const luna::RHI::Ref<luna::RHI::Device> device = m_device;
        const luna::RHI::BackendType backend_type = m_backend_type;
        reset();
        m_device = device;
        m_backend_type = backend_type;
    }

    if (!m_source_texture.texture) {
        if (source_signature_to_prepare.kind == SourceKind::TextureAsset && requested_texture_is_available &&
            requested_texture) {
            ImageData environment_image = requested_texture->getImageData();
            m_irradiance_sh = renderer_detail::computeDiffuseIrradianceSH(
                environment_image, render_flow::default_scene_detail::kEnvironmentFormat);
            environment_image = renderer_detail::generateEnvironmentMipChain(
                environment_image, render_flow::default_scene_detail::kEnvironmentFormat);
            m_source_texture = renderer_detail::createTextureUpload(
                context.device, environment_image, Texture::SamplerSettings{}, "SceneEnvironmentSource");
            m_source_is_gpu_generated = false;
            m_procedural_source_generated = false;
            LUNA_RENDERER_INFO("Prepared scene environment texture asset '{}' ({}x{}, mips={})",
                               source_signature_to_prepare.texture_handle.toString(),
                               environment_image.Width,
                               environment_image.Height,
                               1u + static_cast<uint32_t>(environment_image.MipLevels.size()));
        } else {
            ImageData procedural_sky_image = createProceduralSkyImageData(source_signature_to_prepare);
            m_irradiance_sh = renderer_detail::computeDiffuseIrradianceSH(
                procedural_sky_image, render_flow::default_scene_detail::kEnvironmentFormat);
            m_source_texture.texture = createProceduralSourceTexture(context.device);
            m_source_texture.debug_name = "SceneEnvironmentProceduralSkySource";
            m_source_texture.uploaded = true;
            m_source_is_gpu_generated = true;
            m_procedural_source_generated = false;
            if (m_source_texture.texture) {
                m_procedural_source_uav = m_source_texture.texture->CreateView(
                    texture2DStorageViewDesc(render_flow::default_scene_detail::kEnvironmentFormat));
            }
            LUNA_RENDERER_INFO("Prepared GPU procedural scene environment source texture ({}x{})",
                               kProceduralSkyWidth,
                               kProceduralSkyHeight);
        }

        m_source_signature = source_signature_to_prepare;
        m_has_source_signature = true;
        m_precomputed = false;
    }

    if (!m_environment_cube_texture) {
        m_environment_cube_texture = createCubeTexture(
            context.device, render_flow::default_scene_detail::kEnvironmentCubeSize, 1, "SceneEnvironmentCube");
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

    if (m_source_is_gpu_generated && !m_procedural_source_uav && m_source_texture.texture) {
        m_procedural_source_uav =
            m_source_texture.texture->CreateView(texture2DStorageViewDesc(render_flow::default_scene_detail::kEnvironmentFormat));
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
        for (uint32_t mip_level = 0; mip_level < render_flow::default_scene_detail::kEnvironmentPrefilterMipLevels;
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
        m_procedural_sky_layout =
            context.device->CreateDescriptorSetLayout(luna::RHI::DescriptorSetLayoutBuilder()
                                                          .AddBinding(procedural_sky_binding::OutputTexture,
                                                                      luna::RHI::DescriptorType::StorageImage,
                                                                      1,
                                                                      luna::RHI::ShaderStage::Compute)
                                                          .Build());
        m_equirect_to_cube_layout =
            context.device->CreateDescriptorSetLayout(luna::RHI::DescriptorSetLayoutBuilder()
                                                          .AddBinding(environment_binding::SourceTexture,
                                                                      luna::RHI::DescriptorType::SampledImage,
                                                                      1,
                                                                      luna::RHI::ShaderStage::Compute)
                                                          .AddBinding(environment_binding::Sampler,
                                                                      luna::RHI::DescriptorType::Sampler,
                                                                      1,
                                                                      luna::RHI::ShaderStage::Compute)
                                                          .AddBinding(environment_binding::OutputTexture,
                                                                      luna::RHI::DescriptorType::StorageImage,
                                                                      1,
                                                                      luna::RHI::ShaderStage::Compute)
                                                          .Build());
        m_cube_filter_layout =
            context.device->CreateDescriptorSetLayout(luna::RHI::DescriptorSetLayoutBuilder()
                                                          .AddBinding(environment_binding::Sampler,
                                                                      luna::RHI::DescriptorType::Sampler,
                                                                      1,
                                                                      luna::RHI::ShaderStage::Compute)
                                                          .AddBinding(environment_binding::OutputTexture,
                                                                      luna::RHI::DescriptorType::StorageImage,
                                                                      1,
                                                                      luna::RHI::ShaderStage::Compute)
                                                          .AddBinding(environment_binding::SourceCubeTexture,
                                                                      luna::RHI::DescriptorType::SampledImage,
                                                                      1,
                                                                      luna::RHI::ShaderStage::Compute)
                                                          .Build());
        m_brdf_lut_layout =
            context.device->CreateDescriptorSetLayout(luna::RHI::DescriptorSetLayoutBuilder()
                                                          .AddBinding(environment_binding::BrdfLutOutputTexture,
                                                                      luna::RHI::DescriptorType::StorageImage,
                                                                      1,
                                                                      luna::RHI::ShaderStage::Compute)
                                                          .Build());
    }

    if (!m_descriptor_pool) {
        m_descriptor_pool =
            context.device->CreateDescriptorPool(luna::RHI::DescriptorPoolBuilder()
                                                     .SetMaxSets(16)
                                                     .AddPoolSize(luna::RHI::DescriptorType::SampledImage, 16)
                                                     .AddPoolSize(luna::RHI::DescriptorType::Sampler, 16)
                                                     .AddPoolSize(luna::RHI::DescriptorType::StorageImage, 16)
                                                     .Build());
    }

    if (m_descriptor_pool && !m_equirect_to_cube_descriptor_set) {
        m_procedural_sky_descriptor_set = m_descriptor_pool->AllocateDescriptorSet(m_procedural_sky_layout);
        m_equirect_to_cube_descriptor_set = m_descriptor_pool->AllocateDescriptorSet(m_equirect_to_cube_layout);
        m_irradiance_descriptor_set = m_descriptor_pool->AllocateDescriptorSet(m_cube_filter_layout);
        for (auto& descriptor_set : m_prefilter_descriptor_sets) {
            descriptor_set = m_descriptor_pool->AllocateDescriptorSet(m_cube_filter_layout);
        }
        m_brdf_lut_descriptor_set = m_descriptor_pool->AllocateDescriptorSet(m_brdf_lut_layout);
    }

    if (!m_equirect_to_cube_pipeline_layout) {
        m_procedural_sky_pipeline_layout =
            createComputePipelineLayout(context.device, m_procedural_sky_layout, sizeof(ProceduralSkyParams));
        m_equirect_to_cube_pipeline_layout =
            createComputePipelineLayout(context.device, m_equirect_to_cube_layout, sizeof(IblDispatchParams));
        m_cube_filter_pipeline_layout =
            createComputePipelineLayout(context.device, m_cube_filter_layout, sizeof(IblDispatchParams));
        m_brdf_lut_pipeline_layout =
            createComputePipelineLayout(context.device, m_brdf_lut_layout, sizeof(IblDispatchParams));
    }

    if (!context.compiler) {
        LUNA_RENDERER_WARN("Scene environment IBL precompute cannot compile shaders because compiler is null");
        return;
    }

    if (!m_equirect_to_cube_shader) {
        m_procedural_sky_shader = renderer_detail::loadShaderModule(context.device,
                                                                    context.compiler,
                                                                    shader_paths.procedural_sky_path,
                                                                    "proceduralSkyEquirectMain",
                                                                    luna::RHI::ShaderStage::Compute);
        m_equirect_to_cube_shader = renderer_detail::loadShaderModule(context.device,
                                                                      context.compiler,
                                                                      shader_paths.environment_ibl_path,
                                                                      "environmentEquirectToCubeMain",
                                                                      luna::RHI::ShaderStage::Compute);
        m_irradiance_shader = renderer_detail::loadShaderModule(context.device,
                                                                context.compiler,
                                                                shader_paths.environment_ibl_path,
                                                                "environmentIrradianceMain",
                                                                luna::RHI::ShaderStage::Compute);
        m_prefilter_shader = renderer_detail::loadShaderModule(context.device,
                                                               context.compiler,
                                                               shader_paths.environment_ibl_path,
                                                               "environmentPrefilterMain",
                                                               luna::RHI::ShaderStage::Compute);
        m_brdf_lut_shader = renderer_detail::loadShaderModule(context.device,
                                                              context.compiler,
                                                              shader_paths.environment_ibl_path,
                                                              "environmentBrdfLutMain",
                                                              luna::RHI::ShaderStage::Compute);
    }

    if (!m_equirect_to_cube_pipeline) {
        m_procedural_sky_pipeline =
            createComputePipeline(context.device, m_procedural_sky_pipeline_layout, m_procedural_sky_shader);
        m_equirect_to_cube_pipeline =
            createComputePipeline(context.device, m_equirect_to_cube_pipeline_layout, m_equirect_to_cube_shader);
        m_irradiance_pipeline =
            createComputePipeline(context.device, m_cube_filter_pipeline_layout, m_irradiance_shader);
        m_prefilter_pipeline = createComputePipeline(context.device, m_cube_filter_pipeline_layout, m_prefilter_shader);
        m_brdf_lut_pipeline = createComputePipeline(context.device, m_brdf_lut_pipeline_layout, m_brdf_lut_shader);
    }
}

void EnvironmentResources::uploadIfNeeded(luna::RHI::CommandBufferEncoder& commands)
{
    if (m_source_is_gpu_generated) {
        return;
    }

    if (!m_source_texture.uploaded) {
        LUNA_RENDERER_DEBUG("Uploading environment source texture '{}'", m_source_texture.debug_name);
    }
    renderer_detail::uploadTextureIfNeeded(commands,
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
        std::any_of(
            m_prefilter_descriptor_sets.begin(), m_prefilter_descriptor_sets.end(), [](const auto& descriptor_set) {
                return descriptor_set == nullptr;
            })) {
        LUNA_RENDERER_WARN("Skipping environment IBL precompute because GPU resources are incomplete");
        return;
    }

    if (m_source_is_gpu_generated) {
        if (!m_procedural_sky_pipeline || !m_procedural_sky_descriptor_set || !m_procedural_source_uav) {
            LUNA_RENDERER_WARN("Skipping procedural environment generation because GPU resources are incomplete");
            return;
        }

        if (!m_procedural_source_generated) {
            transitionTexture(commands,
                              m_source_texture.texture,
                              luna::RHI::ResourceState::Undefined,
                              luna::RHI::ResourceState::UnorderedAccess);
            m_procedural_sky_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
                .Binding = procedural_sky_binding::OutputTexture,
                .TextureView = m_procedural_source_uav,
                .Layout = luna::RHI::ResourceState::UnorderedAccess,
                .Type = luna::RHI::DescriptorType::StorageImage,
            });
            m_procedural_sky_descriptor_set->Update();

            const ProceduralSkyParams params = proceduralSkyParams(m_source_signature);
            const std::array<luna::RHI::Ref<luna::RHI::DescriptorSet>, 1> descriptor_sets{
                m_procedural_sky_descriptor_set};
            commands.BindComputePipeline(m_procedural_sky_pipeline);
            commands.BindComputeDescriptorSets(m_procedural_sky_pipeline, 0, descriptor_sets);
            commands.ComputePushConstants(
                m_procedural_sky_pipeline, luna::RHI::ShaderStage::Compute, 0, sizeof(ProceduralSkyParams), &params);
            commands.Dispatch(divideRoundUp(kProceduralSkyWidth, 8u), divideRoundUp(kProceduralSkyHeight, 8u), 1);
            commands.MemoryBarrierFast(luna::RHI::MemoryTransition::AllWriteToAllRead);
            transitionTexture(commands,
                              m_source_texture.texture,
                              luna::RHI::ResourceState::UnorderedAccess,
                              luna::RHI::ResourceState::ShaderRead);
            m_procedural_source_generated = true;
        }
    } else {
        uploadIfNeeded(commands);
    }

    LUNA_RENDERER_INFO("Precomputing scene environment IBL maps on GPU");

    auto dispatchCube = [&commands](const luna::RHI::Ref<luna::RHI::ComputePipeline>& pipeline,
                                    const luna::RHI::Ref<luna::RHI::DescriptorSet>& descriptor_set,
                                    const IblDispatchParams& params) {
        const std::array<luna::RHI::Ref<luna::RHI::DescriptorSet>, 1> descriptor_sets{descriptor_set};
        commands.BindComputePipeline(pipeline);
        commands.BindComputeDescriptorSets(pipeline, 0, descriptor_sets);
        commands.ComputePushConstants(pipeline, luna::RHI::ShaderStage::Compute, 0, sizeof(IblDispatchParams), &params);
        commands.Dispatch(divideRoundUp(params.output_size, 8u), divideRoundUp(params.output_size, 8u), 6);
    };

    auto dispatch2D = [&commands](const luna::RHI::Ref<luna::RHI::ComputePipeline>& pipeline,
                                  const luna::RHI::Ref<luna::RHI::DescriptorSet>& descriptor_set,
                                  const IblDispatchParams& params) {
        const std::array<luna::RHI::Ref<luna::RHI::DescriptorSet>, 1> descriptor_sets{descriptor_set};
        commands.BindComputePipeline(pipeline);
        commands.BindComputeDescriptorSets(pipeline, 0, descriptor_sets);
        commands.ComputePushConstants(pipeline, luna::RHI::ShaderStage::Compute, 0, sizeof(IblDispatchParams), &params);
        commands.Dispatch(divideRoundUp(params.output_size, 8u), divideRoundUp(params.output_size, 8u), 1);
    };

    transitionTexture(commands,
                      m_environment_cube_texture,
                      luna::RHI::ResourceState::Undefined,
                      luna::RHI::ResourceState::UnorderedAccess);
    m_equirect_to_cube_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = environment_binding::SourceTexture,
        .TextureView = m_source_texture.texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    m_equirect_to_cube_descriptor_set->WriteSampler(
        luna::RHI::SamplerWriteInfo{.Binding = environment_binding::Sampler, .Sampler = m_sampler});
    m_equirect_to_cube_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = environment_binding::OutputTexture,
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

    transitionTexture(
        commands, m_irradiance_texture, luna::RHI::ResourceState::Undefined, luna::RHI::ResourceState::UnorderedAccess);
    m_irradiance_descriptor_set->WriteSampler(
        luna::RHI::SamplerWriteInfo{.Binding = environment_binding::Sampler, .Sampler = m_sampler});
    m_irradiance_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = environment_binding::OutputTexture,
        .TextureView = m_irradiance_uav,
        .Layout = luna::RHI::ResourceState::UnorderedAccess,
        .Type = luna::RHI::DescriptorType::StorageImage,
    });
    m_irradiance_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = environment_binding::SourceCubeTexture,
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
        prefilter_descriptor_set->WriteSampler(
            luna::RHI::SamplerWriteInfo{.Binding = environment_binding::Sampler, .Sampler = m_sampler});
        prefilter_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
            .Binding = environment_binding::OutputTexture,
            .TextureView = m_prefiltered_uavs[mip_level],
            .Layout = luna::RHI::ResourceState::UnorderedAccess,
            .Type = luna::RHI::DescriptorType::StorageImage,
        });
        prefilter_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
            .Binding = environment_binding::SourceCubeTexture,
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

    transitionTexture(
        commands, m_brdf_lut_texture, luna::RHI::ResourceState::Undefined, luna::RHI::ResourceState::UnorderedAccess);
    m_brdf_lut_descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = environment_binding::BrdfLutOutputTexture,
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
    transitionTexture(
        commands, m_brdf_lut_texture, luna::RHI::ResourceState::UnorderedAccess, luna::RHI::ResourceState::ShaderRead);

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
