#include "vk_rhi_device.h"

#include "Core/log.h"
#include "Imgui/ImGuiLayer.hpp"
#include "RHI/CommandContext.h"
#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"
#include "vk_shader.h"
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <vector>

namespace luna {
namespace {

class NativeWindowProxy final : public Window {
public:
    explicit NativeWindowProxy(GLFWwindow* nativeWindow)
        : m_nativeWindow(nativeWindow)
    {}

    void onUpdate() override {}

    uint32_t getWidth() const override
    {
        int width = 0;
        int height = 0;
        glfwGetWindowSize(m_nativeWindow, &width, &height);
        return width > 0 ? static_cast<uint32_t>(width) : 0u;
    }

    uint32_t getHeight() const override
    {
        int width = 0;
        int height = 0;
        glfwGetWindowSize(m_nativeWindow, &width, &height);
        return height > 0 ? static_cast<uint32_t>(height) : 0u;
    }

    void setEventCallback(const EventCallbackFn&) override {}

    void getWindowPos(int* x, int* y) const override
    {
        glfwGetWindowPos(m_nativeWindow, x, y);
    }

    void setWindowPos(int x, int y) override
    {
        glfwSetWindowPos(m_nativeWindow, x, y);
    }

    void setMaximized() override
    {
        glfwMaximizeWindow(m_nativeWindow);
    }

    void setRestored() override
    {
        glfwRestoreWindow(m_nativeWindow);
    }

    void setMinimized() override
    {
        glfwIconifyWindow(m_nativeWindow);
    }

    void setVSync(bool enabled) override
    {
        m_vsync = enabled;
    }

    bool isVSync() const override
    {
        return m_vsync;
    }

    void* getNativeWindow() const override
    {
        return m_nativeWindow;
    }

private:
    GLFWwindow* m_nativeWindow = nullptr;
    bool m_vsync = false;
};

bool is_depth_format(PixelFormat format)
{
    return format == PixelFormat::D32Float;
}

PixelFormat from_vulkan_format(vk::Format format)
{
    switch (format) {
        case vk::Format::eB8G8R8A8Unorm:
            return PixelFormat::BGRA8Unorm;
        case vk::Format::eR8G8B8A8Unorm:
            return PixelFormat::RGBA8Unorm;
        case vk::Format::eR8G8B8A8Srgb:
            return PixelFormat::RGBA8Srgb;
        case vk::Format::eR16G16B16A16Sfloat:
            return PixelFormat::RGBA16Float;
        case vk::Format::eD32Sfloat:
            return PixelFormat::D32Float;
        default:
            return PixelFormat::Undefined;
    }
}

vk::ImageLayout to_vulkan_image_layout(luna::ImageLayout layout)
{
    switch (layout) {
        case luna::ImageLayout::General:
            return vk::ImageLayout::eGeneral;
        case luna::ImageLayout::ColorAttachment:
            return vk::ImageLayout::eColorAttachmentOptimal;
        case luna::ImageLayout::DepthStencilAttachment:
            return vk::ImageLayout::eDepthAttachmentOptimal;
        case luna::ImageLayout::TransferSrc:
            return vk::ImageLayout::eTransferSrcOptimal;
        case luna::ImageLayout::TransferDst:
            return vk::ImageLayout::eTransferDstOptimal;
        case luna::ImageLayout::ShaderReadOnly:
            return vk::ImageLayout::eShaderReadOnlyOptimal;
        case luna::ImageLayout::Present:
            return vk::ImageLayout::ePresentSrcKHR;
        case luna::ImageLayout::Undefined:
        default:
            return vk::ImageLayout::eUndefined;
    }
}

vk::IndexType to_vulkan_index_type(IndexFormat format)
{
    return format == IndexFormat::UInt16 ? vk::IndexType::eUint16 : vk::IndexType::eUint32;
}

std::optional<std::vector<uint32_t>> load_spirv_code(const std::filesystem::path& filePath)
{
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        return std::nullopt;
    }

    const size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0 || (fileSize % sizeof(uint32_t)) != 0) {
        return std::nullopt;
    }

    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(fileSize));
    return buffer;
}

const auto& descriptor_pool_ratios()
{
    static const std::array<DescriptorAllocatorGrowable::PoolSizeRatio, 7> kRatios = {
        DescriptorAllocatorGrowable::PoolSizeRatio{vk::DescriptorType::eUniformBuffer, 4.0f},
        DescriptorAllocatorGrowable::PoolSizeRatio{vk::DescriptorType::eUniformBufferDynamic, 2.0f},
        DescriptorAllocatorGrowable::PoolSizeRatio{vk::DescriptorType::eCombinedImageSampler, 4.0f},
        DescriptorAllocatorGrowable::PoolSizeRatio{vk::DescriptorType::eSampledImage, 2.0f},
        DescriptorAllocatorGrowable::PoolSizeRatio{vk::DescriptorType::eSampler, 2.0f},
        DescriptorAllocatorGrowable::PoolSizeRatio{vk::DescriptorType::eStorageBuffer, 2.0f},
        DescriptorAllocatorGrowable::PoolSizeRatio{vk::DescriptorType::eStorageImage, 2.0f},
    };
    return kRatios;
}

} // namespace

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
    }

    bool isRendering() const
    {
        return m_rendering;
    }

    RHIResult beginRendering(const RenderingInfo& renderingInfo) override;
    RHIResult endRendering() override;
    RHIResult clearColor(const ClearColorValue& color) override;
    RHIResult transitionImage(ImageHandle image, luna::ImageLayout newLayout) override;
    RHIResult copyImage(const ImageCopyInfo& copyInfo) override;
    RHIResult bindGraphicsPipeline(PipelineHandle pipeline) override;
    RHIResult bindComputePipeline(PipelineHandle pipeline) override;
    RHIResult bindVertexBuffer(BufferHandle buffer, uint64_t offset) override;
    RHIResult bindIndexBuffer(BufferHandle buffer, IndexFormat indexFormat, uint64_t offset) override;
    RHIResult bindResourceSet(ResourceSetHandle resourceSet) override;
    RHIResult pushConstants(const void* data, uint32_t size, uint32_t offset, ShaderType visibility) override;
    RHIResult draw(const DrawArguments& arguments) override;
    RHIResult drawIndexed(const IndexedDrawArguments& arguments) override;
    RHIResult dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;

private:
    VulkanRHIDevice& m_device;
    vk::CommandBuffer m_commandBuffer{};
    bool m_rendering = false;
    vk::Extent2D m_renderExtent{};
    ImageHandle m_currentAttachment{};
    vk::PipelineLayout m_boundPipelineLayout{};
    vk::PipelineBindPoint m_boundPipelineBindPoint = vk::PipelineBindPoint::eGraphics;
};

VulkanRHIDevice::VulkanRHIDevice()
    : m_commandContext(std::make_unique<CommandContext>(*this))
{}

VulkanRHIDevice::~VulkanRHIDevice()
{
    shutdown();
}

RHIBackend VulkanRHIDevice::getBackend() const
{
    return RHIBackend::Vulkan;
}

RHICapabilities VulkanRHIDevice::getCapabilities() const
{
    return m_capabilities;
}

RHIResult VulkanRHIDevice::init(const DeviceCreateInfo& createInfo)
{
    if (m_initialized || createInfo.backend != RHIBackend::Vulkan || createInfo.nativeWindow == nullptr ||
        createInfo.applicationName.empty()) {
        LUNA_CORE_ERROR("VulkanRHIDevice::init rejected invalid arguments");
        return RHIResult::InvalidArgument;
    }

    m_applicationName = std::string(createInfo.applicationName);
    m_createInfo = createInfo;
    m_createInfo.applicationName = m_applicationName;
    m_nativeWindow = static_cast<GLFWwindow*>(createInfo.nativeWindow);
    m_capabilities = QueryRHICapabilities(RHIBackend::Vulkan);
    m_engine.setLegacyRendererMode(VulkanEngine::LegacyRendererMode::ClearColor);

    NativeWindowProxy windowProxy(m_nativeWindow);
    if (!m_engine.init(windowProxy)) {
        LUNA_CORE_ERROR("VulkanRHIDevice::init failed during VulkanEngine bootstrap");
        m_nativeWindow = nullptr;
        m_applicationName.clear();
        return RHIResult::InternalError;
    }

    m_initialized = true;
    m_frameInProgress = false;
    m_pendingPresent = false;
    m_recreateAfterPresent = false;
    m_resourceSetAllocatorInitialized = false;
    refreshBackbufferHandle();
    LUNA_CORE_INFO("VulkanRHIDevice created");
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::waitIdle()
{
    if (!m_initialized || !m_engine._device) {
        return RHIResult::InvalidArgument;
    }

    const vk::Result waitResult = m_engine._device.waitIdle();
    if (waitResult != vk::Result::eSuccess) {
        LUNA_CORE_ERROR("VulkanRHIDevice::waitIdle failed: {}", vk::to_string(waitResult));
        return RHIResult::InternalError;
    }

    return RHIResult::Success;
}

void VulkanRHIDevice::shutdown()
{
    if (!m_initialized) {
        return;
    }

    if (m_engine._device) {
        VK_CHECK(m_engine._device.waitIdle());
    }

    destroyAllResources();
    m_commandContext->reset();
    m_engine.cleanup();
    m_nativeWindow = nullptr;
    m_applicationName.clear();
    m_initialized = false;
    m_frameInProgress = false;
    m_pendingPresent = false;
    m_recreateAfterPresent = false;
}

RHIResult VulkanRHIDevice::createBuffer(const BufferDesc& desc, BufferHandle* outHandle, const void* initialData)
{
    if (!m_initialized || outHandle == nullptr || desc.size == 0) {
        return RHIResult::InvalidArgument;
    }

    AllocatedBuffer buffer = m_engine.create_buffer(desc, initialData);
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

    m_engine.destroy_buffer(buffer->buffer);
    m_bindingRegistry.unregister_buffer(handle);
    m_buffers.erase(handle.value);
}

RHIResult VulkanRHIDevice::writeBuffer(BufferHandle handle, const void* data, uint64_t size, uint64_t offset)
{
    BufferResource* buffer = findBuffer(handle);
    if (buffer == nullptr || data == nullptr || size == 0) {
        return RHIResult::InvalidArgument;
    }

    return m_engine.uploadBufferData(buffer->buffer,
                                     data,
                                     static_cast<size_t>(size),
                                     static_cast<size_t>(offset))
               ? RHIResult::Success
               : RHIResult::InternalError;
}

RHIResult VulkanRHIDevice::createImage(const ImageDesc& desc, ImageHandle* outHandle, const void* initialData)
{
    if (!m_initialized || outHandle == nullptr || desc.width == 0 || desc.height == 0 ||
        desc.format == PixelFormat::Undefined) {
        return RHIResult::InvalidArgument;
    }

    AllocatedImage image = m_engine.create_image(desc, initialData);
    const ImageHandle handle = m_bindingRegistry.register_image_view(image.imageView);
    if (!handle.isValid()) {
        return RHIResult::InternalError;
    }

    const vk::ImageLayout initialLayout =
        initialData != nullptr ? vk::ImageLayout::eShaderReadOnlyOptimal : vk::ImageLayout::eUndefined;
    m_images.insert_or_assign(handle.value,
                              ImageResource{.desc = desc, .image = image, .layout = initialLayout, .owned = true});
    *outHandle = handle;
    return RHIResult::Success;
}

void VulkanRHIDevice::destroyImage(ImageHandle handle)
{
    ImageResource* image = findImage(handle);
    if (image == nullptr) {
        return;
    }

    if (image->owned) {
        m_engine.destroy_image(image->image);
    }

    m_bindingRegistry.unregister_image_view(handle);
    m_images.erase(handle.value);

    if (handle == m_currentBackbufferHandle) {
        m_currentBackbufferHandle = {};
    }
}

RHIResult VulkanRHIDevice::createSampler(const SamplerDesc& desc, SamplerHandle* outHandle)
{
    if (!m_initialized || outHandle == nullptr) {
        return RHIResult::InvalidArgument;
    }

    const vk::Sampler sampler = m_engine.create_sampler(desc);
    const SamplerHandle handle = m_bindingRegistry.register_sampler(sampler);
    if (!handle.isValid()) {
        return RHIResult::InternalError;
    }

    m_samplers.insert_or_assign(handle.value, SamplerResource{.desc = desc, .sampler = sampler});
    *outHandle = handle;
    return RHIResult::Success;
}

void VulkanRHIDevice::destroySampler(SamplerHandle handle)
{
    SamplerResource* sampler = findSampler(handle);
    if (sampler == nullptr) {
        return;
    }

    if (m_engine._device && sampler->sampler) {
        m_engine._device.destroySampler(sampler->sampler, nullptr);
    }

    m_bindingRegistry.unregister_sampler(handle);
    m_samplers.erase(handle.value);
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

    const vk::DescriptorSetLayout layout = build_resource_layout(m_engine._device, desc);
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

    if (m_engine._device && layout->layout) {
        m_engine._device.destroyDescriptorSetLayout(layout->layout, nullptr);
    }

    m_resourceLayouts.erase(handle.value);
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
        m_resourceSetAllocator.init(m_engine._device, 32, poolRatios);
        m_resourceSetAllocatorInitialized = true;
    }

    const vk::DescriptorSet descriptorSet = m_resourceSetAllocator.allocate(m_engine._device, layout->layout);
    const uint64_t id = nextHandleValue(&m_nextResourceSetId);
    m_resourceSets.insert_or_assign(id, ResourceSetResource{.layoutHandle = layoutHandle, .set = descriptorSet});
    *outHandle = ResourceSetHandle::fromRaw(id);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::updateResourceSet(ResourceSetHandle resourceSet, const ResourceSetWriteDesc& writeDesc)
{
    const ResourceSetResource* resourceSetResource = findResourceSet(resourceSet);
    if (resourceSetResource == nullptr || !resourceSetResource->set) {
        return RHIResult::InvalidArgument;
    }

    return update_resource_set(m_engine._device, m_bindingRegistry, resourceSetResource->set, writeDesc)
               ? RHIResult::Success
               : RHIResult::InternalError;
}

void VulkanRHIDevice::destroyResourceSet(ResourceSetHandle handle)
{
    if (!handle.isValid()) {
        return;
    }

    m_resourceSets.erase(handle.value);
}

RHIResult VulkanRHIDevice::createGraphicsPipeline(const GraphicsPipelineDesc& desc, PipelineHandle* outHandle)
{
    if (!m_initialized || outHandle == nullptr) {
        return RHIResult::InvalidArgument;
    }

    std::vector<vk::DescriptorSetLayout> setLayouts;
    setLayouts.reserve(desc.resourceLayouts.size());
    for (const ResourceLayoutHandle layoutHandle : desc.resourceLayouts) {
        const ResourceLayoutResource* layout = findResourceLayout(layoutHandle);
        if (layout == nullptr || !layout->layout) {
            return RHIResult::InvalidArgument;
        }

        setLayouts.push_back(layout->layout);
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
    VK_CHECK(m_engine._device.createPipelineLayout(&pipelineLayoutInfo, nullptr, &pipelineLayout));

    const vk::Pipeline pipeline = build_graphics_pipeline(m_engine._device, desc, pipelineLayout);
    if (!pipeline) {
        m_engine._device.destroyPipelineLayout(pipelineLayout, nullptr);
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

    *outHandle = PipelineHandle::fromRaw(id);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::createComputePipeline(const ComputePipelineDesc& desc, PipelineHandle* outHandle)
{
    if (!m_initialized || outHandle == nullptr || desc.computeShader.stage != ShaderType::Compute ||
        desc.computeShader.filePath.empty()) {
        return RHIResult::InvalidArgument;
    }

    std::vector<vk::DescriptorSetLayout> setLayouts;
    setLayouts.reserve(desc.resourceLayouts.size());
    for (const ResourceLayoutHandle layoutHandle : desc.resourceLayouts) {
        const ResourceLayoutResource* layout = findResourceLayout(layoutHandle);
        if (layout == nullptr || !layout->layout) {
            return RHIResult::InvalidArgument;
        }

        setLayouts.push_back(layout->layout);
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
    VK_CHECK(m_engine._device.createPipelineLayout(&pipelineLayoutInfo, nullptr, &pipelineLayout));

    vk::ShaderModule computeShader{};
    const std::string computeShaderPath{desc.computeShader.filePath};
    if (!vkutil::load_shader_module(computeShaderPath.c_str(), m_engine._device, &computeShader)) {
        m_engine._device.destroyPipelineLayout(pipelineLayout, nullptr);
        LUNA_CORE_ERROR("Failed to load compute shader '{}'", computeShaderPath);
        return RHIResult::InternalError;
    }

    vk::ComputePipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.layout = pipelineLayout;
    pipelineCreateInfo.stage =
        vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eCompute, computeShader);

    vk::Pipeline pipeline{};
    const vk::Result pipelineResult =
        m_engine._device.createComputePipelines({}, 1, &pipelineCreateInfo, nullptr, &pipeline);
    m_engine._device.destroyShaderModule(computeShader, nullptr);
    if (pipelineResult != vk::Result::eSuccess || !pipeline) {
        m_engine._device.destroyPipelineLayout(pipelineLayout, nullptr);
        LUNA_CORE_ERROR("Failed to create compute pipeline '{}': {}",
                        desc.debugName.empty() ? computeShaderPath : std::string(desc.debugName),
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

    *outHandle = PipelineHandle::fromRaw(id);
    return RHIResult::Success;
}

void VulkanRHIDevice::destroyPipeline(PipelineHandle handle)
{
    PipelineResource* pipeline = findPipeline(handle);
    if (pipeline == nullptr) {
        return;
    }

    if (m_engine._device && pipeline->pipeline) {
        m_engine._device.destroyPipeline(pipeline->pipeline, nullptr);
    }
    if (m_engine._device && pipeline->layout) {
        m_engine._device.destroyPipelineLayout(pipeline->layout, nullptr);
    }

    m_pipelines.erase(handle.value);
}

RHIResult VulkanRHIDevice::beginFrame(FrameContext* outFrameContext)
{
    if (!m_initialized || outFrameContext == nullptr || m_frameInProgress || m_pendingPresent) {
        return RHIResult::InvalidArgument;
    }

    FrameData& frame = m_engine.get_current_frame();
    VK_CHECK(m_engine._device.waitForFences(1, &frame._renderFence, VK_TRUE, 1'000'000'000));
    frame._deletionQueue.flush();
    frame._frameDescriptors.clear_pools(m_engine._device);

    for (uint32_t attempt = 0; attempt < 2; ++attempt) {
        if (!ensureFramebufferReady()) {
            return RHIResult::NotReady;
        }

        m_swapchainImageIndex = 0;
        m_recreateAfterPresent = false;
        const vk::Result acquireResult = m_engine._device.acquireNextImageKHR(
            m_engine._swapchain, 1'000'000'000, frame._swapchainSemaphore, nullptr, &m_swapchainImageIndex);
        if (acquireResult == vk::Result::eErrorOutOfDateKHR) {
            if (!m_engine.resize_swapchain()) {
                return RHIResult::InternalError;
            }
            continue;
        }
        if (acquireResult == vk::Result::eSuboptimalKHR) {
            m_recreateAfterPresent = true;
        } else if (acquireResult != vk::Result::eSuccess) {
            LUNA_CORE_ERROR("vkAcquireNextImageKHR failed: {}", vk::to_string(acquireResult));
            return RHIResult::InternalError;
        }

        VK_CHECK(m_engine._device.resetFences(1, &frame._renderFence));
        VK_CHECK(frame._mainCommandBuffer.reset({}));
        const vk::CommandBufferBeginInfo beginInfo =
            vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        VK_CHECK(frame._mainCommandBuffer.begin(&beginInfo));

        m_commandContext->beginFrame(frame._mainCommandBuffer);
        refreshBackbufferHandle();

        outFrameContext->frameIndex = static_cast<uint32_t>(m_engine._frameNumber);
        outFrameContext->renderWidth = m_engine._swapchainExtent.width;
        outFrameContext->renderHeight = m_engine._swapchainExtent.height;
        outFrameContext->backbuffer = m_currentBackbufferHandle;
        outFrameContext->backbufferFormat = from_vulkan_format(m_engine._swapchainImageFormat);
        outFrameContext->commandContext = m_commandContext.get();

        m_frameInProgress = true;
        if (!m_loggedBeginFramePass) {
            LUNA_CORE_INFO("BeginFrame PASS");
            m_loggedBeginFramePass = true;
        }
        if (!m_loggedAcquirePass) {
            LUNA_CORE_INFO("Acquire PASS");
            m_loggedAcquirePass = true;
        }
        return RHIResult::Success;
    }

    return RHIResult::InternalError;
}

RHIResult VulkanRHIDevice::endFrame()
{
    if (!m_frameInProgress) {
        return RHIResult::InvalidArgument;
    }

    FrameData& frame = m_engine.get_current_frame();

    if (m_commandContext->isRendering()) {
        const RHIResult endRenderingResult = m_commandContext->endRendering();
        if (endRenderingResult != RHIResult::Success) {
            return endRenderingResult;
        }
    }

    if (m_currentBackbufferHandle.isValid()) {
        ImageResource* backbuffer = findImage(m_currentBackbufferHandle);
        if (backbuffer != nullptr) {
            vk::ImageLayout currentLayout = backbuffer->layout;
            if (m_swapchainImageIndex < m_engine._swapchainImageLayouts.size()) {
                currentLayout = static_cast<vk::ImageLayout>(m_engine._swapchainImageLayouts[m_swapchainImageIndex]);
            }

            if (currentLayout != vk::ImageLayout::ePresentSrcKHR) {
                vkutil::transition_image(frame._mainCommandBuffer,
                                         backbuffer->image.image,
                                         currentLayout,
                                         vk::ImageLayout::ePresentSrcKHR);
                backbuffer->layout = vk::ImageLayout::ePresentSrcKHR;
                if (m_swapchainImageIndex < m_engine._swapchainImageLayouts.size()) {
                    m_engine._swapchainImageLayouts[m_swapchainImageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                }
            }
        }
    }

    VK_CHECK(frame._mainCommandBuffer.end());

    const vk::CommandBufferSubmitInfo commandInfo = vkinit::command_buffer_submit_info(frame._mainCommandBuffer);
    const vk::SemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(
        vk::PipelineStageFlagBits2::eColorAttachmentOutput, frame._swapchainSemaphore);
    const vk::SemaphoreSubmitInfo signalInfo =
        vkinit::semaphore_submit_info(vk::PipelineStageFlagBits2::eAllGraphics, frame._renderSemaphore);
    const vk::SubmitInfo2 submitInfo = vkinit::submit_info(&commandInfo, &signalInfo, &waitInfo);
    VK_CHECK(m_engine._graphicsQueue.submit2(1, &submitInfo, frame._renderFence));

    m_commandContext->reset();
    m_frameInProgress = false;
    m_pendingPresent = true;
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::present()
{
    if (!m_pendingPresent) {
        return RHIResult::InvalidArgument;
    }

    FrameData& frame = m_engine.get_current_frame();
    vk::PresentInfoKHR presentInfo{};
    presentInfo.pSwapchains = &m_engine._swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pWaitSemaphores = &frame._renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pImageIndices = &m_swapchainImageIndex;

    const vk::Result presentResult = m_engine._graphicsQueue.presentKHR(&presentInfo);
    if (presentResult == vk::Result::eErrorOutOfDateKHR || presentResult == vk::Result::eSuboptimalKHR) {
        m_engine.request_swapchain_resize();
    } else if (presentResult != vk::Result::eSuccess) {
        LUNA_CORE_ERROR("vkQueuePresentKHR failed: {}", vk::to_string(presentResult));
        return RHIResult::InternalError;
    }

    if (m_recreateAfterPresent) {
        m_engine.request_swapchain_resize();
    }

    if (!m_loggedPresentPass) {
        LUNA_CORE_INFO("Present PASS");
        m_loggedPresentPass = true;
    }

    m_engine._frameNumber++;
    m_pendingPresent = false;
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::renderOverlay(ImGuiLayer& imguiLayer)
{
    if (!m_frameInProgress || m_swapchainImageIndex >= m_engine._swapchainImageViews.size() ||
        m_swapchainImageIndex >= m_engine._swapchainImageLayouts.size()) {
        return RHIResult::InvalidArgument;
    }

    FrameData& frame = m_engine.get_current_frame();
    vk::ImageLayout currentLayout = static_cast<vk::ImageLayout>(m_engine._swapchainImageLayouts[m_swapchainImageIndex]);
    if (currentLayout != vk::ImageLayout::eColorAttachmentOptimal) {
        vkutil::transition_image(frame._mainCommandBuffer,
                                 m_engine._swapchainImages[m_swapchainImageIndex],
                                 currentLayout,
                                 vk::ImageLayout::eColorAttachmentOptimal);
        m_engine._swapchainImageLayouts[m_swapchainImageIndex] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    imguiLayer.render(frame._mainCommandBuffer,
                      m_engine._swapchainImageViews[m_swapchainImageIndex],
                      m_engine._swapchainExtent);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::beginRendering(const RenderingInfo& renderingInfo)
{
    if (!m_commandBuffer || renderingInfo.colorAttachments.empty()) {
        return RHIResult::InvalidArgument;
    }
    if (m_rendering) {
        return RHIResult::InvalidArgument;
    }

    const ColorAttachmentInfo& colorAttachment = renderingInfo.colorAttachments.front();
    ImageResource* image = m_device.findImage(colorAttachment.image);
    if (image == nullptr) {
        return RHIResult::InvalidArgument;
    }

    vk::ImageLayout currentLayout = image->layout;
    if (colorAttachment.image == m_device.m_currentBackbufferHandle &&
        m_device.m_swapchainImageIndex < m_device.m_engine._swapchainImageLayouts.size()) {
        currentLayout = static_cast<vk::ImageLayout>(m_device.m_engine._swapchainImageLayouts[m_device.m_swapchainImageIndex]);
    }

    vkutil::transition_image(m_commandBuffer, image->image.image, currentLayout, vk::ImageLayout::eColorAttachmentOptimal);

    image->layout = vk::ImageLayout::eColorAttachmentOptimal;
    if (colorAttachment.image == m_device.m_currentBackbufferHandle &&
        m_device.m_swapchainImageIndex < m_device.m_engine._swapchainImageLayouts.size()) {
        m_device.m_engine._swapchainImageLayouts[m_device.m_swapchainImageIndex] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    vk::ClearValue clearValue{};
    clearValue.color = vk::ClearColorValue(std::array<float, 4>{
        colorAttachment.clearColor.r,
        colorAttachment.clearColor.g,
        colorAttachment.clearColor.b,
        colorAttachment.clearColor.a,
    });

    vk::RenderingAttachmentInfo colorAttachmentInfo =
        vkinit::attachment_info(image->image.imageView, &clearValue, vk::ImageLayout::eColorAttachmentOptimal);
    colorAttachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;

    vk::RenderingAttachmentInfo depthAttachmentInfo{};
    const vk::RenderingAttachmentInfo* depthAttachmentPtr = nullptr;
    if (renderingInfo.depthAttachment.image.isValid()) {
        ImageResource* depthImage = m_device.findImage(renderingInfo.depthAttachment.image);
        if (depthImage == nullptr || !is_depth_format(depthImage->desc.format)) {
            return RHIResult::InvalidArgument;
        }

        vkutil::transition_image(
            m_commandBuffer, depthImage->image.image, depthImage->layout, vk::ImageLayout::eDepthAttachmentOptimal);
        depthImage->layout = vk::ImageLayout::eDepthAttachmentOptimal;

        depthAttachmentInfo =
            vkinit::depth_attachment_info(depthImage->image.imageView, vk::ImageLayout::eDepthAttachmentOptimal);
        depthAttachmentInfo.clearValue.depthStencil.depth = renderingInfo.depthAttachment.clearDepth;
        depthAttachmentPtr = &depthAttachmentInfo;
    }

    m_renderExtent = {renderingInfo.width, renderingInfo.height};
    if (m_renderExtent.width == 0 || m_renderExtent.height == 0) {
        m_renderExtent = {image->desc.width, image->desc.height};
    }

    const vk::RenderingInfo vkRenderingInfo =
        vkinit::rendering_info(m_renderExtent, &colorAttachmentInfo, depthAttachmentPtr);
    m_commandBuffer.beginRendering(&vkRenderingInfo);

    const vk::Viewport viewport{0.0f,
                                0.0f,
                                static_cast<float>(m_renderExtent.width),
                                static_cast<float>(m_renderExtent.height),
                                0.0f,
                                1.0f};
    const vk::Rect2D scissor{{0, 0}, m_renderExtent};
    m_commandBuffer.setViewport(0, 1, &viewport);
    m_commandBuffer.setScissor(0, 1, &scissor);

    m_rendering = true;
    m_currentAttachment = colorAttachment.image;
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::endRendering()
{
    if (!m_commandBuffer || !m_rendering) {
        return RHIResult::InvalidArgument;
    }

    m_commandBuffer.endRendering();
    m_rendering = false;
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::clearColor(const ClearColorValue& color)
{
    if (!m_commandBuffer) {
        return RHIResult::NotReady;
    }

    const ImageHandle targetHandle = m_currentAttachment.isValid() ? m_currentAttachment : m_device.m_currentBackbufferHandle;
    ImageResource* image = m_device.findImage(targetHandle);
    if (image == nullptr || is_depth_format(image->desc.format)) {
        return RHIResult::InvalidArgument;
    }

    if (m_rendering) {
        vk::ClearAttachment clearAttachment{};
        clearAttachment.aspectMask = vk::ImageAspectFlagBits::eColor;
        clearAttachment.colorAttachment = 0;
        clearAttachment.clearValue.color =
            vk::ClearColorValue(std::array<float, 4>{color.r, color.g, color.b, color.a});

        vk::ClearRect clearRect{};
        clearRect.rect = vk::Rect2D{{0, 0}, m_renderExtent};
        clearRect.baseArrayLayer = 0;
        clearRect.layerCount = 1;
        m_commandBuffer.clearAttachments(1, &clearAttachment, 1, &clearRect);
        return RHIResult::Success;
    }

    vk::ImageLayout currentLayout = image->layout;
    if (targetHandle == m_device.m_currentBackbufferHandle &&
        m_device.m_swapchainImageIndex < m_device.m_engine._swapchainImageLayouts.size()) {
        currentLayout = static_cast<vk::ImageLayout>(m_device.m_engine._swapchainImageLayouts[m_device.m_swapchainImageIndex]);
    }

    vkutil::transition_image(m_commandBuffer, image->image.image, currentLayout, vk::ImageLayout::eTransferDstOptimal);

    const vk::ClearColorValue clearValue(std::array<float, 4>{color.r, color.g, color.b, color.a});
    const vk::ImageSubresourceRange subresourceRange =
        vkinit::image_subresource_range(vk::ImageAspectFlagBits::eColor);
    m_commandBuffer.clearColorImage(
        image->image.image, vk::ImageLayout::eTransferDstOptimal, &clearValue, 1, &subresourceRange);

    image->layout = vk::ImageLayout::eTransferDstOptimal;
    if (targetHandle == m_device.m_currentBackbufferHandle &&
        m_device.m_swapchainImageIndex < m_device.m_engine._swapchainImageLayouts.size()) {
        m_device.m_engine._swapchainImageLayouts[m_device.m_swapchainImageIndex] = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    }

    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::transitionImage(ImageHandle imageHandle, luna::ImageLayout newLayout)
{
    if (!m_commandBuffer || m_rendering) {
        return RHIResult::InvalidArgument;
    }

    ImageResource* image = m_device.findImage(imageHandle);
    if (image == nullptr) {
        return RHIResult::InvalidArgument;
    }

    vk::ImageLayout currentLayout = image->layout;
    if (imageHandle == m_device.m_currentBackbufferHandle &&
        m_device.m_swapchainImageIndex < m_device.m_engine._swapchainImageLayouts.size()) {
        currentLayout = static_cast<vk::ImageLayout>(m_device.m_engine._swapchainImageLayouts[m_device.m_swapchainImageIndex]);
    }

    const vk::ImageLayout targetLayout = to_vulkan_image_layout(newLayout);
    vkutil::transition_image(m_commandBuffer, image->image.image, currentLayout, targetLayout);

    image->layout = targetLayout;
    if (imageHandle == m_device.m_currentBackbufferHandle &&
        m_device.m_swapchainImageIndex < m_device.m_engine._swapchainImageLayouts.size()) {
        m_device.m_engine._swapchainImageLayouts[m_device.m_swapchainImageIndex] = static_cast<VkImageLayout>(targetLayout);
    }

    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::copyImage(const ImageCopyInfo& copyInfo)
{
    if (!m_commandBuffer || m_rendering || !copyInfo.source.isValid() || !copyInfo.destination.isValid()) {
        return RHIResult::InvalidArgument;
    }

    ImageResource* sourceImage = m_device.findImage(copyInfo.source);
    ImageResource* destinationImage = m_device.findImage(copyInfo.destination);
    if (sourceImage == nullptr || destinationImage == nullptr) {
        return RHIResult::InvalidArgument;
    }

    vk::ImageLayout sourceLayout = sourceImage->layout;
    if (copyInfo.source == m_device.m_currentBackbufferHandle &&
        m_device.m_swapchainImageIndex < m_device.m_engine._swapchainImageLayouts.size()) {
        sourceLayout = static_cast<vk::ImageLayout>(m_device.m_engine._swapchainImageLayouts[m_device.m_swapchainImageIndex]);
    }

    vk::ImageLayout destinationLayout = destinationImage->layout;
    if (copyInfo.destination == m_device.m_currentBackbufferHandle &&
        m_device.m_swapchainImageIndex < m_device.m_engine._swapchainImageLayouts.size()) {
        destinationLayout =
            static_cast<vk::ImageLayout>(m_device.m_engine._swapchainImageLayouts[m_device.m_swapchainImageIndex]);
    }

    if (sourceLayout != vk::ImageLayout::eTransferSrcOptimal ||
        destinationLayout != vk::ImageLayout::eTransferDstOptimal) {
        return RHIResult::InvalidArgument;
    }

    const vk::Extent2D sourceExtent{
        copyInfo.sourceWidth > 0 ? copyInfo.sourceWidth : sourceImage->desc.width,
        copyInfo.sourceHeight > 0 ? copyInfo.sourceHeight : sourceImage->desc.height,
    };
    const vk::Extent2D destinationExtent{
        copyInfo.destinationWidth > 0 ? copyInfo.destinationWidth : destinationImage->desc.width,
        copyInfo.destinationHeight > 0 ? copyInfo.destinationHeight : destinationImage->desc.height,
    };

    if (sourceExtent.width == 0 || sourceExtent.height == 0 || destinationExtent.width == 0 ||
        destinationExtent.height == 0) {
        return RHIResult::InvalidArgument;
    }

    vkutil::copy_image_to_image(
        m_commandBuffer, sourceImage->image.image, destinationImage->image.image, sourceExtent, destinationExtent);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::bindGraphicsPipeline(PipelineHandle pipeline)
{
    if (!m_commandBuffer) {
        return RHIResult::NotReady;
    }

    const PipelineResource* pipelineResource = m_device.findPipeline(pipeline);
    if (pipelineResource == nullptr || !pipelineResource->pipeline ||
        pipelineResource->bindPoint != vk::PipelineBindPoint::eGraphics) {
        return RHIResult::InvalidArgument;
    }

    m_commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelineResource->pipeline);
    m_boundPipelineLayout = pipelineResource->layout;
    m_boundPipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::bindComputePipeline(PipelineHandle pipeline)
{
    if (!m_commandBuffer || m_rendering) {
        return RHIResult::InvalidArgument;
    }

    const PipelineResource* pipelineResource = m_device.findPipeline(pipeline);
    if (pipelineResource == nullptr || !pipelineResource->pipeline ||
        pipelineResource->bindPoint != vk::PipelineBindPoint::eCompute) {
        return RHIResult::InvalidArgument;
    }

    m_commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipelineResource->pipeline);
    m_boundPipelineLayout = pipelineResource->layout;
    m_boundPipelineBindPoint = vk::PipelineBindPoint::eCompute;
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::bindVertexBuffer(BufferHandle buffer, uint64_t offset)
{
    if (!m_commandBuffer) {
        return RHIResult::NotReady;
    }

    const BufferResource* bufferResource = m_device.findBuffer(buffer);
    if (bufferResource == nullptr || !bufferResource->buffer.buffer) {
        return RHIResult::InvalidArgument;
    }

    const vk::DeviceSize vkOffset = static_cast<vk::DeviceSize>(offset);
    m_commandBuffer.bindVertexBuffers(0, 1, &bufferResource->buffer.buffer, &vkOffset);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::bindIndexBuffer(BufferHandle buffer,
                                                           IndexFormat indexFormat,
                                                           uint64_t offset)
{
    if (!m_commandBuffer) {
        return RHIResult::NotReady;
    }

    const BufferResource* bufferResource = m_device.findBuffer(buffer);
    if (bufferResource == nullptr || !bufferResource->buffer.buffer) {
        return RHIResult::InvalidArgument;
    }

    m_commandBuffer.bindIndexBuffer(
        bufferResource->buffer.buffer, static_cast<vk::DeviceSize>(offset), to_vulkan_index_type(indexFormat));
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::bindResourceSet(ResourceSetHandle resourceSet)
{
    if (!m_commandBuffer || !m_boundPipelineLayout) {
        return RHIResult::NotReady;
    }

    const ResourceSetResource* resourceSetResource = m_device.findResourceSet(resourceSet);
    if (resourceSetResource == nullptr || !resourceSetResource->set) {
        return RHIResult::InvalidArgument;
    }

    m_commandBuffer.bindDescriptorSets(
        m_boundPipelineBindPoint, m_boundPipelineLayout, 0, 1, &resourceSetResource->set, 0, nullptr);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::pushConstants(const void* data,
                                                         uint32_t size,
                                                         uint32_t offset,
                                                         ShaderType visibility)
{
    if (!m_commandBuffer || !m_boundPipelineLayout || data == nullptr || size == 0) {
        return RHIResult::InvalidArgument;
    }

    m_commandBuffer.pushConstants(m_boundPipelineLayout, to_vulkan_shader_stages(visibility), offset, size, data);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::draw(const DrawArguments& arguments)
{
    if (!m_commandBuffer || m_boundPipelineBindPoint != vk::PipelineBindPoint::eGraphics) {
        return RHIResult::NotReady;
    }

    m_commandBuffer.draw(arguments.vertexCount, arguments.instanceCount, arguments.firstVertex, arguments.firstInstance);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::drawIndexed(const IndexedDrawArguments& arguments)
{
    if (!m_commandBuffer || m_boundPipelineBindPoint != vk::PipelineBindPoint::eGraphics) {
        return RHIResult::NotReady;
    }

    m_commandBuffer.drawIndexed(arguments.indexCount,
                                arguments.instanceCount,
                                arguments.firstIndex,
                                arguments.vertexOffset,
                                arguments.firstInstance);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    if (!m_commandBuffer || m_rendering || m_boundPipelineBindPoint != vk::PipelineBindPoint::eCompute ||
        groupCountX == 0 || groupCountY == 0 || groupCountZ == 0) {
        return RHIResult::InvalidArgument;
    }

    m_commandBuffer.dispatch(groupCountX, groupCountY, groupCountZ);
    return RHIResult::Success;
}

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
    for (auto& [handle, pipeline] : m_pipelines) {
        if (m_engine._device && pipeline.pipeline) {
            m_engine._device.destroyPipeline(pipeline.pipeline, nullptr);
        }
        if (m_engine._device && pipeline.layout) {
            m_engine._device.destroyPipelineLayout(pipeline.layout, nullptr);
        }
    }
    m_pipelines.clear();

    m_resourceSets.clear();

    for (auto& [handle, layout] : m_resourceLayouts) {
        if (m_engine._device && layout.layout) {
            m_engine._device.destroyDescriptorSetLayout(layout.layout, nullptr);
        }
    }
    m_resourceLayouts.clear();

    if (m_resourceSetAllocatorInitialized && m_engine._device) {
        m_resourceSetAllocator.destroy_pools(m_engine._device);
    }
    m_resourceSetAllocatorInitialized = false;

    for (auto& [handle, sampler] : m_samplers) {
        if (m_engine._device && sampler.sampler) {
            m_engine._device.destroySampler(sampler.sampler, nullptr);
        }
        m_bindingRegistry.unregister_sampler(SamplerHandle::fromRaw(handle));
    }
    m_samplers.clear();

    for (auto& [handle, image] : m_images) {
        if (image.owned) {
            m_engine.destroy_image(image.image);
        }
        m_bindingRegistry.unregister_image_view(ImageHandle::fromRaw(handle));
    }
    m_images.clear();
    m_currentBackbufferHandle = {};

    for (auto& [handle, buffer] : m_buffers) {
        m_engine.destroy_buffer(buffer.buffer);
        m_bindingRegistry.unregister_buffer(BufferHandle::fromRaw(handle));
    }
    m_buffers.clear();

    m_shaders.clear();
}

void VulkanRHIDevice::refreshBackbufferHandle()
{
    if (!m_initialized || m_swapchainImageIndex >= m_engine._swapchainImageViews.size() ||
        m_swapchainImageIndex >= m_engine._swapchainImages.size()) {
        m_currentBackbufferHandle = {};
        return;
    }

    if (m_currentBackbufferHandle.isValid()) {
        m_bindingRegistry.unregister_image_view(m_currentBackbufferHandle);
        m_images.erase(m_currentBackbufferHandle.value);
        m_currentBackbufferHandle = {};
    }

    const ImageHandle handle =
        m_bindingRegistry.register_image_view(m_engine._swapchainImageViews[m_swapchainImageIndex]);
    if (!handle.isValid()) {
        return;
    }

    ImageDesc desc{};
    desc.width = m_engine._swapchainExtent.width;
    desc.height = m_engine._swapchainExtent.height;
    desc.depth = 1;
    desc.mipLevels = 1;
    desc.arrayLayers = 1;
    desc.format = from_vulkan_format(m_engine._swapchainImageFormat);
    desc.usage = ImageUsage::ColorAttachment | ImageUsage::TransferDst | ImageUsage::TransferSrc;
    desc.debugName = "SwapchainBackbuffer";

    AllocatedImage image{};
    image.image = m_engine._swapchainImages[m_swapchainImageIndex];
    image.imageView = m_engine._swapchainImageViews[m_swapchainImageIndex];
    image.imageFormat = m_engine._swapchainImageFormat;
    image.imageExtent = {m_engine._swapchainExtent.width, m_engine._swapchainExtent.height, 1};

    const vk::ImageLayout layout = m_swapchainImageIndex < m_engine._swapchainImageLayouts.size()
                                       ? static_cast<vk::ImageLayout>(m_engine._swapchainImageLayouts[m_swapchainImageIndex])
                                       : vk::ImageLayout::eUndefined;
    m_images.insert_or_assign(handle.value, ImageResource{.desc = desc, .image = image, .layout = layout, .owned = false});
    m_currentBackbufferHandle = handle;
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

    const bool extentMismatch = m_engine._swapchainExtent.width != static_cast<uint32_t>(framebufferWidth) ||
                                m_engine._swapchainExtent.height != static_cast<uint32_t>(framebufferHeight);
    if (m_engine.is_swapchain_resize_requested() || extentMismatch) {
        return m_engine.resize_swapchain();
    }

    return m_engine._swapchain != VK_NULL_HANDLE;
}

uint64_t VulkanRHIDevice::nextHandleValue(uint64_t* counter)
{
    const uint64_t value = *counter;
    ++(*counter);
    return value;
}

} // namespace luna
