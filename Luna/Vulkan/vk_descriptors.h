#pragma once

#include "RHI/ResourceLayout.h"
#include "RHI/Shader.h"
#include "vk_types.h"

#include <unordered_map>

struct DescriptorLayoutBuilder {

    std::vector<vk::DescriptorSetLayoutBinding> bindings;

    void add_binding(uint32_t binding, vk::DescriptorType type);

    void add_binding(uint32_t binding, VkDescriptorType type)
    {
        add_binding(binding, static_cast<vk::DescriptorType>(type));
    }

    void add_binding_from_reflection(const luna::ShaderReflectionData& data, vk::ShaderStageFlags shaderStages);

    void add_binding_from_reflection(const luna::ShaderReflectionData& data, VkShaderStageFlags shaderStages)
    {
        add_binding_from_reflection(data, static_cast<vk::ShaderStageFlags>(shaderStages));
    }

    void add_bindings_from_reflection(const luna::Shader::ReflectionMap& reflectionMap,
                                      uint32_t setIndex,
                                      vk::ShaderStageFlags shaderStages);

    void add_bindings_from_reflection(const luna::Shader::ReflectionMap& reflectionMap,
                                      uint32_t setIndex,
                                      VkShaderStageFlags shaderStages)
    {
        add_bindings_from_reflection(reflectionMap, setIndex, static_cast<vk::ShaderStageFlags>(shaderStages));
    }

    void clear();
    vk::DescriptorSetLayout build(vk::Device device,
                                  vk::ShaderStageFlags shaderStages = {},
                                  const void* pNext = nullptr,
                                  vk::DescriptorSetLayoutCreateFlags flags = {});

    vk::DescriptorSetLayout build(VkDevice device,
                                  VkShaderStageFlags shaderStages = 0,
                                  const void* pNext = nullptr,
                                  VkDescriptorSetLayoutCreateFlags flags = 0)
    {
        return build(vk::Device(device),
                     static_cast<vk::ShaderStageFlags>(shaderStages),
                     pNext,
                     static_cast<vk::DescriptorSetLayoutCreateFlags>(flags));
    }
};

struct DescriptorAllocator {

    struct PoolSizeRatio {
        vk::DescriptorType type;
        float ratio;
    };

    vk::DescriptorPool pool{};

    void init_pool(vk::Device device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios);
    void clear_descriptors(vk::Device device);
    void destroy_pool(vk::Device device);

    vk::DescriptorSet allocate(vk::Device device, vk::DescriptorSetLayout layout);
};

struct DescriptorAllocatorGrowable {

    using PoolSizeRatio = DescriptorAllocator::PoolSizeRatio;

    std::vector<PoolSizeRatio> ratios;
    std::vector<vk::DescriptorPool> fullPools;
    std::vector<vk::DescriptorPool> readyPools;
    uint32_t setsPerPool{0};

    void init(vk::Device device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios);
    void clear_pools(vk::Device device);
    void destroy_pools(vk::Device device);

    vk::DescriptorSet allocate(vk::Device device, vk::DescriptorSetLayout layout, const void* pNext = nullptr);

private:
    vk::DescriptorPool get_pool(vk::Device device);
    vk::DescriptorPool create_pool(vk::Device device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios);
};

struct DescriptorWriter {
    std::deque<vk::DescriptorImageInfo> imageInfos;
    std::deque<vk::DescriptorBufferInfo> bufferInfos;
    std::vector<vk::WriteDescriptorSet> writes;

    void write_image(
        uint32_t binding, vk::ImageView image, vk::Sampler sampler, vk::ImageLayout layout, vk::DescriptorType type);

    void write_image(
        uint32_t binding, vk::ImageView image, vk::Sampler sampler, VkImageLayout layout, VkDescriptorType type)
    {
        write_image(
            binding, image, sampler, static_cast<vk::ImageLayout>(layout), static_cast<vk::DescriptorType>(type));
    }

    void write_buffer(uint32_t binding, vk::Buffer buffer, size_t size, size_t offset, vk::DescriptorType type);

    void write_buffer(uint32_t binding, vk::Buffer buffer, size_t size, size_t offset, VkDescriptorType type)
    {
        write_buffer(binding, buffer, size, offset, static_cast<vk::DescriptorType>(type));
    }

    void clear();
    void update_set(vk::Device device, vk::DescriptorSet set);
};

vk::DescriptorType to_vulkan_descriptor_type(luna::ResourceType type);
vk::ShaderStageFlags to_vulkan_shader_stages(luna::ShaderType visibility);
vk::ImageLayout to_vulkan_image_layout(luna::ResourceType type);

class VulkanResourceBindingRegistry {
public:
    luna::BufferHandle register_buffer(vk::Buffer buffer);
    luna::ImageHandle register_image_view(vk::ImageView imageView);
    luna::SamplerHandle register_sampler(vk::Sampler sampler);

    void unregister_buffer(luna::BufferHandle handle);
    void unregister_image_view(luna::ImageHandle handle);
    void unregister_sampler(luna::SamplerHandle handle);

    vk::Buffer resolve_buffer(luna::BufferHandle handle) const;
    vk::ImageView resolve_image_view(luna::ImageHandle handle) const;
    vk::Sampler resolve_sampler(luna::SamplerHandle handle) const;

private:
    uint64_t m_nextBufferId = 1;
    uint64_t m_nextImageViewId = 1;
    uint64_t m_nextSamplerId = 1;
    std::unordered_map<uint64_t, vk::Buffer> m_buffers;
    std::unordered_map<uint64_t, vk::ImageView> m_imageViews;
    std::unordered_map<uint64_t, vk::Sampler> m_samplers;
};

vk::DescriptorSetLayout build_resource_layout(vk::Device device,
                                              const luna::ResourceLayoutDesc& desc,
                                              const void* pNext = nullptr,
                                              vk::DescriptorSetLayoutCreateFlags flags = {});

bool update_resource_set(vk::Device device,
                         const VulkanResourceBindingRegistry& registry,
                         vk::DescriptorSet set,
                         const luna::ResourceSetWriteDesc& writeDesc);
