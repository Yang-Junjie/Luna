#pragma once

#include "Core/Log.h"

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
#include <vk_mem_alloc.h>
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

inline vk::Extent3D toVk(VkExtent3D extent)
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

#define VK_CHECK(x)                                                                            \
    do {                                                                                       \
        const auto err__ = (x);                                                                \
        const vk::Result vkErr__ = toVkResult(err__);                                          \
        if (vkErr__ != vk::Result::eSuccess) {                                                 \
            LUNA_CORE_FATAL("Vulkan call failed: {} returned {}", #x, vk::to_string(vkErr__)); \
            std::abort();                                                                      \
        }                                                                                      \
    } while (0)

struct AllocatedImage {
    vk::Image m_image{};
    vk::ImageView m_image_view{};
    VmaAllocation m_allocation{VK_NULL_HANDLE};
    vk::Extent3D m_image_extent{};
    vk::Format m_image_format{vk::Format::eUndefined};
};

struct AllocatedBuffer {
    vk::Buffer m_buffer{};
    VmaAllocation m_allocation;
    VmaAllocationInfo m_info;
};

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

