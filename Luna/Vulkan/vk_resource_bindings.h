#pragma once

#include "RHI/ResourceLayout.h"
#include "vk_types.h"

#include <cstdint>
#include <unordered_map>

class VulkanResourceBindingRegistry {
public:
    luna::BufferHandle register_buffer(vk::Buffer buffer);
    luna::ImageViewHandle register_image_view(vk::ImageView imageView);
    bool register_image_view(luna::ImageViewHandle handle, vk::ImageView imageView);
    luna::SamplerHandle register_sampler(vk::Sampler sampler);

    void unregister_buffer(luna::BufferHandle handle);
    void unregister_image_view(luna::ImageViewHandle handle);
    void unregister_sampler(luna::SamplerHandle handle);

    vk::Buffer resolve_buffer(luna::BufferHandle handle) const;
    vk::ImageView resolve_image_view(luna::ImageViewHandle handle) const;
    vk::Sampler resolve_sampler(luna::SamplerHandle handle) const;

private:
    uint64_t m_nextBufferId = 1;
    uint64_t m_nextImageViewId = 1;
    uint64_t m_nextSamplerId = 1;
    std::unordered_map<uint64_t, vk::Buffer> m_buffers;
    std::unordered_map<uint64_t, vk::ImageView> m_imageViews;
    std::unordered_map<uint64_t, vk::Sampler> m_samplers;
};

bool update_resource_set(vk::Device device,
                         const VulkanResourceBindingRegistry& registry,
                         vk::DescriptorSet set,
                         const luna::ResourceSetWriteDesc& writeDesc);
