#include "vk_descriptors.h"

#include <cassert>

#include <algorithm>

namespace {
vk::DescriptorType mapResourceTypeToVulkan(luna::ResourceType type)
{
    switch (type) {
        case luna::ResourceType::UniformBuffer:
            return vk::DescriptorType::eUniformBuffer;
        case luna::ResourceType::DynamicUniformBuffer:
            return vk::DescriptorType::eUniformBufferDynamic;
        case luna::ResourceType::CombinedImageSampler:
            return vk::DescriptorType::eCombinedImageSampler;
        case luna::ResourceType::SampledImage:
            return vk::DescriptorType::eSampledImage;
        case luna::ResourceType::Sampler:
            return vk::DescriptorType::eSampler;
        case luna::ResourceType::StorageBuffer:
            return vk::DescriptorType::eStorageBuffer;
        case luna::ResourceType::StorageImage:
            return vk::DescriptorType::eStorageImage;
        case luna::ResourceType::InputAttachment:
            return vk::DescriptorType::eInputAttachment;
        default:
            assert(false && "Unsupported resource type.");
            return static_cast<vk::DescriptorType>(VK_DESCRIPTOR_TYPE_MAX_ENUM);
    }
}

void add_or_merge_binding(std::vector<vk::DescriptorSetLayoutBinding>& bindings,
                          const vk::DescriptorSetLayoutBinding& binding)
{
    const auto existing =
        std::find_if(bindings.begin(), bindings.end(), [&](const vk::DescriptorSetLayoutBinding& candidate) {
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
                                                          vk::ShaderStageFlags shaderStages)
{
    vk::DescriptorSetLayoutBinding binding{};
    binding.binding = data.binding;
    binding.descriptorType = mapResourceTypeToVulkan(data.type);
    binding.descriptorCount = std::max(1u, data.count);
    binding.stageFlags = shaderStages;
    binding.pImmutableSamplers = nullptr;

    add_or_merge_binding(bindings, binding);
}

void DescriptorLayoutBuilder::add_binding(uint32_t binding, vk::DescriptorType type)
{
    vk::DescriptorSetLayoutBinding newbind{};
    newbind.binding = binding;
    newbind.descriptorType = type;
    newbind.descriptorCount = 1;

    add_or_merge_binding(bindings, newbind);
}

void DescriptorLayoutBuilder::add_bindings_from_reflection(const luna::Shader::ReflectionMap& reflectionMap,
                                                           uint32_t setIndex,
                                                           vk::ShaderStageFlags shaderStages)
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

vk::DescriptorSetLayout DescriptorLayoutBuilder::build(vk::Device device,
                                                       vk::ShaderStageFlags shaderStages,
                                                       const void* pNext,
                                                       vk::DescriptorSetLayoutCreateFlags flags)
{
    for (auto& b : bindings) {
        b.stageFlags |= shaderStages;
    }

    std::sort(bindings.begin(),
              bindings.end(),
              [](const vk::DescriptorSetLayoutBinding& lhs, const vk::DescriptorSetLayoutBinding& rhs) {
                  return lhs.binding < rhs.binding;
              });

    vk::DescriptorSetLayoutCreateInfo info{};
    info.pNext = pNext;
    info.flags = flags;
    info.bindingCount = static_cast<uint32_t>(bindings.size());
    info.pBindings = bindings.data();

    vk::DescriptorSetLayout set{};
    VK_CHECK(device.createDescriptorSetLayout(&info, nullptr, &set));
    return set;
}

void DescriptorAllocator::init_pool(vk::Device device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios)
{
    std::vector<vk::DescriptorPoolSize> poolSizes;
    poolSizes.reserve(poolRatios.size());
    for (const PoolSizeRatio ratio : poolRatios) {
        vk::DescriptorPoolSize poolSize{};
        poolSize.type = ratio.type;
        poolSize.descriptorCount = static_cast<uint32_t>(ratio.ratio * maxSets);
        poolSizes.push_back(poolSize);
    }

    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.flags = {};
    poolInfo.maxSets = maxSets;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    VK_CHECK(device.createDescriptorPool(&poolInfo, nullptr, &pool));
}

void DescriptorAllocator::clear_descriptors(vk::Device device)
{
    if (!pool) {
        return;
    }

    VK_CHECK(device.resetDescriptorPool(pool, {}));
}

void DescriptorAllocator::destroy_pool(vk::Device device)
{
    if (!pool) {
        return;
    }

    device.destroyDescriptorPool(pool, nullptr);
    pool = nullptr;
}

vk::DescriptorSet DescriptorAllocator::allocate(vk::Device device, vk::DescriptorSetLayout layout)
{
    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    vk::DescriptorSet ds{};
    VK_CHECK(device.allocateDescriptorSets(&allocInfo, &ds));
    return ds;
}

void DescriptorAllocatorGrowable::init(vk::Device device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios)
{
    ratios.assign(poolRatios.begin(), poolRatios.end());
    fullPools.clear();
    readyPools.clear();
    setsPerPool = initialSets;

    readyPools.push_back(create_pool(device, initialSets, poolRatios));
}

void DescriptorAllocatorGrowable::clear_pools(vk::Device device)
{
    for (const vk::DescriptorPool poolHandle : readyPools) {
        VK_CHECK(device.resetDescriptorPool(poolHandle, {}));
    }

    for (const vk::DescriptorPool poolHandle : fullPools) {
        VK_CHECK(device.resetDescriptorPool(poolHandle, {}));
        readyPools.push_back(poolHandle);
    }

    fullPools.clear();
}

void DescriptorAllocatorGrowable::destroy_pools(vk::Device device)
{
    for (const vk::DescriptorPool poolHandle : readyPools) {
        device.destroyDescriptorPool(poolHandle, nullptr);
    }
    readyPools.clear();

    for (const vk::DescriptorPool poolHandle : fullPools) {
        device.destroyDescriptorPool(poolHandle, nullptr);
    }
    fullPools.clear();
}

vk::DescriptorSet DescriptorAllocatorGrowable::allocate(vk::Device device,
                                                        vk::DescriptorSetLayout layout,
                                                        const void* pNext)
{
    vk::DescriptorPool poolToUse = get_pool(device);

    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.pNext = pNext;
    allocInfo.descriptorPool = poolToUse;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    vk::DescriptorSet ds{};
    const vk::Result result = device.allocateDescriptorSets(&allocInfo, &ds);
    if (result == vk::Result::eErrorOutOfPoolMemory || result == vk::Result::eErrorFragmentedPool) {
        fullPools.push_back(poolToUse);

        poolToUse = get_pool(device);
        allocInfo.descriptorPool = poolToUse;
        VK_CHECK(device.allocateDescriptorSets(&allocInfo, &ds));
    } else if (result != vk::Result::eSuccess) {
        LUNA_CORE_FATAL("Vulkan call failed: vk::Device::allocateDescriptorSets returned {}", vk::to_string(result));
        std::abort();
    }

    readyPools.push_back(poolToUse);
    return ds;
}

vk::DescriptorPool DescriptorAllocatorGrowable::get_pool(vk::Device device)
{
    if (!readyPools.empty()) {
        const vk::DescriptorPool poolHandle = readyPools.back();
        readyPools.pop_back();
        return poolHandle;
    }

    setsPerPool = std::min(static_cast<uint32_t>(setsPerPool * 1.5f), 4092u);
    return create_pool(device, setsPerPool, ratios);
}

vk::DescriptorPool DescriptorAllocatorGrowable::create_pool(vk::Device device,
                                                            uint32_t setCount,
                                                            std::span<PoolSizeRatio> poolRatios)
{
    std::vector<vk::DescriptorPoolSize> poolSizes;
    poolSizes.reserve(poolRatios.size());
    for (const PoolSizeRatio ratio : poolRatios) {
        vk::DescriptorPoolSize poolSize{};
        poolSize.type = ratio.type;
        poolSize.descriptorCount = static_cast<uint32_t>(ratio.ratio * setCount);
        poolSizes.push_back(poolSize);
    }

    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.flags = {};
    poolInfo.maxSets = setCount;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    vk::DescriptorPool newPool{};
    VK_CHECK(device.createDescriptorPool(&poolInfo, nullptr, &newPool));
    return newPool;
}

void DescriptorWriter::write_image(uint32_t binding,
                                   vk::ImageView image,
                                   vk::Sampler sampler,
                                   vk::ImageLayout layout,
                                   vk::DescriptorType type)
{
    vk::DescriptorImageInfo imageInfo{};
    imageInfo.sampler = sampler;
    imageInfo.imageView = image;
    imageInfo.imageLayout = layout;
    imageInfos.push_back(imageInfo);

    vk::WriteDescriptorSet write{};
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pImageInfo = &imageInfos.back();
    writes.push_back(write);
}

void DescriptorWriter::write_buffer(uint32_t binding,
                                    vk::Buffer buffer,
                                    size_t size,
                                    size_t offset,
                                    vk::DescriptorType type)
{
    vk::DescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = buffer;
    bufferInfo.offset = static_cast<vk::DeviceSize>(offset);
    bufferInfo.range = static_cast<vk::DeviceSize>(size);
    bufferInfos.push_back(bufferInfo);

    vk::WriteDescriptorSet write{};
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = &bufferInfos.back();
    writes.push_back(write);
}

void DescriptorWriter::clear()
{
    imageInfos.clear();
    bufferInfos.clear();
    writes.clear();
}

void DescriptorWriter::update_set(vk::Device device, vk::DescriptorSet set)
{
    for (vk::WriteDescriptorSet& write : writes) {
        write.dstSet = set;
    }

    device.updateDescriptorSets(static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}
