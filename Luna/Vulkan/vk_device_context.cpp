#include "vk_device_context.h"

#include "Core/log.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "VkBootstrap.h"
#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <optional>
#include <string>

#ifndef NDEBUG
constexpr bool bUseValidationLayers = true;
#else
constexpr bool bUseValidationLayers = false;
#endif

namespace {

vk::BufferUsageFlags to_vulkan_buffer_usage(luna::BufferUsage usage)
{
    vk::BufferUsageFlags flags{};
    const uint32_t bits = static_cast<uint32_t>(usage);

    if ((bits & static_cast<uint32_t>(luna::BufferUsage::TransferSrc)) != 0) {
        flags |= vk::BufferUsageFlagBits::eTransferSrc;
    }
    if ((bits & static_cast<uint32_t>(luna::BufferUsage::TransferDst)) != 0) {
        flags |= vk::BufferUsageFlagBits::eTransferDst;
    }
    if ((bits & static_cast<uint32_t>(luna::BufferUsage::Vertex)) != 0) {
        flags |= vk::BufferUsageFlagBits::eVertexBuffer;
    }
    if ((bits & static_cast<uint32_t>(luna::BufferUsage::Index)) != 0) {
        flags |= vk::BufferUsageFlagBits::eIndexBuffer;
    }
    if ((bits & static_cast<uint32_t>(luna::BufferUsage::Uniform)) != 0) {
        flags |= vk::BufferUsageFlagBits::eUniformBuffer;
    }
    if ((bits & static_cast<uint32_t>(luna::BufferUsage::Storage)) != 0) {
        flags |= vk::BufferUsageFlagBits::eStorageBuffer;
    }
    if ((bits & static_cast<uint32_t>(luna::BufferUsage::Indirect)) != 0) {
        flags |= vk::BufferUsageFlagBits::eIndirectBuffer;
    }

    return flags;
}

vk::ImageUsageFlags to_vulkan_image_usage(luna::ImageUsage usage)
{
    vk::ImageUsageFlags flags{};
    const uint32_t bits = static_cast<uint32_t>(usage);

    if ((bits & static_cast<uint32_t>(luna::ImageUsage::TransferSrc)) != 0) {
        flags |= vk::ImageUsageFlagBits::eTransferSrc;
    }
    if ((bits & static_cast<uint32_t>(luna::ImageUsage::TransferDst)) != 0) {
        flags |= vk::ImageUsageFlagBits::eTransferDst;
    }
    if ((bits & static_cast<uint32_t>(luna::ImageUsage::Sampled)) != 0) {
        flags |= vk::ImageUsageFlagBits::eSampled;
    }
    if ((bits & static_cast<uint32_t>(luna::ImageUsage::ColorAttachment)) != 0) {
        flags |= vk::ImageUsageFlagBits::eColorAttachment;
    }
    if ((bits & static_cast<uint32_t>(luna::ImageUsage::DepthStencilAttachment)) != 0) {
        flags |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
    }
    if ((bits & static_cast<uint32_t>(luna::ImageUsage::Storage)) != 0) {
        flags |= vk::ImageUsageFlagBits::eStorage;
    }

    return flags;
}

VmaMemoryUsage to_vma_memory_usage(luna::MemoryUsage usage)
{
    switch (usage) {
        case luna::MemoryUsage::Upload:
            return VMA_MEMORY_USAGE_CPU_TO_GPU;
        case luna::MemoryUsage::Readback:
            return VMA_MEMORY_USAGE_GPU_TO_CPU;
        case luna::MemoryUsage::Default:
        default:
            return VMA_MEMORY_USAGE_GPU_ONLY;
    }
}

vk::Filter to_vulkan_filter(luna::FilterMode filter)
{
    switch (filter) {
        case luna::FilterMode::Nearest:
            return vk::Filter::eNearest;
        case luna::FilterMode::Linear:
        default:
            return vk::Filter::eLinear;
    }
}

vk::SamplerMipmapMode to_vulkan_mipmap_mode(luna::SamplerMipmapMode mode)
{
    switch (mode) {
        case luna::SamplerMipmapMode::Nearest:
            return vk::SamplerMipmapMode::eNearest;
        case luna::SamplerMipmapMode::Linear:
        default:
            return vk::SamplerMipmapMode::eLinear;
    }
}

vk::SamplerAddressMode to_vulkan_sampler_address_mode(luna::SamplerAddressMode mode)
{
    switch (mode) {
        case luna::SamplerAddressMode::ClampToBorder:
            return vk::SamplerAddressMode::eClampToBorder;
        case luna::SamplerAddressMode::ClampToEdge:
            return vk::SamplerAddressMode::eClampToEdge;
        case luna::SamplerAddressMode::MirroredRepeat:
            return vk::SamplerAddressMode::eMirroredRepeat;
        case luna::SamplerAddressMode::Repeat:
        default:
            return vk::SamplerAddressMode::eRepeat;
    }
}

vk::BorderColor to_vulkan_border_color(luna::SamplerBorderColor color)
{
    switch (color) {
        case luna::SamplerBorderColor::FloatOpaqueBlack:
            return vk::BorderColor::eFloatOpaqueBlack;
        case luna::SamplerBorderColor::FloatOpaqueWhite:
            return vk::BorderColor::eFloatOpaqueWhite;
        case luna::SamplerBorderColor::FloatTransparentBlack:
        default:
            return vk::BorderColor::eFloatTransparentBlack;
    }
}

vk::ImageType to_vulkan_image_type(luna::ImageType type)
{
    switch (type) {
        case luna::ImageType::Image3D:
            return vk::ImageType::e3D;
        case luna::ImageType::Cube:
        case luna::ImageType::Image2DArray:
        case luna::ImageType::Image2D:
        default:
            return vk::ImageType::e2D;
    }
}

vk::ImageViewType to_vulkan_image_view_type(luna::ImageType type)
{
    switch (type) {
        case luna::ImageType::Cube:
            return vk::ImageViewType::eCube;
        case luna::ImageType::Image2DArray:
            return vk::ImageViewType::e2DArray;
        case luna::ImageType::Image3D:
            return vk::ImageViewType::e3D;
        case luna::ImageType::Image2D:
        default:
            return vk::ImageViewType::e2D;
    }
}

uint32_t image_array_layer_count(const luna::ImageDesc& desc)
{
    return (desc.type == luna::ImageType::Image2DArray || desc.type == luna::ImageType::Cube) ? desc.arrayLayers : 1u;
}

uint32_t image_upload_depth(const luna::ImageDesc& desc)
{
    return desc.type == luna::ImageType::Image3D ? desc.depth : 1u;
}

size_t image_base_level_data_size(const luna::ImageDesc& desc)
{
    const uint64_t texelCount = static_cast<uint64_t>(desc.width) * static_cast<uint64_t>(desc.height) *
                                static_cast<uint64_t>(image_upload_depth(desc)) *
                                static_cast<uint64_t>(image_array_layer_count(desc));
    return static_cast<size_t>(texelCount * luna::pixel_format_bytes_per_pixel(desc.format));
}

void transition_image_subresource(vk::CommandBuffer cmd,
                                  vk::Image image,
                                  vk::ImageAspectFlags aspectMask,
                                  vk::ImageLayout currentLayout,
                                  vk::ImageLayout newLayout,
                                  uint32_t baseMipLevel,
                                  uint32_t levelCount,
                                  uint32_t baseArrayLayer,
                                  uint32_t layerCount)
{
    vk::ImageMemoryBarrier2 imageBarrier{};
    imageBarrier.srcStageMask = vk::PipelineStageFlagBits2::eAllCommands;
    imageBarrier.srcAccessMask = vk::AccessFlagBits2::eMemoryWrite;
    imageBarrier.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands;
    imageBarrier.dstAccessMask = vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead;
    imageBarrier.oldLayout = currentLayout;
    imageBarrier.newLayout = newLayout;
    imageBarrier.image = image;
    imageBarrier.subresourceRange.aspectMask = aspectMask;
    imageBarrier.subresourceRange.baseMipLevel = baseMipLevel;
    imageBarrier.subresourceRange.levelCount = levelCount;
    imageBarrier.subresourceRange.baseArrayLayer = baseArrayLayer;
    imageBarrier.subresourceRange.layerCount = layerCount;

    vk::DependencyInfo depInfo{};
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &imageBarrier;
    cmd.pipelineBarrier2(&depInfo);
}

size_t align_up_size(size_t value, size_t alignment)
{
    if (alignment <= 1) {
        return value;
    }

    const size_t mask = alignment - 1;
    return (value + mask) & ~mask;
}

uint64_t make_adapter_id(const VkPhysicalDeviceProperties& properties)
{
    const uint64_t combined =
        (static_cast<uint64_t>(properties.vendorID) << 32u) | static_cast<uint64_t>(properties.deviceID);
    return combined != 0 ? combined : (1ull << 63u) | static_cast<uint64_t>(properties.deviceType);
}

template <typename T> void logVkbError(const char* step, const vkb::Result<T>& result)
{
    LUNA_CORE_ERROR("{} failed: {}", step, result.error().message());
    for (const std::string& reason : result.full_error().detailed_failure_reasons) {
        LUNA_CORE_ERROR("  {}", reason);
    }
}

} // namespace

void VulkanDeviceContext::begin_upload_batch(vk::CommandBuffer commandBuffer, FrameData& frame)
{
    m_activeUploadContext.commandBuffer = commandBuffer;
    m_activeUploadContext.frame = &frame;
    reset_frame_upload_state(frame);
}

void VulkanDeviceContext::end_upload_batch()
{
    m_activeUploadContext = {};
}

bool VulkanDeviceContext::has_active_upload_batch() const
{
    return m_activeUploadContext.commandBuffer && m_activeUploadContext.frame != nullptr;
}

void VulkanDeviceContext::reset_frame_upload_state(FrameData& frame)
{
    frame._uploadStagingOffset = 0;
    frame._uploadBatchBytes = 0;
    frame._uploadBatchOps = 0;
}

AllocatedBuffer VulkanDeviceContext::acquire_upload_staging_buffer(size_t size,
                                                                   size_t alignment,
                                                                   vk::DeviceSize* outOffset)
{
    if (outOffset == nullptr || !has_active_upload_batch()) {
        return {};
    }

    FrameData& frame = *m_activeUploadContext.frame;
    const size_t alignedOffset = align_up_size(frame._uploadStagingOffset, alignment);
    if (frame._uploadStagingBuffer.buffer && alignedOffset + size <= frame._uploadStagingCapacity) {
        *outOffset = static_cast<vk::DeviceSize>(alignedOffset);
        frame._uploadStagingOffset = alignedOffset + size;
        return frame._uploadStagingBuffer;
    }

    if (frame._uploadBatchOps == 0) {
        const size_t desiredCapacity = std::max(size, std::max(frame._uploadStagingCapacity * 2, size_t{64 * 1024}));
        if (frame._uploadStagingBuffer.buffer) {
            destroy_buffer(frame._uploadStagingBuffer);
            frame._uploadStagingBuffer = {};
        }

        frame._uploadStagingBuffer =
            create_buffer(desiredCapacity, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        frame._uploadStagingCapacity = desiredCapacity;
        frame._uploadStagingOffset = size;
        *outOffset = 0;
        LUNA_CORE_INFO("staging ring resized: frame={}, capacity={} bytes",
                       _frameNumber,
                       static_cast<unsigned long long>(frame._uploadStagingCapacity));
        return frame._uploadStagingBuffer;
    }

    AllocatedBuffer spillBuffer =
        create_buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    frame._deletionQueue.push_function([this, spillBuffer]() {
        destroy_buffer(spillBuffer);
    });
    *outOffset = 0;
    LUNA_CORE_INFO("staging ring spill: frame={}, bytes={}", _frameNumber, static_cast<unsigned long long>(size));
    return spillBuffer;
}

bool VulkanDeviceContext::init(luna::Window& window)
{
    if (_isInitialized) {
        LUNA_CORE_ERROR("VulkanDeviceContext already initialized");
        return false;
    }

    _frameNumber = 0;
    resize_requested = false;
    LUNA_CORE_INFO("Initializing Vulkan device context");

    _window = static_cast<GLFWwindow*>(window.getNativeWindow());
    if (_window == nullptr) {
        LUNA_CORE_ERROR("Failed to acquire native GLFW window");
        return false;
    }

    _windowExtent = {window.getWidth(), window.getHeight()};
    const vk::Extent2D framebufferExtent = get_framebuffer_extent();
    LUNA_CORE_INFO("Created GLFW window: logical={}x{}, framebuffer={}x{}",
                   _windowExtent.width,
                   _windowExtent.height,
                   framebufferExtent.width,
                   framebufferExtent.height);

    if (!init_vulkan() || !init_swapchain() || !init_commands() || !init_sync_structures()) {
        cleanup();
        return false;
    }

    _isInitialized = true;
    LUNA_CORE_INFO("Vulkan device context initialized");

    return true;
}

bool VulkanDeviceContext::init_headless()
{
    if (_isInitialized) {
        LUNA_CORE_ERROR("VulkanDeviceContext already initialized");
        return false;
    }

    _frameNumber = 0;
    resize_requested = false;
    _window = nullptr;
    _windowExtent = {m_requestedSwapchainDesc.width, m_requestedSwapchainDesc.height};
    LUNA_CORE_INFO("Initializing Vulkan device context (headless)");

    if (!init_vulkan() || !init_commands() || !init_sync_structures()) {
        cleanup();
        return false;
    }

    _isInitialized = true;
    LUNA_CORE_INFO("Vulkan device context initialized (headless)");
    return true;
}

void VulkanDeviceContext::cleanup()
{
    LUNA_CORE_INFO("Cleaning up Vulkan device context");

    if (_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(_device);

        for (int i = 0; i < FRAME_OVERLAP; i++) {
            if (_frames[i]._uploadStagingBuffer.buffer != VK_NULL_HANDLE) {
                destroy_buffer(_frames[i]._uploadStagingBuffer);
                _frames[i]._uploadStagingBuffer = {};
            }

            if (_frames[i]._commandPool != VK_NULL_HANDLE) {
                vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);
                _frames[i]._commandPool = VK_NULL_HANDLE;
            }

            if (_frames[i]._renderFence != VK_NULL_HANDLE) {
                vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
                _frames[i]._renderFence = VK_NULL_HANDLE;
            }

            if (_frames[i]._renderSemaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
                _frames[i]._renderSemaphore = VK_NULL_HANDLE;
            }

            if (_frames[i]._swapchainSemaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(_device, _frames[i]._swapchainSemaphore, nullptr);
                _frames[i]._swapchainSemaphore = VK_NULL_HANDLE;
            }

            _frames[i]._mainCommandBuffer = VK_NULL_HANDLE;
            _frames[i]._uploadStagingCapacity = 0;
            reset_frame_upload_state(_frames[i]);
            _frames[i]._submitSerial = 0;
            _frames[i]._deletionQueue.flush();
        }

        if (_immContext._commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(_device, _immContext._commandPool, nullptr);
            _immContext._commandPool = VK_NULL_HANDLE;
            _immContext._commandBuffer = VK_NULL_HANDLE;
        }

        if (_immContext._fence != VK_NULL_HANDLE) {
            vkDestroyFence(_device, _immContext._fence, nullptr);
            _immContext._fence = VK_NULL_HANDLE;
        }

        destroy_swapchain();
        _mainDeletionQueue.flush();
    }

    if (_instance != VK_NULL_HANDLE && _surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        _surface = VK_NULL_HANDLE;
    }

    if (_device != VK_NULL_HANDLE) {
        vkDestroyDevice(_device, nullptr);
        _device = VK_NULL_HANDLE;
    }

    if (_instance != VK_NULL_HANDLE && _debug_messenger != VK_NULL_HANDLE) {
        vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
        _debug_messenger = VK_NULL_HANDLE;
    }

    if (_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(_instance, nullptr);
        _instance = VK_NULL_HANDLE;
    }

    _window = nullptr;
    _chosenGPU = VK_NULL_HANDLE;
    _graphicsQueue = VK_NULL_HANDLE;
    _graphicsQueueFamily = 0;
    _swapchain = VK_NULL_HANDLE;
    _swapchainImageFormat = vk::Format::eUndefined;
    _swapchainPresentMode = vk::PresentModeKHR::eFifo;
    _swapchainExtent = {};
    _allocator = VK_NULL_HANDLE;
    m_activeUploadContext = {};
    _isInitialized = false;
    m_swapchainGeneration = 0;
    m_preferredAdapterId = 0;
    m_samplerAnisotropyEnabled = false;
    m_applicationName = "Luna";
    m_requestedSwapchainDesc = {};

    LUNA_CORE_INFO("Vulkan device context cleanup complete");
}

bool VulkanDeviceContext::init_vulkan()
{
    m_samplerAnisotropyEnabled = false;

    vkb::InstanceBuilder builder;
    builder.set_app_name(m_applicationName.c_str())
        .request_validation_layers(bUseValidationLayers)
        .use_default_debug_messenger()
        .require_api_version(1, 3, 0);

    if (_window != nullptr) {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        if (glfwExtensions == nullptr || glfwExtensionCount == 0) {
            LUNA_CORE_ERROR("Failed to query GLFW Vulkan instance extensions");
            return false;
        }

        builder.enable_extensions(glfwExtensionCount, glfwExtensions);
    } else {
        builder.set_headless(true);
    }

    auto inst_ret = builder.build();
    if (!inst_ret) {
        logVkbError("Vulkan instance creation", inst_ret);
        return false;
    }

    vkb::Instance vkb_inst = inst_ret.value();
    _instance = vkb_inst.instance;
    _debug_messenger = vkb_inst.debug_messenger;

    if (_window != nullptr) {
        VkSurfaceKHR rawSurface = VK_NULL_HANDLE;
        const VkResult surfaceResult = glfwCreateWindowSurface(_instance, _window, nullptr, &rawSurface);
        if (surfaceResult != VK_SUCCESS) {
            LUNA_CORE_ERROR("Failed to create Vulkan surface: {}", string_VkResult(surfaceResult));
            return false;
        }
        _surface = rawSurface;
        LUNA_CORE_INFO("Surface created");
    }

    VkPhysicalDeviceVulkan13Features features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features.dynamicRendering = true;
    features.synchronization2 = true;

    VkPhysicalDeviceVulkan12Features features12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    vkb::PhysicalDeviceSelector selector{vkb_inst};
    selector.set_minimum_version(1, 3).set_required_features_13(features).set_required_features_12(features12);
    if (_surface != VK_NULL_HANDLE) {
        selector.set_surface(_surface);
    }

    auto physical_devices_ret = selector.select_devices();
    if (!physical_devices_ret) {
        logVkbError("Physical device selection", physical_devices_ret);
        return false;
    }

    const std::vector<vkb::PhysicalDevice>& physicalDevices = physical_devices_ret.value();
    if (physicalDevices.empty()) {
        LUNA_CORE_ERROR("No suitable Vulkan physical devices were found");
        return false;
    }

    std::optional<vkb::PhysicalDevice> selectedDevice;
    if (m_preferredAdapterId != 0) {
        for (const vkb::PhysicalDevice& candidate : physicalDevices) {
            if (make_adapter_id(candidate.properties) == m_preferredAdapterId) {
                selectedDevice = candidate;
                break;
            }
        }

        if (!selectedDevice.has_value()) {
            LUNA_CORE_ERROR("Preferred Vulkan adapter 0x{:016X} was not found", m_preferredAdapterId);
            return false;
        }
    } else {
        selectedDevice = physicalDevices.front();
    }

    vkb::PhysicalDevice physicalDevice = selectedDevice.value();
    VkPhysicalDeviceFeatures optionalFeatures{};
    optionalFeatures.samplerAnisotropy = VK_TRUE;
    m_samplerAnisotropyEnabled = physicalDevice.enable_features_if_present(optionalFeatures);

    vkb::DeviceBuilder deviceBuilder{physicalDevice};

    auto device_ret = deviceBuilder.build();
    if (!device_ret) {
        logVkbError("Logical device creation", device_ret);
        return false;
    }

    vkb::Device vkbDevice = device_ret.value();

    _device = vkbDevice.device;
    _chosenGPU = physicalDevice.physical_device;

    auto graphicsQueueRet = vkbDevice.get_queue(vkb::QueueType::graphics);
    if (!graphicsQueueRet) {
        logVkbError("Graphics queue fetch", graphicsQueueRet);
        return false;
    }
    _graphicsQueue = graphicsQueueRet.value();

    auto graphicsQueueIndexRet = vkbDevice.get_queue_index(vkb::QueueType::graphics);
    if (!graphicsQueueIndexRet) {
        logVkbError("Graphics queue family fetch", graphicsQueueIndexRet);
        return false;
    }
    _graphicsQueueFamily = graphicsQueueIndexRet.value();

    VkPhysicalDeviceProperties gpuProperties{};
    vkGetPhysicalDeviceProperties(_chosenGPU, &gpuProperties);
    LUNA_CORE_INFO("Backend=Vulkan, GPU={}, GraphicsQueueFamily={}", gpuProperties.deviceName, _graphicsQueueFamily);
    LUNA_CORE_INFO("Selected GPU: {}", gpuProperties.deviceName);

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = _chosenGPU;
    allocatorInfo.device = _device;
    allocatorInfo.instance = _instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    const VkResult allocatorResult = vmaCreateAllocator(&allocatorInfo, &_allocator);
    if (allocatorResult != VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to create VMA allocator: {}", string_VkResult(allocatorResult));
        return false;
    }

    _mainDeletionQueue.push_function([&]() {
        vmaDestroyAllocator(_allocator);
    });

    return true;
}

bool VulkanDeviceContext::create_swapchain(uint32_t width, uint32_t height)
{
    LUNA_CORE_INFO("Creating swapchain for framebuffer {}x{}", width, height);
    vkb::SwapchainBuilder swapchainBuilder{_chosenGPU, _device, _surface};

    vk::Format desiredFormat = to_vulkan_format(m_requestedSwapchainDesc.format);
    if (desiredFormat == vk::Format::eUndefined || desiredFormat == vk::Format::eD32Sfloat ||
        desiredFormat == vk::Format::eR16G16Sfloat || desiredFormat == vk::Format::eR32Sfloat ||
        desiredFormat == vk::Format::eB10G11R11UfloatPack32 || desiredFormat == vk::Format::eR16G16B16A16Sfloat) {
        desiredFormat = vk::Format::eB8G8R8A8Unorm;
    }

    const VkPresentModeKHR desiredPresentMode =
        m_requestedSwapchainDesc.vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;

    auto swapchain_ret = swapchainBuilder
                             .set_desired_format(VkSurfaceFormatKHR{.format = static_cast<VkFormat>(desiredFormat),
                                                                    .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
                             .add_fallback_format(
                                 VkSurfaceFormatKHR{.format = VK_FORMAT_B8G8R8A8_UNORM,
                                                    .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
                             .add_fallback_format(
                                 VkSurfaceFormatKHR{.format = VK_FORMAT_R8G8B8A8_UNORM,
                                                    .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
                             .add_fallback_format(
                                 VkSurfaceFormatKHR{.format = VK_FORMAT_R8G8B8A8_SRGB,
                                                    .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
                             .set_desired_present_mode(desiredPresentMode)
                             .add_fallback_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
                             .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                             .set_desired_min_image_count(std::max(1u, m_requestedSwapchainDesc.bufferCount))
                             .set_desired_extent(width, height)
                             .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                             .build();
    if (!swapchain_ret) {
        logVkbError("Swapchain creation", swapchain_ret);
        return false;
    }

    vkb::Swapchain vkbSwapchain = swapchain_ret.value();

    _swapchainExtent = vkbSwapchain.extent;
    _swapchain = vkbSwapchain.swapchain;
    _swapchainImageFormat = static_cast<vk::Format>(vkbSwapchain.image_format);
    _swapchainPresentMode = static_cast<vk::PresentModeKHR>(vkbSwapchain.present_mode);
    ++m_swapchainGeneration;

    auto imagesRet = vkbSwapchain.get_images();
    if (!imagesRet) {
        logVkbError("Swapchain image fetch", imagesRet);
        return false;
    }
    _swapchainImages.clear();
    _swapchainImages.reserve(imagesRet.value().size());
    for (VkImage image : imagesRet.value()) {
        vk::Image swapchainImage{};
        swapchainImage = image;
        _swapchainImages.push_back(swapchainImage);
    }

    auto imageViewsRet = vkbSwapchain.get_image_views();
    if (!imageViewsRet) {
        logVkbError("Swapchain image view fetch", imageViewsRet);
        return false;
    }
    _swapchainImageViews.clear();
    _swapchainImageViews.reserve(imageViewsRet.value().size());
    for (VkImageView imageView : imageViewsRet.value()) {
        vk::ImageView swapchainImageView{};
        swapchainImageView = imageView;
        _swapchainImageViews.push_back(swapchainImageView);
    }
    _swapchainImageLayouts.assign(_swapchainImages.size(), VK_IMAGE_LAYOUT_UNDEFINED);

    LUNA_CORE_INFO("Created swapchain: {}x{}, images={}",
                   _swapchainExtent.width,
                   _swapchainExtent.height,
                   _swapchainImages.size());
    LUNA_CORE_INFO("Swapchain created, Format={}, PresentMode={}, Extent={}x{}, RequestedBufferCount={}",
                   string_VkFormat(_swapchainImageFormat),
                   vk::to_string(_swapchainPresentMode),
                   _swapchainExtent.width,
                   _swapchainExtent.height,
                   std::max(1u, m_requestedSwapchainDesc.bufferCount));

    return true;
}

void VulkanDeviceContext::destroy_swapchain()
{
    if (_swapchain == VK_NULL_HANDLE) {
        return;
    }

    for (size_t i = 0; i < _swapchainImageViews.size(); i++) {
        vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
    }

    _swapchainImageViews.clear();
    _swapchainImages.clear();
    _swapchainImageLayouts.clear();

    vkDestroySwapchainKHR(_device, _swapchain, nullptr);
    _swapchain = VK_NULL_HANDLE;
    _swapchainExtent = {};
    _swapchainImageFormat = vk::Format::eUndefined;
}

bool VulkanDeviceContext::init_swapchain()
{
    if (_window == nullptr || _surface == VK_NULL_HANDLE) {
        LUNA_CORE_ERROR("Cannot create swapchain without a valid window surface");
        return false;
    }

    const vk::Extent2D framebufferExtent = get_framebuffer_extent();
    if (framebufferExtent.width == 0 || framebufferExtent.height == 0) {
        LUNA_CORE_ERROR("Cannot create swapchain because framebuffer extent is {}x{}",
                        framebufferExtent.width,
                        framebufferExtent.height);
        return false;
    }

    LUNA_CORE_INFO("Preparing swapchain using logical window {}x{} and framebuffer {}x{}",
                   _windowExtent.width,
                   _windowExtent.height,
                   framebufferExtent.width,
                   framebufferExtent.height);
    return create_swapchain(framebufferExtent.width, framebufferExtent.height);
}

bool VulkanDeviceContext::init_commands()
{
    vk::CommandPoolCreateInfo commandPoolInfo =
        vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        const vk::Result commandPoolResult =
            _device.createCommandPool(&commandPoolInfo, nullptr, &_frames[i]._commandPool);
        if (commandPoolResult != vk::Result::eSuccess) {
            LUNA_CORE_ERROR("Failed to create command pool: {}", string_VkResult(commandPoolResult));
            return false;
        }

        vk::CommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

        const vk::Result commandBufferResult =
            _device.allocateCommandBuffers(&cmdAllocInfo, &_frames[i]._mainCommandBuffer);
        if (commandBufferResult != vk::Result::eSuccess) {
            LUNA_CORE_ERROR("Failed to allocate command buffer: {}", string_VkResult(commandBufferResult));
            return false;
        }
    }

    const vk::Result uploadCommandPoolResult =
        _device.createCommandPool(&commandPoolInfo, nullptr, &_immContext._commandPool);
    if (uploadCommandPoolResult != vk::Result::eSuccess) {
        LUNA_CORE_ERROR("Failed to create immediate command pool: {}", string_VkResult(uploadCommandPoolResult));
        return false;
    }

    vk::CommandBufferAllocateInfo uploadCmdAllocInfo = vkinit::command_buffer_allocate_info(_immContext._commandPool, 1);
    const vk::Result uploadCommandBufferResult =
        _device.allocateCommandBuffers(&uploadCmdAllocInfo, &_immContext._commandBuffer);
    if (uploadCommandBufferResult != vk::Result::eSuccess) {
        LUNA_CORE_ERROR("Failed to allocate immediate command buffer: {}", string_VkResult(uploadCommandBufferResult));
        return false;
    }

    LUNA_CORE_INFO("Initialized command pools and command buffers for {} frames", FRAME_OVERLAP);
    return true;
}

bool VulkanDeviceContext::init_sync_structures()
{
    vk::FenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    vk::SemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        const vk::Result fenceResult = _device.createFence(&fenceCreateInfo, nullptr, &_frames[i]._renderFence);
        if (fenceResult != vk::Result::eSuccess) {
            LUNA_CORE_ERROR("Failed to create fence: {}", string_VkResult(fenceResult));
            return false;
        }

        const vk::Result swapchainSemaphoreResult =
            _device.createSemaphore(&semaphoreCreateInfo, nullptr, &_frames[i]._swapchainSemaphore);
        if (swapchainSemaphoreResult != vk::Result::eSuccess) {
            LUNA_CORE_ERROR("Failed to create swapchain semaphore: {}", string_VkResult(swapchainSemaphoreResult));
            return false;
        }

        const vk::Result renderSemaphoreResult =
            _device.createSemaphore(&semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore);
        if (renderSemaphoreResult != vk::Result::eSuccess) {
            LUNA_CORE_ERROR("Failed to create render semaphore: {}", string_VkResult(renderSemaphoreResult));
            return false;
        }
    }

    const vk::Result uploadFenceResult = _device.createFence(&fenceCreateInfo, nullptr, &_immContext._fence);
    if (uploadFenceResult != vk::Result::eSuccess) {
        LUNA_CORE_ERROR("Failed to create immediate submit fence: {}", string_VkResult(uploadFenceResult));
        return false;
    }

    LUNA_CORE_INFO("Initialized synchronization primitives for {} frames", FRAME_OVERLAP);
    return true;
}

bool VulkanDeviceContext::resize_swapchain()
{
    if (_window == nullptr || _surface == VK_NULL_HANDLE || _swapchain == VK_NULL_HANDLE) {
        LUNA_CORE_WARN("Skipping swapchain recreation because no window-backed swapchain is active");
        return false;
    }

    const vk::Extent2D framebufferExtent = get_framebuffer_extent();
    if (framebufferExtent.width == 0 || framebufferExtent.height == 0) {
        LUNA_CORE_WARN("Skipping swapchain recreation because framebuffer extent is {}x{}",
                       framebufferExtent.width,
                       framebufferExtent.height);
        return false;
    }

    LUNA_CORE_INFO("Recreating swapchain: old={}x{}, new framebuffer={}x{}",
                   _swapchainExtent.width,
                   _swapchainExtent.height,
                   framebufferExtent.width,
                   framebufferExtent.height);
    VK_CHECK(vkDeviceWaitIdle(_device));
    destroy_swapchain();
    if (!create_swapchain(framebufferExtent.width, framebufferExtent.height)) {
        LUNA_CORE_ERROR("Swapchain recreation failed");
        return false;
    }

    resize_requested = false;
    LUNA_CORE_INFO("Swapchain recreated");
    return true;
}

vk::Extent2D VulkanDeviceContext::get_framebuffer_extent() const
{
    if (_window == nullptr) {
        return {};
    }

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(_window, &framebufferWidth, &framebufferHeight);

    return {static_cast<uint32_t>(framebufferWidth > 0 ? framebufferWidth : 0),
            static_cast<uint32_t>(framebufferHeight > 0 ? framebufferHeight : 0)};
}

AllocatedImage VulkanDeviceContext::create_image(vk::Extent3D size,
                                                 vk::Format format,
                                                 vk::ImageUsageFlags usage,
                                                 bool mipmapped)
{
    AllocatedImage newImage;
    newImage.imageFormat = format;
    newImage.imageExtent = size;

    vk::ImageCreateInfo img_info = vkinit::image_create_info(format, usage, size);
    if (mipmapped) {
        img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }

    VmaAllocationCreateInfo allocinfo = {};
    allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkImage rawImage = VK_NULL_HANDLE;
    VK_CHECK(vmaCreateImage(_allocator,
                            reinterpret_cast<const VkImageCreateInfo*>(&img_info),
                            &allocinfo,
                            &rawImage,
                            &newImage.allocation,
                            nullptr));
    newImage.image = rawImage;

    vk::ImageAspectFlags aspectFlags = vk::ImageAspectFlagBits::eColor;
    if (format == vk::Format::eD32Sfloat) {
        aspectFlags = vk::ImageAspectFlagBits::eDepth;
    }

    vk::ImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlags);
    view_info.subresourceRange.levelCount = img_info.mipLevels;

    VK_CHECK(_device.createImageView(&view_info, nullptr, &newImage.imageView));
    return newImage;
}

AllocatedImage VulkanDeviceContext::create_image(void* data,
                                                 vk::Extent3D size,
                                                 vk::Format format,
                                                 vk::ImageUsageFlags usage,
                                                 bool mipmapped)
{
    const size_t dataSize = size.depth * size.width * size.height * 4;
    AllocatedBuffer uploadbuffer = create_buffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    std::memcpy(uploadbuffer.info.pMappedData, data, dataSize);

    AllocatedImage newImage =
        create_image(size,
                     format,
                     usage | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc,
                     mipmapped);

    immediate_submit([&](vk::CommandBuffer cmd) {
        vkutil::transition_image(cmd, newImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = size;

        cmd.copyBufferToImage(uploadbuffer.buffer,
                              newImage.image,
                              vk::ImageLayout::eTransferDstOptimal,
                              1,
                              reinterpret_cast<const vk::BufferImageCopy*>(&copyRegion));

        vkutil::transition_image(
            cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    });

    destroy_buffer(uploadbuffer);
    return newImage;
}

void VulkanDeviceContext::destroy_image(const AllocatedImage& image)
{
    if (_device == VK_NULL_HANDLE || _allocator == VK_NULL_HANDLE) {
        return;
    }

    if (image.imageView) {
        _device.destroyImageView(image.imageView, nullptr);
    }

    if (image.image && image.allocation != VK_NULL_HANDLE) {
        vmaDestroyImage(_allocator, static_cast<VkImage>(image.image), image.allocation);
    }
}

AllocatedBuffer VulkanDeviceContext::create_buffer(const luna::BufferDesc& desc, const void* initialData)
{
    luna::BufferUsage effectiveUsage = desc.usage;
    if (initialData != nullptr) {
        effectiveUsage = effectiveUsage | luna::BufferUsage::TransferDst;
    }

    AllocatedBuffer buffer = create_buffer(
        static_cast<size_t>(desc.size), to_vulkan_buffer_usage(effectiveUsage), to_vma_memory_usage(desc.memoryUsage));

    if (initialData == nullptr || desc.size == 0) {
        return buffer;
    }

    if (buffer.info.pMappedData != nullptr) {
        std::memcpy(buffer.info.pMappedData, initialData, static_cast<size_t>(desc.size));
        return buffer;
    }

    if (has_active_upload_batch()) {
        vk::DeviceSize stagingOffset = 0;
        const AllocatedBuffer stagingBuffer =
            acquire_upload_staging_buffer(static_cast<size_t>(desc.size), 4, &stagingOffset);
        if (stagingBuffer.buffer == VK_NULL_HANDLE || stagingBuffer.info.pMappedData == nullptr) {
            destroy_buffer(buffer);
            return {};
        }

        std::memcpy(static_cast<std::byte*>(stagingBuffer.info.pMappedData) + stagingOffset,
                    initialData,
                    static_cast<size_t>(desc.size));

        vk::BufferCopy copy{};
        copy.srcOffset = stagingOffset;
        copy.dstOffset = 0;
        copy.size = static_cast<vk::DeviceSize>(desc.size);
        m_activeUploadContext.commandBuffer.copyBuffer(stagingBuffer.buffer, buffer.buffer, 1, &copy);
        m_activeUploadContext.frame->_uploadBatchBytes += static_cast<size_t>(desc.size);
        ++m_activeUploadContext.frame->_uploadBatchOps;
        return buffer;
    }

    AllocatedBuffer stagingBuffer =
        create_buffer(static_cast<size_t>(desc.size), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    std::memcpy(stagingBuffer.info.pMappedData, initialData, static_cast<size_t>(desc.size));

    immediate_submit([&](vk::CommandBuffer cmd) {
        vk::BufferCopy copy{};
        copy.srcOffset = 0;
        copy.dstOffset = 0;
        copy.size = static_cast<vk::DeviceSize>(desc.size);
        cmd.copyBuffer(stagingBuffer.buffer, buffer.buffer, 1, &copy);
    });

    destroy_buffer(stagingBuffer);
    return buffer;
}

AllocatedImage VulkanDeviceContext::create_image(const luna::ImageDesc& desc, const void* initialData)
{
    AllocatedImage newImage{};
    const vk::Extent3D extent{desc.width, desc.height, desc.depth};
    const vk::Format format = to_vulkan_format(desc.format);
    if (format == vk::Format::eUndefined) {
        LUNA_CORE_ERROR("create_image rejected unsupported PixelFormat={}", static_cast<uint32_t>(desc.format));
        return newImage;
    }

    const vk::ImageType imageType = to_vulkan_image_type(desc.type);
    const vk::ImageViewType imageViewType = to_vulkan_image_view_type(desc.type);
    const uint32_t layerCount = image_array_layer_count(desc);
    const uint32_t uploadDepth = image_upload_depth(desc);
    const vk::ImageAspectFlags aspectFlags =
        luna::is_depth_format(desc.format) ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor;

    vk::ImageUsageFlags usage = to_vulkan_image_usage(desc.usage);
    if (initialData != nullptr) {
        usage |= vk::ImageUsageFlagBits::eTransferDst;
    }
    if (desc.mipLevels > 1) {
        usage |= vk::ImageUsageFlagBits::eTransferSrc;
    }

    newImage.imageFormat = format;
    newImage.imageExtent = extent;

    vk::ImageCreateInfo imgInfo =
        vkinit::image_create_info(format, usage, extent, imageType, desc.mipLevels, layerCount);
    if (desc.type == luna::ImageType::Cube) {
        imgInfo.flags |= vk::ImageCreateFlagBits::eCubeCompatible;
    }

    VmaAllocationCreateInfo allocinfo = {};
    allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkImage rawImage = VK_NULL_HANDLE;
    const VkResult createImageResult = vmaCreateImage(_allocator,
                                                      reinterpret_cast<const VkImageCreateInfo*>(&imgInfo),
                                                      &allocinfo,
                                                      &rawImage,
                                                      &newImage.allocation,
                                                      nullptr);
    if (createImageResult != VK_SUCCESS || rawImage == VK_NULL_HANDLE) {
        LUNA_CORE_ERROR("Failed to create image '{}': {}",
                        desc.debugName.empty() ? "<unnamed>" : std::string(desc.debugName),
                        string_VkResult(createImageResult));
        return {};
    }
    newImage.image = rawImage;

    const uint32_t viewLayerCount =
        (desc.type == luna::ImageType::Image2DArray || desc.type == luna::ImageType::Cube) ? desc.arrayLayers : 1u;
    vk::ImageViewCreateInfo viewInfo =
        vkinit::imageview_create_info(format, newImage.image, aspectFlags, imageViewType, desc.mipLevels, viewLayerCount);
    const vk::Result createViewResult = _device.createImageView(&viewInfo, nullptr, &newImage.imageView);
    if (createViewResult != vk::Result::eSuccess || !newImage.imageView) {
        LUNA_CORE_ERROR("Failed to create image view '{}': {}",
                        desc.debugName.empty() ? "<unnamed>" : std::string(desc.debugName),
                        vk::to_string(createViewResult));
        vmaDestroyImage(_allocator, static_cast<VkImage>(newImage.image), newImage.allocation);
        return {};
    }

    if (initialData == nullptr) {
        return newImage;
    }

    const size_t dataSize = image_base_level_data_size(desc);
    if (dataSize == 0) {
        LUNA_CORE_ERROR("Initial image data size is zero for '{}'",
                        desc.debugName.empty() ? "<unnamed>" : std::string(desc.debugName));
        destroy_image(newImage);
        return {};
    }

    if (has_active_upload_batch()) {
        vk::DeviceSize stagingOffset = 0;
        const AllocatedBuffer stagingBuffer = acquire_upload_staging_buffer(dataSize, 4, &stagingOffset);
        if (stagingBuffer.buffer == VK_NULL_HANDLE || stagingBuffer.info.pMappedData == nullptr) {
            destroy_image(newImage);
            return {};
        }

        std::memcpy(static_cast<std::byte*>(stagingBuffer.info.pMappedData) + stagingOffset, initialData, dataSize);

        vk::CommandBuffer cmd = m_activeUploadContext.commandBuffer;
        transition_image_subresource(cmd,
                                     newImage.image,
                                     aspectFlags,
                                     vk::ImageLayout::eUndefined,
                                     vk::ImageLayout::eTransferDstOptimal,
                                     0,
                                     desc.mipLevels,
                                     0,
                                     viewLayerCount);

        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset = static_cast<VkDeviceSize>(stagingOffset);
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;
        copyRegion.imageSubresource.aspectMask = static_cast<VkImageAspectFlags>(aspectFlags);
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = viewLayerCount;
        copyRegion.imageExtent = {desc.width, desc.height, uploadDepth};

        cmd.copyBufferToImage(stagingBuffer.buffer,
                              newImage.image,
                              vk::ImageLayout::eTransferDstOptimal,
                              1,
                              reinterpret_cast<const vk::BufferImageCopy*>(&copyRegion));

        if (desc.mipLevels > 1 && !luna::is_depth_format(desc.format)) {
            int32_t mipWidth = static_cast<int32_t>(desc.width);
            int32_t mipHeight = static_cast<int32_t>(desc.height);
            int32_t mipDepth = static_cast<int32_t>(uploadDepth);

            for (uint32_t mipLevel = 1; mipLevel < desc.mipLevels; ++mipLevel) {
                transition_image_subresource(cmd,
                                             newImage.image,
                                             aspectFlags,
                                             vk::ImageLayout::eTransferDstOptimal,
                                             vk::ImageLayout::eTransferSrcOptimal,
                                             mipLevel - 1,
                                             1,
                                             0,
                                             viewLayerCount);

                vk::ImageBlit2 blitRegion{};
                blitRegion.srcOffsets[1] = vk::Offset3D{mipWidth, mipHeight, mipDepth};
                blitRegion.dstOffsets[1] = vk::Offset3D{std::max(1, mipWidth / 2),
                                                        std::max(1, mipHeight / 2),
                                                        desc.type == luna::ImageType::Image3D ? std::max(1, mipDepth / 2) : 1};
                blitRegion.srcSubresource.aspectMask = aspectFlags;
                blitRegion.srcSubresource.mipLevel = mipLevel - 1;
                blitRegion.srcSubresource.baseArrayLayer = 0;
                blitRegion.srcSubresource.layerCount = viewLayerCount;
                blitRegion.dstSubresource.aspectMask = aspectFlags;
                blitRegion.dstSubresource.mipLevel = mipLevel;
                blitRegion.dstSubresource.baseArrayLayer = 0;
                blitRegion.dstSubresource.layerCount = viewLayerCount;

                vk::BlitImageInfo2 blitInfo{};
                blitInfo.srcImage = newImage.image;
                blitInfo.srcImageLayout = vk::ImageLayout::eTransferSrcOptimal;
                blitInfo.dstImage = newImage.image;
                blitInfo.dstImageLayout = vk::ImageLayout::eTransferDstOptimal;
                blitInfo.filter = vk::Filter::eLinear;
                blitInfo.regionCount = 1;
                blitInfo.pRegions = &blitRegion;
                cmd.blitImage2(&blitInfo);

                transition_image_subresource(cmd,
                                             newImage.image,
                                             aspectFlags,
                                             vk::ImageLayout::eTransferSrcOptimal,
                                             vk::ImageLayout::eShaderReadOnlyOptimal,
                                             mipLevel - 1,
                                             1,
                                             0,
                                             viewLayerCount);

                mipWidth = std::max(1, mipWidth / 2);
                mipHeight = std::max(1, mipHeight / 2);
                if (desc.type == luna::ImageType::Image3D) {
                    mipDepth = std::max(1, mipDepth / 2);
                }
            }

            transition_image_subresource(cmd,
                                         newImage.image,
                                         aspectFlags,
                                         vk::ImageLayout::eTransferDstOptimal,
                                         vk::ImageLayout::eShaderReadOnlyOptimal,
                                         desc.mipLevels - 1,
                                         1,
                                         0,
                                         viewLayerCount);
        } else {
            transition_image_subresource(cmd,
                                         newImage.image,
                                         aspectFlags,
                                         vk::ImageLayout::eTransferDstOptimal,
                                         vk::ImageLayout::eShaderReadOnlyOptimal,
                                         0,
                                         desc.mipLevels,
                                         0,
                                         viewLayerCount);
        }

        m_activeUploadContext.frame->_uploadBatchBytes += dataSize;
        ++m_activeUploadContext.frame->_uploadBatchOps;
        return newImage;
    }

    AllocatedBuffer uploadBuffer =
        create_buffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    std::memcpy(uploadBuffer.info.pMappedData, initialData, dataSize);

    immediate_submit([&](vk::CommandBuffer cmd) {
        transition_image_subresource(cmd,
                                     newImage.image,
                                     aspectFlags,
                                     vk::ImageLayout::eUndefined,
                                     vk::ImageLayout::eTransferDstOptimal,
                                     0,
                                     desc.mipLevels,
                                     0,
                                     viewLayerCount);

        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;
        copyRegion.imageSubresource.aspectMask = static_cast<VkImageAspectFlags>(aspectFlags);
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = viewLayerCount;
        copyRegion.imageExtent = {desc.width, desc.height, uploadDepth};

        cmd.copyBufferToImage(uploadBuffer.buffer,
                              newImage.image,
                              vk::ImageLayout::eTransferDstOptimal,
                              1,
                              reinterpret_cast<const vk::BufferImageCopy*>(&copyRegion));

        if (desc.mipLevels > 1 && !luna::is_depth_format(desc.format)) {
            int32_t mipWidth = static_cast<int32_t>(desc.width);
            int32_t mipHeight = static_cast<int32_t>(desc.height);
            int32_t mipDepth = static_cast<int32_t>(uploadDepth);

            for (uint32_t mipLevel = 1; mipLevel < desc.mipLevels; ++mipLevel) {
                transition_image_subresource(cmd,
                                             newImage.image,
                                             aspectFlags,
                                             vk::ImageLayout::eTransferDstOptimal,
                                             vk::ImageLayout::eTransferSrcOptimal,
                                             mipLevel - 1,
                                             1,
                                             0,
                                             viewLayerCount);

                vk::ImageBlit2 blitRegion{};
                blitRegion.srcOffsets[1] = vk::Offset3D{mipWidth, mipHeight, mipDepth};
                blitRegion.dstOffsets[1] = vk::Offset3D{std::max(1, mipWidth / 2),
                                                        std::max(1, mipHeight / 2),
                                                        desc.type == luna::ImageType::Image3D ? std::max(1, mipDepth / 2) : 1};
                blitRegion.srcSubresource.aspectMask = aspectFlags;
                blitRegion.srcSubresource.mipLevel = mipLevel - 1;
                blitRegion.srcSubresource.baseArrayLayer = 0;
                blitRegion.srcSubresource.layerCount = viewLayerCount;
                blitRegion.dstSubresource.aspectMask = aspectFlags;
                blitRegion.dstSubresource.mipLevel = mipLevel;
                blitRegion.dstSubresource.baseArrayLayer = 0;
                blitRegion.dstSubresource.layerCount = viewLayerCount;

                vk::BlitImageInfo2 blitInfo{};
                blitInfo.srcImage = newImage.image;
                blitInfo.srcImageLayout = vk::ImageLayout::eTransferSrcOptimal;
                blitInfo.dstImage = newImage.image;
                blitInfo.dstImageLayout = vk::ImageLayout::eTransferDstOptimal;
                blitInfo.filter = vk::Filter::eLinear;
                blitInfo.regionCount = 1;
                blitInfo.pRegions = &blitRegion;
                cmd.blitImage2(&blitInfo);

                transition_image_subresource(cmd,
                                             newImage.image,
                                             aspectFlags,
                                             vk::ImageLayout::eTransferSrcOptimal,
                                             vk::ImageLayout::eShaderReadOnlyOptimal,
                                             mipLevel - 1,
                                             1,
                                             0,
                                             viewLayerCount);

                mipWidth = std::max(1, mipWidth / 2);
                mipHeight = std::max(1, mipHeight / 2);
                if (desc.type == luna::ImageType::Image3D) {
                    mipDepth = std::max(1, mipDepth / 2);
                }
            }

            transition_image_subresource(cmd,
                                         newImage.image,
                                         aspectFlags,
                                         vk::ImageLayout::eTransferDstOptimal,
                                         vk::ImageLayout::eShaderReadOnlyOptimal,
                                         desc.mipLevels - 1,
                                         1,
                                         0,
                                         viewLayerCount);
            return;
        }

        transition_image_subresource(cmd,
                                     newImage.image,
                                     aspectFlags,
                                     vk::ImageLayout::eTransferDstOptimal,
                                     vk::ImageLayout::eShaderReadOnlyOptimal,
                                     0,
                                     desc.mipLevels,
                                     0,
                                     viewLayerCount);
    });

    destroy_buffer(uploadBuffer);
    return newImage;
}

vk::Sampler VulkanDeviceContext::create_sampler(const luna::SamplerDesc& desc, vk::SamplerCreateInfo* outCreateInfo)
{
    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter = to_vulkan_filter(desc.magFilter);
    samplerInfo.minFilter = to_vulkan_filter(desc.minFilter);
    samplerInfo.mipmapMode = to_vulkan_mipmap_mode(desc.mipmapMode);
    samplerInfo.addressModeU = to_vulkan_sampler_address_mode(desc.addressModeU);
    samplerInfo.addressModeV = to_vulkan_sampler_address_mode(desc.addressModeV);
    samplerInfo.addressModeW = to_vulkan_sampler_address_mode(desc.addressModeW);
    samplerInfo.mipLodBias = desc.mipLodBias;
    samplerInfo.minLod = desc.minLod;
    samplerInfo.maxLod = desc.maxLod;
    samplerInfo.anisotropyEnable = desc.anisotropyEnable;
    samplerInfo.maxAnisotropy = desc.anisotropyEnable ? desc.maxAnisotropy : 1.0f;
    samplerInfo.compareEnable = desc.compareEnable;
    samplerInfo.compareOp = to_vulkan_compare_op(desc.compareOp);
    samplerInfo.borderColor = to_vulkan_border_color(desc.borderColor);

    if (outCreateInfo != nullptr) {
        *outCreateInfo = samplerInfo;
    }

    vk::Sampler sampler{};
    const vk::Result result = _device.createSampler(&samplerInfo, nullptr, &sampler);
    if (result != vk::Result::eSuccess) {
        LUNA_CORE_ERROR("Failed to create sampler '{}': {}",
                        desc.debugName.empty() ? std::string("<unnamed>") : std::string(desc.debugName),
                        vk::to_string(result));
        return {};
    }
    return sampler;
}

AllocatedBuffer VulkanDeviceContext::create_buffer(size_t allocSize,
                                                   vk::BufferUsageFlags usage,
                                                   VmaMemoryUsage memoryUsage)
{
    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;
    bufferInfo.usage = static_cast<VkBufferUsageFlags>(usage);

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = memoryUsage;
    vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    AllocatedBuffer newBuffer;

    VkBuffer rawBuffer = VK_NULL_HANDLE;
    VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaallocInfo, &rawBuffer, &newBuffer.allocation, &newBuffer.info));
    newBuffer.buffer = rawBuffer;

    return newBuffer;
}

void VulkanDeviceContext::destroy_buffer(const AllocatedBuffer& buffer)
{
    if (_allocator == VK_NULL_HANDLE || buffer.buffer == VK_NULL_HANDLE || buffer.allocation == VK_NULL_HANDLE) {
        return;
    }

    vmaDestroyBuffer(_allocator, static_cast<VkBuffer>(buffer.buffer), buffer.allocation);
}

bool VulkanDeviceContext::uploadBufferData(const AllocatedBuffer& buffer,
                                           const void* data,
                                           size_t size,
                                           size_t offset)
{
    if (buffer.buffer == VK_NULL_HANDLE || buffer.allocation == VK_NULL_HANDLE || data == nullptr || size == 0) {
        LUNA_CORE_ERROR("uploadBufferData requires a valid buffer and non-empty source data");
        return false;
    }

    if (offset + size > buffer.info.size) {
        LUNA_CORE_ERROR("uploadBufferData out of range: offset={} size={} capacity={}", offset, size, buffer.info.size);
        return false;
    }

    if (buffer.info.pMappedData != nullptr) {
        std::memcpy(static_cast<std::byte*>(buffer.info.pMappedData) + offset, data, size);
        return true;
    }

    if (has_active_upload_batch()) {
        vk::DeviceSize stagingOffset = 0;
        const AllocatedBuffer stagingBuffer = acquire_upload_staging_buffer(size, 4, &stagingOffset);
        if (stagingBuffer.buffer == VK_NULL_HANDLE || stagingBuffer.info.pMappedData == nullptr) {
            return false;
        }

        std::memcpy(static_cast<std::byte*>(stagingBuffer.info.pMappedData) + stagingOffset, data, size);

        vk::BufferCopy copy{};
        copy.srcOffset = stagingOffset;
        copy.dstOffset = static_cast<vk::DeviceSize>(offset);
        copy.size = static_cast<vk::DeviceSize>(size);
        m_activeUploadContext.commandBuffer.copyBuffer(stagingBuffer.buffer, buffer.buffer, 1, &copy);
        m_activeUploadContext.frame->_uploadBatchBytes += size;
        ++m_activeUploadContext.frame->_uploadBatchOps;
        return true;
    }

    const luna::BufferDesc stagingDesc{
        .size = size,
        .usage = luna::BufferUsage::TransferSrc,
        .memoryUsage = luna::MemoryUsage::Upload,
        .debugName = "RHIUploadStagingBuffer",
    };
    AllocatedBuffer stagingBuffer = create_buffer(stagingDesc, data);

    immediate_submit([&](vk::CommandBuffer cmd) {
        vk::BufferCopy copy{};
        copy.srcOffset = 0;
        copy.dstOffset = static_cast<vk::DeviceSize>(offset);
        copy.size = static_cast<vk::DeviceSize>(size);
        cmd.copyBuffer(stagingBuffer.buffer, buffer.buffer, 1, &copy);
    });

    destroy_buffer(stagingBuffer);
    return true;
}

void VulkanDeviceContext::immediate_submit(const std::function<void(vk::CommandBuffer cmd)>& function)
{
    VK_CHECK(_device.resetFences(1, &_immContext._fence));
    VK_CHECK(_immContext._commandBuffer.reset({}));

    vk::CommandBuffer cmd = _immContext._commandBuffer;
    vk::CommandBufferBeginInfo cmdBeginInfo =
        vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(cmd.begin(&cmdBeginInfo));

    function(cmd);

    VK_CHECK(cmd.end());

    vk::CommandBufferSubmitInfo cmdInfo = vkinit::command_buffer_submit_info(cmd);
    vk::SubmitInfo2 submit = vkinit::submit_info(&cmdInfo, nullptr, nullptr);
    VK_CHECK(_graphicsQueue.submit2(1, &submit, _immContext._fence));
    VK_CHECK(_device.waitForFences(1, &_immContext._fence, VK_TRUE, 1'000'000'000));
}
