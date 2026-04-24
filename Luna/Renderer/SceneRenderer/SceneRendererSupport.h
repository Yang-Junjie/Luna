#pragma once

// Shared helper types and utility functions used across scene-renderer internals.
// Centralizes common GPU parameter structs, texture upload helpers, math helpers,
// and fallback asset generation to keep implementation files smaller and consistent.

#include "Renderer/Camera.h"
#include "Renderer/Material.h"
#include "Renderer/Texture.h"

#include <Buffer.h>
#include <CommandBufferEncoder.h>
#include <Core.h>
#include <Device.h>
#include <Instance.h>
#include <Pipeline.h>
#include <Sampler.h>
#include <ShaderCompiler.h>
#include <ShaderModule.h>

#include <array>
#include <filesystem>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace luna::scene_renderer_detail {

inline constexpr luna::RHI::Format kGBufferBaseColorFormat = luna::RHI::Format::RGBA8_UNORM;
inline constexpr luna::RHI::Format kGBufferLightingFormat = luna::RHI::Format::RGBA16_FLOAT;
inline constexpr luna::RHI::Format kScenePickingFormat = luna::RHI::Format::R32_UINT;
inline constexpr luna::RHI::Format kEnvironmentFormat = luna::RHI::Format::RGBA32_FLOAT;
inline constexpr float kDefaultMaterialAlphaCutoff = 0.5f;
inline constexpr float kEnvironmentFallbackValue = 0.08f;

struct MeshPushConstants {
    glm::mat4 model{1.0f};
    uint32_t picking_id{0};
    uint32_t padding[3]{0, 0, 0};
};

struct SceneGpuParams {
    glm::mat4 view_projection{1.0f};
    glm::mat4 inverse_view_projection{1.0f};
    glm::vec4 camera_position_env_mip{0.0f, 0.0f, 0.0f, 0.0f};
    glm::vec4 light_direction_intensity{0.45f, 0.80f, 0.35f, 4.0f};
    glm::vec4 light_color_exposure{1.0f, 0.98f, 0.95f, 1.0f};
    glm::vec4 ibl_factors{1.0f, 1.0f, 1.0f, 0.0f};
    glm::vec4 debug_overlay_params{0.0f, 0.65f, 0.0f, 0.0f};
    glm::vec4 debug_pick_marker{0.0f, 0.0f, 0.0f, 1.0f};
    std::array<glm::vec4, 9> irradiance_sh{};
};

struct MaterialGpuParams {
    glm::vec4 base_color_factor{1.0f};
    glm::vec4 emissive_factor_normal_scale{0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 material_factors{0.0f, 1.0f, 1.0f, kDefaultMaterialAlphaCutoff};
    glm::vec4 material_flags{0.0f};
};

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

std::filesystem::path projectRoot();
luna::RHI::Ref<luna::RHI::ShaderModule> loadShaderModule(const luna::RHI::Ref<luna::RHI::Device>& device,
                                                         const luna::RHI::Ref<luna::RHI::ShaderCompiler>& compiler,
                                                         const std::filesystem::path& path,
                                                         std::string_view entry_point,
                                                         luna::RHI::ShaderStage stage);
glm::mat4 buildViewProjection(const Camera& camera, float aspect_ratio, luna::RHI::BackendType backend_type);
glm::vec3 resolveCameraPosition(const Camera& camera);
float materialBlendModeToFloat(luna::Material::BlendMode blend_mode);
luna::RHI::ColorBlendAttachmentState makeAlphaBlendAttachment();
luna::RHI::Filter toRhiFilter(luna::Texture::FilterMode filter_mode);
luna::RHI::SamplerMipmapMode toRhiMipmapMode(luna::Texture::MipFilterMode mip_filter_mode);
luna::RHI::SamplerAddressMode toRhiAddressMode(luna::Texture::WrapMode wrap_mode);
float transparentSortDistanceSq(const glm::mat4& transform, const glm::vec3& camera_position);

luna::ImageData createFallbackColorImageData(const glm::vec4& color);
luna::ImageData createFallbackMetallicRoughnessImageData(float roughness, float metallic);
luna::ImageData createFallbackFloatImageData(const glm::vec4& value);
std::array<glm::vec4, 9> computeDiffuseIrradianceSH(const luna::ImageData& image);
luna::ImageData generateEnvironmentMipChain(const luna::ImageData& source);

PendingTextureUpload createTextureUpload(const luna::RHI::Ref<luna::RHI::Device>& device,
                                         const luna::ImageData& image,
                                         const luna::Texture::SamplerSettings& sampler_settings,
                                         std::string_view debug_name);
void uploadTextureIfNeeded(luna::RHI::CommandBufferEncoder& commands,
                           PendingTextureUpload& uploaded_texture,
                           luna::RHI::ResourceState final_state = luna::RHI::ResourceState::ShaderRead,
                           luna::RHI::SyncScope final_stage = luna::RHI::SyncScope::FragmentStage);

} // namespace luna::scene_renderer_detail
