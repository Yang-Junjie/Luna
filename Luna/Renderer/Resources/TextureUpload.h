#pragma once

#include "Renderer/Texture.h"

#include <Barrier.h>
#include <CommandBufferEncoder.h>
#include <Core.h>
#include <string>
#include <string_view>
#include <vector>

namespace luna::RHI {
class Buffer;
class Device;
class Sampler;
class Texture;
} // namespace luna::RHI

namespace luna::renderer_detail {

struct PendingTextureUpload {
    luna::RHI::Ref<luna::RHI::Texture> texture;
    luna::RHI::Ref<luna::RHI::Sampler> sampler;
    luna::RHI::Ref<luna::RHI::Buffer> staging_buffer;
    std::vector<luna::RHI::BufferImageCopy> copy_regions;
    std::string debug_name;
    bool uploaded{false};

    [[nodiscard]] bool isValid() const noexcept
    {
        return texture != nullptr;
    }
};

PendingTextureUpload createTextureUpload(const luna::RHI::Ref<luna::RHI::Device>& device,
                                         const luna::ImageData& image,
                                         const luna::Texture::SamplerSettings& sampler_settings,
                                         std::string_view debug_name);
void uploadTextureIfNeeded(luna::RHI::CommandBufferEncoder& commands,
                           PendingTextureUpload& uploaded_texture,
                           luna::RHI::ResourceState final_state = luna::RHI::ResourceState::ShaderRead,
                           luna::RHI::SyncScope final_stage = luna::RHI::SyncScope::FragmentStage);

} // namespace luna::renderer_detail
