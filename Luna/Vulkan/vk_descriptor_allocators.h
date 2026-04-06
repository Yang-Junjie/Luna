#pragma once

#include "vk_types.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>
#include <vector>

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

    vk::DescriptorSet allocate(vk::Device device,
                               vk::DescriptorSetLayout layout,
                               const void* pNext = nullptr,
                               vk::DescriptorPool* outPool = nullptr);
    void free(vk::Device device, vk::DescriptorPool pool, vk::DescriptorSet set);

private:
    vk::DescriptorPool get_pool(vk::Device device);
    vk::DescriptorPool create_pool(vk::Device device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios);
};

struct DescriptorWriter {
    std::deque<std::vector<vk::DescriptorImageInfo>> imageInfos;
    std::deque<std::vector<vk::DescriptorBufferInfo>> bufferInfos;
    std::vector<vk::WriteDescriptorSet> writes;

    void write_image(
        uint32_t binding,
        vk::ImageView image,
        vk::Sampler sampler,
        vk::ImageLayout layout,
        vk::DescriptorType type,
        uint32_t arrayElement = 0);

    void write_image(
        uint32_t binding,
        vk::ImageView image,
        vk::Sampler sampler,
        VkImageLayout layout,
        VkDescriptorType type,
        uint32_t arrayElement = 0)
    {
        write_image(
            binding, image, sampler, static_cast<vk::ImageLayout>(layout), static_cast<vk::DescriptorType>(type), arrayElement);
    }

    void write_images(uint32_t binding,
                      std::span<const vk::DescriptorImageInfo> infos,
                      vk::DescriptorType type,
                      uint32_t firstArrayElement = 0);

    void write_buffer(uint32_t binding,
                      vk::Buffer buffer,
                      size_t size,
                      size_t offset,
                      vk::DescriptorType type,
                      uint32_t arrayElement = 0);

    void write_buffer(uint32_t binding,
                      vk::Buffer buffer,
                      size_t size,
                      size_t offset,
                      VkDescriptorType type,
                      uint32_t arrayElement = 0)
    {
        write_buffer(binding, buffer, size, offset, static_cast<vk::DescriptorType>(type), arrayElement);
    }

    void write_buffers(uint32_t binding,
                       std::span<const vk::DescriptorBufferInfo> infos,
                       vk::DescriptorType type,
                       uint32_t firstArrayElement = 0);

    void clear();
    void update_set(vk::Device device, vk::DescriptorSet set);
};
