#pragma once

#include "RHI/shader.h"
#include "vk_types.h"

struct DescriptorLayoutBuilder {

    std::vector<VkDescriptorSetLayoutBinding> bindings;

    void add_binding(uint32_t binding, VkDescriptorType type);
    void add_binding_from_reflection(const luna::ShaderReflectionData& data, VkShaderStageFlags shaderStages);
    void add_bindings_from_reflection(const luna::Shader::ReflectionMap& reflectionMap,
                                      uint32_t setIndex,
                                      VkShaderStageFlags shaderStages);
    void clear();
    VkDescriptorSetLayout build(VkDevice device,
                                VkShaderStageFlags shaderStages = 0,
                                void* pNext = nullptr,
                                VkDescriptorSetLayoutCreateFlags flags = 0);
};

struct DescriptorAllocator {

    struct PoolSizeRatio {
        VkDescriptorType type;
        float ratio;
    };

    VkDescriptorPool pool{VK_NULL_HANDLE};

    void init_pool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios);
    void clear_descriptors(VkDevice device);
    void destroy_pool(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
};
