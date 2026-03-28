#include "vk_engine.h"

#define GLFW_INCLUDE_NONE
#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"
#include "vk_types.h"
#include "VkBootstrap.h"
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <cmath>

#include <GLFW/glfw3.h>

VulkanEngine* loadedEngine = nullptr;

#ifndef NDEBUG
constexpr bool bUseValidationLayers = true;
#else
constexpr bool bUseValidationLayers = false;
#endif

namespace {

template <typename T> void logVkbError(const char* step, const vkb::Result<T>& result)
{
    LUNA_CORE_ERROR("{} failed: {}", step, result.error().message());
    for (const std::string& reason : result.full_error().detailed_failure_reasons) {
        LUNA_CORE_ERROR("  {}", reason);
    }
}

void logSwapchainResult(const char* step, VkResult result, VkExtent2D swapchainExtent, VkExtent2D framebufferExtent)
{
    LUNA_CORE_WARN("{} returned {}. swapchain={}x{}, framebuffer={}x{}",
                   step,
                   string_VkResult(result),
                   swapchainExtent.width,
                   swapchainExtent.height,
                   framebufferExtent.width,
                   framebufferExtent.height);
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
    const VkExtent2D framebufferExtent = get_framebuffer_extent();
    LUNA_CORE_INFO("Created GLFW window: logical={}x{}, framebuffer={}x{}",
                   _windowExtent.width,
                   _windowExtent.height,
                   framebufferExtent.width,
                   framebufferExtent.height);

    if (!init_vulkan() || !init_swapchain() || !init_commands() || !init_sync_structures() || !init_descriptors() ||
        !init_pipelines()) {
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

            _frames[i]._mainCommandBuffer = VK_NULL_HANDLE;
            _frames[i]._deletionQueue.flush();
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
    _swapchainImageFormat = VK_FORMAT_UNDEFINED;
    _swapchainExtent = {};
    _allocator = VK_NULL_HANDLE;
    _drawImageDescriptors = VK_NULL_HANDLE;
    _drawImageDescriptorLayout = VK_NULL_HANDLE;
    _gradientPipeline = VK_NULL_HANDLE;
    _gradientPipelineLayout = VK_NULL_HANDLE;
    _isInitialized = false;
    loadedEngine = nullptr;

    LUNA_CORE_INFO("Vulkan engine cleanup complete");
}

void VulkanEngine::draw()
{
    if (_device == VK_NULL_HANDLE || _swapchain == VK_NULL_HANDLE) {
        LUNA_CORE_WARN("Skipping frame because Vulkan device or swapchain is not ready");
        return;
    }

    if (_frameNumber == 0) {
        LUNA_CORE_INFO("Starting render loop: swapchain={}x{}, draw={}x{}",
                       _swapchainExtent.width,
                       _swapchainExtent.height,
                       _drawImage.imageExtent.width,
                       _drawImage.imageExtent.height);
    }

    VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence, true, 1'000'000'000));
    get_current_frame()._deletionQueue.flush();

    VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._renderFence));

    uint32_t swapchainImageIndex = 0;
    bool recreateAfterPresent = false;
    const VkResult acquireResult = vkAcquireNextImageKHR(
        _device, _swapchain, 1'000'000'000, get_current_frame()._swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        logSwapchainResult("vkAcquireNextImageKHR", acquireResult, _swapchainExtent, get_framebuffer_extent());
        recreate_swapchain();
        return;
    }
    if (acquireResult == VK_SUBOPTIMAL_KHR) {
        recreateAfterPresent = true;
        logSwapchainResult("vkAcquireNextImageKHR", acquireResult, _swapchainExtent, get_framebuffer_extent());
    } else if (acquireResult != VK_SUCCESS) {
        LUNA_CORE_FATAL("Vulkan call failed: vkAcquireNextImageKHR returned {}", string_VkResult(acquireResult));
        std::abort();
    }

    VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;

    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    VkCommandBufferBeginInfo cmdBeginInfo =
        vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    _drawExtent.width = _drawImage.imageExtent.width;
    _drawExtent.height = _drawImage.imageExtent.height;

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // transition our main draw image into general layout so we can write into it
    // we will overwrite it all so we dont care about what was the older layout
    vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    draw_background(cmd);

    // transition the draw image and the swapchain image into their correct transfer layouts
    vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transition_image(
        cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // execute a copy from the draw image into the swapchain
    vkutil::copy_image_to_image(
        cmd, _drawImage.image, _swapchainImages[swapchainImageIndex], _drawExtent, _swapchainExtent);

    // set swapchain image layout to Present so we can show it on the screen
    vkutil::transition_image(cmd,
                             _swapchainImages[swapchainImageIndex],
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // finalize the command buffer (we can no longer add commands, but it can now be executed)
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

    const VkResult presentResult = vkQueuePresentKHR(_graphicsQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        logSwapchainResult("vkQueuePresentKHR", presentResult, _swapchainExtent, get_framebuffer_extent());
        recreate_swapchain();
    } else if (presentResult != VK_SUCCESS) {
        LUNA_CORE_FATAL("Vulkan call failed: vkQueuePresentKHR returned {}", string_VkResult(presentResult));
        std::abort();
    } else if (recreateAfterPresent) {
        recreate_swapchain();
    }

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

bool VulkanEngine::create_swapchain(uint32_t width, uint32_t height)
{
    LUNA_CORE_INFO("Creating swapchain for framebuffer {}x{}", width, height);
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

    return create_draw_resources(_swapchainExtent);
}

void VulkanEngine::draw_background(VkCommandBuffer cmd)
{
    // // make a clear-color from frame number. This will flash with a 120 frame period.
    // VkClearColorValue clearValue;
    // float flash = std::abs(std::sin(_frameNumber / 120.f));
    // clearValue = {{0.0f, 0.0f, flash, 1.0f}};

    // VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

    // // clear image
    // vkCmdClearColorImage(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
    // bind the gradient drawing compute pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipeline);

    // bind the descriptor set containing the draw image for the compute pipeline
    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipelineLayout, 0, 1, &_drawImageDescriptors, 0, nullptr);

    // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
    vkCmdDispatch(cmd, std::ceil(_drawExtent.width / 16.0), std::ceil(_drawExtent.height / 16.0), 1);
}

void VulkanEngine::destroy_swapchain()
{
    destroy_draw_resources();

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
    _swapchainExtent = {};
    _swapchainImageFormat = VK_FORMAT_UNDEFINED;
}

bool VulkanEngine::init_swapchain()
{
    const VkExtent2D framebufferExtent = get_framebuffer_extent();
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

    LUNA_CORE_INFO("Initialized command pools and command buffers for {} frames", FRAME_OVERLAP);
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

    LUNA_CORE_INFO("Initialized synchronization primitives for {} frames", FRAME_OVERLAP);
    return true;
}

bool VulkanEngine::init_descriptors()
{
    // create a descriptor pool that will hold 10 sets with 1 image each
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}};

    globalDescriptorAllocator.init_pool(_device, 10, sizes);

    // make the descriptor set layout for our compute draw
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        _drawImageDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    // allocate a descriptor set for our draw image
    _drawImageDescriptors = globalDescriptorAllocator.allocate(_device, _drawImageDescriptorLayout);

    update_draw_image_descriptors();

    // make sure both the descriptor allocator and the new layout get cleaned up properly
    _mainDeletionQueue.push_function([&]() {
        globalDescriptorAllocator.destroy_pool(_device);

        vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout, nullptr);
    });
    LUNA_CORE_INFO("Initialized draw image descriptors");
    return true;
}

bool VulkanEngine::init_pipelines()
{
    return init_background_pipelines();
}

bool VulkanEngine::init_background_pipelines()
{
    VkPipelineLayoutCreateInfo computeLayout{};
    computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayout.pNext = nullptr;
    computeLayout.pSetLayouts = &_drawImageDescriptorLayout;
    computeLayout.setLayoutCount = 1;

    VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr, &_gradientPipelineLayout));

    VkShaderModule computeDrawShader;
    if (!vkutil::load_shader_module("../Shaders/Internal/gradient.spv", _device, &computeDrawShader)) {
        LUNA_CORE_ERROR("Failed to load shader module");
        return false;
    }

    VkPipelineShaderStageCreateInfo stageinfo{};
    stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageinfo.pNext = nullptr;
    stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageinfo.module = computeDrawShader;
    stageinfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.layout = _gradientPipelineLayout;
    computePipelineCreateInfo.stage = stageinfo;

    VK_CHECK(
        vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &_gradientPipeline));

    vkDestroyShaderModule(_device, computeDrawShader, nullptr);

    _mainDeletionQueue.push_function([&]() {
        vkDestroyPipelineLayout(_device, _gradientPipelineLayout, nullptr);
        vkDestroyPipeline(_device, _gradientPipeline, nullptr);
    });

    LUNA_CORE_INFO("Initialized background compute pipeline");
    return true;
}

bool VulkanEngine::recreate_swapchain()
{
    const VkExtent2D framebufferExtent = get_framebuffer_extent();
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

    if (_drawImageDescriptors != VK_NULL_HANDLE) {
        update_draw_image_descriptors();
    }

    return true;
}

bool VulkanEngine::create_draw_resources(VkExtent2D extent)
{
    if (extent.width == 0 || extent.height == 0) {
        LUNA_CORE_ERROR("Cannot create draw resources for extent {}x{}", extent.width, extent.height);
        return false;
    }

    const VkExtent3D drawImageExtent = {extent.width, extent.height, 1};

    _drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    _drawImage.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const VkImageCreateInfo imageInfo =
        vkinit::image_create_info(_drawImage.imageFormat, drawImageUsages, drawImageExtent);

    VmaAllocationCreateInfo allocationInfo = {};
    allocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocationInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    const VkResult imageResult =
        vmaCreateImage(_allocator, &imageInfo, &allocationInfo, &_drawImage.image, &_drawImage.allocation, nullptr);
    if (imageResult != VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to create draw image: {}", string_VkResult(imageResult));
        return false;
    }

    const VkImageViewCreateInfo imageViewInfo =
        vkinit::imageview_create_info(_drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);
    const VkResult imageViewResult = vkCreateImageView(_device, &imageViewInfo, nullptr, &_drawImage.imageView);
    if (imageViewResult != VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to create draw image view: {}", string_VkResult(imageViewResult));
        vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
        _drawImage.image = VK_NULL_HANDLE;
        _drawImage.allocation = VK_NULL_HANDLE;
        return false;
    }

    LUNA_CORE_INFO("Created draw image: {}x{}, format={}",
                   extent.width,
                   extent.height,
                   string_VkFormat(_drawImage.imageFormat));
    return true;
}

void VulkanEngine::destroy_draw_resources()
{
    if (_device == VK_NULL_HANDLE || _allocator == VK_NULL_HANDLE) {
        _drawImage = {};
        return;
    }

    if (_drawImage.imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(_device, _drawImage.imageView, nullptr);
        _drawImage.imageView = VK_NULL_HANDLE;
    }

    if (_drawImage.image != VK_NULL_HANDLE && _drawImage.allocation != VK_NULL_HANDLE) {
        vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
        _drawImage.image = VK_NULL_HANDLE;
        _drawImage.allocation = VK_NULL_HANDLE;
    }

    _drawImage.imageExtent = {};
    _drawImage.imageFormat = VK_FORMAT_UNDEFINED;
}

void VulkanEngine::update_draw_image_descriptors()
{
    if (_device == VK_NULL_HANDLE || _drawImageDescriptors == VK_NULL_HANDLE || _drawImage.imageView == VK_NULL_HANDLE) {
        return;
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo.imageView = _drawImage.imageView;

    VkWriteDescriptorSet drawImageWrite = {};
    drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    drawImageWrite.pNext = nullptr;
    drawImageWrite.dstBinding = 0;
    drawImageWrite.dstSet = _drawImageDescriptors;
    drawImageWrite.descriptorCount = 1;
    drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    drawImageWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(_device, 1, &drawImageWrite, 0, nullptr);
}

VkExtent2D VulkanEngine::get_framebuffer_extent() const
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
