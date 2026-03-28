#include "vk_engine.h"

#define GLFW_INCLUDE_NONE
#include "VkBootstrap.h"
#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_types.h"

#include <cmath>
#include <GLFW/glfw3.h>

VulkanEngine* loadedEngine = nullptr;

#ifndef NDEBUG
constexpr bool bUseValidationLayers = true;
#else
constexpr bool bUseValidationLayers = false;
#endif

namespace {

template <typename T>
void logVkbError(const char* step, const vkb::Result<T>& result)
{
    LUNA_CORE_ERROR("{} failed: {}", step, result.error().message());
    for (const std::string& reason : result.full_error().detailed_failure_reasons) {
        LUNA_CORE_ERROR("  {}", reason);
    }
}

} // namespace

VulkanEngine& VulkanEngine::Get()
{
    return *loadedEngine;
}

bool VulkanEngine::init(luna::Window& window)
{
    if (loadedEngine != nullptr) {
        LUNA_CORE_ERROR("VulkanEngine already initialized");
        return false;
    }

    loadedEngine = this;
    LUNA_CORE_INFO("Initializing Vulkan engine");

    _window = static_cast<GLFWwindow*>(window.getNativeWindow());
    if (_window == nullptr) {
        LUNA_CORE_ERROR("Failed to acquire native GLFW window");
        loadedEngine = nullptr;
        return false;
    }

    _windowExtent = {window.getWidth(), window.getHeight()};
    LUNA_CORE_INFO("Created GLFW window: {}x{}", _windowExtent.width, _windowExtent.height);

    if (!init_vulkan() || !init_swapchain() || !init_commands() || !init_sync_structures()) {
        cleanup();
        return false;
    }

    _isInitialized = true;
    LUNA_CORE_INFO("Vulkan engine initialized");
    return true;
}

void VulkanEngine::cleanup()
{
    LUNA_CORE_INFO("Cleaning up Vulkan engine");

    if (_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(_device);

        for (int i = 0; i < FRAME_OVERLAP; i++) {
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
        }
    }

    destroy_swapchain();

    if (_instance != VK_NULL_HANDLE && _surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        _surface = VK_NULL_HANDLE;
    }

    if (_device != VK_NULL_HANDLE) {
        vkDestroyDevice(_device, nullptr);
        _device = VK_NULL_HANDLE;
    }

    if (_instance != VK_NULL_HANDLE) {
        if (_debug_messenger != VK_NULL_HANDLE) {
            vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
            _debug_messenger = VK_NULL_HANDLE;
        }

        vkDestroyInstance(_instance, nullptr);
        _instance = VK_NULL_HANDLE;
    }

    _window = nullptr;
    _chosenGPU = VK_NULL_HANDLE;
    _graphicsQueue = VK_NULL_HANDLE;
    _graphicsQueueFamily = 0;
    _swapchain = VK_NULL_HANDLE;
    _swapchainImageFormat = VK_FORMAT_UNDEFINED;
    _swapchainExtent = {};
    _isInitialized = false;
    loadedEngine = nullptr;

    LUNA_CORE_INFO("Vulkan engine cleanup complete");
}

void VulkanEngine::draw()
{
    VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence, true, 1'000'000'000));
    VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._renderFence));

    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(
        _device, _swapchain, 1'000'000'000, get_current_frame()._swapchainSemaphore, nullptr, &swapchainImageIndex));

    VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;

    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    VkCommandBufferBeginInfo cmdBeginInfo =
        vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    vkutil::transition_image(
        cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    VkClearColorValue clearValue;
    const float flash = std::abs(std::sin(_frameNumber / 120.0f));
    clearValue = {{0.0f, 0.0f, flash, 1.0f}};

    VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

    vkCmdClearColorImage(
        cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

    vkutil::transition_image(
        cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                                                   get_current_frame()._swapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo =
        vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame()._renderSemaphore);

    VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, get_current_frame()._renderFence));

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &_swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &get_current_frame()._renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

    _frameNumber++;
}

bool VulkanEngine::init_vulkan()
{
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    if (glfwExtensions == nullptr || glfwExtensionCount == 0) {
        LUNA_CORE_ERROR("Failed to query GLFW Vulkan instance extensions");
        return false;
    }

    vkb::InstanceBuilder builder;
    auto inst_ret = builder.set_app_name("Example Vulkan Application")
                        .request_validation_layers(bUseValidationLayers)
                        .use_default_debug_messenger()
                        .enable_extensions(glfwExtensionCount, glfwExtensions)
                        .require_api_version(1, 3, 0)
                        .build();
    if (!inst_ret) {
        logVkbError("Vulkan instance creation", inst_ret);
        return false;
    }

    vkb::Instance vkb_inst = inst_ret.value();
    _instance = vkb_inst.instance;
    _debug_messenger = vkb_inst.debug_messenger;

    if (_window == nullptr) {
        LUNA_CORE_ERROR("No native GLFW window available for Vulkan surface creation");
        return false;
    }

    const VkResult surfaceResult = glfwCreateWindowSurface(_instance, _window, nullptr, &_surface);
    if (surfaceResult != VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to create Vulkan surface: {}", string_VkResult(surfaceResult));
        return false;
    }

    VkPhysicalDeviceVulkan13Features features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features.dynamicRendering = true;
    features.synchronization2 = true;

    VkPhysicalDeviceVulkan12Features features12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    vkb::PhysicalDeviceSelector selector{vkb_inst};
    auto physical_device_ret = selector.set_minimum_version(1, 3)
                                   .set_required_features_13(features)
                                   .set_required_features_12(features12)
                                   .set_surface(_surface)
                                   .select();
    if (!physical_device_ret) {
        logVkbError("Physical device selection", physical_device_ret);
        return false;
    }

    vkb::PhysicalDevice physicalDevice = physical_device_ret.value();

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
    LUNA_CORE_INFO("Selected GPU: {}", gpuProperties.deviceName);
    return true;
}

bool VulkanEngine::create_swapchain(uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder swapchainBuilder{_chosenGPU, _device, _surface};

    _swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    auto swapchain_ret = swapchainBuilder
                             .set_desired_format(VkSurfaceFormatKHR{.format = _swapchainImageFormat,
                                                                    .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
                             .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
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

    auto imagesRet = vkbSwapchain.get_images();
    if (!imagesRet) {
        logVkbError("Swapchain image fetch", imagesRet);
        return false;
    }
    _swapchainImages = imagesRet.value();

    auto imageViewsRet = vkbSwapchain.get_image_views();
    if (!imageViewsRet) {
        logVkbError("Swapchain image view fetch", imageViewsRet);
        return false;
    }
    _swapchainImageViews = imageViewsRet.value();

    LUNA_CORE_INFO("Created swapchain: {}x{}, images={}",
                   _swapchainExtent.width,
                   _swapchainExtent.height,
                   _swapchainImages.size());
    return true;
}

void VulkanEngine::destroy_swapchain()
{
    if (_swapchain == VK_NULL_HANDLE) {
        return;
    }

    for (size_t i = 0; i < _swapchainImageViews.size(); i++) {
        vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
    }

    _swapchainImageViews.clear();
    _swapchainImages.clear();

    vkDestroySwapchainKHR(_device, _swapchain, nullptr);
    _swapchain = VK_NULL_HANDLE;
}

bool VulkanEngine::init_swapchain()
{
    return create_swapchain(_windowExtent.width, _windowExtent.height);
}

bool VulkanEngine::init_commands()
{
    VkCommandPoolCreateInfo commandPoolInfo =
        vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        const VkResult commandPoolResult =
            vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool);
        if (commandPoolResult != VK_SUCCESS) {
            LUNA_CORE_ERROR("Failed to create command pool: {}", string_VkResult(commandPoolResult));
            return false;
        }

        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

        const VkResult commandBufferResult =
            vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer);
        if (commandBufferResult != VK_SUCCESS) {
            LUNA_CORE_ERROR("Failed to allocate command buffer: {}", string_VkResult(commandBufferResult));
            return false;
        }
    }

    return true;
}

bool VulkanEngine::init_sync_structures()
{
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        const VkResult fenceResult = vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence);
        if (fenceResult != VK_SUCCESS) {
            LUNA_CORE_ERROR("Failed to create fence: {}", string_VkResult(fenceResult));
            return false;
        }

        const VkResult swapchainSemaphoreResult =
            vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._swapchainSemaphore);
        if (swapchainSemaphoreResult != VK_SUCCESS) {
            LUNA_CORE_ERROR("Failed to create swapchain semaphore: {}", string_VkResult(swapchainSemaphoreResult));
            return false;
        }

        const VkResult renderSemaphoreResult =
            vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore);
        if (renderSemaphoreResult != VK_SUCCESS) {
            LUNA_CORE_ERROR("Failed to create render semaphore: {}", string_VkResult(renderSemaphoreResult));
            return false;
        }
    }

    return true;
}
