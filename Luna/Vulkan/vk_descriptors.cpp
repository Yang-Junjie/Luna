#include "vk_descriptors.h"

#include <cassert>

#include <algorithm>

namespace {
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

vk::DescriptorType to_vulkan_descriptor_type(luna::ResourceType type)
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

vk::ShaderStageFlags to_vulkan_shader_stages(luna::ShaderType visibility)
{
    vk::ShaderStageFlags stages{};
    const uint32_t bits = static_cast<uint32_t>(visibility);

    if ((bits & static_cast<uint32_t>(luna::ShaderType::Vertex)) != 0) {
        stages |= vk::ShaderStageFlagBits::eVertex;
    }
    if ((bits & static_cast<uint32_t>(luna::ShaderType::TessControl)) != 0) {
        stages |= vk::ShaderStageFlagBits::eTessellationControl;
    }
    if ((bits & static_cast<uint32_t>(luna::ShaderType::TessEval)) != 0) {
        stages |= vk::ShaderStageFlagBits::eTessellationEvaluation;
    }
    if ((bits & static_cast<uint32_t>(luna::ShaderType::Geometry)) != 0) {
        stages |= vk::ShaderStageFlagBits::eGeometry;
    }
    if ((bits & static_cast<uint32_t>(luna::ShaderType::Fragment)) != 0) {
        stages |= vk::ShaderStageFlagBits::eFragment;
    }
    if ((bits & static_cast<uint32_t>(luna::ShaderType::Compute)) != 0) {
        stages |= vk::ShaderStageFlagBits::eCompute;
    }

    return stages;
}

vk::ImageLayout to_vulkan_image_layout(luna::ResourceType type)
{
    switch (type) {
        case luna::ResourceType::StorageImage:
            return vk::ImageLayout::eGeneral;
        case luna::ResourceType::CombinedImageSampler:
        case luna::ResourceType::SampledImage:
        case luna::ResourceType::InputAttachment:
            return vk::ImageLayout::eShaderReadOnlyOptimal;
        case luna::ResourceType::Sampler:
            return vk::ImageLayout::eUndefined;
        default:
            return vk::ImageLayout::eShaderReadOnlyOptimal;
    }
}

void DescriptorLayoutBuilder::add_binding_from_reflection(const luna::ShaderReflectionData& data,
                                                          vk::ShaderStageFlags shaderStages)
{
    vk::DescriptorSetLayoutBinding binding{};
    binding.binding = data.binding;
    binding.descriptorType = to_vulkan_descriptor_type(data.type);
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

luna::BufferHandle VulkanResourceBindingRegistry::register_buffer(vk::Buffer buffer)
{
    if (!buffer) {
        return {};
    }

    const uint64_t id = m_nextBufferId++;
    m_buffers.insert_or_assign(id, buffer);
    return luna::BufferHandle::fromRaw(id);
}

luna::ImageHandle VulkanResourceBindingRegistry::register_image_view(vk::ImageView imageView)
{
    if (!imageView) {
        return {};
    }

    const uint64_t id = m_nextImageViewId++;
    m_imageViews.insert_or_assign(id, imageView);
    return luna::ImageHandle::fromRaw(id);
}

luna::SamplerHandle VulkanResourceBindingRegistry::register_sampler(vk::Sampler sampler)
{
    if (!sampler) {
        return {};
    }

    const uint64_t id = m_nextSamplerId++;
    m_samplers.insert_or_assign(id, sampler);
    return luna::SamplerHandle::fromRaw(id);
}

void VulkanResourceBindingRegistry::unregister_buffer(luna::BufferHandle handle)
{
    if (!handle.isValid()) {
        return;
    }

    m_buffers.erase(handle.value);
}

void VulkanResourceBindingRegistry::unregister_image_view(luna::ImageHandle handle)
{
    if (!handle.isValid()) {
        return;
    }

    m_imageViews.erase(handle.value);
}

void VulkanResourceBindingRegistry::unregister_sampler(luna::SamplerHandle handle)
{
    if (!handle.isValid()) {
        return;
    }

    m_samplers.erase(handle.value);
}

vk::Buffer VulkanResourceBindingRegistry::resolve_buffer(luna::BufferHandle handle) const
{
    const auto it = m_buffers.find(handle.value);
    return it != m_buffers.end() ? it->second : vk::Buffer{};
}

vk::ImageView VulkanResourceBindingRegistry::resolve_image_view(luna::ImageHandle handle) const
{
    const auto it = m_imageViews.find(handle.value);
    return it != m_imageViews.end() ? it->second : vk::ImageView{};
}

vk::Sampler VulkanResourceBindingRegistry::resolve_sampler(luna::SamplerHandle handle) const
{
    const auto it = m_samplers.find(handle.value);
    return it != m_samplers.end() ? it->second : vk::Sampler{};
}

vk::DescriptorSetLayout build_resource_layout(vk::Device device,
                                              const luna::ResourceLayoutDesc& desc,
                                              const void* pNext,
                                              vk::DescriptorSetLayoutCreateFlags flags)
{
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    bindings.reserve(desc.bindings.size());

    for (const luna::ResourceBindingDesc& bindingDesc : desc.bindings) {
        vk::DescriptorSetLayoutBinding binding{};
        binding.binding = bindingDesc.binding;
        binding.descriptorType = to_vulkan_descriptor_type(bindingDesc.type);
        binding.descriptorCount = std::max(1u, bindingDesc.count);
        binding.stageFlags = to_vulkan_shader_stages(bindingDesc.visibility);
        binding.pImmutableSamplers = nullptr;
        add_or_merge_binding(bindings, binding);
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

    vk::DescriptorSetLayout layout{};
    VK_CHECK(device.createDescriptorSetLayout(&info, nullptr, &layout));

    if (!desc.debugName.empty()) {
        LUNA_CORE_INFO("{} created via RHI", desc.debugName);
    }

    return layout;
}

bool update_resource_set(vk::Device device,
                         const VulkanResourceBindingRegistry& registry,
                         vk::DescriptorSet set,
                         const luna::ResourceSetWriteDesc& writeDesc)
{
    DescriptorWriter writer;

    for (const luna::BufferBindingWriteDesc& bufferWrite : writeDesc.buffers) {
        const vk::Buffer buffer = registry.resolve_buffer(bufferWrite.buffer);
        if (!buffer) {
            LUNA_CORE_ERROR("Failed to resolve RHI buffer binding {}", bufferWrite.binding);
            return false;
        }

        writer.write_buffer(bufferWrite.binding,
                            buffer,
                            static_cast<size_t>(bufferWrite.size),
                            static_cast<size_t>(bufferWrite.offset),
                            to_vulkan_descriptor_type(bufferWrite.type));
    }

    for (const luna::ImageBindingWriteDesc& imageWrite : writeDesc.images) {
        const vk::ImageView imageView = registry.resolve_image_view(imageWrite.image);
        if (!imageView) {
            LUNA_CORE_ERROR("Failed to resolve RHI image binding {}", imageWrite.binding);
            return false;
        }

        const vk::Sampler sampler = registry.resolve_sampler(imageWrite.sampler);
        if (imageWrite.type == luna::ResourceType::CombinedImageSampler && !sampler) {
            LUNA_CORE_ERROR("Failed to resolve RHI sampler binding {}", imageWrite.binding);
            return false;
        }

        writer.write_image(imageWrite.binding,
                           imageView,
                           sampler,
                           to_vulkan_image_layout(imageWrite.type),
                           to_vulkan_descriptor_type(imageWrite.type));
    }

    writer.update_set(device, set);
    return true;
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
