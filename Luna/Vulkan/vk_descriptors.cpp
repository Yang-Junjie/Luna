#include "vk_descriptors.h"

#include <cassert>

#include <algorithm>

namespace {
VkDescriptorType mapResourceTypeToVulkan(luna::ResourceType type)
{
    switch (type) {
        case luna::ResourceType::UniformBuffer:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case luna::ResourceType::DynamicUniformBuffer:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        case luna::ResourceType::CombinedImageSampler:
            return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case luna::ResourceType::SampledImage:
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case luna::ResourceType::Sampler:
            return VK_DESCRIPTOR_TYPE_SAMPLER;
        case luna::ResourceType::StorageBuffer:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case luna::ResourceType::StorageImage:
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case luna::ResourceType::InputAttachment:
            return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        default:
            assert(false && "Unsupported resource type.");
            return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }
}

void add_or_merge_binding(std::vector<VkDescriptorSetLayoutBinding>& bindings,
                          const VkDescriptorSetLayoutBinding& binding)
{
    const auto existing =
        std::find_if(bindings.begin(), bindings.end(), [&](const VkDescriptorSetLayoutBinding& candidate) {
            return candidate.binding == binding.binding;
        });

    if (existing == bindings.end()) {
        bindings.push_back(binding);
        return;
    }

    assert(existing->descriptorType == binding.descriptorType);
    assert(existing->descriptorCount == binding.descriptorCount);
    existing->stageFlags |= binding.stageFlags;
}
} // namespace

void DescriptorLayoutBuilder::add_binding_from_reflection(const luna::ShaderReflectionData& data,
                                                          VkShaderStageFlags shaderStages)
{
    VkDescriptorSetLayoutBinding binding{
        .binding = data.binding,
        .descriptorType = mapResourceTypeToVulkan(data.type),
        .descriptorCount = std::max(1u, data.count),
        .stageFlags = shaderStages,
        .pImmutableSamplers = nullptr,
    };

    add_or_merge_binding(bindings, binding);
}

void DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType type)
{
    VkDescriptorSetLayoutBinding newbind{
        .binding = binding,
        .descriptorType = type,
        .descriptorCount = 1,
    };

    add_or_merge_binding(bindings, newbind);
}

void DescriptorLayoutBuilder::add_bindings_from_reflection(const luna::Shader::ReflectionMap& reflectionMap,
                                                           uint32_t setIndex,
                                                           VkShaderStageFlags shaderStages)
{
    const auto setIt = reflectionMap.find(setIndex);
    if (setIt == reflectionMap.end()) {
        return;
    }

    for (const auto& binding : setIt->second) {
        add_binding_from_reflection(binding, shaderStages);
    }
}

void DescriptorLayoutBuilder::clear()
{
    bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device,
                                                     VkShaderStageFlags shaderStages,
                                                     void* pNext,
                                                     VkDescriptorSetLayoutCreateFlags flags)
{
    for (auto& b : bindings) {
        b.stageFlags |= shaderStages;
    }

    std::sort(bindings.begin(),
              bindings.end(),
              [](const VkDescriptorSetLayoutBinding& lhs, const VkDescriptorSetLayoutBinding& rhs) {
                  return lhs.binding < rhs.binding;
              });

    VkDescriptorSetLayoutCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = pNext,
        .flags = flags,
        .bindingCount = (uint32_t) bindings.size(),
        .pBindings = bindings.data(),
    };

    VkDescriptorSetLayout set;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

    return set;
}

void DescriptorAllocator::init_pool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios)
{
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (PoolSizeRatio ratio : poolRatios) {
        poolSizes.push_back(
            VkDescriptorPoolSize{.type = ratio.type, .descriptorCount = uint32_t(ratio.ratio * maxSets)});
    }

    VkDescriptorPoolCreateInfo pool_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                            .flags = 0,
                                            .maxSets = maxSets,
                                            .poolSizeCount = (uint32_t) poolSizes.size(),
                                            .pPoolSizes = poolSizes.data()};

    VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &pool));
}

void DescriptorAllocator::clear_descriptors(VkDevice device)
{
    if (pool == VK_NULL_HANDLE) {
        return;
    }

    vkResetDescriptorPool(device, pool, 0);
}

void DescriptorAllocator::destroy_pool(VkDevice device)
{
    if (pool == VK_NULL_HANDLE) {
        return;
    }

    vkDestroyDescriptorPool(device, pool, nullptr);
    pool = VK_NULL_HANDLE;
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };

    VkDescriptorSet ds;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));

    return ds;
}

void DescriptorAllocatorGrowable::init(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios)
{
    ratios.assign(poolRatios.begin(), poolRatios.end());
    fullPools.clear();
    readyPools.clear();
    setsPerPool = initialSets;

    readyPools.push_back(create_pool(device, initialSets, poolRatios));
}

void DescriptorAllocatorGrowable::clear_pools(VkDevice device)
{
    for (const VkDescriptorPool poolHandle : readyPools) {
        vkResetDescriptorPool(device, poolHandle, 0);
    }

    for (const VkDescriptorPool poolHandle : fullPools) {
        vkResetDescriptorPool(device, poolHandle, 0);
        readyPools.push_back(poolHandle);
    }

    fullPools.clear();
}

void DescriptorAllocatorGrowable::destroy_pools(VkDevice device)
{
    for (const VkDescriptorPool poolHandle : readyPools) {
        vkDestroyDescriptorPool(device, poolHandle, nullptr);
    }
    readyPools.clear();

    for (const VkDescriptorPool poolHandle : fullPools) {
        vkDestroyDescriptorPool(device, poolHandle, nullptr);
    }
    fullPools.clear();
}

VkDescriptorSet DescriptorAllocatorGrowable::allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext)
{
    VkDescriptorPool poolToUse = get_pool(device);

    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = pNext,
        .descriptorPool = poolToUse,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };

    VkDescriptorSet ds = VK_NULL_HANDLE;
    VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &ds);
    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
        fullPools.push_back(poolToUse);

        poolToUse = get_pool(device);
        allocInfo.descriptorPool = poolToUse;
        VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));
    } else if (result != VK_SUCCESS) {
        LUNA_CORE_FATAL("Vulkan call failed: vkAllocateDescriptorSets returned {}", string_VkResult(result));
        std::abort();
    }

    readyPools.push_back(poolToUse);
    return ds;
}

VkDescriptorPool DescriptorAllocatorGrowable::get_pool(VkDevice device)
{
    if (!readyPools.empty()) {
        const VkDescriptorPool poolHandle = readyPools.back();
        readyPools.pop_back();
        return poolHandle;
    }

    setsPerPool = std::min(static_cast<uint32_t>(setsPerPool * 1.5f), 4092u);
    return create_pool(device, setsPerPool, ratios);
}

VkDescriptorPool DescriptorAllocatorGrowable::create_pool(VkDevice device,
                                                          uint32_t setCount,
                                                          std::span<PoolSizeRatio> poolRatios)
{
    std::vector<VkDescriptorPoolSize> poolSizes;
    poolSizes.reserve(poolRatios.size());
    for (const PoolSizeRatio ratio : poolRatios) {
        poolSizes.push_back(
            VkDescriptorPoolSize{.type = ratio.type, .descriptorCount = uint32_t(ratio.ratio * setCount)});
    }

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = 0,
        .maxSets = setCount,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };

    VkDescriptorPool newPool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &newPool));
    return newPool;
}

void DescriptorWriter::write_image(uint32_t binding,
                                   VkImageView image,
                                   VkSampler sampler,
                                   VkImageLayout layout,
                                   VkDescriptorType type)
{
    imageInfos.push_back(VkDescriptorImageInfo{
        .sampler = sampler,
        .imageView = image,
        .imageLayout = layout,
    });

    writes.push_back(VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = binding,
        .descriptorCount = 1,
        .descriptorType = type,
        .pImageInfo = &imageInfos.back(),
    });
}

void DescriptorWriter::write_buffer(
    uint32_t binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type)
{
    bufferInfos.push_back(VkDescriptorBufferInfo{
        .buffer = buffer,
        .offset = static_cast<VkDeviceSize>(offset),
        .range = static_cast<VkDeviceSize>(size),
    });

    writes.push_back(VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = binding,
        .descriptorCount = 1,
        .descriptorType = type,
        .pBufferInfo = &bufferInfos.back(),
    });
}

void DescriptorWriter::clear()
{
    imageInfos.clear();
    bufferInfos.clear();
    writes.clear();
}

void DescriptorWriter::update_set(VkDevice device, VkDescriptorSet set)
{
    for (VkWriteDescriptorSet& write : writes) {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}
