#pragma once

#include "Asset/Editor/ImageLoader.h"
#include "Renderer/Mesh.h"
#include "Renderer/SceneRenderer.h"
#include "Renderer/Texture.h"

#include <cmath>
#include <cstring>

#include <algorithm>
#include <array>
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
#include <glm/trigonometric.hpp>
#include <Pipeline.h>
#include <PipelineLayout.h>
#include <Sampler.h>
#include <ShaderCompiler.h>
#include <ShaderModule.h>
#include <string_view>
#include <Texture.h>
#include <utility>
#include <vector>

namespace luna::scene_renderer_detail {

inline constexpr luna::RHI::Format kGBufferBaseColorFormat = luna::RHI::Format::RGBA8_UNORM;
inline constexpr luna::RHI::Format kGBufferLightingFormat = luna::RHI::Format::RGBA16_FLOAT;
inline constexpr luna::RHI::Format kEnvironmentFormat = luna::RHI::Format::RGBA32_FLOAT;
inline constexpr float kDefaultMaterialAlphaCutoff = 0.5f;
inline constexpr float kEnvironmentFallbackValue = 0.08f;

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

inline std::filesystem::path projectRoot()
{
    return std::filesystem::path(LUNA_PROJECT_ROOT);
}

inline luna::RHI::Ref<luna::RHI::ShaderModule>
    loadShaderModule(const luna::RHI::Ref<luna::RHI::Device>& device,
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

inline glm::mat4 adjustProjectionForBackend(glm::mat4 projection, luna::RHI::BackendType backend_type)
{
    if (backend_type == luna::RHI::BackendType::Vulkan) {
        projection[1][1] *= -1.0f;
    }
    return projection;
}

inline glm::mat4 buildViewProjection(const Camera& camera, float aspect_ratio, luna::RHI::BackendType backend_type)
{
    return adjustProjectionForBackend(camera.getProjectionMatrix(aspect_ratio), backend_type) * camera.getViewMatrix();
}

inline glm::vec3 resolveCameraPosition(const Camera& camera)
{
    return camera.getPosition();
}


inline float materialBlendModeToFloat(luna::Material::BlendMode blend_mode)
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

inline luna::RHI::ColorBlendAttachmentState makeAlphaBlendAttachment()
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

inline luna::RHI::Filter toRhiFilter(luna::rhi::Texture::FilterMode filter_mode)
{
    switch (filter_mode) {
        case luna::rhi::Texture::FilterMode::Nearest:
            return luna::RHI::Filter::Nearest;
        case luna::rhi::Texture::FilterMode::Linear:
        default:
            return luna::RHI::Filter::Linear;
    }
}

inline luna::RHI::SamplerMipmapMode toRhiMipmapMode(luna::rhi::Texture::MipFilterMode mip_filter_mode)
{
    switch (mip_filter_mode) {
        case luna::rhi::Texture::MipFilterMode::Nearest:
            return luna::RHI::SamplerMipmapMode::Nearest;
        case luna::rhi::Texture::MipFilterMode::None:
        case luna::rhi::Texture::MipFilterMode::Linear:
        default:
            return luna::RHI::SamplerMipmapMode::Linear;
    }
}

inline luna::RHI::SamplerAddressMode toRhiAddressMode(luna::rhi::Texture::WrapMode wrap_mode)
{
    switch (wrap_mode) {
        case luna::rhi::Texture::WrapMode::MirroredRepeat:
            return luna::RHI::SamplerAddressMode::MirroredRepeat;
        case luna::rhi::Texture::WrapMode::ClampToEdge:
            return luna::RHI::SamplerAddressMode::ClampToEdge;
        case luna::rhi::Texture::WrapMode::ClampToBorder:
            return luna::RHI::SamplerAddressMode::ClampToBorder;
        case luna::rhi::Texture::WrapMode::MirrorClampToEdge:
            return luna::RHI::SamplerAddressMode::MirrorClampToEdge;
        case luna::rhi::Texture::WrapMode::Repeat:
        default:
            return luna::RHI::SamplerAddressMode::Repeat;
    }
}

inline float transparentSortDistanceSq(const glm::mat4& transform, const glm::vec3& camera_position)
{
    const glm::vec3 object_position(transform[3]);
    return glm::length2(object_position - camera_position);
}

inline luna::rhi::ImageData createFallbackFloatImageData(const glm::vec4& value)
{
    std::vector<uint8_t> bytes(sizeof(float) * 4u, 0);
    std::memcpy(bytes.data(), &value[0], bytes.size());
    return luna::rhi::ImageData{
        .ByteData = std::move(bytes),
        .ImageFormat = kEnvironmentFormat,
        .Width = 1,
        .Height = 1,
    };
}

inline luna::rhi::ImageData generateEnvironmentMipChain(const luna::rhi::ImageData& source)
{
    if (!source.isValid() || source.ImageFormat != kEnvironmentFormat ||
        source.ByteData.size() !=
            static_cast<size_t>(source.Width) * static_cast<size_t>(source.Height) * 4u * sizeof(float)) {
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

} // namespace luna::scene_renderer_detail
