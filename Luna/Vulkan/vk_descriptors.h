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

struct DescriptorAllocatorGrowable {

    using PoolSizeRatio = DescriptorAllocator::PoolSizeRatio;

    std::vector<PoolSizeRatio> ratios;
    std::vector<VkDescriptorPool> fullPools;
    std::vector<VkDescriptorPool> readyPools;
    uint32_t setsPerPool{0};

    void init(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios);
    void clear_pools(VkDevice device);
    void destroy_pools(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext = nullptr);

private:
    VkDescriptorPool get_pool(VkDevice device);
    VkDescriptorPool create_pool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios);
};

struct DescriptorWriter {
    std::deque<VkDescriptorImageInfo> imageInfos;
    std::deque<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkWriteDescriptorSet> writes;

    void write_image(uint32_t binding,
                     VkImageView image,
                     VkSampler sampler,
                     VkImageLayout layout,
                     VkDescriptorType type);
    void write_buffer(
        uint32_t binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type);
    void clear();
    void update_set(VkDevice device, VkDescriptorSet set);
};
