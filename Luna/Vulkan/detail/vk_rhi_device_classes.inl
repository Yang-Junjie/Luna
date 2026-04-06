class VulkanRHIDevice::CommandContext final : public IRHICommandContext {
public:
    explicit CommandContext(VulkanRHIDevice& device)
        : m_device(device)
    {}

    void beginFrame(vk::CommandBuffer commandBuffer)
    {
        m_commandBuffer = commandBuffer;
        m_rendering = false;
        m_renderExtent = {};
        m_currentAttachment = {};
        m_boundPipelineLayout = nullptr;
        m_boundPipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        m_debugLabelStack.clear();
    }

    void reset()
    {
        if (m_rendering && m_commandBuffer) {
            m_commandBuffer.endRendering();
        }

        m_commandBuffer = nullptr;
        m_rendering = false;
        m_renderExtent = {};
        m_currentAttachment = {};
        m_boundPipelineLayout = nullptr;
        m_boundPipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        m_debugLabelStack.clear();
    }

    bool isRendering() const
    {
        return m_rendering;
    }

    RHIResult beginRendering(const RenderingInfo& renderingInfo) override;
    RHIResult endRendering() override;
    RHIResult clearColor(const ClearColorValue& color) override;
    RHIResult imageBarrier(const ImageBarrierInfo& barrierInfo) override;
    RHIResult bufferBarrier(const BufferBarrierInfo& barrierInfo) override;
    RHIResult transitionImage(ImageHandle image, luna::ImageLayout newLayout) override;
    RHIResult copyBuffer(const BufferCopyInfo& copyInfo) override;
    RHIResult copyImage(const ImageCopyInfo& copyInfo) override;
    RHIResult copyBufferToImage(const BufferImageCopyInfo& copyInfo) override;
    RHIResult copyImageToBuffer(const BufferImageCopyInfo& copyInfo) override;
    RHIResult bindGraphicsPipeline(PipelineHandle pipeline) override;
    RHIResult bindComputePipeline(PipelineHandle pipeline) override;
    RHIResult bindVertexBuffer(BufferHandle buffer, uint64_t offset) override;
    RHIResult bindIndexBuffer(BufferHandle buffer, IndexFormat indexFormat, uint64_t offset) override;
    RHIResult setViewport(const Viewport& viewport) override;
    RHIResult setScissor(const ScissorRect& scissor) override;
    RHIResult bindResourceSet(ResourceSetHandle resourceSet, std::span<const uint32_t> dynamicOffsets) override;
    RHIResult pushConstants(const void* data, uint32_t size, uint32_t offset, ShaderType visibility) override;
    RHIResult draw(const DrawArguments& arguments) override;
    RHIResult drawIndexed(const IndexedDrawArguments& arguments) override;
    RHIResult drawIndirect(BufferHandle argumentsBuffer, uint64_t offset) override;
    RHIResult dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;
    RHIResult dispatchIndirect(BufferHandle argumentsBuffer, uint64_t offset) override;
    RHIResult beginDebugLabel(std::string_view label, const ClearColorValue& color) override;
    RHIResult endDebugLabel() override;

private:
    struct ResolvedAttachmentTarget {
        ImageResource* image = nullptr;
        const ImageViewResource* view = nullptr;
        ImageHandle imageHandle{};
        vk::ImageView imageView{};
        vk::Extent2D extent{};
        uint32_t baseMipLevel = 0;
        uint32_t mipCount = 1;
        uint32_t baseArrayLayer = 0;
        uint32_t layerCount = 1;
    };

    bool resolveAttachmentTarget(ImageHandle imageHandle,
                                 ImageViewHandle viewHandle,
                                 ImageAspect expectedAspect,
                                 ResolvedAttachmentTarget* outTarget);
    vk::ImageLayout currentImageLayout(const ResolvedAttachmentTarget& target) const;
    void setCurrentImageLayout(const ResolvedAttachmentTarget& target, vk::ImageLayout layout);

    VulkanRHIDevice& m_device;
    vk::CommandBuffer m_commandBuffer{};
    bool m_rendering = false;
    vk::Extent2D m_renderExtent{};
    ImageHandle m_currentAttachment{};
    vk::PipelineLayout m_boundPipelineLayout{};
    vk::PipelineBindPoint m_boundPipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    std::vector<std::string> m_debugLabelStack;
};

bool VulkanRHIDevice::CommandContext::resolveAttachmentTarget(ImageHandle imageHandle,
                                                              ImageViewHandle viewHandle,
                                                              ImageAspect expectedAspect,
                                                              ResolvedAttachmentTarget* outTarget)
{
    if (outTarget == nullptr) {
        return false;
    }

    *outTarget = {};
    if (viewHandle.isValid()) {
        const ImageViewResource* view = m_device.findImageView(viewHandle);
        if (view == nullptr || !view->view || view->desc.aspect != expectedAspect) {
            return false;
        }

        ImageResource* image = m_device.findImage(view->imageHandle);
        if (image == nullptr || !image->image.image) {
            return false;
        }

        outTarget->image = image;
        outTarget->view = view;
        outTarget->imageHandle = view->imageHandle;
        outTarget->imageView = view->view;
        outTarget->extent = mip_extent(image->desc, view->desc.baseMipLevel);
        outTarget->baseMipLevel = view->desc.baseMipLevel;
        outTarget->mipCount = view->desc.mipCount;
        outTarget->baseArrayLayer = view->desc.baseArrayLayer;
        outTarget->layerCount = view->desc.layerCount;
        return true;
    }

    if (!imageHandle.isValid()) {
        return false;
    }

    ImageResource* image = m_device.findImage(imageHandle);
    if (image == nullptr || !image->image.image || !image->image.imageView) {
        return false;
    }

    outTarget->image = image;
    outTarget->imageHandle = imageHandle;
    outTarget->imageView = image->image.imageView;
    outTarget->extent = {image->desc.width, image->desc.height};
    outTarget->baseMipLevel = 0;
    outTarget->mipCount = image->desc.mipLevels;
    outTarget->baseArrayLayer = 0;
    outTarget->layerCount = m_device.imageLayerCount(image->desc);
    return true;
}

vk::ImageLayout VulkanRHIDevice::CommandContext::currentImageLayout(const ResolvedAttachmentTarget& target) const
{
    if (target.image == nullptr) {
        return vk::ImageLayout::eUndefined;
    }

    if (target.imageHandle == m_device.m_currentBackbufferHandle &&
        m_device.m_swapchainImageIndex < m_device.m_context._swapchainImageLayouts.size()) {
        return static_cast<vk::ImageLayout>(m_device.m_context._swapchainImageLayouts[m_device.m_swapchainImageIndex]);
    }

    vk::ImageLayout layout = vk::ImageLayout::eUndefined;
    if (!m_device.getImageRangeLayout(*target.image,
                                      target.baseMipLevel,
                                      target.mipCount,
                                      target.baseArrayLayer,
                                      target.layerCount,
                                      &layout)) {
        return vk::ImageLayout::eUndefined;
    }
    return layout;
}

void VulkanRHIDevice::CommandContext::setCurrentImageLayout(const ResolvedAttachmentTarget& target, vk::ImageLayout layout)
{
    if (target.image == nullptr) {
        return;
    }

    m_device.setImageRangeLayout(*target.image,
                                 target.baseMipLevel,
                                 target.mipCount,
                                 target.baseArrayLayer,
                                 target.layerCount,
                                 layout);
    if (target.imageHandle == m_device.m_currentBackbufferHandle &&
        m_device.m_swapchainImageIndex < m_device.m_context._swapchainImageLayouts.size()) {
        m_device.m_context._swapchainImageLayouts[m_device.m_swapchainImageIndex] = static_cast<VkImageLayout>(layout);
    }
}

class VulkanRHIDevice::Surface final : public IRHISurface {
public:
    explicit Surface(VulkanRHIDevice& device)
        : m_device(device)
    {}

    RHIBackend getBackend() const override
    {
        return RHIBackend::Vulkan;
    }

    uint64_t getSurfaceId() const override
    {
        return m_device.m_nativeWindow != nullptr ? m_surfaceId : 0;
    }

    void assign(uint64_t surfaceId)
    {
        m_surfaceId = surfaceId;
    }

    void reset()
    {
        m_surfaceId = 0;
    }

private:
    VulkanRHIDevice& m_device;
    uint64_t m_surfaceId = 0;
};

class VulkanRHIDevice::Swapchain final : public IRHISwapchain {
public:
    explicit Swapchain(VulkanRHIDevice& device)
        : m_device(device)
    {}

    RHIBackend getBackend() const override
    {
        return RHIBackend::Vulkan;
    }

    SwapchainHandle getHandle() const override
    {
        return m_handle;
    }

    RHISwapchainState getState() const override
    {
        return m_device.getSwapchainState();
    }

    IRHISurface* getSurface() const override
    {
        return m_device.getPrimarySurface();
    }

    RHIResult requestRecreate() override
    {
        return m_device.requestPrimarySwapchainRecreate();
    }

    void assign(SwapchainHandle handle)
    {
        m_handle = handle;
    }

    void reset()
    {
        m_handle = {};
    }

private:
    VulkanRHIDevice& m_device;
    SwapchainHandle m_handle{};
};

class VulkanRHIDevice::Fence final : public IRHIFence {
public:
    Fence(VulkanRHIDevice& device, bool signaled)
        : m_device(device),
          m_signaled(signaled)
    {
        vk::FenceCreateInfo fenceInfo = vkinit::fence_create_info(signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0);
        const vk::Result result = m_device.m_context._device.createFence(&fenceInfo, nullptr, &m_fence);
        if (result != vk::Result::eSuccess) {
            LUNA_CORE_ERROR("Failed to create standalone fence: {}", vk::to_string(result));
            m_fence = nullptr;
            m_signaled = false;
        }
    }

    ~Fence() override
    {
        if (m_fence && m_device.m_context._device) {
            m_device.m_context._device.destroyFence(m_fence, nullptr);
            m_fence = nullptr;
        }
    }

    bool isValid() const
    {
        return m_fence != VK_NULL_HANDLE;
    }

    vk::Fence getFence() const
    {
        return m_fence;
    }

    void arm(uint64_t submitSerial, RHIQueueType queueType, std::string label)
    {
        m_submitSerial = submitSerial;
        m_queueType = queueType;
        m_label = std::move(label);
        m_signaled = false;
    }

    RHIResult wait(uint64_t timeoutNanoseconds) override
    {
        if (!m_fence || !m_device.m_context._device) {
            return RHIResult::InvalidArgument;
        }

        const vk::Result result = m_device.m_context._device.waitForFences(1, &m_fence, VK_TRUE, timeoutNanoseconds);
        if (result == vk::Result::eTimeout) {
            return RHIResult::NotReady;
        }
        if (result != vk::Result::eSuccess) {
            LUNA_CORE_ERROR("Fence wait failed: {}", vk::to_string(result));
            return RHIResult::InternalError;
        }

        if (!m_signaled && m_submitSerial != 0) {
            m_device.appendTimelineEvent("Fence signal #" + std::to_string(m_submitSerial) + " queue=" +
                                         std::string(to_string(m_queueType)));
            m_signaled = true;
        }

        if (m_submitSerial != 0) {
            m_device.appendTimelineEvent("Fence wait #" + std::to_string(m_submitSerial) + " queue=" +
                                         std::string(to_string(m_queueType)));
            m_device.retireCompletedSerial(m_submitSerial);
        }
        return RHIResult::Success;
    }

    RHIResult reset() override
    {
        if (!m_fence || !m_device.m_context._device) {
            return RHIResult::InvalidArgument;
        }

        const vk::Result result = m_device.m_context._device.resetFences(1, &m_fence);
        if (result != vk::Result::eSuccess) {
            LUNA_CORE_ERROR("Fence reset failed: {}", vk::to_string(result));
            return RHIResult::InternalError;
        }

        m_signaled = false;
        return RHIResult::Success;
    }

    bool isSignaled() const override
    {
        if (!m_fence || !m_device.m_context._device) {
            return false;
        }

        if (m_signaled) {
            return true;
        }

        return m_device.m_context._device.getFenceStatus(m_fence) == vk::Result::eSuccess;
    }

private:
    VulkanRHIDevice& m_device;
    vk::Fence m_fence{};
    bool m_signaled = false;
    uint64_t m_submitSerial = 0;
    RHIQueueType m_queueType = RHIQueueType::Graphics;
    std::string m_label;
};

class VulkanRHIDevice::CommandList final : public IRHICommandList {
public:
    CommandList(VulkanRHIDevice& device, RHIQueueType queueType)
        : m_device(device),
          m_queueType(queueType),
          m_context(device)
    {}

    ~CommandList() override
    {
        m_context.reset();
        if (m_commandPool && m_device.m_context._device) {
            m_device.m_context._device.destroyCommandPool(m_commandPool, nullptr);
            m_commandPool = nullptr;
            m_commandBuffer = nullptr;
        }
    }

    RHIQueueType getQueueType() const override
    {
        return m_queueType;
    }

    RHIResult begin() override
    {
        if (!m_device.m_initialized || !m_device.m_context._device || m_recording || m_submitted) {
            return RHIResult::InvalidArgument;
        }

        if (!m_commandPool) {
            vk::CommandPoolCreateInfo poolInfo =
                vkinit::command_pool_create_info(m_device.m_context._graphicsQueueFamily,
                                                 VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
            const vk::Result poolResult = m_device.m_context._device.createCommandPool(&poolInfo, nullptr, &m_commandPool);
            if (poolResult != vk::Result::eSuccess) {
                LUNA_CORE_ERROR("Failed to create standalone command pool: {}", vk::to_string(poolResult));
                return RHIResult::InternalError;
            }

            vk::CommandBufferAllocateInfo allocInfo = vkinit::command_buffer_allocate_info(m_commandPool, 1);
            const vk::Result bufferResult =
                m_device.m_context._device.allocateCommandBuffers(&allocInfo, &m_commandBuffer);
            if (bufferResult != vk::Result::eSuccess) {
                LUNA_CORE_ERROR("Failed to allocate standalone command buffer: {}", vk::to_string(bufferResult));
                return RHIResult::InternalError;
            }
        }

        const vk::Result resetResult = m_commandBuffer.reset({});
        if (resetResult != vk::Result::eSuccess) {
            LUNA_CORE_ERROR("Failed to reset standalone command buffer: {}", vk::to_string(resetResult));
            return RHIResult::InternalError;
        }

        const vk::CommandBufferBeginInfo beginInfo =
            vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        const vk::Result beginResult = m_commandBuffer.begin(&beginInfo);
        if (beginResult != vk::Result::eSuccess) {
            LUNA_CORE_ERROR("Failed to begin standalone command buffer: {}", vk::to_string(beginResult));
            return RHIResult::InternalError;
        }

        m_context.beginFrame(m_commandBuffer);
        m_recording = true;
        m_readyForSubmit = false;
        return RHIResult::Success;
    }

    RHIResult end() override
    {
        if (!m_recording) {
            return RHIResult::InvalidArgument;
        }

        if (m_context.isRendering()) {
            const RHIResult endRenderingResult = m_context.endRendering();
            if (endRenderingResult != RHIResult::Success) {
                return endRenderingResult;
            }
        }

        const vk::Result endResult = m_commandBuffer.end();
        if (endResult != vk::Result::eSuccess) {
            LUNA_CORE_ERROR("Failed to end standalone command buffer: {}", vk::to_string(endResult));
            return RHIResult::InternalError;
        }

        m_context.reset();
        m_recording = false;
        m_readyForSubmit = true;
        return RHIResult::Success;
    }

    IRHICommandContext* getContext() override
    {
        return &m_context;
    }

    const IRHICommandContext* getContext() const override
    {
        return &m_context;
    }

    vk::CommandBuffer getCommandBuffer() const
    {
        return m_commandBuffer;
    }

    bool readyForSubmit() const
    {
        return m_readyForSubmit && !m_recording;
    }

    void markSubmitted()
    {
        m_readyForSubmit = false;
        m_submitted = true;
    }

private:
    VulkanRHIDevice& m_device;
    RHIQueueType m_queueType = RHIQueueType::Graphics;
    CommandContext m_context;
    vk::CommandPool m_commandPool{};
    vk::CommandBuffer m_commandBuffer{};
    bool m_recording = false;
    bool m_readyForSubmit = false;
    bool m_submitted = false;
};

class VulkanRHIDevice::CommandQueue final : public IRHICommandQueue {
public:
    CommandQueue(VulkanRHIDevice& device, RHIQueueType queueType)
        : m_device(device),
          m_queueType(queueType)
    {}

    RHIQueueType getQueueType() const override
    {
        return m_queueType;
    }

    RHIResult submit(IRHICommandList& commandList, IRHIFence* signalFence, std::string_view label) override
    {
        auto* const vkCommandList = dynamic_cast<CommandList*>(&commandList);
        if (vkCommandList == nullptr || vkCommandList->getQueueType() != m_queueType || !vkCommandList->readyForSubmit()) {
            return RHIResult::InvalidArgument;
        }

        Fence* vkFence = nullptr;
        if (signalFence != nullptr) {
            vkFence = dynamic_cast<Fence*>(signalFence);
            if (vkFence == nullptr || !vkFence->isValid() || vkFence->reset() != RHIResult::Success) {
                return RHIResult::InvalidArgument;
            }
        }

        const vk::CommandBufferSubmitInfo commandInfo = vkinit::command_buffer_submit_info(vkCommandList->getCommandBuffer());
        const vk::SubmitInfo2 submitInfo = vkinit::submit_info(&commandInfo, nullptr, nullptr);
        const vk::Fence fenceHandle = vkFence != nullptr ? vkFence->getFence() : vk::Fence{};
        const uint64_t submitSerial = ++m_device.m_lastSubmittedSerial;
        const vk::Result submitResult = m_device.m_context._graphicsQueue.submit2(1, &submitInfo, fenceHandle);
        if (submitResult != vk::Result::eSuccess) {
            LUNA_CORE_ERROR("Standalone {} submit failed: {}",
                            to_string(m_queueType),
                            vk::to_string(submitResult));
            return RHIResult::InternalError;
        }

        std::string eventLabel = std::string(to_string(m_queueType)) + " submit #" + std::to_string(submitSerial);
        if (!label.empty()) {
            eventLabel += " label=";
            eventLabel += label;
        }
        m_device.appendTimelineEvent(std::move(eventLabel));

        if (vkFence != nullptr) {
            vkFence->arm(submitSerial, m_queueType, std::string(label));
        }
        vkCommandList->markSubmitted();
        return RHIResult::Success;
    }

    RHIResult waitIdle() override
    {
        if (!m_device.m_context._graphicsQueue) {
            return RHIResult::InvalidArgument;
        }

        const vk::Result result = m_device.m_context._graphicsQueue.waitIdle();
        if (result != vk::Result::eSuccess) {
            LUNA_CORE_ERROR("{} queue waitIdle failed: {}", to_string(m_queueType), vk::to_string(result));
            return RHIResult::InternalError;
        }

        m_device.appendTimelineEvent(std::string(to_string(m_queueType)) + " queue idle");
        if (m_device.m_lastSubmittedSerial > m_device.m_lastCompletedSerial) {
            m_device.retireCompletedSerial(m_device.m_lastSubmittedSerial);
        }
        return RHIResult::Success;
    }

private:
    VulkanRHIDevice& m_device;
    RHIQueueType m_queueType = RHIQueueType::Graphics;
};


