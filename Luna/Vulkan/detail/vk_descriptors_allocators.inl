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
    poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
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
                                                        const void* pNext,
                                                        vk::DescriptorPool* outPool)
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

    if (outPool != nullptr) {
        *outPool = poolToUse;
    }
    readyPools.push_back(poolToUse);
    return ds;
}

void DescriptorAllocatorGrowable::free(vk::Device device, vk::DescriptorPool pool, vk::DescriptorSet set)
{
    if (!pool || !set) {
        return;
    }

    const vk::Result result = device.freeDescriptorSets(pool, 1, &set);
    if (result != vk::Result::eSuccess) {
        LUNA_CORE_WARN("Failed to free descriptor set from pool: {}", vk::to_string(result));
        return;
    }

    const auto fullIt = std::find(fullPools.begin(), fullPools.end(), pool);
    if (fullIt != fullPools.end()) {
        fullPools.erase(fullIt);
    }

    if (std::find(readyPools.begin(), readyPools.end(), pool) == readyPools.end()) {
        readyPools.push_back(pool);
    }
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
    poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    poolInfo.maxSets = setCount;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    vk::DescriptorPool newPool{};
    VK_CHECK(device.createDescriptorPool(&poolInfo, nullptr, &newPool));
    return newPool;
}
