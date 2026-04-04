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
#include <cstring>
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

uint32_t max_mip_levels_for_desc(const ImageDesc& desc)
{
    uint32_t maxDimension = std::max(desc.width, desc.height);
    if (desc.type == ImageType::Image3D) {
        maxDimension = std::max(maxDimension, desc.depth);
    }

    uint32_t levels = 0;
    do {
        ++levels;
        maxDimension >>= 1;
    } while (maxDimension > 0);

    return levels;
}

bool validate_image_desc(const ImageDesc& desc)
{
    if (desc.width == 0 || desc.height == 0 || desc.depth == 0 || desc.mipLevels == 0 || desc.arrayLayers == 0 ||
        desc.format == PixelFormat::Undefined) {
        return false;
    }

    switch (desc.type) {
        case ImageType::Image2D:
            return desc.depth == 1 && desc.arrayLayers == 1;
        case ImageType::Image2DArray:
            return desc.depth == 1;
        case ImageType::Image3D:
            return desc.arrayLayers == 1;
        default:
            return false;
    }
}

vk::ImageAspectFlags to_vulkan_image_aspect_flags(ImageAspect aspect)
{
    switch (aspect) {
        case ImageAspect::Depth:
            return vk::ImageAspectFlagBits::eDepth;
        case ImageAspect::Color:
        default:
            return vk::ImageAspectFlagBits::eColor;
    }
}

vk::ImageViewType to_vulkan_image_view_type(ImageViewType type)
{
    switch (type) {
        case ImageViewType::Image2DArray:
            return vk::ImageViewType::e2DArray;
        case ImageViewType::Image3D:
            return vk::ImageViewType::e3D;
        case ImageViewType::Image2D:
        default:
            return vk::ImageViewType::e2D;
    }
}

ImageAspect default_image_aspect(const ImageDesc& desc)
{
    return luna::is_depth_format(desc.format) ? ImageAspect::Depth : ImageAspect::Color;
}

ImageViewType default_image_view_type(const ImageDesc& desc)
{
    switch (desc.type) {
        case ImageType::Image2DArray:
            return ImageViewType::Image2DArray;
        case ImageType::Image3D:
            return ImageViewType::Image3D;
        case ImageType::Image2D:
        default:
            return ImageViewType::Image2D;
    }
}

bool validate_image_view_desc(const ImageDesc& imageDesc, const ImageViewDesc& desc)
{
    if (!desc.image.isValid() || desc.mipCount == 0 || desc.layerCount == 0 || desc.baseMipLevel >= imageDesc.mipLevels ||
        desc.baseMipLevel + desc.mipCount > imageDesc.mipLevels) {
        return false;
    }

    const ImageAspect expectedAspect = default_image_aspect(imageDesc);
    if (desc.aspect != expectedAspect) {
        return false;
    }

    switch (imageDesc.type) {
        case ImageType::Image2D:
            return desc.type == ImageViewType::Image2D && desc.baseArrayLayer == 0 && desc.layerCount == 1;

        case ImageType::Image2DArray:
            if (desc.baseArrayLayer >= imageDesc.arrayLayers || desc.baseArrayLayer + desc.layerCount > imageDesc.arrayLayers) {
                return false;
            }
            return (desc.type == ImageViewType::Image2D && desc.layerCount == 1) ||
                   desc.type == ImageViewType::Image2DArray;

        case ImageType::Image3D:
            return desc.type == ImageViewType::Image3D && desc.baseArrayLayer == 0 && desc.layerCount == 1;

        default:
            return false;
    }
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
        case vk::Format::eR16G16Sfloat:
            return PixelFormat::RG16Float;
        case vk::Format::eR16G16B16A16Sfloat:
            return PixelFormat::RGBA16Float;
        case vk::Format::eR32Sfloat:
            return PixelFormat::R32Float;
        case vk::Format::eB10G11R11UfloatPack32:
            return PixelFormat::R11G11B10Float;
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

vk::PipelineStageFlags2 to_vulkan_pipeline_stages(PipelineStage stages)
{
    const uint32_t bits = static_cast<uint32_t>(stages);
    if (bits == static_cast<uint32_t>(PipelineStage::AllCommands)) {
        return vk::PipelineStageFlagBits2::eAllCommands;
    }

    vk::PipelineStageFlags2 flags{};
    if ((bits & static_cast<uint32_t>(PipelineStage::Top)) != 0) {
        flags |= vk::PipelineStageFlagBits2::eTopOfPipe;
    }
    if ((bits & static_cast<uint32_t>(PipelineStage::DrawIndirect)) != 0) {
        flags |= vk::PipelineStageFlagBits2::eDrawIndirect;
    }
    if ((bits & static_cast<uint32_t>(PipelineStage::VertexInput)) != 0) {
        flags |= vk::PipelineStageFlagBits2::eVertexInput;
    }
    if ((bits & static_cast<uint32_t>(PipelineStage::VertexShader)) != 0) {
        flags |= vk::PipelineStageFlagBits2::eVertexShader;
    }
    if ((bits & static_cast<uint32_t>(PipelineStage::FragmentShader)) != 0) {
        flags |= vk::PipelineStageFlagBits2::eFragmentShader;
    }
    if ((bits & static_cast<uint32_t>(PipelineStage::ComputeShader)) != 0) {
        flags |= vk::PipelineStageFlagBits2::eComputeShader;
    }
    if ((bits & static_cast<uint32_t>(PipelineStage::ColorAttachmentOutput)) != 0) {
        flags |= vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    }
    if ((bits & static_cast<uint32_t>(PipelineStage::Transfer)) != 0) {
        flags |= vk::PipelineStageFlagBits2::eTransfer;
    }
    if ((bits & static_cast<uint32_t>(PipelineStage::Host)) != 0) {
        flags |= vk::PipelineStageFlagBits2::eHost;
    }
    if ((bits & static_cast<uint32_t>(PipelineStage::Bottom)) != 0) {
        flags |= vk::PipelineStageFlagBits2::eBottomOfPipe;
    }

    return flags;
}

vk::AccessFlags2 to_vulkan_access_flags(ResourceAccess access)
{
    vk::AccessFlags2 flags{};
    const uint32_t bits = static_cast<uint32_t>(access);

    if ((bits & static_cast<uint32_t>(ResourceAccess::IndirectCommandRead)) != 0) {
        flags |= vk::AccessFlagBits2::eIndirectCommandRead;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::VertexBufferRead)) != 0) {
        flags |= vk::AccessFlagBits2::eVertexAttributeRead;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::IndexBufferRead)) != 0) {
        flags |= vk::AccessFlagBits2::eIndexRead;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::UniformRead)) != 0) {
        flags |= vk::AccessFlagBits2::eUniformRead;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::ShaderRead)) != 0) {
        flags |= vk::AccessFlagBits2::eShaderRead;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::ShaderWrite)) != 0) {
        flags |= vk::AccessFlagBits2::eShaderWrite;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::ColorAttachmentRead)) != 0) {
        flags |= vk::AccessFlagBits2::eColorAttachmentRead;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::ColorAttachmentWrite)) != 0) {
        flags |= vk::AccessFlagBits2::eColorAttachmentWrite;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::DepthStencilRead)) != 0) {
        flags |= vk::AccessFlagBits2::eDepthStencilAttachmentRead;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::DepthStencilWrite)) != 0) {
        flags |= vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::TransferRead)) != 0) {
        flags |= vk::AccessFlagBits2::eTransferRead;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::TransferWrite)) != 0) {
        flags |= vk::AccessFlagBits2::eTransferWrite;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::HostRead)) != 0) {
        flags |= vk::AccessFlagBits2::eHostRead;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::HostWrite)) != 0) {
        flags |= vk::AccessFlagBits2::eHostWrite;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::MemoryRead)) != 0) {
        flags |= vk::AccessFlagBits2::eMemoryRead;
    }
    if ((bits & static_cast<uint32_t>(ResourceAccess::MemoryWrite)) != 0) {
        flags |= vk::AccessFlagBits2::eMemoryWrite;
    }

    return flags;
}

bool validate_stage_access_pair(PipelineStage stages, ResourceAccess access)
{
    if (stages == PipelineStage::None) {
        return access == ResourceAccess::None;
    }
    return true;
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
    RHIResult bindResourceSet(ResourceSetHandle resourceSet, std::span<const uint32_t> dynamicOffsets) override;
    RHIResult pushConstants(const void* data, uint32_t size, uint32_t offset, ShaderType visibility) override;
    RHIResult draw(const DrawArguments& arguments) override;
    RHIResult drawIndexed(const IndexedDrawArguments& arguments) override;
    RHIResult dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;
    RHIResult dispatchIndirect(BufferHandle argumentsBuffer, uint64_t offset) override;

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
    if (!m_initialized || outHandle == nullptr || !validate_image_desc(desc) ||
        desc.mipLevels > max_mip_levels_for_desc(desc) || (initialData != nullptr && luna::is_depth_format(desc.format))) {
        return RHIResult::InvalidArgument;
    }

    AllocatedImage image = m_engine.create_image(desc, initialData);
    if (!image.image || !image.imageView) {
        return RHIResult::InternalError;
    }

    const ImageHandle handle = ImageHandle::fromRaw(nextHandleValue(&m_nextImageId));
    if (!handle.isValid() ||
        !m_bindingRegistry.register_image_view(ImageViewHandle::fromRaw(handle.value), image.imageView)) {
        m_engine.destroy_image(image);
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

    if (image->owned) {
        m_engine.destroy_image(image->image);
    }

    m_bindingRegistry.unregister_image_view(ImageViewHandle::fromRaw(handle.value));
    m_images.erase(handle.value);

    if (handle == m_currentBackbufferHandle) {
        m_currentBackbufferHandle = {};
    }
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
    const vk::Result result = m_engine._device.createImageView(&viewInfo, nullptr, &view);
    if (result != vk::Result::eSuccess || !view) {
        LUNA_CORE_ERROR("VulkanRHIDevice::createImageView failed: {}", vk::to_string(result));
        return RHIResult::InternalError;
    }

    const ImageViewHandle handle = ImageViewHandle::fromRaw(nextHandleValue(&m_nextImageViewId));
    if (!handle.isValid() || !m_bindingRegistry.register_image_view(handle, view)) {
        m_engine._device.destroyImageView(view, nullptr);
        return RHIResult::InternalError;
    }

    m_imageViews.insert_or_assign(handle.value, ImageViewResource{.desc = desc, .imageHandle = desc.image, .view = view});
    *outHandle = handle;
    return RHIResult::Success;
}

void VulkanRHIDevice::destroyImageView(ImageViewHandle handle)
{
    ImageViewResource* imageView = findImageView(handle);
    if (imageView == nullptr) {
        return;
    }

    if (m_engine._device && imageView->view) {
        m_engine._device.destroyImageView(imageView->view, nullptr);
    }

    m_bindingRegistry.unregister_image_view(handle);
    m_imageViews.erase(handle.value);
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

    const auto collectSetLayouts = [&](std::span<const ResourceLayoutHandle> handles,
                                       std::vector<vk::DescriptorSetLayout>* outSetLayouts) -> bool {
        if (outSetLayouts == nullptr) {
            return false;
        }

        std::vector<const ResourceLayoutResource*> layouts;
        layouts.reserve(handles.size());
        for (const ResourceLayoutHandle handle : handles) {
            const ResourceLayoutResource* layout = findResourceLayout(handle);
            if (layout == nullptr || !layout->layout) {
                return false;
            }
            layouts.push_back(layout);
        }

        std::sort(layouts.begin(), layouts.end(), [](const ResourceLayoutResource* lhs, const ResourceLayoutResource* rhs) {
            return lhs->desc.setIndex < rhs->desc.setIndex;
        });
        for (size_t index = 1; index < layouts.size(); ++index) {
            if (layouts[index - 1]->desc.setIndex == layouts[index]->desc.setIndex) {
                return false;
            }
        }
        for (size_t index = 0; index < layouts.size(); ++index) {
            if (layouts[index]->desc.setIndex != static_cast<uint32_t>(index)) {
                return false;
            }
        }

        outSetLayouts->clear();
        outSetLayouts->reserve(layouts.size());
        for (const ResourceLayoutResource* layout : layouts) {
            outSetLayouts->push_back(layout->layout);
        }
        return true;
    };

    std::vector<vk::DescriptorSetLayout> setLayouts;
    if (!collectSetLayouts(desc.resourceLayouts, &setLayouts)) {
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

    const auto collectSetLayouts = [&](std::span<const ResourceLayoutHandle> handles,
                                       std::vector<vk::DescriptorSetLayout>* outSetLayouts) -> bool {
        if (outSetLayouts == nullptr) {
            return false;
        }

        std::vector<const ResourceLayoutResource*> layouts;
        layouts.reserve(handles.size());
        for (const ResourceLayoutHandle handle : handles) {
            const ResourceLayoutResource* layout = findResourceLayout(handle);
            if (layout == nullptr || !layout->layout) {
                return false;
            }
            layouts.push_back(layout);
        }

        std::sort(layouts.begin(), layouts.end(), [](const ResourceLayoutResource* lhs, const ResourceLayoutResource* rhs) {
            return lhs->desc.setIndex < rhs->desc.setIndex;
        });
        for (size_t index = 1; index < layouts.size(); ++index) {
            if (layouts[index - 1]->desc.setIndex == layouts[index]->desc.setIndex) {
                return false;
            }
        }
        for (size_t index = 0; index < layouts.size(); ++index) {
            if (layouts[index]->desc.setIndex != static_cast<uint32_t>(index)) {
                return false;
            }
        }

        outSetLayouts->clear();
        outSetLayouts->reserve(layouts.size());
        for (const ResourceLayoutResource* layout : layouts) {
            outSetLayouts->push_back(layout->layout);
        }
        return true;
    };

    std::vector<vk::DescriptorSetLayout> setLayouts;
    if (!collectSetLayouts(desc.resourceLayouts, &setLayouts)) {
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

    std::vector<vk::RenderingAttachmentInfo> colorAttachmentInfos;
    colorAttachmentInfos.reserve(renderingInfo.colorAttachments.size());

    ImageHandle firstColorAttachment{};
    vk::Extent2D fallbackExtent{};
    for (size_t attachmentIndex = 0; attachmentIndex < renderingInfo.colorAttachments.size(); ++attachmentIndex) {
        const ColorAttachmentInfo& colorAttachment = renderingInfo.colorAttachments[attachmentIndex];
        ImageResource* image = m_device.findImage(colorAttachment.image);
        if (image == nullptr || luna::is_depth_format(image->desc.format)) {
            return RHIResult::InvalidArgument;
        }

        vk::ImageLayout currentLayout = image->layout;
        if (colorAttachment.image == m_device.m_currentBackbufferHandle &&
            m_device.m_swapchainImageIndex < m_device.m_engine._swapchainImageLayouts.size()) {
            currentLayout =
                static_cast<vk::ImageLayout>(m_device.m_engine._swapchainImageLayouts[m_device.m_swapchainImageIndex]);
        }

        vkutil::transition_image(
            m_commandBuffer, image->image.image, currentLayout, vk::ImageLayout::eColorAttachmentOptimal);

        image->layout = vk::ImageLayout::eColorAttachmentOptimal;
        if (colorAttachment.image == m_device.m_currentBackbufferHandle &&
            m_device.m_swapchainImageIndex < m_device.m_engine._swapchainImageLayouts.size()) {
            m_device.m_engine._swapchainImageLayouts[m_device.m_swapchainImageIndex] =
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
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
        colorAttachmentInfos.push_back(colorAttachmentInfo);

        if (attachmentIndex == 0) {
            firstColorAttachment = colorAttachment.image;
            fallbackExtent = {image->desc.width, image->desc.height};
        }
    }

    vk::RenderingAttachmentInfo depthAttachmentInfo{};
    const vk::RenderingAttachmentInfo* depthAttachmentPtr = nullptr;
    if (renderingInfo.depthAttachment.image.isValid()) {
        ImageResource* depthImage = m_device.findImage(renderingInfo.depthAttachment.image);
        if (depthImage == nullptr || !luna::is_depth_format(depthImage->desc.format)) {
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
        m_renderExtent = fallbackExtent;
    }

    const vk::RenderingInfo vkRenderingInfo =
        vkinit::rendering_info(m_renderExtent, std::span<const vk::RenderingAttachmentInfo>(colorAttachmentInfos), depthAttachmentPtr);
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
    m_currentAttachment = firstColorAttachment;
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
    if (image == nullptr || luna::is_depth_format(image->desc.format)) {
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

RHIResult VulkanRHIDevice::CommandContext::imageBarrier(const ImageBarrierInfo& barrierInfo)
{
    if (!m_commandBuffer || m_rendering || !barrierInfo.image.isValid() || barrierInfo.mipCount == 0 ||
        barrierInfo.layerCount == 0 || barrierInfo.newLayout == ImageLayout::Undefined ||
        !validate_stage_access_pair(barrierInfo.srcStage, barrierInfo.srcAccess) ||
        !validate_stage_access_pair(barrierInfo.dstStage, barrierInfo.dstAccess)) {
        return RHIResult::InvalidArgument;
    }

    ImageResource* image = m_device.findImage(barrierInfo.image);
    if (image == nullptr) {
        return RHIResult::InvalidArgument;
    }

    const uint32_t fullLayerCount = image->desc.type == ImageType::Image2DArray ? image->desc.arrayLayers : 1u;
    if (barrierInfo.baseMipLevel != 0 || barrierInfo.baseArrayLayer != 0 || barrierInfo.mipCount != image->desc.mipLevels ||
        barrierInfo.layerCount != fullLayerCount) {
        return RHIResult::InvalidArgument;
    }

    vk::ImageLayout currentLayout = image->layout;
    if (barrierInfo.image == m_device.m_currentBackbufferHandle &&
        m_device.m_swapchainImageIndex < m_device.m_engine._swapchainImageLayouts.size()) {
        currentLayout = static_cast<vk::ImageLayout>(m_device.m_engine._swapchainImageLayouts[m_device.m_swapchainImageIndex]);
    }

    if (barrierInfo.oldLayout != ImageLayout::Undefined &&
        currentLayout != to_vulkan_image_layout(barrierInfo.oldLayout)) {
        return RHIResult::InvalidArgument;
    }

    const vk::ImageLayout targetLayout = to_vulkan_image_layout(barrierInfo.newLayout);
    vk::ImageMemoryBarrier2 barrier{};
    barrier.srcStageMask = to_vulkan_pipeline_stages(barrierInfo.srcStage);
    barrier.srcAccessMask = to_vulkan_access_flags(barrierInfo.srcAccess);
    barrier.dstStageMask = to_vulkan_pipeline_stages(barrierInfo.dstStage);
    barrier.dstAccessMask = to_vulkan_access_flags(barrierInfo.dstAccess);
    barrier.oldLayout = currentLayout;
    barrier.newLayout = targetLayout;
    barrier.image = image->image.image;
    barrier.subresourceRange.aspectMask = to_vulkan_image_aspect_flags(barrierInfo.aspect);
    barrier.subresourceRange.baseMipLevel = barrierInfo.baseMipLevel;
    barrier.subresourceRange.levelCount = barrierInfo.mipCount;
    barrier.subresourceRange.baseArrayLayer = barrierInfo.baseArrayLayer;
    barrier.subresourceRange.layerCount = barrierInfo.layerCount;

    vk::DependencyInfo dependencyInfo{};
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &barrier;
    m_commandBuffer.pipelineBarrier2(&dependencyInfo);

    image->layout = targetLayout;
    if (barrierInfo.image == m_device.m_currentBackbufferHandle &&
        m_device.m_swapchainImageIndex < m_device.m_engine._swapchainImageLayouts.size()) {
        m_device.m_engine._swapchainImageLayouts[m_device.m_swapchainImageIndex] = static_cast<VkImageLayout>(targetLayout);
    }

    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::bufferBarrier(const BufferBarrierInfo& barrierInfo)
{
    if (!m_commandBuffer || m_rendering || !barrierInfo.buffer.isValid() ||
        !validate_stage_access_pair(barrierInfo.srcStage, barrierInfo.srcAccess) ||
        !validate_stage_access_pair(barrierInfo.dstStage, barrierInfo.dstAccess)) {
        return RHIResult::InvalidArgument;
    }

    const BufferResource* buffer = m_device.findBuffer(barrierInfo.buffer);
    if (buffer == nullptr || !buffer->buffer.buffer || barrierInfo.offset > buffer->desc.size) {
        return RHIResult::InvalidArgument;
    }

    const uint64_t size = barrierInfo.size == 0 ? (buffer->desc.size - barrierInfo.offset) : barrierInfo.size;
    if (size == 0 || barrierInfo.offset + size > buffer->desc.size) {
        return RHIResult::InvalidArgument;
    }

    vk::BufferMemoryBarrier2 barrier{};
    barrier.srcStageMask = to_vulkan_pipeline_stages(barrierInfo.srcStage);
    barrier.srcAccessMask = to_vulkan_access_flags(barrierInfo.srcAccess);
    barrier.dstStageMask = to_vulkan_pipeline_stages(barrierInfo.dstStage);
    barrier.dstAccessMask = to_vulkan_access_flags(barrierInfo.dstAccess);
    barrier.buffer = buffer->buffer.buffer;
    barrier.offset = static_cast<vk::DeviceSize>(barrierInfo.offset);
    barrier.size = barrierInfo.size == 0 ? VK_WHOLE_SIZE : static_cast<vk::DeviceSize>(barrierInfo.size);

    vk::DependencyInfo dependencyInfo{};
    dependencyInfo.bufferMemoryBarrierCount = 1;
    dependencyInfo.pBufferMemoryBarriers = &barrier;
    m_commandBuffer.pipelineBarrier2(&dependencyInfo);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::transitionImage(ImageHandle imageHandle, luna::ImageLayout newLayout)
{
    ImageResource* image = m_device.findImage(imageHandle);
    if (!m_commandBuffer || m_rendering || image == nullptr) {
        return RHIResult::InvalidArgument;
    }

    const uint32_t fullLayerCount = image->desc.type == ImageType::Image2DArray ? image->desc.arrayLayers : 1u;
    return imageBarrier({.image = imageHandle,
                         .oldLayout = ImageLayout::Undefined,
                         .newLayout = newLayout,
                         .srcStage = PipelineStage::AllCommands,
                         .dstStage = PipelineStage::AllCommands,
                         .srcAccess = ResourceAccess::MemoryWrite,
                         .dstAccess = ResourceAccess::MemoryRead | ResourceAccess::MemoryWrite,
                         .aspect = default_image_aspect(image->desc),
                         .baseMipLevel = 0,
                         .mipCount = image->desc.mipLevels,
                         .baseArrayLayer = 0,
                         .layerCount = fullLayerCount});
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

RHIResult VulkanRHIDevice::CommandContext::copyBuffer(const BufferCopyInfo& copyInfo)
{
    if (!m_commandBuffer || m_rendering || !copyInfo.source.isValid() || !copyInfo.destination.isValid() || copyInfo.size == 0) {
        return RHIResult::InvalidArgument;
    }

    const BufferResource* sourceBuffer = m_device.findBuffer(copyInfo.source);
    const BufferResource* destinationBuffer = m_device.findBuffer(copyInfo.destination);
    if (sourceBuffer == nullptr || destinationBuffer == nullptr || !sourceBuffer->buffer.buffer ||
        !destinationBuffer->buffer.buffer || copyInfo.sourceOffset + copyInfo.size > sourceBuffer->desc.size ||
        copyInfo.destinationOffset + copyInfo.size > destinationBuffer->desc.size) {
        return RHIResult::InvalidArgument;
    }

    vk::BufferCopy region{};
    region.srcOffset = static_cast<vk::DeviceSize>(copyInfo.sourceOffset);
    region.dstOffset = static_cast<vk::DeviceSize>(copyInfo.destinationOffset);
    region.size = static_cast<vk::DeviceSize>(copyInfo.size);
    m_commandBuffer.copyBuffer(sourceBuffer->buffer.buffer, destinationBuffer->buffer.buffer, 1, &region);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::copyBufferToImage(const BufferImageCopyInfo& copyInfo)
{
    if (!m_commandBuffer || m_rendering || !copyInfo.buffer.isValid() || !copyInfo.image.isValid()) {
        return RHIResult::InvalidArgument;
    }

    const BufferResource* buffer = m_device.findBuffer(copyInfo.buffer);
    ImageResource* image = m_device.findImage(copyInfo.image);
    if (buffer == nullptr || image == nullptr || !buffer->buffer.buffer || !image->image.image) {
        return RHIResult::InvalidArgument;
    }

    vk::ImageLayout imageLayout = image->layout;
    if (copyInfo.image == m_device.m_currentBackbufferHandle &&
        m_device.m_swapchainImageIndex < m_device.m_engine._swapchainImageLayouts.size()) {
        imageLayout = static_cast<vk::ImageLayout>(m_device.m_engine._swapchainImageLayouts[m_device.m_swapchainImageIndex]);
    }
    if (imageLayout != vk::ImageLayout::eTransferDstOptimal) {
        return RHIResult::InvalidArgument;
    }

    const uint32_t extentWidth = copyInfo.imageExtentWidth > 0 ? copyInfo.imageExtentWidth : image->desc.width;
    const uint32_t extentHeight = copyInfo.imageExtentHeight > 0 ? copyInfo.imageExtentHeight : image->desc.height;
    const uint32_t extentDepth = copyInfo.imageExtentDepth > 0
                                     ? copyInfo.imageExtentDepth
                                     : (image->desc.type == ImageType::Image3D ? image->desc.depth : 1u);
    if (extentWidth == 0 || extentHeight == 0 || extentDepth == 0) {
        return RHIResult::InvalidArgument;
    }

    vk::BufferImageCopy region{};
    region.bufferOffset = static_cast<vk::DeviceSize>(copyInfo.bufferOffset);
    region.bufferRowLength = copyInfo.bufferRowLength;
    region.bufferImageHeight = copyInfo.bufferImageHeight;
    region.imageSubresource.aspectMask = to_vulkan_image_aspect_flags(copyInfo.aspect);
    region.imageSubresource.mipLevel = copyInfo.mipLevel;
    region.imageSubresource.baseArrayLayer = copyInfo.baseArrayLayer;
    region.imageSubresource.layerCount = copyInfo.layerCount;
    region.imageOffset =
        vk::Offset3D{static_cast<int32_t>(copyInfo.imageOffsetX),
                     static_cast<int32_t>(copyInfo.imageOffsetY),
                     static_cast<int32_t>(copyInfo.imageOffsetZ)};
    region.imageExtent = vk::Extent3D{extentWidth, extentHeight, extentDepth};

    m_commandBuffer.copyBufferToImage(buffer->buffer.buffer, image->image.image, vk::ImageLayout::eTransferDstOptimal, 1, &region);
    return RHIResult::Success;
}

RHIResult VulkanRHIDevice::CommandContext::copyImageToBuffer(const BufferImageCopyInfo& copyInfo)
{
    if (!m_commandBuffer || m_rendering || !copyInfo.buffer.isValid() || !copyInfo.image.isValid()) {
        return RHIResult::InvalidArgument;
    }

    const BufferResource* buffer = m_device.findBuffer(copyInfo.buffer);
    ImageResource* image = m_device.findImage(copyInfo.image);
    if (buffer == nullptr || image == nullptr || !buffer->buffer.buffer || !image->image.image) {
        return RHIResult::InvalidArgument;
    }

    vk::ImageLayout imageLayout = image->layout;
    if (copyInfo.image == m_device.m_currentBackbufferHandle &&
        m_device.m_swapchainImageIndex < m_device.m_engine._swapchainImageLayouts.size()) {
        imageLayout = static_cast<vk::ImageLayout>(m_device.m_engine._swapchainImageLayouts[m_device.m_swapchainImageIndex]);
    }
    if (imageLayout != vk::ImageLayout::eTransferSrcOptimal) {
        return RHIResult::InvalidArgument;
    }

    const uint32_t extentWidth = copyInfo.imageExtentWidth > 0 ? copyInfo.imageExtentWidth : image->desc.width;
    const uint32_t extentHeight = copyInfo.imageExtentHeight > 0 ? copyInfo.imageExtentHeight : image->desc.height;
    const uint32_t extentDepth = copyInfo.imageExtentDepth > 0
                                     ? copyInfo.imageExtentDepth
                                     : (image->desc.type == ImageType::Image3D ? image->desc.depth : 1u);
    if (extentWidth == 0 || extentHeight == 0 || extentDepth == 0) {
        return RHIResult::InvalidArgument;
    }

    vk::BufferImageCopy region{};
    region.bufferOffset = static_cast<vk::DeviceSize>(copyInfo.bufferOffset);
    region.bufferRowLength = copyInfo.bufferRowLength;
    region.bufferImageHeight = copyInfo.bufferImageHeight;
    region.imageSubresource.aspectMask = to_vulkan_image_aspect_flags(copyInfo.aspect);
    region.imageSubresource.mipLevel = copyInfo.mipLevel;
    region.imageSubresource.baseArrayLayer = copyInfo.baseArrayLayer;
    region.imageSubresource.layerCount = copyInfo.layerCount;
    region.imageOffset =
        vk::Offset3D{static_cast<int32_t>(copyInfo.imageOffsetX),
                     static_cast<int32_t>(copyInfo.imageOffsetY),
                     static_cast<int32_t>(copyInfo.imageOffsetZ)};
    region.imageExtent = vk::Extent3D{extentWidth, extentHeight, extentDepth};

    m_commandBuffer.copyImageToBuffer(image->image.image, vk::ImageLayout::eTransferSrcOptimal, buffer->buffer.buffer, 1, &region);
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

RHIResult VulkanRHIDevice::CommandContext::bindResourceSet(ResourceSetHandle resourceSet,
                                                           std::span<const uint32_t> dynamicOffsets)
{
    if (!m_commandBuffer || !m_boundPipelineLayout) {
        return RHIResult::NotReady;
    }

    const ResourceSetResource* resourceSetResource = m_device.findResourceSet(resourceSet);
    if (resourceSetResource == nullptr || !resourceSetResource->set) {
        return RHIResult::InvalidArgument;
    }

    const ResourceLayoutResource* layout = m_device.findResourceLayout(resourceSetResource->layoutHandle);
    if (layout == nullptr || !layout->layout) {
        return RHIResult::InvalidArgument;
    }

    m_commandBuffer.bindDescriptorSets(
        m_boundPipelineBindPoint,
        m_boundPipelineLayout,
        layout->desc.setIndex,
        1,
        &resourceSetResource->set,
        static_cast<uint32_t>(dynamicOffsets.size()),
        dynamicOffsets.data());
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

RHIResult VulkanRHIDevice::CommandContext::dispatchIndirect(BufferHandle argumentsBuffer, uint64_t offset)
{
    if (!m_commandBuffer || m_rendering || m_boundPipelineBindPoint != vk::PipelineBindPoint::eCompute) {
        return RHIResult::InvalidArgument;
    }

    const BufferResource* buffer = m_device.findBuffer(argumentsBuffer);
    if (buffer == nullptr || !buffer->buffer.buffer || offset + sizeof(VkDispatchIndirectCommand) > buffer->desc.size) {
        return RHIResult::InvalidArgument;
    }

    m_commandBuffer.dispatchIndirect(buffer->buffer.buffer, static_cast<vk::DeviceSize>(offset));
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

    for (auto& [handle, imageView] : m_imageViews) {
        if (m_engine._device && imageView.view) {
            m_engine._device.destroyImageView(imageView.view, nullptr);
        }
        m_bindingRegistry.unregister_image_view(ImageViewHandle::fromRaw(handle));
    }
    m_imageViews.clear();

    for (auto& [handle, image] : m_images) {
        if (image.owned) {
            m_engine.destroy_image(image.image);
        }
        m_bindingRegistry.unregister_image_view(ImageViewHandle::fromRaw(handle));
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
        m_bindingRegistry.unregister_image_view(ImageViewHandle::fromRaw(m_currentBackbufferHandle.value));
        m_images.erase(m_currentBackbufferHandle.value);
        m_currentBackbufferHandle = {};
    }

    const ImageHandle handle = ImageHandle::fromRaw(nextHandleValue(&m_nextImageId));
    if (!handle.isValid() ||
        !m_bindingRegistry.register_image_view(ImageViewHandle::fromRaw(handle.value),
                                               m_engine._swapchainImageViews[m_swapchainImageIndex])) {
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
