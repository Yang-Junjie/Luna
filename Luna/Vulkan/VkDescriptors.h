#pragma once

#include "Renderer/Shader.h"
#include "VkTypes.h"

struct DescriptorLayoutBuilder {

    std::vector<vk::DescriptorSetLayoutBinding> m_bindings;

    void addBinding(uint32_t binding, vk::DescriptorType type);

    void addBinding(uint32_t binding, VkDescriptorType type)
    {
        addBinding(binding, static_cast<vk::DescriptorType>(type));
    }

    void addBindingFromReflection(const luna::ShaderReflectionData& data, vk::ShaderStageFlags shader_stages);

    void addBindingFromReflection(const luna::ShaderReflectionData& data, VkShaderStageFlags shader_stages)
    {
        addBindingFromReflection(data, static_cast<vk::ShaderStageFlags>(shader_stages));
    }

    void addBindingsFromReflection(const luna::Shader::ReflectionMap& reflection_map,
                                      uint32_t set_index,
                                      vk::ShaderStageFlags shader_stages);

    void addBindingsFromReflection(const luna::Shader::ReflectionMap& reflection_map,
                                      uint32_t set_index,
                                      VkShaderStageFlags shader_stages)
    {
        addBindingsFromReflection(reflection_map, set_index, static_cast<vk::ShaderStageFlags>(shader_stages));
    }

    void clear();
    vk::DescriptorSetLayout build(vk::Device device,
                                  vk::ShaderStageFlags shader_stages = {},
                                  const void* p_next = nullptr,
                                  vk::DescriptorSetLayoutCreateFlags flags = {});

    vk::DescriptorSetLayout build(VkDevice device,
                                  VkShaderStageFlags shader_stages = 0,
                                  const void* p_next = nullptr,
                                  VkDescriptorSetLayoutCreateFlags flags = 0)
    {
        return build(vk::Device(device),
                     static_cast<vk::ShaderStageFlags>(shader_stages),
                     p_next,
                     static_cast<vk::DescriptorSetLayoutCreateFlags>(flags));
    }
};

struct DescriptorAllocator {

    struct PoolSizeRatio {
        vk::DescriptorType m_type;
        float m_ratio;
    };

    vk::DescriptorPool m_pool{};

    void initPool(vk::Device device, uint32_t max_sets, std::span<PoolSizeRatio> pool_ratios);
    void clearDescriptors(vk::Device device);
    void destroyPool(vk::Device device);

    vk::DescriptorSet allocate(vk::Device device, vk::DescriptorSetLayout layout);
};

struct DescriptorAllocatorGrowable {

    using PoolSizeRatio = DescriptorAllocator::PoolSizeRatio;

    std::vector<PoolSizeRatio> m_ratios;
    std::vector<vk::DescriptorPool> m_full_pools;
    std::vector<vk::DescriptorPool> m_ready_pools;
    uint32_t m_sets_per_pool{0};

    void init(vk::Device device, uint32_t initial_sets, std::span<PoolSizeRatio> pool_ratios);
    void clearPools(vk::Device device);
    void destroyPools(vk::Device device);

    vk::DescriptorSet allocate(vk::Device device, vk::DescriptorSetLayout layout, const void* p_next = nullptr);

private:
    vk::DescriptorPool getPool(vk::Device device);
    vk::DescriptorPool createPool(vk::Device device, uint32_t set_count, std::span<PoolSizeRatio> pool_ratios);
};

struct DescriptorWriter {
    std::deque<vk::DescriptorImageInfo> m_image_infos;
    std::deque<vk::DescriptorBufferInfo> m_buffer_infos;
    std::vector<vk::WriteDescriptorSet> m_writes;

    void writeImage(
        uint32_t binding, vk::ImageView image, vk::Sampler sampler, vk::ImageLayout layout, vk::DescriptorType type);

    void writeImage(
        uint32_t binding, vk::ImageView image, vk::Sampler sampler, VkImageLayout layout, VkDescriptorType type)
    {
        writeImage(
            binding, image, sampler, static_cast<vk::ImageLayout>(layout), static_cast<vk::DescriptorType>(type));
    }

    void writeBuffer(uint32_t binding, vk::Buffer buffer, size_t size, size_t offset, vk::DescriptorType type);

    void writeBuffer(uint32_t binding, vk::Buffer buffer, size_t size, size_t offset, VkDescriptorType type)
    {
        writeBuffer(binding, buffer, size, offset, static_cast<vk::DescriptorType>(type));
    }

    void clear();
    void updateSet(vk::Device device, vk::DescriptorSet set);
};

