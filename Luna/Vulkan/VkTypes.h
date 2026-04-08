#pragma once

#include "Core/Log.h"
#include "Renderer/RenderTypes.h"
#include "VkBuffer.h"
#include "VkImage.h"
#include "VkSampler.h"

#include <cstdint>
#include <cstdlib>

#include <array>
#include <deque>
#include <functional>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>

inline vk::Result toVkResult(vk::Result result)
{
    return result;
}

inline vk::Result toVkResult(VkResult result)
{
    return static_cast<vk::Result>(result);
}

inline vk::Extent2D toVk(VkExtent2D extent)
{
    return vk::Extent2D{extent.width, extent.height};
}

inline vk::Extent2D toVk(luna::render::Extent2D extent)
{
    return vk::Extent2D{extent.width, extent.height};
}

inline vk::Extent3D toVk(VkExtent3D extent)
{
    return vk::Extent3D{extent.width, extent.height, extent.depth};
}

inline vk::Extent3D toVk(luna::render::Extent3D extent)
{
    return vk::Extent3D{extent.width, extent.height, extent.depth};
}

inline vk::Offset2D toVk(VkOffset2D offset)
{
    return vk::Offset2D{offset.x, offset.y};
}

inline vk::Offset3D toVk(VkOffset3D offset)
{
    return vk::Offset3D{offset.x, offset.y, offset.z};
}

inline std::string stringVkResult(VkResult result)
{
    return vk::to_string(static_cast<vk::Result>(result));
}

inline std::string stringVkResult(vk::Result result)
{
    return vk::to_string(result);
}

inline std::string stringVkFormat(VkFormat format)
{
    return vk::to_string(static_cast<vk::Format>(format));
}

inline std::string stringVkFormat(vk::Format format)
{
    return vk::to_string(format);
}

inline luna::render::Extent2D fromVk(vk::Extent2D extent)
{
    return {extent.width, extent.height};
}

inline luna::render::Extent3D fromVk(vk::Extent3D extent)
{
    return {extent.width, extent.height, extent.depth};
}

inline vk::Format toVk(luna::render::PixelFormat format)
{
    switch (format) {
        case luna::render::PixelFormat::R8G8B8A8Unorm:
            return vk::Format::eR8G8B8A8Unorm;
        case luna::render::PixelFormat::R16G16B16A16Sfloat:
            return vk::Format::eR16G16B16A16Sfloat;
        case luna::render::PixelFormat::D32Sfloat:
            return vk::Format::eD32Sfloat;
        case luna::render::PixelFormat::Undefined:
        default:
            return vk::Format::eUndefined;
    }
}

inline luna::render::PixelFormat fromVk(vk::Format format)
{
    switch (format) {
        case vk::Format::eR8G8B8A8Unorm:
            return luna::render::PixelFormat::R8G8B8A8Unorm;
        case vk::Format::eR16G16B16A16Sfloat:
            return luna::render::PixelFormat::R16G16B16A16Sfloat;
        case vk::Format::eD32Sfloat:
            return luna::render::PixelFormat::D32Sfloat;
        case vk::Format::eUndefined:
        default:
            return luna::render::PixelFormat::Undefined;
    }
}

inline vk::ImageLayout toVk(luna::render::ImageLayout layout)
{
    switch (layout) {
        case luna::render::ImageLayout::General:
            return vk::ImageLayout::eGeneral;
        case luna::render::ImageLayout::ColorAttachment:
            return vk::ImageLayout::eColorAttachmentOptimal;
        case luna::render::ImageLayout::DepthAttachment:
            return vk::ImageLayout::eDepthAttachmentOptimal;
        case luna::render::ImageLayout::TransferSrc:
            return vk::ImageLayout::eTransferSrcOptimal;
        case luna::render::ImageLayout::TransferDst:
            return vk::ImageLayout::eTransferDstOptimal;
        case luna::render::ImageLayout::ShaderReadOnly:
            return vk::ImageLayout::eShaderReadOnlyOptimal;
        case luna::render::ImageLayout::Present:
            return vk::ImageLayout::ePresentSrcKHR;
        case luna::render::ImageLayout::Undefined:
        default:
            return vk::ImageLayout::eUndefined;
    }
}

inline luna::render::ImageLayout fromVk(vk::ImageLayout layout)
{
    switch (layout) {
        case vk::ImageLayout::eGeneral:
            return luna::render::ImageLayout::General;
        case vk::ImageLayout::eColorAttachmentOptimal:
            return luna::render::ImageLayout::ColorAttachment;
        case vk::ImageLayout::eDepthAttachmentOptimal:
            return luna::render::ImageLayout::DepthAttachment;
        case vk::ImageLayout::eTransferSrcOptimal:
            return luna::render::ImageLayout::TransferSrc;
        case vk::ImageLayout::eTransferDstOptimal:
            return luna::render::ImageLayout::TransferDst;
        case vk::ImageLayout::eShaderReadOnlyOptimal:
            return luna::render::ImageLayout::ShaderReadOnly;
        case vk::ImageLayout::ePresentSrcKHR:
            return luna::render::ImageLayout::Present;
        case vk::ImageLayout::eUndefined:
        default:
            return luna::render::ImageLayout::Undefined;
    }
}

inline vk::BufferUsageFlags toVk(luna::render::BufferUsage usage)
{
    vk::BufferUsageFlags flags{};
    if (luna::render::hasFlag(usage, luna::render::BufferUsage::TransferSrc)) {
        flags |= vk::BufferUsageFlagBits::eTransferSrc;
    }
    if (luna::render::hasFlag(usage, luna::render::BufferUsage::TransferDst)) {
        flags |= vk::BufferUsageFlagBits::eTransferDst;
    }
    if (luna::render::hasFlag(usage, luna::render::BufferUsage::Uniform)) {
        flags |= vk::BufferUsageFlagBits::eUniformBuffer;
    }
    if (luna::render::hasFlag(usage, luna::render::BufferUsage::Index)) {
        flags |= vk::BufferUsageFlagBits::eIndexBuffer;
    }
    if (luna::render::hasFlag(usage, luna::render::BufferUsage::Storage)) {
        flags |= vk::BufferUsageFlagBits::eStorageBuffer;
    }
    if (luna::render::hasFlag(usage, luna::render::BufferUsage::ShaderDeviceAddress)) {
        flags |= vk::BufferUsageFlagBits::eShaderDeviceAddress;
    }
    return flags;
}

inline vk::ImageUsageFlags toVk(luna::render::ImageUsage usage)
{
    vk::ImageUsageFlags flags{};
    if (luna::render::hasFlag(usage, luna::render::ImageUsage::TransferSrc)) {
        flags |= vk::ImageUsageFlagBits::eTransferSrc;
    }
    if (luna::render::hasFlag(usage, luna::render::ImageUsage::TransferDst)) {
        flags |= vk::ImageUsageFlagBits::eTransferDst;
    }
    if (luna::render::hasFlag(usage, luna::render::ImageUsage::Sampled)) {
        flags |= vk::ImageUsageFlagBits::eSampled;
    }
    if (luna::render::hasFlag(usage, luna::render::ImageUsage::Storage)) {
        flags |= vk::ImageUsageFlagBits::eStorage;
    }
    if (luna::render::hasFlag(usage, luna::render::ImageUsage::ColorAttachment)) {
        flags |= vk::ImageUsageFlagBits::eColorAttachment;
    }
    if (luna::render::hasFlag(usage, luna::render::ImageUsage::DepthStencilAttachment)) {
        flags |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
    }
    return flags;
}

inline VmaMemoryUsage toVk(luna::render::MemoryUsage usage)
{
    switch (usage) {
        case luna::render::MemoryUsage::CpuOnly:
            return VMA_MEMORY_USAGE_CPU_ONLY;
        case luna::render::MemoryUsage::CpuToGpu:
            return VMA_MEMORY_USAGE_CPU_TO_GPU;
        case luna::render::MemoryUsage::GpuOnly:
        default:
            return VMA_MEMORY_USAGE_GPU_ONLY;
    }
}

#define VK_CHECK(x)                                                                            \
    do {                                                                                       \
        const auto err__ = (x);                                                                \
        const vk::Result vkErr__ = toVkResult(err__);                                          \
        if (vkErr__ != vk::Result::eSuccess) {                                                 \
            LUNA_CORE_FATAL("Vulkan call failed: {} returned {}", #x, vk::to_string(vkErr__)); \
            std::abort();                                                                      \
        }                                                                                      \
    } while (0)

using AllocatedImage = luna::vkcore::Image;
using AllocatedBuffer = luna::vkcore::Buffer;
using AllocatedSampler = luna::vkcore::Sampler;

struct Vertex {

    glm::vec3 m_position;
    float m_uv_x;
    glm::vec3 m_normal;
    float m_uv_y;
    glm::vec4 m_color;
};

// holds the resources needed for a mesh
struct GPUMeshBuffers {

    AllocatedBuffer m_index_buffer;
    AllocatedBuffer m_vertex_buffer;
    vk::DeviceAddress m_vertex_buffer_address{};
};

// push constants for our mesh object draws
struct GPUDrawPushConstants {
    glm::mat4 m_world_matrix;
    vk::DeviceAddress m_vertex_buffer{};
};

struct GPUSceneData {
    glm::mat4 m_view;
    glm::mat4 m_proj;
    glm::mat4 m_viewproj;
    glm::vec4 m_ambient_color;
    glm::vec4 m_sunlight_direction;
    glm::vec4 m_sunlight_color;
};

enum class MaterialPass : uint8_t {
    MainColor,
    Transparent,
    Other,
};

struct MaterialPipeline {
    vk::Pipeline m_pipeline{};
    vk::PipelineLayout m_layout{};
};

struct MaterialInstance {
    MaterialPipeline* m_pipeline{nullptr};
    vk::DescriptorSet m_material_set{};
    MaterialPass m_pass_type{MaterialPass::MainColor};
};

