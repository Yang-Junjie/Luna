#include "Renderer/Resources/TextureUpload.h"

#include "Core/Log.h"
#include "Renderer/RendererUtilities.h"

#include <Builders.h>
#include <Device.h>

#include <algorithm>
#include <array>
#include <cstring>

namespace luna::renderer_detail {
namespace {

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

} // namespace

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

} // namespace luna::renderer_detail
