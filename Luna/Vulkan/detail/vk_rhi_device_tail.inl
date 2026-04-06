VulkanRHIDevice::BufferResource* VulkanRHIDevice::findBuffer(BufferHandle handle)
{
    const auto it = m_buffers.find(handle.value);
    return it != m_buffers.end() ? &it->second : nullptr;
}

const VulkanRHIDevice::BufferResource* VulkanRHIDevice::findBuffer(BufferHandle handle) const
{
    const auto it = m_buffers.find(handle.value);
    return it != m_buffers.end() ? &it->second : nullptr;
}

VulkanRHIDevice::ImageResource* VulkanRHIDevice::findImage(ImageHandle handle)
{
    const auto it = m_images.find(handle.value);
    return it != m_images.end() ? &it->second : nullptr;
}

const VulkanRHIDevice::ImageResource* VulkanRHIDevice::findImage(ImageHandle handle) const
{
    const auto it = m_images.find(handle.value);
    return it != m_images.end() ? &it->second : nullptr;
}

VulkanRHIDevice::ImageViewResource* VulkanRHIDevice::findImageView(ImageViewHandle handle)
{
    const auto it = m_imageViews.find(handle.value);
    return it != m_imageViews.end() ? &it->second : nullptr;
}

const VulkanRHIDevice::ImageViewResource* VulkanRHIDevice::findImageView(ImageViewHandle handle) const
{
    const auto it = m_imageViews.find(handle.value);
    return it != m_imageViews.end() ? &it->second : nullptr;
}

VulkanRHIDevice::SamplerResource* VulkanRHIDevice::findSampler(SamplerHandle handle)
{
    const auto it = m_samplers.find(handle.value);
    return it != m_samplers.end() ? &it->second : nullptr;
}

const VulkanRHIDevice::SamplerResource* VulkanRHIDevice::findSampler(SamplerHandle handle) const
{
    const auto it = m_samplers.find(handle.value);
    return it != m_samplers.end() ? &it->second : nullptr;
}

VulkanRHIDevice::ShaderResource* VulkanRHIDevice::findShader(ShaderHandle handle)
{
    const auto it = m_shaders.find(handle.value);
    return it != m_shaders.end() ? &it->second : nullptr;
}

const VulkanRHIDevice::ShaderResource* VulkanRHIDevice::findShader(ShaderHandle handle) const
{
    const auto it = m_shaders.find(handle.value);
    return it != m_shaders.end() ? &it->second : nullptr;
}

VulkanRHIDevice::ResourceLayoutResource* VulkanRHIDevice::findResourceLayout(ResourceLayoutHandle handle)
{
    const auto it = m_resourceLayouts.find(handle.value);
    return it != m_resourceLayouts.end() ? &it->second : nullptr;
}

const VulkanRHIDevice::ResourceLayoutResource* VulkanRHIDevice::findResourceLayout(ResourceLayoutHandle handle) const
{
    const auto it = m_resourceLayouts.find(handle.value);
    return it != m_resourceLayouts.end() ? &it->second : nullptr;
}

VulkanRHIDevice::ResourceSetResource* VulkanRHIDevice::findResourceSet(ResourceSetHandle handle)
{
    const auto it = m_resourceSets.find(handle.value);
    return it != m_resourceSets.end() ? &it->second : nullptr;
}

const VulkanRHIDevice::ResourceSetResource* VulkanRHIDevice::findResourceSet(ResourceSetHandle handle) const
{
    const auto it = m_resourceSets.find(handle.value);
    return it != m_resourceSets.end() ? &it->second : nullptr;
}

VulkanRHIDevice::PipelineResource* VulkanRHIDevice::findPipeline(PipelineHandle handle)
{
    const auto it = m_pipelines.find(handle.value);
    return it != m_pipelines.end() ? &it->second : nullptr;
}

const VulkanRHIDevice::PipelineResource* VulkanRHIDevice::findPipeline(PipelineHandle handle) const
{
    const auto it = m_pipelines.find(handle.value);
    return it != m_pipelines.end() ? &it->second : nullptr;
}

void VulkanRHIDevice::destroyAllResources()
{
    while (!m_retirementQueue.empty()) {
        RetirementEntry entry = std::move(m_retirementQueue.front());
        m_retirementQueue.pop_front();
        if (entry.action) {
            entry.action();
        }
    }

    for (auto& [handle, pipeline] : m_pipelines) {
        if (m_context._device && pipeline.pipeline) {
            m_context._device.destroyPipeline(pipeline.pipeline, nullptr);
        }
        if (m_context._device && pipeline.layout) {
            m_context._device.destroyPipelineLayout(pipeline.layout, nullptr);
        }
    }
    m_pipelines.clear();

    m_resourceSets.clear();

    for (auto& [handle, layout] : m_resourceLayouts) {
        if (m_context._device && layout.layout) {
            m_context._device.destroyDescriptorSetLayout(layout.layout, nullptr);
        }
    }
    m_resourceLayouts.clear();

    if (m_resourceSetAllocatorInitialized && m_context._device) {
        m_resourceSetAllocator.destroy_pools(m_context._device);
    }
    m_resourceSetAllocatorInitialized = false;

    for (auto& [handle, sampler] : m_samplers) {
        if (m_context._device && sampler.sampler) {
            m_context._device.destroySampler(sampler.sampler, nullptr);
        }
        m_bindingRegistry.unregister_sampler(SamplerHandle::fromRaw(handle));
    }
    m_samplers.clear();

    for (auto& [handle, imageView] : m_imageViews) {
        if (m_context._device && imageView.view) {
            m_context._device.destroyImageView(imageView.view, nullptr);
        }
        m_bindingRegistry.unregister_image_view(ImageViewHandle::fromRaw(handle));
    }
    m_imageViews.clear();

    for (auto& [handle, image] : m_images) {
        if (image.owned) {
            m_context.destroy_image(image.image);
        }
        m_bindingRegistry.unregister_image_view(ImageViewHandle::fromRaw(handle));
    }
    m_images.clear();
    m_currentBackbufferHandle = {};

    for (auto& [handle, buffer] : m_buffers) {
        m_context.destroy_buffer(buffer.buffer);
        m_bindingRegistry.unregister_buffer(BufferHandle::fromRaw(handle));
    }
    m_buffers.clear();

    m_shaders.clear();
}

void VulkanRHIDevice::refreshBackbufferHandle()
{
    if (!m_initialized || m_swapchainImageIndex >= m_context._swapchainImageViews.size() ||
        m_swapchainImageIndex >= m_context._swapchainImages.size()) {
        m_currentBackbufferHandle = {};
        return;
    }

    if (m_currentBackbufferHandle.isValid()) {
        m_bindingRegistry.unregister_image_view(ImageViewHandle::fromRaw(m_currentBackbufferHandle.value));
        m_images.erase(m_currentBackbufferHandle.value);
        m_currentBackbufferHandle = {};
    }

    const ImageHandle handle = ImageHandle::fromRaw(nextHandleValue(&m_nextImageId));
    if (!handle.isValid() ||
        !m_bindingRegistry.register_image_view(ImageViewHandle::fromRaw(handle.value),
                                               m_context._swapchainImageViews[m_swapchainImageIndex])) {
        return;
    }

    ImageDesc desc{};
    desc.width = m_context._swapchainExtent.width;
    desc.height = m_context._swapchainExtent.height;
    desc.depth = 1;
    desc.mipLevels = 1;
    desc.arrayLayers = 1;
    desc.format = from_vulkan_format(m_context._swapchainImageFormat);
    desc.usage = ImageUsage::ColorAttachment | ImageUsage::TransferDst | ImageUsage::TransferSrc;
    desc.debugName = "SwapchainBackbuffer";

    AllocatedImage image{};
    image.image = m_context._swapchainImages[m_swapchainImageIndex];
    image.imageView = m_context._swapchainImageViews[m_swapchainImageIndex];
    image.imageFormat = m_context._swapchainImageFormat;
    image.imageExtent = {m_context._swapchainExtent.width, m_context._swapchainExtent.height, 1};

    const vk::ImageLayout layout = m_swapchainImageIndex < m_context._swapchainImageLayouts.size()
                                       ? static_cast<vk::ImageLayout>(m_context._swapchainImageLayouts[m_swapchainImageIndex])
                                       : vk::ImageLayout::eUndefined;
    std::vector<vk::ImageLayout> subresourceLayouts(1, layout);
    m_images.insert_or_assign(handle.value,
                              ImageResource{.desc = desc,
                                            .image = image,
                                            .layout = layout,
                                            .subresourceLayouts = std::move(subresourceLayouts),
                                            .owned = false});
    m_currentBackbufferHandle = handle;
}

void VulkanRHIDevice::refreshRuntimeProperties()
{
    m_limits.framesInFlight = std::max(1u, static_cast<uint32_t>(FRAME_OVERLAP));
    m_capabilities.framesInFlight = m_limits.framesInFlight;

    if (m_context._chosenGPU) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(static_cast<VkPhysicalDevice>(m_context._chosenGPU), &properties);
        m_limits.minUniformBufferOffsetAlignment =
            static_cast<uint32_t>(properties.limits.minUniformBufferOffsetAlignment);
        m_limits.maxColorAttachments = properties.limits.maxColorAttachments;
        m_limits.maxImageArrayLayers = properties.limits.maxImageArrayLayers;
    }

    const uint64_t swapchainGeneration = m_context.getSwapchainGeneration();
    if (swapchainGeneration == 0 || !m_context.hasSwapchain()) {
        m_lastObservedSwapchainGeneration = 0;
        m_primarySwapchain->reset();
    } else if (swapchainGeneration != m_lastObservedSwapchainGeneration) {
        m_lastObservedSwapchainGeneration = swapchainGeneration;
        m_primarySwapchain->assign(
            SwapchainHandle::fromRaw(g_nextVulkanSwapchainId.fetch_add(1, std::memory_order_relaxed)));
    }

    m_swapchainState.valid = m_context._swapchain != VK_NULL_HANDLE;
    m_swapchainState.deviceId = m_deviceHandle.value;
    m_swapchainState.swapchainId = m_primarySwapchain->getHandle().value;
    m_swapchainState.desc = m_createInfo.swapchain;
    m_swapchainState.width = m_context._swapchainExtent.width;
    m_swapchainState.height = m_context._swapchainExtent.height;
    m_swapchainState.imageCount = static_cast<uint32_t>(m_context._swapchainImages.size());
    m_swapchainState.currentFormat = from_vulkan_format(m_context._swapchainImageFormat);
    m_swapchainState.vsyncActive = is_vsync_present_mode(m_context.getSwapchainPresentMode());
    m_swapchainState.presentModeName = to_present_mode_name(m_context.getSwapchainPresentMode());
}

bool VulkanRHIDevice::ensureFramebufferReady()
{
    if (!m_initialized || m_nativeWindow == nullptr) {
        return false;
    }

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(m_nativeWindow, &framebufferWidth, &framebufferHeight);
    if (framebufferWidth <= 0 || framebufferHeight <= 0) {
        return false;
    }

    const bool extentMismatch = m_context._swapchainExtent.width != static_cast<uint32_t>(framebufferWidth) ||
                                m_context._swapchainExtent.height != static_cast<uint32_t>(framebufferHeight);
    if (m_context.is_swapchain_resize_requested() || extentMismatch) {
        const bool resized = m_context.resize_swapchain();
        if (resized) {
            refreshRuntimeProperties();
        }
        return resized;
    }

    return m_context._swapchain != VK_NULL_HANDLE;
}

RHIResult VulkanRHIDevice::requestPrimarySwapchainRecreate()
{
    if (!m_initialized || m_nativeWindow == nullptr || !m_context.hasSwapchain()) {
        return RHIResult::InvalidArgument;
    }

    m_context.request_swapchain_resize();
    if (m_frameInProgress) {
        m_recreateAfterPresent = true;
    }

    return RHIResult::Success;
}

uint32_t VulkanRHIDevice::imageLayerCount(const ImageDesc& desc) const
{
    return (desc.type == ImageType::Image2DArray || desc.type == ImageType::Cube) ? std::max(1u, desc.arrayLayers) : 1u;
}

bool VulkanRHIDevice::validateImageRange(const ImageResource& image,
                                         uint32_t baseMipLevel,
                                         uint32_t mipCount,
                                         uint32_t baseArrayLayer,
                                         uint32_t layerCount) const
{
    const uint32_t totalLayers = imageLayerCount(image.desc);
    return mipCount > 0 && layerCount > 0 && baseMipLevel < image.desc.mipLevels &&
           baseMipLevel + mipCount <= image.desc.mipLevels && baseArrayLayer < totalLayers &&
           baseArrayLayer + layerCount <= totalLayers;
}

vk::ImageLayout VulkanRHIDevice::getImageSubresourceLayout(const ImageResource& image,
                                                           uint32_t mipLevel,
                                                           uint32_t arrayLayer) const
{
    const uint32_t totalLayers = imageLayerCount(image.desc);
    const size_t index = static_cast<size_t>(mipLevel) * totalLayers + arrayLayer;
    if (index >= image.subresourceLayouts.size()) {
        return image.layout;
    }
    return image.subresourceLayouts[index];
}

void VulkanRHIDevice::setImageSubresourceLayout(ImageResource& image,
                                                uint32_t mipLevel,
                                                uint32_t arrayLayer,
                                                vk::ImageLayout layout)
{
    const uint32_t totalLayers = imageLayerCount(image.desc);
    const size_t index = static_cast<size_t>(mipLevel) * totalLayers + arrayLayer;
    if (index < image.subresourceLayouts.size()) {
        image.subresourceLayouts[index] = layout;
    }
    image.layout = layout;
}

bool VulkanRHIDevice::getImageRangeLayout(const ImageResource& image,
                                          uint32_t baseMipLevel,
                                          uint32_t mipCount,
                                          uint32_t baseArrayLayer,
                                          uint32_t layerCount,
                                          vk::ImageLayout* outLayout) const
{
    if (outLayout == nullptr || !validateImageRange(image, baseMipLevel, mipCount, baseArrayLayer, layerCount)) {
        return false;
    }

    const vk::ImageLayout firstLayout = getImageSubresourceLayout(image, baseMipLevel, baseArrayLayer);
    for (uint32_t mipLevel = baseMipLevel; mipLevel < baseMipLevel + mipCount; ++mipLevel) {
        for (uint32_t arrayLayer = baseArrayLayer; arrayLayer < baseArrayLayer + layerCount; ++arrayLayer) {
            if (getImageSubresourceLayout(image, mipLevel, arrayLayer) != firstLayout) {
                return false;
            }
        }
    }

    *outLayout = firstLayout;
    return true;
}

void VulkanRHIDevice::setImageRangeLayout(ImageResource& image,
                                          uint32_t baseMipLevel,
                                          uint32_t mipCount,
                                          uint32_t baseArrayLayer,
                                          uint32_t layerCount,
                                          vk::ImageLayout layout)
{
    if (!validateImageRange(image, baseMipLevel, mipCount, baseArrayLayer, layerCount)) {
        return;
    }

    for (uint32_t mipLevel = baseMipLevel; mipLevel < baseMipLevel + mipCount; ++mipLevel) {
        for (uint32_t arrayLayer = baseArrayLayer; arrayLayer < baseArrayLayer + layerCount; ++arrayLayer) {
            setImageSubresourceLayout(image, mipLevel, arrayLayer, layout);
        }
    }
}

uint64_t VulkanRHIDevice::nextHandleValue(uint64_t* counter)
{
    const uint64_t value = *counter;
    ++(*counter);
    return value;
}

uint64_t VulkanRHIDevice::pendingSubmitSerial() const
{
    if (m_frameInProgress) {
        return m_lastSubmittedSerial + 1;
    }

    return m_lastSubmittedSerial;
}

void VulkanRHIDevice::appendTimelineEvent(std::string label)
{
    if (label.empty()) {
        return;
    }

    m_timelineEvents.push_back({++m_nextTimelineEventSerial, std::move(label)});
    if (m_timelineEvents.size() > 128) {
        m_timelineEvents.erase(m_timelineEvents.begin(), m_timelineEvents.begin() + 1);
    }

    LUNA_CORE_INFO("{}", m_timelineEvents.back().label);
}

void VulkanRHIDevice::scheduleRetirement(uint64_t serial, std::string label, std::function<void()> action)
{
    if (serial == 0 || serial <= m_lastCompletedSerial) {
        if (action) {
            action();
        }
        return;
    }

    appendTimelineEvent(std::move(label));
    m_retirementQueue.push_back({serial, {}, std::move(action)});
}

void VulkanRHIDevice::retireCompletedSerial(uint64_t serial)
{
    if (serial <= m_lastCompletedSerial) {
        return;
    }

    m_lastCompletedSerial = serial;
    appendTimelineEvent("retired fence #" + std::to_string(serial));

    while (!m_retirementQueue.empty() && m_retirementQueue.front().serial <= serial) {
        RetirementEntry entry = std::move(m_retirementQueue.front());
        m_retirementQueue.pop_front();
        if (entry.action) {
            entry.action();
        }
    }
}

bool VulkanRHIDevice::debugGetImageInfo(ImageHandle handle, DebugImageInfo* outInfo) const
{
    if (outInfo == nullptr) {
        return false;
    }

    const ImageResource* image = findImage(handle);
    if (image == nullptr) {
        return false;
    }

    *outInfo = {
        .desc = image->desc,
        .backendImageType = image->backendImageType,
        .backendDefaultViewType = image->backendDefaultViewType,
        .backendCreateFlags = image->backendCreateFlags,
        .backendLayerCount = image->backendLayerCount,
    };
    return true;
}

bool VulkanRHIDevice::debugGetImageViewInfo(ImageViewHandle handle, DebugImageViewInfo* outInfo) const
{
    if (outInfo == nullptr) {
        return false;
    }

    const ImageViewResource* view = findImageView(handle);
    if (view == nullptr) {
        return false;
    }

    *outInfo = {
        .desc = view->desc,
        .imageHandle = view->imageHandle,
        .backendViewType = view->backendViewType,
        .backendFormat = view->backendFormat,
    };
    return true;
}

bool VulkanRHIDevice::debugGetSamplerInfo(SamplerHandle handle, DebugSamplerInfo* outInfo) const
{
    if (outInfo == nullptr) {
        return false;
    }

    const SamplerResource* sampler = findSampler(handle);
    if (sampler == nullptr) {
        return false;
    }

    *outInfo = sampler->debugInfo;
    return true;
}

float VulkanRHIDevice::debugGetMaxSamplerAnisotropy() const
{
    if (!m_context._chosenGPU || !m_context.isSamplerAnisotropyEnabled()) {
        return 1.0f;
    }

    vk::PhysicalDeviceProperties properties{};
    m_context._chosenGPU.getProperties(&properties);
    return properties.limits.maxSamplerAnisotropy;
}

bool VulkanRHIDevice::debugSupportsSamplerAnisotropy() const
{
    return m_context.isSamplerAnisotropyEnabled();
}

namespace {

std::vector<RHIAdapterInfo> enumerate_vulkan_adapter_infos()
{
    vkb::InstanceBuilder builder;
    auto instanceRet = builder.set_app_name("LunaAdapterEnumeration")
                           .request_validation_layers(kUseValidationLayers)
                           .use_default_debug_messenger()
                           .require_api_version(1, 3, 0)
                           .set_headless(true)
                           .build();
    if (!instanceRet) {
        log_vkb_error("Adapter enumeration instance creation", instanceRet);
        return {};
    }

    vkb::Instance instance = instanceRet.value();

    VkPhysicalDeviceVulkan13Features features13{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features13.dynamicRendering = true;
    features13.synchronization2 = true;

    VkPhysicalDeviceVulkan12Features features12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    vkb::PhysicalDeviceSelector selector{instance};
    auto devicesRet =
        selector.set_minimum_version(1, 3).set_required_features_13(features13).set_required_features_12(features12).select_devices();
    if (!devicesRet) {
        log_vkb_error("Adapter enumeration physical device selection", devicesRet);
        vkb::destroy_instance(instance);
        return {};
    }

    std::vector<RHIAdapterInfo> adapters;
    adapters.reserve(devicesRet.value().size());
    for (const vkb::PhysicalDevice& physicalDevice : devicesRet.value()) {
        RHIAdapterInfo info{};
        info.adapterId = make_adapter_id(physicalDevice.properties);
        info.backend = RHIBackend::Vulkan;
        info.name = physicalDevice.name;
        info.capabilities = QueryRHICapabilities(RHIBackend::Vulkan);
        info.capabilities.supportsPresent = physicalDevice.is_extension_present(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        info.capabilities.framesInFlight = std::max(1u, static_cast<uint32_t>(FRAME_OVERLAP));
        info.limits.framesInFlight = info.capabilities.framesInFlight;
        info.limits.minUniformBufferOffsetAlignment =
            static_cast<uint32_t>(physicalDevice.properties.limits.minUniformBufferOffsetAlignment);
        info.limits.maxColorAttachments = physicalDevice.properties.limits.maxColorAttachments;
        info.limits.maxImageArrayLayers = physicalDevice.properties.limits.maxImageArrayLayers;
        adapters.push_back(std::move(info));
    }

    vkb::destroy_instance(instance);
    return adapters;
}

class VulkanAdapter final : public IRHIAdapter {
public:
    explicit VulkanAdapter(RHIAdapterInfo info)
        : m_info(std::move(info))
    {}

    RHIAdapterInfo getInfo() const override
    {
        return m_info;
    }

    RHIResult createDevice(const DeviceCreateInfo& createInfo, std::unique_ptr<IRHIDevice>* outDevice) const override
    {
        if (outDevice == nullptr || createInfo.backend != RHIBackend::Vulkan) {
            return RHIResult::InvalidArgument;
        }

        DeviceCreateInfo resolvedCreateInfo = createInfo;
        resolvedCreateInfo.backend = RHIBackend::Vulkan;
        resolvedCreateInfo.adapterId = m_info.adapterId;

        std::unique_ptr<VulkanRHIDevice> device = std::make_unique<VulkanRHIDevice>();
        const RHIResult initResult = device->init(resolvedCreateInfo);
        if (initResult != RHIResult::Success) {
            return initResult;
        }

        *outDevice = std::move(device);
        return RHIResult::Success;
    }

private:
    RHIAdapterInfo m_info;
};

} // namespace

std::vector<std::unique_ptr<IRHIAdapter>> EnumerateVulkanAdapters()
{
    std::vector<std::unique_ptr<IRHIAdapter>> adapters;
    for (RHIAdapterInfo& info : enumerate_vulkan_adapter_infos()) {
        adapters.push_back(std::make_unique<VulkanAdapter>(std::move(info)));
    }

    return adapters;
}


