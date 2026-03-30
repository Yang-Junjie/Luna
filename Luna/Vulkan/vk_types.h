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
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

#define VK_CHECK(x)                                                                            \
    do {                                                                                       \
        const VkResult err__ = (x);                                                            \
        if (err__ != VK_SUCCESS) {                                                             \
            LUNA_CORE_FATAL("Vulkan call failed: {} returned {}", #x, string_VkResult(err__)); \
            std::abort();                                                                      \
        }                                                                                      \
    } while (0)

struct AllocatedImage {
    VkImage image{VK_NULL_HANDLE};
    VkImageView imageView{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
    VkExtent3D imageExtent{};
    VkFormat imageFormat{VK_FORMAT_UNDEFINED};
};

struct AllocatedBuffer {
    VkBuffer buffer;
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
    VkDeviceAddress vertexBufferAddress;
};

// push constants for our mesh object draws
struct GPUDrawPushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
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
    VkPipeline pipeline{VK_NULL_HANDLE};
    VkPipelineLayout layout{VK_NULL_HANDLE};
};

struct MaterialInstance {
    MaterialPipeline* pipeline{nullptr};
    VkDescriptorSet materialSet{VK_NULL_HANDLE};
    MaterialPass passType{MaterialPass::MainColor};
};
