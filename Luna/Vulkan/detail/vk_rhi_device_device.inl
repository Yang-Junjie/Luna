VulkanRHIDevice::VulkanRHIDevice()
    : m_commandContext(std::make_unique<CommandContext>(*this)),
      m_deviceHandle(DeviceHandle::fromRaw(g_nextVulkanDeviceId.fetch_add(1, std::memory_order_relaxed))),
      m_primarySurface(std::make_unique<Surface>(*this)),
      m_primarySwapchain(std::make_unique<Swapchain>(*this)),
      m_graphicsQueue(std::make_unique<CommandQueue>(*this, RHIQueueType::Graphics))
{}

VulkanRHIDevice::~VulkanRHIDevice()
{
    shutdown();
}

DeviceHandle VulkanRHIDevice::getHandle() const
{
    return m_deviceHandle;
}

RHIBackend VulkanRHIDevice::getBackend() const
{
    return RHIBackend::Vulkan;
}

RHICapabilities VulkanRHIDevice::getCapabilities() const
{
    return m_capabilities;
}

RHIDeviceLimits VulkanRHIDevice::getDeviceLimits() const
{
    return m_limits;
}

RHIFormatSupport VulkanRHIDevice::queryFormatSupport(PixelFormat format) const
{
    const vk::Format vkFormat = to_vulkan_format(format);

    RHIFormatSupport support{};
    support.format = format;
    support.backendFormatName = to_backend_format_name(vkFormat);
    if (!m_initialized || !m_context._chosenGPU || vkFormat == vk::Format::eUndefined) {
        return support;
    }

    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(
        static_cast<VkPhysicalDevice>(m_context._chosenGPU), static_cast<VkFormat>(vkFormat), &properties);

    const VkFormatFeatureFlags features = properties.optimalTilingFeatures;
    support.sampled = (features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0;
    support.colorAttachment = (features & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0;
    support.depthStencilAttachment = (features & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
    support.storage = (features & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0;
    return support;
}

RHISwapchainState VulkanRHIDevice::getSwapchainState() const
{
    return m_swapchainState;
}

IRHISurface* VulkanRHIDevice::getPrimarySurface()
{
    return m_primarySurface.get();
}

const IRHISurface* VulkanRHIDevice::getPrimarySurface() const
{
    return m_primarySurface.get();
}

IRHISwapchain* VulkanRHIDevice::getPrimarySwapchain()
{
    return m_primarySwapchain.get();
}

const IRHISwapchain* VulkanRHIDevice::getPrimarySwapchain() const
{
    return m_primarySwapchain.get();
}

IRHICommandQueue* VulkanRHIDevice::getCommandQueue(RHIQueueType queueType)
{
    return queueType == RHIQueueType::Graphics ? m_graphicsQueue.get() : nullptr;
}

const IRHICommandQueue* VulkanRHIDevice::getCommandQueue(RHIQueueType queueType) const
{
    return queueType == RHIQueueType::Graphics ? m_graphicsQueue.get() : nullptr;
}

RHIResult VulkanRHIDevice::init(const DeviceCreateInfo& createInfo)
{
    if (m_initialized || createInfo.backend != RHIBackend::Vulkan || createInfo.applicationName.empty()) {
        LUNA_CORE_ERROR("VulkanRHIDevice::init rejected invalid arguments");
        return RHIResult::InvalidArgument;
    }

    m_applicationName = std::string(createInfo.applicationName);
    m_createInfo = createInfo;
    m_createInfo.applicationName = m_applicationName;
    if (m_createInfo.swapchain.bufferCount == 0) {
        m_createInfo.swapchain.bufferCount = 2;
    }
    if (m_createInfo.swapchain.format == PixelFormat::Undefined) {
        m_createInfo.swapchain.format = PixelFormat::BGRA8Unorm;
    }
    m_nativeWindow = static_cast<GLFWwindow*>(createInfo.nativeWindow);
    m_capabilities = QueryRHICapabilities(RHIBackend::Vulkan);
    m_capabilities.supportsPresent = m_nativeWindow != nullptr;
    m_context.setSwapchainDesc(m_createInfo.swapchain);
    m_context.setApplicationName(m_applicationName);
    m_context.setPreferredAdapterId(m_createInfo.adapterId);

    bool engineInitialized = false;
    if (m_nativeWindow != nullptr) {
        NativeWindowProxy windowProxy(m_nativeWindow);
        windowProxy.setVSync(m_createInfo.swapchain.vsync);
        engineInitialized = m_context.init(windowProxy);
        m_primarySurface->assign(g_nextVulkanSurfaceId.fetch_add(1, std::memory_order_relaxed));
    } else {
        engineInitialized = m_context.init_headless();
        m_primarySurface->reset();
    }

    if (!engineInitialized) {
        LUNA_CORE_ERROR("VulkanRHIDevice::init failed during VulkanDeviceContext bootstrap");
        m_nativeWindow = nullptr;
        m_applicationName.clear();
        m_primarySurface->reset();
        return RHIResult::InternalError;
    }

    m_initialized = true;
    m_frameInProgress = false;
    m_pendingPresent = false;
    m_recreateAfterPresent = false;
    m_resourceSetAllocatorInitialized = false;
    m_retirementQueue.clear();
    m_timelineEvents.clear();
    m_nextTimelineEventSerial = 0;
    m_lastSubmittedSerial = 0;
    m_lastCompletedSerial = 0;
    m_descriptorRetireCount = 0;
    m_descriptorRecycleCount = 0;
    m_lastObservedSwapchainGeneration = 0;
    refreshRuntimeProperties();
    refreshBackbufferHandle();
    LUNA_CORE_INFO("VulkanRHIDevice created");
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::waitIdle()
{
    if (!m_initialized || !m_context._device) {
        return RHIResult::InvalidArgument;
    }

    const vk::Result waitResult = m_context._device.waitIdle();
    if (waitResult != vk::Result::eSuccess) {
        LUNA_CORE_ERROR("VulkanRHIDevice::waitIdle failed: {}", vk::to_string(waitResult));
        return RHIResult::InternalError;
    }

    appendTimelineEvent("device idle");
    if (m_lastSubmittedSerial > m_lastCompletedSerial) {
        retireCompletedSerial(m_lastSubmittedSerial);
    }

    return RHIResult::Success;
}

void VulkanRHIDevice::shutdown()
{
    if (!m_initialized) {
        return;
    }

    if (m_context._device) {
        VK_CHECK(m_context._device.waitIdle());
    }

    if (m_lastSubmittedSerial > m_lastCompletedSerial) {
        retireCompletedSerial(m_lastSubmittedSerial);
    }

    destroyAllResources();
    m_commandContext->reset();
    m_context.end_upload_batch();
    m_context.cleanup();
    m_nativeWindow = nullptr;
    m_applicationName.clear();
    m_primarySurface->reset();
    m_primarySwapchain->reset();
    m_initialized = false;
    m_frameInProgress = false;
    m_pendingPresent = false;
    m_recreateAfterPresent = false;
    m_limits = {};
    m_swapchainState = {};
    m_retirementQueue.clear();
    m_timelineEvents.clear();
    m_nextTimelineEventSerial = 0;
    m_lastSubmittedSerial = 0;
    m_lastCompletedSerial = 0;
    m_descriptorRetireCount = 0;
    m_descriptorRecycleCount = 0;
    m_lastObservedSwapchainGeneration = 0;
}

RHIResult VulkanRHIDevice::createCommandList(RHIQueueType queueType, std::unique_ptr<IRHICommandList>* outCommandList)
{
    if (!m_initialized || outCommandList == nullptr) {
        return RHIResult::InvalidArgument;
    }

    if (queueType != RHIQueueType::Graphics) {
        *outCommandList = nullptr;
        return RHIResult::Unsupported;
    }

    *outCommandList = std::make_unique<CommandList>(*this, queueType);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::createFence(std::unique_ptr<IRHIFence>* outFence, bool signaled)
{
    if (!m_initialized || outFence == nullptr) {
        return RHIResult::InvalidArgument;
    }

    std::unique_ptr<Fence> fence = std::make_unique<Fence>(*this, signaled);
    if (!fence->isValid()) {
        return RHIResult::InternalError;
    }

    *outFence = std::move(fence);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::createBuffer(const BufferDesc& desc, BufferHandle* outHandle, const void* initialData)
{
    if (!m_initialized || outHandle == nullptr || desc.size == 0) {
        return RHIResult::InvalidArgument;
    }

    AllocatedBuffer buffer = m_context.create_buffer(desc, initialData);
    const BufferHandle handle = m_bindingRegistry.register_buffer(buffer.buffer);
    if (!handle.isValid()) {
        return RHIResult::InternalError;
    }

    m_buffers.insert_or_assign(handle.value, BufferResource{.desc = desc, .buffer = buffer});
    *outHandle = handle;
    return RHIResult::Success;
}

void VulkanRHIDevice::destroyBuffer(BufferHandle handle)
{
    BufferResource* buffer = findBuffer(handle);
    if (buffer == nullptr) {
        return;
    }

    const BufferResource resource = *buffer;
    m_bindingRegistry.unregister_buffer(handle);
    m_buffers.erase(handle.value);

    const uint64_t serial = pendingSubmitSerial();
    scheduleRetirement(serial,
                       "deferred destroy scheduled type=buffer serial=" + std::to_string(serial),
                       [this, resource, serial]() {
                           m_context.destroy_buffer(resource.buffer);
                           appendTimelineEvent("retired destroy type=buffer serial=" + std::to_string(serial));
                       });
}

RHIResult VulkanRHIDevice::writeBuffer(BufferHandle handle, const void* data, uint64_t size, uint64_t offset)
{
    BufferResource* buffer = findBuffer(handle);
    if (buffer == nullptr || data == nullptr || size == 0) {
        return RHIResult::InvalidArgument;
    }

    return m_context.uploadBufferData(buffer->buffer,
                                     data,
                                     static_cast<size_t>(size),
                                     static_cast<size_t>(offset))
               ? RHIResult::Success
               : RHIResult::InternalError;
}

RHIResult VulkanRHIDevice::readBuffer(BufferHandle handle, void* outData, uint64_t size, uint64_t offset)
{
    BufferResource* buffer = findBuffer(handle);
    if (buffer == nullptr || outData == nullptr || size == 0) {
        return RHIResult::InvalidArgument;
    }

    if (buffer->buffer.info.pMappedData == nullptr || offset + size > buffer->buffer.info.size) {
        return RHIResult::InvalidArgument;
    }

    std::memcpy(outData, static_cast<const std::byte*>(buffer->buffer.info.pMappedData) + offset, static_cast<size_t>(size));
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::createImage(const ImageDesc& desc, ImageHandle* outHandle, const void* initialData)
{
    if (!m_initialized || outHandle == nullptr) {
        return RHIResult::InvalidArgument;
    }

    if (!validate_image_desc(desc)) {
        LUNA_CORE_ERROR("VulkanRHIDevice::createImage rejected '{}': {}",
                        desc.debugName.empty() ? "<unnamed>" : std::string(desc.debugName),
                        image_desc_contract_error(desc));
        return RHIResult::InvalidArgument;
    }

    if (desc.mipLevels > max_mip_levels_for_desc(desc) || (initialData != nullptr && luna::is_depth_format(desc.format))) {
        return RHIResult::InvalidArgument;
    }

    AllocatedImage image = m_context.create_image(desc, initialData);
    if (!image.image || !image.imageView) {
        return RHIResult::InternalError;
    }

    const ImageHandle handle = ImageHandle::fromRaw(nextHandleValue(&m_nextImageId));
    if (!handle.isValid() ||
        !m_bindingRegistry.register_image_view(ImageViewHandle::fromRaw(handle.value), image.imageView)) {
        m_context.destroy_image(image);
        return RHIResult::InternalError;
    }

    const vk::ImageLayout initialLayout =
        initialData != nullptr ? vk::ImageLayout::eShaderReadOnlyOptimal : vk::ImageLayout::eUndefined;
    const uint32_t layerCount = imageLayerCount(desc);
    const vk::ImageType backendImageType = desc.type == ImageType::Image3D ? vk::ImageType::e3D : vk::ImageType::e2D;
    const vk::ImageViewType backendDefaultViewType = to_vulkan_image_view_type(default_image_view_type(desc));
    const vk::ImageCreateFlags backendCreateFlags =
        desc.type == ImageType::Cube ? vk::ImageCreateFlagBits::eCubeCompatible : vk::ImageCreateFlags{};
    std::vector<vk::ImageLayout> subresourceLayouts(static_cast<size_t>(desc.mipLevels) * layerCount, initialLayout);
    m_images.insert_or_assign(handle.value,
                              ImageResource{.desc = desc,
                                            .image = image,
                                            .layout = initialLayout,
                                            .subresourceLayouts = std::move(subresourceLayouts),
                                            .owned = true,
                                            .backendImageType = backendImageType,
                                            .backendDefaultViewType = backendDefaultViewType,
                                            .backendCreateFlags = backendCreateFlags,
                                            .backendLayerCount = layerCount});
    *outHandle = handle;
    return RHIResult::Success;
}

void VulkanRHIDevice::destroyImage(ImageHandle handle)
{
    ImageResource* image = findImage(handle);
    if (image == nullptr) {
        return;
    }

    std::vector<uint64_t> childViews;
    childViews.reserve(m_imageViews.size());
    for (const auto& [viewHandle, imageView] : m_imageViews) {
        if (imageView.imageHandle == handle) {
            childViews.push_back(viewHandle);
        }
    }
    for (const uint64_t viewHandle : childViews) {
        destroyImageView(ImageViewHandle::fromRaw(viewHandle));
    }

    const ImageResource resource = *image;

    m_bindingRegistry.unregister_image_view(ImageViewHandle::fromRaw(handle.value));
    m_images.erase(handle.value);

    if (handle == m_currentBackbufferHandle) {
        m_currentBackbufferHandle = {};
    }

    if (!resource.owned) {
        return;
    }

    const uint64_t serial = pendingSubmitSerial();
    scheduleRetirement(serial,
                       "deferred destroy scheduled type=image serial=" + std::to_string(serial),
                       [this, resource, serial]() {
                           m_context.destroy_image(resource.image);
                           appendTimelineEvent("retired destroy type=image serial=" + std::to_string(serial));
                       });
}

RHIResult VulkanRHIDevice::createImageView(const ImageViewDesc& desc, ImageViewHandle* outHandle)
{
    if (!m_initialized || outHandle == nullptr) {
        return RHIResult::InvalidArgument;
    }

    const ImageResource* image = findImage(desc.image);
    if (image == nullptr || !image->image.image) {
        return RHIResult::InvalidArgument;
    }

    if (!validate_image_view_desc(image->desc, desc)) {
        LUNA_CORE_ERROR("VulkanRHIDevice::createImageView rejected '{}': {}",
                        desc.debugName.empty() ? "<unnamed>" : std::string(desc.debugName),
                        image_view_desc_contract_error(image->desc, desc));
        return RHIResult::InvalidArgument;
    }

    const PixelFormat format = desc.format == PixelFormat::Undefined ? image->desc.format : desc.format;
    if (format != image->desc.format) {
        return RHIResult::Unsupported;
    }

    const vk::Format vkFormat = to_vulkan_format(format);
    if (vkFormat == vk::Format::eUndefined) {
        return RHIResult::InvalidArgument;
    }

    vk::ImageViewCreateInfo viewInfo = vkinit::imageview_create_info(vkFormat,
                                                                     image->image.image,
                                                                     to_vulkan_image_aspect_flags(desc.aspect),
                                                                     to_vulkan_image_view_type(desc.type),
                                                                     desc.mipCount,
                                                                     desc.layerCount,
                                                                     desc.baseMipLevel,
                                                                     desc.baseArrayLayer);

    vk::ImageView view{};
    const vk::Result result = m_context._device.createImageView(&viewInfo, nullptr, &view);
    if (result != vk::Result::eSuccess || !view) {
        LUNA_CORE_ERROR("VulkanRHIDevice::createImageView failed: {}", vk::to_string(result));
        return RHIResult::InternalError;
    }

    const ImageViewHandle handle = ImageViewHandle::fromRaw(nextHandleValue(&m_nextImageViewId));
    if (!handle.isValid() || !m_bindingRegistry.register_image_view(handle, view)) {
        m_context._device.destroyImageView(view, nullptr);
        return RHIResult::InternalError;
    }

    m_imageViews.insert_or_assign(handle.value,
                                  ImageViewResource{.desc = desc,
                                                    .imageHandle = desc.image,
                                                    .view = view,
                                                    .backendViewType = to_vulkan_image_view_type(desc.type),
                                                    .backendFormat = vkFormat});
    *outHandle = handle;
    return RHIResult::Success;
}

void VulkanRHIDevice::destroyImageView(ImageViewHandle handle)
{
    ImageViewResource* imageView = findImageView(handle);
    if (imageView == nullptr) {
        return;
    }

    const ImageViewResource resource = *imageView;

    m_bindingRegistry.unregister_image_view(handle);
    m_imageViews.erase(handle.value);

    const uint64_t serial = pendingSubmitSerial();
    scheduleRetirement(serial,
                       "deferred destroy scheduled type=image_view serial=" + std::to_string(serial),
                       [this, resource, serial]() {
                           if (m_context._device && resource.view) {
                               m_context._device.destroyImageView(resource.view, nullptr);
                           }
                           appendTimelineEvent("retired destroy type=image_view serial=" + std::to_string(serial));
                       });
}

RHIResult VulkanRHIDevice::createSampler(const SamplerDesc& desc, SamplerHandle* outHandle)
{
    if (!m_initialized || outHandle == nullptr) {
        return RHIResult::InvalidArgument;
    }

    if (desc.maxLod < desc.minLod) {
        LUNA_CORE_ERROR("VulkanRHIDevice::createSampler rejected '{}': maxLod ({}) < minLod ({})",
                        desc.debugName.empty() ? "<unnamed>" : std::string(desc.debugName),
                        desc.maxLod,
                        desc.minLod);
        return RHIResult::InvalidArgument;
    }

    vk::PhysicalDeviceProperties properties{};
    if (m_context._chosenGPU) {
        m_context._chosenGPU.getProperties(&properties);
    }
    if (desc.anisotropyEnable) {
        if (!m_context.isSamplerAnisotropyEnabled()) {
            LUNA_CORE_ERROR("VulkanRHIDevice::createSampler rejected '{}': samplerAnisotropy feature is unavailable",
                            desc.debugName.empty() ? "<unnamed>" : std::string(desc.debugName));
            return RHIResult::Unsupported;
        }

        if (desc.maxAnisotropy < 1.0f || desc.maxAnisotropy > properties.limits.maxSamplerAnisotropy) {
            LUNA_CORE_ERROR("VulkanRHIDevice::createSampler rejected '{}': requested maxAnisotropy={} exceeds device limit={}",
                            desc.debugName.empty() ? "<unnamed>" : std::string(desc.debugName),
                            desc.maxAnisotropy,
                            properties.limits.maxSamplerAnisotropy);
            return RHIResult::InvalidArgument;
        }
    }

    vk::SamplerCreateInfo createInfo{};
    const vk::Sampler sampler = m_context.create_sampler(desc, &createInfo);
    if (!sampler) {
        return RHIResult::InternalError;
    }

    const SamplerHandle handle = m_bindingRegistry.register_sampler(sampler);
    if (!handle.isValid()) {
        m_context._device.destroySampler(sampler, nullptr);
        return RHIResult::InternalError;
    }

    DebugSamplerInfo debugInfo{};
    debugInfo.desc = desc;
    debugInfo.magFilter = createInfo.magFilter;
    debugInfo.minFilter = createInfo.minFilter;
    debugInfo.mipmapMode = createInfo.mipmapMode;
    debugInfo.addressModeU = createInfo.addressModeU;
    debugInfo.addressModeV = createInfo.addressModeV;
    debugInfo.addressModeW = createInfo.addressModeW;
    debugInfo.mipLodBias = createInfo.mipLodBias;
    debugInfo.minLod = createInfo.minLod;
    debugInfo.maxLod = createInfo.maxLod;
    debugInfo.anisotropyEnable = createInfo.anisotropyEnable == VK_TRUE;
    debugInfo.maxAnisotropy = createInfo.maxAnisotropy;
    debugInfo.compareEnable = createInfo.compareEnable == VK_TRUE;
    debugInfo.compareOp = createInfo.compareOp;
    debugInfo.borderColor = createInfo.borderColor;

    m_samplers.insert_or_assign(handle.value, SamplerResource{.desc = desc, .sampler = sampler, .debugInfo = debugInfo});
    *outHandle = handle;
    return RHIResult::Success;
}

void VulkanRHIDevice::destroySampler(SamplerHandle handle)
{
    SamplerResource* sampler = findSampler(handle);
    if (sampler == nullptr) {
        return;
    }

    const SamplerResource resource = *sampler;

    m_bindingRegistry.unregister_sampler(handle);
    m_samplers.erase(handle.value);

    const uint64_t serial = pendingSubmitSerial();
    scheduleRetirement(serial,
                       "deferred destroy scheduled type=sampler serial=" + std::to_string(serial),
                       [this, resource, serial]() {
                           if (m_context._device && resource.sampler) {
                               m_context._device.destroySampler(resource.sampler, nullptr);
                           }
                           appendTimelineEvent("retired destroy type=sampler serial=" + std::to_string(serial));
                       });
}

RHIResult VulkanRHIDevice::createShader(const ShaderDesc& desc, ShaderHandle* outHandle)
{
    if (!m_initialized || outHandle == nullptr || desc.stage == ShaderType::None || desc.filePath.empty()) {
        return RHIResult::InvalidArgument;
    }

    const std::filesystem::path filePath = std::filesystem::path(std::string(desc.filePath)).lexically_normal();
    const std::optional<std::vector<uint32_t>> code = load_spirv_code(filePath);
    if (!code.has_value()) {
        LUNA_CORE_ERROR("Failed to load shader '{}'", filePath.generic_string());
        return RHIResult::InternalError;
    }

    const uint64_t shaderId = nextHandleValue(&m_nextShaderId);
    ShaderResource resource{};
    resource.desc = desc;
    resource.filePath = filePath.generic_string();
    resource.desc.filePath = resource.filePath;
    resource.code = *code;
    resource.reflection = std::make_unique<VulkanShader>(resource.code, desc.stage);
    m_shaders.insert_or_assign(shaderId, std::move(resource));

    *outHandle = ShaderHandle::fromRaw(shaderId);
    return RHIResult::Success;
}

void VulkanRHIDevice::destroyShader(ShaderHandle handle)
{
    if (!handle.isValid()) {
        return;
    }

    m_shaders.erase(handle.value);
}

const Shader::ReflectionMap* VulkanRHIDevice::getShaderReflection(ShaderHandle handle) const
{
    const ShaderResource* shader = findShader(handle);
    if (shader == nullptr || shader->reflection == nullptr) {
        return nullptr;
    }

    return &shader->reflection->getReflectionMap();
}

RHIResult VulkanRHIDevice::createResourceLayout(const ResourceLayoutDesc& desc, ResourceLayoutHandle* outHandle)
{
    if (!m_initialized || outHandle == nullptr) {
        return RHIResult::InvalidArgument;
    }

    const vk::DescriptorSetLayout layout = build_resource_layout(m_context._device, desc);
    if (!layout) {
        return RHIResult::InternalError;
    }

    const uint64_t id = nextHandleValue(&m_nextResourceLayoutId);
    m_resourceLayouts.insert_or_assign(id, ResourceLayoutResource{.desc = desc, .layout = layout});
    *outHandle = ResourceLayoutHandle::fromRaw(id);
    return RHIResult::Success;
}

void VulkanRHIDevice::destroyResourceLayout(ResourceLayoutHandle handle)
{
    ResourceLayoutResource* layout = findResourceLayout(handle);
    if (layout == nullptr) {
        return;
    }

    const ResourceLayoutResource resource = *layout;

    m_resourceLayouts.erase(handle.value);

    const uint64_t serial = pendingSubmitSerial();
    scheduleRetirement(serial,
                       "deferred destroy scheduled type=resource_layout serial=" + std::to_string(serial),
                       [this, resource, serial]() {
                           if (m_context._device && resource.layout) {
                               m_context._device.destroyDescriptorSetLayout(resource.layout, nullptr);
                           }
                           appendTimelineEvent("retired destroy type=resource_layout serial=" + std::to_string(serial));
                       });
}

RHIResult VulkanRHIDevice::createResourceSet(ResourceLayoutHandle layoutHandle, ResourceSetHandle* outHandle)
{
    if (!m_initialized || outHandle == nullptr) {
        return RHIResult::InvalidArgument;
    }

    ResourceLayoutResource* layout = findResourceLayout(layoutHandle);
    if (layout == nullptr || !layout->layout) {
        return RHIResult::InvalidArgument;
    }

    if (!m_resourceSetAllocatorInitialized) {
        auto poolRatios = descriptor_pool_ratios();
        m_resourceSetAllocator.init(m_context._device, 32, poolRatios);
        m_resourceSetAllocatorInitialized = true;
    }

    vk::DescriptorPool descriptorPool{};
    const vk::DescriptorSet descriptorSet =
        m_resourceSetAllocator.allocate(m_context._device, layout->layout, nullptr, &descriptorPool);
    const uint64_t id = nextHandleValue(&m_nextResourceSetId);
    m_resourceSets.insert_or_assign(
        id, ResourceSetResource{.layoutHandle = layoutHandle, .pool = descriptorPool, .set = descriptorSet});
    *outHandle = ResourceSetHandle::fromRaw(id);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::updateResourceSet(ResourceSetHandle resourceSet, const ResourceSetWriteDesc& writeDesc)
{
    const ResourceSetResource* resourceSetResource = findResourceSet(resourceSet);
    if (resourceSetResource == nullptr || !resourceSetResource->set) {
        return RHIResult::InvalidArgument;
    }

    return update_resource_set(m_context._device, m_bindingRegistry, resourceSetResource->set, writeDesc)
               ? RHIResult::Success
               : RHIResult::InternalError;
}

void VulkanRHIDevice::destroyResourceSet(ResourceSetHandle handle)
{
    ResourceSetResource* resourceSet = findResourceSet(handle);
    if (resourceSet == nullptr) {
        return;
    }

    const ResourceSetResource resource = *resourceSet;
    m_resourceSets.erase(handle.value);

    const uint64_t serial = pendingSubmitSerial();
    scheduleRetirement(serial,
                       "descriptor set deferred retire serial=" + std::to_string(serial),
                       [this, resource, serial]() {
                           if (m_context._device && resource.pool && resource.set) {
                               m_resourceSetAllocator.free(m_context._device, resource.pool, resource.set);
                               ++m_descriptorRetireCount;
                               ++m_descriptorRecycleCount;
                               appendTimelineEvent("descriptor allocator stats: recycle=" +
                                                   std::to_string(m_descriptorRecycleCount) + " retire=" +
                                                   std::to_string(m_descriptorRetireCount) + " serial=" +
                                                   std::to_string(serial));
                           }
                       });
}

RHIResult VulkanRHIDevice::createGraphicsPipeline(const GraphicsPipelineDesc& desc, PipelineHandle* outHandle)
{
    if (!m_initialized || outHandle == nullptr) {
        return RHIResult::InvalidArgument;
    }

    struct ResolvedShaderStage {
        ShaderHandle handle{};
        std::string sourcePath;
        std::vector<uint32_t> ownedCode;
        std::span<const uint32_t> code;
        std::unique_ptr<VulkanShader> ownedReflection;
        const Shader::ReflectionMap* reflection = nullptr;
    };

    const auto collectSetLayouts =
        [&](std::span<const ResourceLayoutHandle> handles,
            std::vector<const ResourceLayoutDesc*>* outLayoutDescs,
            std::vector<vk::DescriptorSetLayout>* outSetLayouts,
            std::string* outError) -> bool {
        if (outLayoutDescs == nullptr || outSetLayouts == nullptr) {
            return false;
        }

        std::vector<const ResourceLayoutResource*> layouts;
        layouts.reserve(handles.size());
        for (const ResourceLayoutHandle handle : handles) {
            const ResourceLayoutResource* layout = findResourceLayout(handle);
            if (layout == nullptr || !layout->layout) {
                if (outError != nullptr) {
                    *outError = "Pipeline layout collection failed: invalid ResourceLayoutHandle.";
                }
                return false;
            }
            layouts.push_back(layout);
        }

        std::sort(layouts.begin(), layouts.end(), [](const ResourceLayoutResource* lhs, const ResourceLayoutResource* rhs) {
            return lhs->desc.setIndex < rhs->desc.setIndex;
        });
        for (size_t index = 1; index < layouts.size(); ++index) {
            if (layouts[index - 1]->desc.setIndex == layouts[index]->desc.setIndex) {
                if (outError != nullptr) {
                    *outError = "Pipeline layout collection failed: duplicate set index detected.";
                }
                return false;
            }
        }
        for (size_t index = 0; index < layouts.size(); ++index) {
            if (layouts[index]->desc.setIndex != static_cast<uint32_t>(index)) {
                if (outError != nullptr) {
                    *outError = "Pipeline layout collection failed: set indexes must be contiguous and start from 0.";
                }
                return false;
            }
        }

        outLayoutDescs->clear();
        outLayoutDescs->reserve(layouts.size());
        outSetLayouts->clear();
        outSetLayouts->reserve(layouts.size());
        for (const ResourceLayoutResource* layout : layouts) {
            outLayoutDescs->push_back(&layout->desc);
            outSetLayouts->push_back(layout->layout);
        }
        return true;
    };

    const auto resolveShader = [&](ShaderType expectedStage,
                                   ShaderHandle handle,
                                   const ShaderDesc& shaderDesc,
                                   ResolvedShaderStage* outStage,
                                   std::string* outError) -> RHIResult {
        if (outStage == nullptr) {
            return RHIResult::InvalidArgument;
        }

        *outStage = {};
        if (handle.isValid()) {
            const ShaderResource* shader = findShader(handle);
            if (shader == nullptr || shader->reflection == nullptr || shader->desc.stage != expectedStage) {
                if (outError != nullptr) {
                    std::ostringstream builder;
                    builder << "Shader handle " << handle.value << " is invalid for "
                            << shader_stage_label(expectedStage) << " stage.";
                    *outError = builder.str();
                }
                return RHIResult::InvalidArgument;
            }

            outStage->handle = handle;
            outStage->sourcePath = shader->filePath;
            outStage->code = shader->code;
            outStage->reflection = &shader->reflection->getReflectionMap();
            return RHIResult::Success;
        }

        if (shaderDesc.stage != expectedStage || shaderDesc.filePath.empty()) {
            if (outError != nullptr) {
                std::ostringstream builder;
                builder << "Pipeline is missing a valid " << shader_stage_label(expectedStage)
                        << " shader handle or file path.";
                *outError = builder.str();
            }
            return RHIResult::InvalidArgument;
        }

        const std::filesystem::path filePath = std::filesystem::path(std::string(shaderDesc.filePath)).lexically_normal();
        const std::optional<std::vector<uint32_t>> code = load_spirv_code(filePath);
        if (!code.has_value()) {
            if (outError != nullptr) {
                std::ostringstream builder;
                builder << "Failed to load " << shader_stage_label(expectedStage) << " shader '" << filePath.generic_string()
                        << "'.";
                *outError = builder.str();
            }
            return RHIResult::InternalError;
        }

        outStage->sourcePath = filePath.generic_string();
        outStage->ownedCode = *code;
        outStage->code = outStage->ownedCode;
        outStage->ownedReflection = std::make_unique<VulkanShader>(outStage->ownedCode, expectedStage);
        outStage->reflection = &outStage->ownedReflection->getReflectionMap();
        return RHIResult::Success;
    };

    std::string validationError;
    std::vector<const ResourceLayoutDesc*> layoutDescs;
    std::vector<vk::DescriptorSetLayout> setLayouts;
    if (!collectSetLayouts(desc.resourceLayouts, &layoutDescs, &setLayouts, &validationError)) {
        if (!validationError.empty()) {
            LUNA_CORE_ERROR("{}", validationError);
        }
        return RHIResult::InvalidArgument;
    }

    ResolvedShaderStage vertexStage{};
    ResolvedShaderStage fragmentStage{};
    if (const RHIResult result = resolveShader(
            ShaderType::Vertex, desc.vertexShaderHandle, desc.vertexShader, &vertexStage, &validationError);
        result != RHIResult::Success) {
        if (!validationError.empty()) {
            LUNA_CORE_ERROR("{}", validationError);
        }
        return result;
    }
    if (const RHIResult result = resolveShader(
            ShaderType::Fragment, desc.fragmentShaderHandle, desc.fragmentShader, &fragmentStage, &validationError);
        result != RHIResult::Success) {
        if (!validationError.empty()) {
            LUNA_CORE_ERROR("{}", validationError);
        }
        return result;
    }

    std::map<uint64_t, ReflectedBindingInfo> reflectedBindings;
    if (!merge_reflection_map(*vertexStage.reflection, ShaderType::Vertex, &reflectedBindings, &validationError) ||
        !merge_reflection_map(*fragmentStage.reflection, ShaderType::Fragment, &reflectedBindings, &validationError) ||
        !validate_layouts_against_reflection(layoutDescs, reflectedBindings, &validationError)) {
        if (!validationError.empty()) {
            LUNA_CORE_ERROR("{}", validationError);
        }
        return RHIResult::InvalidArgument;
    }

    vk::PushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = desc.pushConstantSize;
    pushConstantRange.stageFlags = to_vulkan_shader_stages(desc.pushConstantVisibility);

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    pipelineLayoutInfo.pSetLayouts = setLayouts.data();
    if (desc.pushConstantSize > 0) {
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    }

    vk::PipelineLayout pipelineLayout{};
    VK_CHECK(m_context._device.createPipelineLayout(&pipelineLayoutInfo, nullptr, &pipelineLayout));

    vk::ShaderModule vertexShaderModule{};
    if (!vkutil::create_shader_module(vertexStage.code, m_context._device, &vertexShaderModule)) {
        m_context._device.destroyPipelineLayout(pipelineLayout, nullptr);
        LUNA_CORE_ERROR("Failed to create vertex shader module for '{}'",
                        desc.debugName.empty() ? vertexStage.sourcePath : std::string(desc.debugName));
        return RHIResult::InternalError;
    }

    vk::ShaderModule fragmentShaderModule{};
    if (!vkutil::create_shader_module(fragmentStage.code, m_context._device, &fragmentShaderModule)) {
        m_context._device.destroyShaderModule(vertexShaderModule, nullptr);
        m_context._device.destroyPipelineLayout(pipelineLayout, nullptr);
        LUNA_CORE_ERROR("Failed to create fragment shader module for '{}'",
                        desc.debugName.empty() ? fragmentStage.sourcePath : std::string(desc.debugName));
        return RHIResult::InternalError;
    }

    const vk::Pipeline pipeline =
        build_graphics_pipeline(m_context._device, desc, pipelineLayout, vertexShaderModule, fragmentShaderModule);
    m_context._device.destroyShaderModule(vertexShaderModule, nullptr);
    m_context._device.destroyShaderModule(fragmentShaderModule, nullptr);
    if (!pipeline) {
        m_context._device.destroyPipelineLayout(pipelineLayout, nullptr);
        return RHIResult::InternalError;
    }

    const uint64_t id = nextHandleValue(&m_nextPipelineId);
    PipelineResource pipelineResource{};
    pipelineResource.resourceLayouts = desc.resourceLayouts;
    pipelineResource.pushConstantSize = desc.pushConstantSize;
    pipelineResource.bindPoint = vk::PipelineBindPoint::eGraphics;
    pipelineResource.pipeline = pipeline;
    pipelineResource.layout = pipelineLayout;
    m_pipelines.insert_or_assign(id, std::move(pipelineResource));

    if (vertexStage.handle.isValid() || fragmentStage.handle.isValid()) {
        LUNA_CORE_INFO("Graphics pipeline '{}' reuses shader handles: VS={} FS={}",
                       desc.debugName.empty() ? std::string_view("unnamed_graphics_pipeline") : desc.debugName,
                       static_cast<unsigned long long>(vertexStage.handle.value),
                       static_cast<unsigned long long>(fragmentStage.handle.value));
    }

    *outHandle = PipelineHandle::fromRaw(id);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::createComputePipeline(const ComputePipelineDesc& desc, PipelineHandle* outHandle)
{
    if (!m_initialized || outHandle == nullptr) {
        return RHIResult::InvalidArgument;
    }

    struct ResolvedShaderStage {
        ShaderHandle handle{};
        std::string sourcePath;
        std::vector<uint32_t> ownedCode;
        std::span<const uint32_t> code;
        std::unique_ptr<VulkanShader> ownedReflection;
        const Shader::ReflectionMap* reflection = nullptr;
    };

    const auto collectSetLayouts =
        [&](std::span<const ResourceLayoutHandle> handles,
            std::vector<const ResourceLayoutDesc*>* outLayoutDescs,
            std::vector<vk::DescriptorSetLayout>* outSetLayouts,
            std::string* outError) -> bool {
        if (outLayoutDescs == nullptr || outSetLayouts == nullptr) {
            return false;
        }

        std::vector<const ResourceLayoutResource*> layouts;
        layouts.reserve(handles.size());
        for (const ResourceLayoutHandle handle : handles) {
            const ResourceLayoutResource* layout = findResourceLayout(handle);
            if (layout == nullptr || !layout->layout) {
                if (outError != nullptr) {
                    *outError = "Pipeline layout collection failed: invalid ResourceLayoutHandle.";
                }
                return false;
            }
            layouts.push_back(layout);
        }

        std::sort(layouts.begin(), layouts.end(), [](const ResourceLayoutResource* lhs, const ResourceLayoutResource* rhs) {
            return lhs->desc.setIndex < rhs->desc.setIndex;
        });
        for (size_t index = 1; index < layouts.size(); ++index) {
            if (layouts[index - 1]->desc.setIndex == layouts[index]->desc.setIndex) {
                if (outError != nullptr) {
                    *outError = "Pipeline layout collection failed: duplicate set index detected.";
                }
                return false;
            }
        }
        for (size_t index = 0; index < layouts.size(); ++index) {
            if (layouts[index]->desc.setIndex != static_cast<uint32_t>(index)) {
                if (outError != nullptr) {
                    *outError = "Pipeline layout collection failed: set indexes must be contiguous and start from 0.";
                }
                return false;
            }
        }

        outLayoutDescs->clear();
        outLayoutDescs->reserve(layouts.size());
        outSetLayouts->clear();
        outSetLayouts->reserve(layouts.size());
        for (const ResourceLayoutResource* layout : layouts) {
            outLayoutDescs->push_back(&layout->desc);
            outSetLayouts->push_back(layout->layout);
        }
        return true;
    };

    const auto resolveShader = [&](ShaderType expectedStage,
                                   ShaderHandle handle,
                                   const ShaderDesc& shaderDesc,
                                   ResolvedShaderStage* outStage,
                                   std::string* outError) -> RHIResult {
        if (outStage == nullptr) {
            return RHIResult::InvalidArgument;
        }

        *outStage = {};
        if (handle.isValid()) {
            const ShaderResource* shader = findShader(handle);
            if (shader == nullptr || shader->reflection == nullptr || shader->desc.stage != expectedStage) {
                if (outError != nullptr) {
                    std::ostringstream builder;
                    builder << "Shader handle " << handle.value << " is invalid for "
                            << shader_stage_label(expectedStage) << " stage.";
                    *outError = builder.str();
                }
                return RHIResult::InvalidArgument;
            }

            outStage->handle = handle;
            outStage->sourcePath = shader->filePath;
            outStage->code = shader->code;
            outStage->reflection = &shader->reflection->getReflectionMap();
            return RHIResult::Success;
        }

        if (shaderDesc.stage != expectedStage || shaderDesc.filePath.empty()) {
            if (outError != nullptr) {
                std::ostringstream builder;
                builder << "Pipeline is missing a valid " << shader_stage_label(expectedStage)
                        << " shader handle or file path.";
                *outError = builder.str();
            }
            return RHIResult::InvalidArgument;
        }

        const std::filesystem::path filePath = std::filesystem::path(std::string(shaderDesc.filePath)).lexically_normal();
        const std::optional<std::vector<uint32_t>> code = load_spirv_code(filePath);
        if (!code.has_value()) {
            if (outError != nullptr) {
                std::ostringstream builder;
                builder << "Failed to load " << shader_stage_label(expectedStage) << " shader '" << filePath.generic_string()
                        << "'.";
                *outError = builder.str();
            }
            return RHIResult::InternalError;
        }

        outStage->sourcePath = filePath.generic_string();
        outStage->ownedCode = *code;
        outStage->code = outStage->ownedCode;
        outStage->ownedReflection = std::make_unique<VulkanShader>(outStage->ownedCode, expectedStage);
        outStage->reflection = &outStage->ownedReflection->getReflectionMap();
        return RHIResult::Success;
    };

    std::string validationError;
    std::vector<const ResourceLayoutDesc*> layoutDescs;
    std::vector<vk::DescriptorSetLayout> setLayouts;
    if (!collectSetLayouts(desc.resourceLayouts, &layoutDescs, &setLayouts, &validationError)) {
        if (!validationError.empty()) {
            LUNA_CORE_ERROR("{}", validationError);
        }
        return RHIResult::InvalidArgument;
    }

    ResolvedShaderStage computeStage{};
    if (const RHIResult result = resolveShader(
            ShaderType::Compute, desc.computeShaderHandle, desc.computeShader, &computeStage, &validationError);
        result != RHIResult::Success) {
        if (!validationError.empty()) {
            LUNA_CORE_ERROR("{}", validationError);
        }
        return result;
    }

    std::map<uint64_t, ReflectedBindingInfo> reflectedBindings;
    if (!merge_reflection_map(*computeStage.reflection, ShaderType::Compute, &reflectedBindings, &validationError) ||
        !validate_layouts_against_reflection(layoutDescs, reflectedBindings, &validationError)) {
        if (!validationError.empty()) {
            LUNA_CORE_ERROR("{}", validationError);
        }
        return RHIResult::InvalidArgument;
    }

    vk::PushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = desc.pushConstantSize;
    pushConstantRange.stageFlags = to_vulkan_shader_stages(desc.pushConstantVisibility);

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    pipelineLayoutInfo.pSetLayouts = setLayouts.data();
    if (desc.pushConstantSize > 0) {
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    }

    vk::PipelineLayout pipelineLayout{};
    VK_CHECK(m_context._device.createPipelineLayout(&pipelineLayoutInfo, nullptr, &pipelineLayout));

    vk::ShaderModule computeShader{};
    if (!vkutil::create_shader_module(computeStage.code, m_context._device, &computeShader)) {
        m_context._device.destroyPipelineLayout(pipelineLayout, nullptr);
        LUNA_CORE_ERROR("Failed to create compute shader module for '{}'",
                        desc.debugName.empty() ? computeStage.sourcePath : std::string(desc.debugName));
        return RHIResult::InternalError;
    }

    vk::ComputePipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.layout = pipelineLayout;
    pipelineCreateInfo.stage =
        vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eCompute, computeShader);

    vk::Pipeline pipeline{};
    const vk::Result pipelineResult =
        m_context._device.createComputePipelines({}, 1, &pipelineCreateInfo, nullptr, &pipeline);
    m_context._device.destroyShaderModule(computeShader, nullptr);
    if (pipelineResult != vk::Result::eSuccess || !pipeline) {
        m_context._device.destroyPipelineLayout(pipelineLayout, nullptr);
        LUNA_CORE_ERROR("Failed to create compute pipeline '{}': {}",
                        desc.debugName.empty() ? computeStage.sourcePath : std::string(desc.debugName),
                        vk::to_string(pipelineResult));
        return RHIResult::InternalError;
    }

    const uint64_t id = nextHandleValue(&m_nextPipelineId);
    PipelineResource pipelineResource{};
    pipelineResource.resourceLayouts = desc.resourceLayouts;
    pipelineResource.pushConstantSize = desc.pushConstantSize;
    pipelineResource.bindPoint = vk::PipelineBindPoint::eCompute;
    pipelineResource.pipeline = pipeline;
    pipelineResource.layout = pipelineLayout;
    m_pipelines.insert_or_assign(id, std::move(pipelineResource));

    if (!desc.debugName.empty()) {
        LUNA_CORE_INFO("{} created via RHI", desc.debugName);
    }
    if (computeStage.handle.isValid()) {
        LUNA_CORE_INFO("Compute pipeline '{}' reuses shader handle={}",
                       desc.debugName.empty() ? std::string_view("unnamed_compute_pipeline") : desc.debugName,
                       static_cast<unsigned long long>(computeStage.handle.value));
    }

    *outHandle = PipelineHandle::fromRaw(id);
    return RHIResult::Success;
}

void VulkanRHIDevice::destroyPipeline(PipelineHandle handle)
{
    PipelineResource* pipeline = findPipeline(handle);
    if (pipeline == nullptr) {
        return;
    }

    const PipelineResource resource = *pipeline;

    m_pipelines.erase(handle.value);

    const uint64_t serial = pendingSubmitSerial();
    scheduleRetirement(serial,
                       "deferred destroy scheduled type=pipeline serial=" + std::to_string(serial),
                       [this, resource, serial]() {
                           if (m_context._device && resource.pipeline) {
                               m_context._device.destroyPipeline(resource.pipeline, nullptr);
                           }
                           if (m_context._device && resource.layout) {
                               m_context._device.destroyPipelineLayout(resource.layout, nullptr);
                           }
                           appendTimelineEvent("retired destroy type=pipeline serial=" + std::to_string(serial));
                       });
}


