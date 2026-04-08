#include "VkDescriptors.h"

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

void addOrMergeBinding(std::vector<vk::DescriptorSetLayoutBinding>& bindings,
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

void DescriptorLayoutBuilder::addBindingFromReflection(const luna::ShaderReflectionData& data,
                                                          vk::ShaderStageFlags shader_stages)
{
    vk::DescriptorSetLayoutBinding binding{};
    binding.binding = data.m_binding;
    binding.descriptorType = mapResourceTypeToVulkan(data.m_type);
    binding.descriptorCount = std::max(1u, data.m_count);
    binding.stageFlags = shader_stages;
    binding.pImmutableSamplers = nullptr;

    addOrMergeBinding(m_bindings, binding);
}

void DescriptorLayoutBuilder::addBinding(uint32_t binding, vk::DescriptorType type)
{
    vk::DescriptorSetLayoutBinding newbind{};
    newbind.binding = binding;
    newbind.descriptorType = type;
    newbind.descriptorCount = 1;

    addOrMergeBinding(m_bindings, newbind);
}

void DescriptorLayoutBuilder::addBindingsFromReflection(const luna::Shader::ReflectionMap& reflection_map,
                                                           uint32_t set_index,
                                                           vk::ShaderStageFlags shader_stages)
{
    const auto set_it = reflection_map.find(set_index);
    if (set_it == reflection_map.end()) {
        return;
    }

    for (const auto& binding : set_it->second) {
        addBindingFromReflection(binding, shader_stages);
    }
}

void DescriptorLayoutBuilder::clear()
{
    m_bindings.clear();
}

vk::DescriptorSetLayout DescriptorLayoutBuilder::build(vk::Device device,
                                                       vk::ShaderStageFlags shader_stages,
                                                       const void* p_next,
                                                       vk::DescriptorSetLayoutCreateFlags flags)
{
    for (auto& b : m_bindings) {
        b.stageFlags |= shader_stages;
    }

    std::sort(m_bindings.begin(),
              m_bindings.end(),
              [](const vk::DescriptorSetLayoutBinding& lhs, const vk::DescriptorSetLayoutBinding& rhs) {
                  return lhs.binding < rhs.binding;
              });

    vk::DescriptorSetLayoutCreateInfo info{};
    info.pNext = p_next;
    info.flags = flags;
    info.bindingCount = static_cast<uint32_t>(m_bindings.size());
    info.pBindings = m_bindings.data();

    vk::DescriptorSetLayout set{};
    VK_CHECK(device.createDescriptorSetLayout(&info, nullptr, &set));
    return set;
}

void DescriptorAllocator::initPool(vk::Device device, uint32_t max_sets, std::span<PoolSizeRatio> pool_ratios)
{
    std::vector<vk::DescriptorPoolSize> pool_sizes;
    pool_sizes.reserve(pool_ratios.size());
    for (const PoolSizeRatio ratio : pool_ratios) {
        vk::DescriptorPoolSize pool_size{};
        pool_size.type = ratio.m_type;
        pool_size.descriptorCount = static_cast<uint32_t>(ratio.m_ratio * max_sets);
        pool_sizes.push_back(pool_size);
    }

    vk::DescriptorPoolCreateInfo pool_info{};
    pool_info.flags = {};
    pool_info.maxSets = max_sets;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();

    VK_CHECK(device.createDescriptorPool(&pool_info, nullptr, &m_pool));
}

void DescriptorAllocator::clearDescriptors(vk::Device device)
{
    if (!m_pool) {
        return;
    }

    VK_CHECK(device.resetDescriptorPool(m_pool, {}));
}

void DescriptorAllocator::destroyPool(vk::Device device)
{
    if (!m_pool) {
        return;
    }

    device.destroyDescriptorPool(m_pool, nullptr);
    m_pool = nullptr;
}

vk::DescriptorSet DescriptorAllocator::allocate(vk::Device device, vk::DescriptorSetLayout layout)
{
    vk::DescriptorSetAllocateInfo alloc_info{};
    alloc_info.pNext = nullptr;
    alloc_info.descriptorPool = m_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout;

    vk::DescriptorSet ds{};
    VK_CHECK(device.allocateDescriptorSets(&alloc_info, &ds));
    return ds;
}

void DescriptorAllocatorGrowable::init(vk::Device device, uint32_t initial_sets, std::span<PoolSizeRatio> pool_ratios)
{
    m_ratios.assign(pool_ratios.begin(), pool_ratios.end());
    m_full_pools.clear();
    m_ready_pools.clear();
    m_sets_per_pool = initial_sets;

    m_ready_pools.push_back(createPool(device, initial_sets, pool_ratios));
}

void DescriptorAllocatorGrowable::clearPools(vk::Device device)
{
    for (const vk::DescriptorPool pool_handle : m_ready_pools) {
        VK_CHECK(device.resetDescriptorPool(pool_handle, {}));
    }

    for (const vk::DescriptorPool pool_handle : m_full_pools) {
        VK_CHECK(device.resetDescriptorPool(pool_handle, {}));
        m_ready_pools.push_back(pool_handle);
    }

    m_full_pools.clear();
}

void DescriptorAllocatorGrowable::destroyPools(vk::Device device)
{
    for (const vk::DescriptorPool pool_handle : m_ready_pools) {
        device.destroyDescriptorPool(pool_handle, nullptr);
    }
    m_ready_pools.clear();

    for (const vk::DescriptorPool pool_handle : m_full_pools) {
        device.destroyDescriptorPool(pool_handle, nullptr);
    }
    m_full_pools.clear();
}

vk::DescriptorSet DescriptorAllocatorGrowable::allocate(vk::Device device,
                                                        vk::DescriptorSetLayout layout,
                                                        const void* p_next)
{
    vk::DescriptorPool pool_to_use = getPool(device);

    vk::DescriptorSetAllocateInfo alloc_info{};
    alloc_info.pNext = p_next;
    alloc_info.descriptorPool = pool_to_use;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout;

    vk::DescriptorSet ds{};
    const vk::Result result = device.allocateDescriptorSets(&alloc_info, &ds);
    if (result == vk::Result::eErrorOutOfPoolMemory || result == vk::Result::eErrorFragmentedPool) {
        m_full_pools.push_back(pool_to_use);

        pool_to_use = getPool(device);
        alloc_info.descriptorPool = pool_to_use;
        VK_CHECK(device.allocateDescriptorSets(&alloc_info, &ds));
    } else if (result != vk::Result::eSuccess) {
        LUNA_CORE_FATAL("Vulkan call failed: vk::Device::allocateDescriptorSets returned {}", vk::to_string(result));
        std::abort();
    }

    m_ready_pools.push_back(pool_to_use);
    return ds;
}

vk::DescriptorPool DescriptorAllocatorGrowable::getPool(vk::Device device)
{
    if (!m_ready_pools.empty()) {
        const vk::DescriptorPool pool_handle = m_ready_pools.back();
        m_ready_pools.pop_back();
        return pool_handle;
    }

    m_sets_per_pool = std::min(static_cast<uint32_t>(m_sets_per_pool * 1.5f), 4092u);
    return createPool(device, m_sets_per_pool, m_ratios);
}

vk::DescriptorPool DescriptorAllocatorGrowable::createPool(vk::Device device,
                                                            uint32_t set_count,
                                                            std::span<PoolSizeRatio> pool_ratios)
{
    std::vector<vk::DescriptorPoolSize> pool_sizes;
    pool_sizes.reserve(pool_ratios.size());
    for (const PoolSizeRatio ratio : pool_ratios) {
        vk::DescriptorPoolSize pool_size{};
        pool_size.type = ratio.m_type;
        pool_size.descriptorCount = static_cast<uint32_t>(ratio.m_ratio * set_count);
        pool_sizes.push_back(pool_size);
    }

    vk::DescriptorPoolCreateInfo pool_info{};
    pool_info.flags = {};
    pool_info.maxSets = set_count;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();

    vk::DescriptorPool new_pool{};
    VK_CHECK(device.createDescriptorPool(&pool_info, nullptr, &new_pool));
    return new_pool;
}

void DescriptorWriter::writeImage(uint32_t binding,
                                   vk::ImageView image,
                                   vk::Sampler sampler,
                                   vk::ImageLayout layout,
                                   vk::DescriptorType type)
{
    vk::DescriptorImageInfo image_info{};
    image_info.sampler = sampler;
    image_info.imageView = image;
    image_info.imageLayout = layout;
    m_image_infos.push_back(image_info);

    vk::WriteDescriptorSet write{};
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pImageInfo = &m_image_infos.back();
    m_writes.push_back(write);
}

void DescriptorWriter::writeBuffer(uint32_t binding,
                                    vk::Buffer buffer,
                                    size_t size,
                                    size_t offset,
                                    vk::DescriptorType type)
{
    vk::DescriptorBufferInfo buffer_info{};
    buffer_info.buffer = buffer;
    buffer_info.offset = static_cast<vk::DeviceSize>(offset);
    buffer_info.range = static_cast<vk::DeviceSize>(size);
    m_buffer_infos.push_back(buffer_info);

    vk::WriteDescriptorSet write{};
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = &m_buffer_infos.back();
    m_writes.push_back(write);
}

void DescriptorWriter::clear()
{
    m_image_infos.clear();
    m_buffer_infos.clear();
    m_writes.clear();
}

void DescriptorWriter::updateSet(vk::Device device, vk::DescriptorSet set)
{
    for (vk::WriteDescriptorSet& write : m_writes) {
        write.dstSet = set;
    }

    device.updateDescriptorSets(static_cast<uint32_t>(m_writes.size()), m_writes.data(), 0, nullptr);
}

