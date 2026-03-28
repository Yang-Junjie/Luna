#pragma once

#include "Core/log.h"

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
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};
