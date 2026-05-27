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
    RHI::Ref<RHI::Texture> texture;
    RHI::Ref<RHI::Sampler> sampler;
    RHI::Ref<RHI::Buffer> staging_buffer;
    std::vector<RHI::BufferImageCopy> copy_regions;
    std::string debug_name;
    bool uploaded{false};

    [[nodiscard]] bool isValid() const noexcept
    {
        return texture != nullptr;
    }
};

PendingTextureUpload createTextureUpload(const RHI::Ref<RHI::Device>& device,
                                         const luna::ImageData& image,
                                         const luna::Texture::SamplerSettings& sampler_settings,
                                         std::string_view debug_name);
void uploadTextureIfNeeded(RHI::CommandBufferEncoder& commands,
                           PendingTextureUpload& uploaded_texture,
                           RHI::ResourceState final_state = RHI::ResourceState::ShaderRead,
                           RHI::SyncScope final_stage = RHI::SyncScope::FragmentStage);

} // namespace luna::renderer_detail
