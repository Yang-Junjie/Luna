#include "vk_descriptors.h"

#include <algorithm>
#include <cassert>

namespace {
VkDescriptorType map_resource_type_to_vk(luna::ResourceType type)
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

void add_or_merge_binding(std::vector<VkDescriptorSetLayoutBinding>& bindings, const VkDescriptorSetLayoutBinding& binding)
{
    const auto existing = std::find_if(bindings.begin(), bindings.end(), [&](const VkDescriptorSetLayoutBinding& candidate) {
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

void DescriptorLayoutBuilder::add_binding_from_reflection(const luna::ShaderReflectionData& data, VkShaderStageFlags shaderStages)
{
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = data.binding;
    binding.descriptorType = map_resource_type_to_vk(data.type);
    binding.descriptorCount = std::max(1u, data.count);
    binding.stageFlags = shaderStages;
    binding.pImmutableSamplers = nullptr;

    add_or_merge_binding(bindings, binding);
}

void DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType type)
{
    VkDescriptorSetLayoutBinding newbind{};
    newbind.binding = binding;
    newbind.descriptorCount = 1;
    newbind.descriptorType = type;

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

    std::sort(bindings.begin(), bindings.end(), [](const VkDescriptorSetLayoutBinding& lhs,
                                                   const VkDescriptorSetLayoutBinding& rhs) {
        return lhs.binding < rhs.binding;
    });

    VkDescriptorSetLayoutCreateInfo info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    info.pNext = pNext;

    info.pBindings = bindings.data();
    info.bindingCount = (uint32_t) bindings.size();
    info.flags = flags;

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

    VkDescriptorPoolCreateInfo pool_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool_info.flags = 0;
    pool_info.maxSets = maxSets;
    pool_info.poolSizeCount = (uint32_t) poolSizes.size();
    pool_info.pPoolSizes = poolSizes.data();

    vkCreateDescriptorPool(device, &pool_info, nullptr, &pool);
}

void DescriptorAllocator::clear_descriptors(VkDevice device)
{
    vkResetDescriptorPool(device, pool, 0);
}

void DescriptorAllocator::destroy_pool(VkDevice device)
{
    vkDestroyDescriptorPool(device, pool, nullptr);
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo allocInfo = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet ds;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));

    return ds;
}
