#pragma once

#include "Core/log.h"

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

inline vk::Result to_vk_result(vk::Result result)
{
    return result;
}

inline vk::Result to_vk_result(VkResult result)
{
    return static_cast<vk::Result>(result);
}

inline vk::Extent2D to_vk(VkExtent2D extent)
{
    return vk::Extent2D{extent.width, extent.height};
}

inline vk::Extent3D to_vk(VkExtent3D extent)
{
    return vk::Extent3D{extent.width, extent.height, extent.depth};
}

inline vk::Offset2D to_vk(VkOffset2D offset)
{
    return vk::Offset2D{offset.x, offset.y};
}

inline vk::Offset3D to_vk(VkOffset3D offset)
{
    return vk::Offset3D{offset.x, offset.y, offset.z};
}

inline std::string string_VkResult(VkResult result)
{
    return vk::to_string(static_cast<vk::Result>(result));
}

inline std::string string_VkResult(vk::Result result)
{
    return vk::to_string(result);
}

inline std::string string_VkFormat(VkFormat format)
{
    return vk::to_string(static_cast<vk::Format>(format));
}

inline std::string string_VkFormat(vk::Format format)
{
    return vk::to_string(format);
}

#define VK_CHECK(x)                                                                            \
    do {                                                                                       \
        const auto err__ = (x);                                                                \
        const vk::Result vkErr__ = to_vk_result(err__);                                        \
        if (vkErr__ != vk::Result::eSuccess) {                                                 \
            LUNA_CORE_FATAL("Vulkan call failed: {} returned {}", #x, vk::to_string(vkErr__)); \
            std::abort();                                                                      \
        }                                                                                      \
    } while (0)

struct AllocatedImage {
    vk::Image image{};
    vk::ImageView imageView{};
    VmaAllocation allocation{VK_NULL_HANDLE};
    vk::Extent3D imageExtent{};
    vk::Format imageFormat{vk::Format::eUndefined};
};

struct AllocatedBuffer {
    vk::Buffer buffer{};
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

struct Vertex {

    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

// holds the resources needed for a mesh
struct GPUMeshBuffers {

    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    vk::DeviceAddress vertexBufferAddress{};
};

// push constants for our mesh object draws
struct GPUDrawPushConstants {
    glm::mat4 worldMatrix;
    vk::DeviceAddress vertexBuffer{};
};

struct GPUSceneData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection;
    glm::vec4 sunlightColor;
};

enum class MaterialPass : uint8_t {
    MainColor,
    Transparent,
    Other,
};

struct MaterialPipeline {
    vk::Pipeline pipeline{};
    vk::PipelineLayout layout{};
};

struct MaterialInstance {
    MaterialPipeline* pipeline{nullptr};
    vk::DescriptorSet materialSet{};
    MaterialPass passType{MaterialPass::MainColor};
};
