#ifndef CACAO_VKCOMMON_H
#define CACAO_VKCOMMON_H
#include "vk_mem_alloc.h"

#include <vulkan/vulkan.hpp>
#ifdef _MSC_VER
#include <intrin.h>
#endif
#include "Barrier.h"
#include "CommandBufferEncoder.h"
#include "Core.h"
#include "DescriptorSetLayout.h"
#include "PipelineDefs.h"
#include "Sampler.h"
#include "Texture.h"

namespace Cacao {
enum class BufferMemoryUsage;
enum class BufferUsageFlags : uint32_t;

namespace VKFastConvert {
inline constexpr VkPipelineStageFlags kPipelineStageLUT[] = {
    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
    VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
    VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT,
    VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
    VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT,
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    VK_PIPELINE_STAGE_TRANSFER_BIT,
    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
    VK_PIPELINE_STAGE_HOST_BIT,
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
};
inline constexpr VkAccessFlags kAccessFlagsLUT[] = {
    VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
    VK_ACCESS_INDEX_READ_BIT,
    VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
    VK_ACCESS_UNIFORM_READ_BIT,
    VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
    VK_ACCESS_SHADER_READ_BIT,
    VK_ACCESS_SHADER_WRITE_BIT,
    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    VK_ACCESS_TRANSFER_READ_BIT,
    VK_ACCESS_TRANSFER_WRITE_BIT,
    VK_ACCESS_HOST_READ_BIT,
    VK_ACCESS_HOST_WRITE_BIT,
    VK_ACCESS_MEMORY_READ_BIT,
    VK_ACCESS_MEMORY_WRITE_BIT,
};
inline constexpr VkImageLayout kImageLayoutLUT[] = {
    VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_GENERAL,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    VK_IMAGE_LAYOUT_PREINITIALIZED,
};
inline constexpr VkImageAspectFlags kImageAspectLUT[] = {
    VK_IMAGE_ASPECT_COLOR_BIT,
    VK_IMAGE_ASPECT_DEPTH_BIT,
    VK_IMAGE_ASPECT_STENCIL_BIT,
    VK_IMAGE_ASPECT_METADATA_BIT,
    VK_IMAGE_ASPECT_PLANE_0_BIT,
    VK_IMAGE_ASPECT_PLANE_1_BIT,
    VK_IMAGE_ASPECT_PLANE_2_BIT,
};

inline VkPipelineStageFlags PipelineStage(Cacao::PipelineStage stage) noexcept
{
    uint32_t bits = static_cast<uint32_t>(stage);
    if (bits == 0) {
        return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
    VkPipelineStageFlags result = 0;
    while (bits) {
        unsigned long idx;
#ifdef _MSC_VER
        _BitScanForward(&idx, bits);
#else
        idx = __builtin_ctz(bits);
#endif
        result |= kPipelineStageLUT[idx];
        bits &= bits - 1;
    }
    return result;
}

inline VkAccessFlags AccessFlags(Cacao::AccessFlags flags) noexcept
{
    uint32_t bits = static_cast<uint32_t>(flags);
    if (bits == 0) {
        return 0;
    }
    VkAccessFlags result = 0;
    while (bits) {
        unsigned long idx;
#ifdef _MSC_VER
        _BitScanForward(&idx, bits);
#else
        idx = __builtin_ctz(bits);
#endif
        result |= kAccessFlagsLUT[idx];
        bits &= bits - 1;
    }
    return result;
}

inline VkImageLayout ImageLayout(Cacao::ImageLayout layout) noexcept
{
    return kImageLayoutLUT[static_cast<uint32_t>(layout)];
}

inline VkImageAspectFlags ImageAspectFlags(Cacao::ImageAspectFlags flags) noexcept
{
    uint32_t bits = static_cast<uint32_t>(flags);
    if (bits == 0) {
        return 0;
    }
    VkImageAspectFlags result = 0;
    while (bits) {
        unsigned long idx;
#ifdef _MSC_VER
        _BitScanForward(&idx, bits);
#else
        idx = __builtin_ctz(bits);
#endif
        result |= kImageAspectLUT[idx];
        bits &= bits - 1;
    }
    return result;
}
} // namespace VKFastConvert

namespace VKResourceStateConvert {
struct ResourceStateMapping {
    VkImageLayout layout;
    VkPipelineStageFlags stage;
    VkAccessFlags access;
};

inline ResourceStateMapping Convert(ResourceState state) noexcept
{
    uint32_t s = static_cast<uint32_t>(state);
    if (s == 0) {
        return {VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0};
    }

    VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL;
    VkPipelineStageFlags stage = 0;
    VkAccessFlags access = 0;

    if (s & static_cast<uint32_t>(ResourceState::RenderTarget)) {
        layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        stage |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        access |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }
    if (s & static_cast<uint32_t>(ResourceState::DepthWrite)) {
        layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        stage |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }
    if (s & static_cast<uint32_t>(ResourceState::DepthRead)) {
        layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        stage |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    }
    if (s & static_cast<uint32_t>(ResourceState::ShaderRead)) {
        layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        stage |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        access |= VK_ACCESS_SHADER_READ_BIT;
    }
    if (s & static_cast<uint32_t>(ResourceState::UnorderedAccess)) {
        layout = VK_IMAGE_LAYOUT_GENERAL;
        stage |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        access |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    }
    if (s & static_cast<uint32_t>(ResourceState::CopySource)) {
        layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        stage |= VK_PIPELINE_STAGE_TRANSFER_BIT;
        access |= VK_ACCESS_TRANSFER_READ_BIT;
    }
    if (s & static_cast<uint32_t>(ResourceState::CopyDest)) {
        layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        stage |= VK_PIPELINE_STAGE_TRANSFER_BIT;
        access |= VK_ACCESS_TRANSFER_WRITE_BIT;
    }
    if (s & static_cast<uint32_t>(ResourceState::Present)) {
        layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        stage |= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        access = 0;
    }
    if (s & static_cast<uint32_t>(ResourceState::VertexBuffer)) {
        stage |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        access |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    }
    if (s & static_cast<uint32_t>(ResourceState::IndexBuffer)) {
        stage |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        access |= VK_ACCESS_INDEX_READ_BIT;
    }
    if (s & static_cast<uint32_t>(ResourceState::UniformBuffer)) {
        stage |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        access |= VK_ACCESS_UNIFORM_READ_BIT;
    }
    if (s & static_cast<uint32_t>(ResourceState::IndirectArgument)) {
        stage |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        access |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    }
    if (s & static_cast<uint32_t>(ResourceState::HostRead)) {
        stage |= VK_PIPELINE_STAGE_HOST_BIT;
        access |= VK_ACCESS_HOST_READ_BIT;
    }
    if (s & static_cast<uint32_t>(ResourceState::HostWrite)) {
        stage |= VK_PIPELINE_STAGE_HOST_BIT;
        access |= VK_ACCESS_HOST_WRITE_BIT;
    }
    if (s & static_cast<uint32_t>(ResourceState::General)) {
        layout = VK_IMAGE_LAYOUT_GENERAL;
        stage |= VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        access |= VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    }

    if (stage == 0) {
        stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }

    return {layout, stage, access};
}

inline VkImageLayout GetLayout(ResourceState state) noexcept
{
    return Convert(state).layout;
}

inline VkPipelineStageFlags GetStage(ResourceState state) noexcept
{
    return Convert(state).stage;
}

inline VkAccessFlags GetAccess(ResourceState state) noexcept
{
    return Convert(state).access;
}
} // namespace VKResourceStateConvert

class VKConverter {
public:
    static vk::BufferUsageFlags Convert(BufferUsageFlags usage);
    static VmaMemoryUsage Convert(BufferMemoryUsage usage);
    static vk::Format Convert(Format format);
    static vk::ShaderStageFlagBits ConvertShaderStageBits(ShaderStage stage);
    static vk::ShaderStageFlags ConvertShaderStageFlags(ShaderStage stage);
    static vk::PrimitiveTopology Convert(PrimitiveTopology topology);
    static vk::CullModeFlags Convert(CullMode cullMode);
    static vk::FrontFace Convert(FrontFace frontFace);
    static vk::PolygonMode Convert(PolygonMode polygonMode);
    static vk::LogicOp Convert(LogicOp logicOp);
    static vk::BlendFactor Convert(BlendFactor blendFactor);
    static vk::BlendOp Convert(BlendOp blendOp);
    static vk::ColorComponentFlags Convert(ColorComponentFlags flags);
    static vk::CompareOp Convert(CompareOp compareOp);
    static vk::StencilOp Convert(StencilOp stencilOp);
    static vk::SampleCountFlagBits ConvertSampleCount(uint32_t sampleCount);
    static vk::PipelineStageFlags ConvertSyncScope(SyncScope flags);
    static vk::AccessFlags ConvertResourceStateToAccess(ResourceState state);
    static vk::ImageLayout ConvertResourceStateToLayout(ResourceState state);
    static vk::ImageAspectFlags Convert(ImageAspectFlags flags);
    static vk::Filter Convert(Filter filter);
    static vk::SamplerAddressMode Convert(SamplerAddressMode addressMode);
    static vk::SamplerMipmapMode Convert(SamplerMipmapMode mipmapMode);
    static vk::BorderColor Convert(BorderColor borderColor);
    static vk::DescriptorType Convert(DescriptorType type);
    static vk::IndexType Convert(IndexType indexType);
};
} // namespace Cacao
#endif
