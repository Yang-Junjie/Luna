#include "Renderer/SceneRenderer/SceneRendererSupport.h"

#include "Core/Log.h"
#include "Renderer/RendererUtilities.h"

#include <Builders.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/trigonometric.hpp>

namespace luna::scene_renderer_detail {

namespace {

glm::mat4 adjustProjectionForBackend(glm::mat4 projection, luna::RHI::BackendType backend_type)
{
    if (backend_type == luna::RHI::BackendType::Vulkan) {
        projection[1][1] *= -1.0f;
    }
    return projection;
}

} // namespace

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
    return adjustProjectionForBackend(camera.getProjectionMatrix(aspect_ratio), backend_type) * camera.getViewMatrix();
}

glm::vec3 resolveCameraPosition(const Camera& camera)
{
    return camera.getPosition();
}

float materialBlendModeToFloat(luna::Material::BlendMode blend_mode)
{
    switch (blend_mode) {
        case luna::Material::BlendMode::Masked:
            return 1.0f;
        case luna::Material::BlendMode::Additive:
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

luna::RHI::Filter toRhiFilter(luna::Texture::FilterMode filter_mode)
{
    switch (filter_mode) {
        case luna::Texture::FilterMode::Nearest:
            return luna::RHI::Filter::Nearest;
        case luna::Texture::FilterMode::Linear:
        default:
            return luna::RHI::Filter::Linear;
    }
}

luna::RHI::SamplerMipmapMode toRhiMipmapMode(luna::Texture::MipFilterMode mip_filter_mode)
{
    switch (mip_filter_mode) {
        case luna::Texture::MipFilterMode::Nearest:
            return luna::RHI::SamplerMipmapMode::Nearest;
        case luna::Texture::MipFilterMode::None:
        case luna::Texture::MipFilterMode::Linear:
        default:
            return luna::RHI::SamplerMipmapMode::Linear;
    }
}

luna::RHI::SamplerAddressMode toRhiAddressMode(luna::Texture::WrapMode wrap_mode)
{
    switch (wrap_mode) {
        case luna::Texture::WrapMode::MirroredRepeat:
            return luna::RHI::SamplerAddressMode::MirroredRepeat;
        case luna::Texture::WrapMode::ClampToEdge:
            return luna::RHI::SamplerAddressMode::ClampToEdge;
        case luna::Texture::WrapMode::ClampToBorder:
            return luna::RHI::SamplerAddressMode::ClampToBorder;
        case luna::Texture::WrapMode::MirrorClampToEdge:
            return luna::RHI::SamplerAddressMode::MirrorClampToEdge;
        case luna::Texture::WrapMode::Repeat:
        default:
            return luna::RHI::SamplerAddressMode::Repeat;
    }
}

float transparentSortDistanceSq(const glm::mat4& transform, const glm::vec3& camera_position)
{
    const glm::vec3 object_position(transform[3]);
    return glm::length2(object_position - camera_position);
}

luna::ImageData createFallbackColorImageData(const glm::vec4& color)
{
    const glm::vec4 clamped_color = glm::clamp(color, glm::vec4(0.0f), glm::vec4(1.0f));
    auto to_byte = [](float channel) {
        return static_cast<uint8_t>(std::lround(channel * 255.0f));
    };

    return luna::ImageData{
        .ByteData = {to_byte(clamped_color.r),
                     to_byte(clamped_color.g),
                     to_byte(clamped_color.b),
                     to_byte(clamped_color.a)},
        .ImageFormat = luna::RHI::Format::RGBA8_UNORM,
        .Width = 1,
        .Height = 1,
    };
}

luna::ImageData createFallbackMetallicRoughnessImageData(float roughness, float metallic)
{
    return createFallbackColorImageData(glm::vec4(0.0f, roughness, metallic, 1.0f));
}

luna::ImageData createFallbackFloatImageData(const glm::vec4& value)
{
    std::vector<uint8_t> bytes(sizeof(float) * 4u, 0);
    std::memcpy(bytes.data(), &value[0], bytes.size());
    return luna::ImageData{
        .ByteData = std::move(bytes),
        .ImageFormat = kEnvironmentFormat,
        .Width = 1,
        .Height = 1,
    };
}

std::array<glm::vec4, 9> computeDiffuseIrradianceSH(const luna::ImageData& image)
{
    std::array<glm::vec4, 9> result{};
    if (!image.isValid() || image.ImageFormat != kEnvironmentFormat ||
        image.ByteData.size() != static_cast<size_t>(image.Width) * static_cast<size_t>(image.Height) * 4u * sizeof(float)) {
        return result;
    }

    const auto* pixels = reinterpret_cast<const float*>(image.ByteData.data());
    std::array<glm::dvec3, 9> coefficients{};
    const double width = static_cast<double>(image.Width);
    const double height = static_cast<double>(image.Height);
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kTwoPi = kPi * 2.0;
    constexpr double kFourPi = kPi * 4.0;
    const double d_phi = kTwoPi / width;
    const double d_theta = kPi / height;
    double total_weight = 0.0;

    for (uint32_t y = 0; y < image.Height; ++y) {
        const double theta = kPi * (static_cast<double>(y) + 0.5) / height;
        const double sin_theta = std::sin(theta);
        const double cos_theta = std::cos(theta);

        for (uint32_t x = 0; x < image.Width; ++x) {
            const double phi = kTwoPi * (static_cast<double>(x) + 0.5) / width - kPi;
            const double sin_phi = std::sin(phi);
            const double cos_phi = std::cos(phi);
            const glm::dvec3 direction(cos_phi * sin_theta, cos_theta, sin_phi * sin_theta);
            const double weight = sin_theta * d_theta * d_phi;
            const size_t pixel_index = (static_cast<size_t>(y) * image.Width + x) * 4u;
            const glm::dvec3 radiance(static_cast<double>(pixels[pixel_index + 0]),
                                      static_cast<double>(pixels[pixel_index + 1]),
                                      static_cast<double>(pixels[pixel_index + 2]));

            const std::array<double, 9> basis{
                0.282095,
                0.488603 * direction.y,
                0.488603 * direction.z,
                0.488603 * direction.x,
                1.092548 * direction.x * direction.y,
                1.092548 * direction.y * direction.z,
                0.315392 * (3.0 * direction.z * direction.z - 1.0),
                1.092548 * direction.x * direction.z,
                0.546274 * (direction.x * direction.x - direction.y * direction.y),
            };

            for (size_t basis_index = 0; basis_index < basis.size(); ++basis_index) {
                coefficients[basis_index] += radiance * (basis[basis_index] * weight);
            }
            total_weight += weight;
        }
    }

    if (total_weight <= 0.0) {
        return result;
    }

    const double normalization = kFourPi / total_weight;
    constexpr std::array<double, 9> lambert_band_scale{
        kPi,
        kTwoPi / 3.0,
        kTwoPi / 3.0,
        kTwoPi / 3.0,
        kPi / 4.0,
        kPi / 4.0,
        kPi / 4.0,
        kPi / 4.0,
        kPi / 4.0,
    };

    for (size_t coefficient_index = 0; coefficient_index < coefficients.size(); ++coefficient_index) {
        const glm::dvec3 irradiance = coefficients[coefficient_index] * (normalization * lambert_band_scale[coefficient_index]);
        result[coefficient_index] = glm::vec4(glm::vec3(irradiance), 0.0f);
    }

    return result;
}

luna::ImageData generateEnvironmentMipChain(const luna::ImageData& source)
{
    if (!source.isValid() || source.ImageFormat != kEnvironmentFormat ||
        source.ByteData.size() !=
            static_cast<size_t>(source.Width) * static_cast<size_t>(source.Height) * 4u * sizeof(float)) {
        return source;
    }

    luna::ImageData result = source;
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

PendingTextureUpload createTextureUpload(const luna::RHI::Ref<luna::RHI::Device>& device,
                                         const luna::ImageData& image,
                                         const luna::Texture::SamplerSettings& sampler_settings,
                                         std::string_view debug_name)
{
    PendingTextureUpload uploaded_texture;
    uploaded_texture.debug_name = std::string(debug_name);
    if (!device || !image.isValid()) {
        LUNA_RENDERER_WARN("Cannot create uploaded texture '{}': device_available={} image_valid={}",
                           uploaded_texture.debug_name,
                           static_cast<bool>(device),
                           image.isValid());
        return uploaded_texture;
    }

    constexpr size_t kTextureDataPlacementAlignment = 512;
    constexpr uint32_t kTextureDataPitchAlignment = 256;
    const uint32_t mip_level_count = 1u + static_cast<uint32_t>(image.MipLevels.size());

    LUNA_RENDERER_DEBUG("Creating uploaded texture '{}' ({}x{}, mips={}, format={} ({}), bytes={})",
                        uploaded_texture.debug_name,
                        image.Width,
                        image.Height,
                        mip_level_count,
                        renderer_detail::formatToString(image.ImageFormat),
                        static_cast<int>(image.ImageFormat),
                        image.ByteData.size());

    uploaded_texture.texture = device->CreateTexture(luna::RHI::TextureBuilder()
                                                         .SetSize(image.Width, image.Height)
                                                         .SetMipLevels(mip_level_count)
                                                         .SetFormat(image.ImageFormat)
                                                         .SetUsage(luna::RHI::TextureUsageFlags::Sampled |
                                                                   luna::RHI::TextureUsageFlags::TransferDst)
                                                         .SetInitialState(luna::RHI::ResourceState::Undefined)
                                                         .SetName(uploaded_texture.debug_name)
                                                         .Build());

    const float max_lod = sampler_settings.MipFilter == luna::Texture::MipFilterMode::None
                              ? 0.0f
                              : static_cast<float>((std::max)(mip_level_count, 1u) - 1u);
    uploaded_texture.sampler = device->CreateSampler(luna::RHI::SamplerBuilder()
                                                         .SetMinFilter(toRhiFilter(sampler_settings.MinFilter))
                                                         .SetMagFilter(toRhiFilter(sampler_settings.MagFilter))
                                                         .SetMipmapMode(toRhiMipmapMode(sampler_settings.MipFilter))
                                                         .SetAddressModeU(toRhiAddressMode(sampler_settings.WrapU))
                                                         .SetAddressModeV(toRhiAddressMode(sampler_settings.WrapV))
                                                         .SetAddressModeW(toRhiAddressMode(sampler_settings.WrapW))
                                                         .SetLodRange(0.0f, max_lod)
                                                         .SetAnisotropy(false)
                                                         .SetName(uploaded_texture.debug_name + "_Sampler")
                                                         .Build());

    struct PackedMipRegion {
        const std::vector<uint8_t>* source = nullptr;
        size_t offset = 0;
        uint32_t row_pitch_bytes = 0;
        uint32_t height = 0;
    };

    auto alignUp = [](size_t value, size_t alignment) -> size_t {
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
        const uint32_t aligned_row_pitch = static_cast<uint32_t>(alignUp(row_pitch_bytes, kTextureDataPitchAlignment));
        const uint32_t row_length_texels = bytes_per_texel > 0 ? aligned_row_pitch / bytes_per_texel : safe_width;

        buffer_offset = alignUp(buffer_offset, kTextureDataPlacementAlignment);
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
        LUNA_RENDERER_WARN("Failed to create uploaded texture '{}': texture={} sampler={} staging_size={}",
                           uploaded_texture.debug_name,
                           static_cast<bool>(uploaded_texture.texture),
                           static_cast<bool>(uploaded_texture.sampler),
                           buffer_offset);
        return uploaded_texture;
    }

    uploaded_texture.staging_buffer = device->CreateBuffer(luna::RHI::BufferBuilder()
                                                               .SetSize(buffer_offset)
                                                               .SetUsage(luna::RHI::BufferUsageFlags::TransferSrc)
                                                               .SetMemoryUsage(luna::RHI::BufferMemoryUsage::CpuToGpu)
                                                               .SetName(uploaded_texture.debug_name + "_Staging")
                                                               .Build());
    if (!uploaded_texture.staging_buffer) {
        LUNA_RENDERER_WARN("Failed to create staging buffer for texture '{}' ({} bytes)",
                           uploaded_texture.debug_name,
                           buffer_offset);
        return uploaded_texture;
    }

    if (void* mapped = uploaded_texture.staging_buffer->Map()) {
        auto* destination = static_cast<uint8_t*>(mapped);
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
    } else {
        LUNA_RENDERER_WARN("Failed to map staging buffer for texture '{}'", uploaded_texture.debug_name);
    }

    return uploaded_texture;
}

void uploadTextureIfNeeded(luna::RHI::CommandBufferEncoder& commands,
                           PendingTextureUpload& uploaded_texture,
                           luna::RHI::ResourceState final_state,
                           luna::RHI::SyncScope final_stage)
{
    if (uploaded_texture.uploaded || !uploaded_texture.texture || !uploaded_texture.staging_buffer ||
        uploaded_texture.copy_regions.empty()) {
        if (!uploaded_texture.uploaded &&
            (!uploaded_texture.texture || !uploaded_texture.staging_buffer || uploaded_texture.copy_regions.empty())) {
            LUNA_RENDERER_WARN("Skipping texture upload '{}': texture={} staging_buffer={} copy_regions={}",
                               uploaded_texture.debug_name,
                               static_cast<bool>(uploaded_texture.texture),
                               static_cast<bool>(uploaded_texture.staging_buffer),
                               uploaded_texture.copy_regions.size());
        }
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
    commands.PipelineBarrier(luna::RHI::SyncScope::TransferStage,
                             final_stage,
                             std::array{luna::RHI::TextureBarrier{
                                 .Texture = uploaded_texture.texture,
                                 .OldState = luna::RHI::ResourceState::CopyDest,
                                 .NewState = final_state,
                                 .SubresourceRange = full_range,
                             }});

    uploaded_texture.uploaded = true;
    LUNA_RENDERER_DEBUG("Uploaded texture '{}' to GPU with {} copy region(s)",
                        uploaded_texture.debug_name,
                        uploaded_texture.copy_regions.size());
}

} // namespace luna::scene_renderer_detail
