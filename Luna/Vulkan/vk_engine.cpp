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
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
    init_default_data();
    LUNA_CORE_INFO("Vulkan engine initialized");

    return true;
}

void VulkanEngine::init_default_data()
{
    auto loadedMeshes = loadGltfMeshes(this, "../assets/basicmesh.glb");
    if (!loadedMeshes.has_value()) {
        LUNA_CORE_WARN("Falling back to empty mesh set because basicmesh.glb failed to load");
        return;
    }

    testMeshes = std::move(loadedMeshes.value());

    _mainDeletionQueue.push_function([this]() {
        for (auto& mesh : testMeshes) {
            if (!mesh) {
                continue;
            }

            destroy_buffer(mesh->meshBuffers.indexBuffer);
            destroy_buffer(mesh->meshBuffers.vertexBuffer);
        }

        testMeshes.clear();
    });
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

void VulkanEngine::draw(const OverlayRenderFunction& overlayRenderer, const BeforePresentFunction& beforePresent)
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
    vkutil::transition_image(cmd, _drawImage.image, _drawImageLayout, VK_IMAGE_LAYOUT_GENERAL);
    _drawImageLayout = VK_IMAGE_LAYOUT_GENERAL;

    draw_background(cmd);

    vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    _drawImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    draw_geometry(cmd);

    // transition the draw image and the swapchain image into their correct transfer layouts
    vkutil::transition_image(
        cmd, _drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    _drawImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    vkutil::transition_image(cmd,
                             _swapchainImages[swapchainImageIndex],
                             _swapchainImageLayouts[swapchainImageIndex],
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    _swapchainImageLayouts[swapchainImageIndex] = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    // execute a copy from the draw image into the swapchain
    vkutil::copy_image_to_image(
        cmd, _drawImage.image, _swapchainImages[swapchainImageIndex], _drawExtent, _swapchainExtent);

    if (overlayRenderer) {
        vkutil::transition_image(cmd,
                                 _swapchainImages[swapchainImageIndex],
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        _swapchainImageLayouts[swapchainImageIndex] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        overlayRenderer(cmd, _swapchainImageViews[swapchainImageIndex], _swapchainExtent);

        vkutil::transition_image(cmd,
                                 _swapchainImages[swapchainImageIndex],
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    } else {
        vkutil::transition_image(cmd,
                                 _swapchainImages[swapchainImageIndex],
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }
    _swapchainImageLayouts[swapchainImageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                                                   get_current_frame()._swapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo =
        vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame()._renderSemaphore);

    VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, get_current_frame()._renderFence));

    if (beforePresent) {
        beforePresent();
    }

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
    _swapchainImageLayouts.assign(_swapchainImages.size(), VK_IMAGE_LAYOUT_UNDEFINED);

    LUNA_CORE_INFO("Created swapchain: {}x{}, images={}",
                   _swapchainExtent.width,
                   _swapchainExtent.height,
                   _swapchainImages.size());

    return create_draw_resources(_swapchainExtent);
}

void VulkanEngine::draw_background(VkCommandBuffer cmd)
{

    ComputeEffect& effect = backgroundEffects[currentBackgroundEffect];

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipelineLayout, 0, 1, &_drawImageDescriptors, 0, nullptr);

    vkCmdPushConstants(
        cmd, _gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

    vkCmdDispatch(cmd, std::ceil(_drawExtent.width / 16.0), std::ceil(_drawExtent.height / 16.0), 1);
}

void VulkanEngine::draw_geometry(VkCommandBuffer cmd)
{
    // begin a render pass  connected to our draw image
    VkRenderingAttachmentInfo colorAttachment =
        vkinit::attachment_info(_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment =
        vkinit::depth_attachment_info(_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    vkutil::transition_image(
        cmd, _depthImage.image, _depthImageLayout, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    _depthImageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

    VkRenderingInfo renderInfo = vkinit::rendering_info(_drawExtent, &colorAttachment, &depthAttachment);
    vkCmdBeginRendering(cmd, &renderInfo);

    // set dynamic viewport and scissor
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = _drawExtent.width;
    viewport.height = _drawExtent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = _drawExtent.width;
    scissor.extent.height = _drawExtent.height;

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _meshPipeline);

    if (!testMeshes.empty()) {
        const float aspect = _drawExtent.height == 0 ? 1.0f : static_cast<float>(_drawExtent.width) / static_cast<float>(_drawExtent.height);
        glm::mat4 projection = glm::perspectiveRH_ZO(glm::radians(70.0f), aspect, 0.1f, 10000.0f);
        projection[1][1] *= -1.0f;

        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -5.0f));

        const MeshAsset* selectedMesh = nullptr;
        for (const auto& mesh : testMeshes) {
            if (mesh && mesh->name == "Suzanne") {
                selectedMesh = mesh.get();
                break;
            }
        }

        if (selectedMesh == nullptr) {
            for (const auto& mesh : testMeshes) {
                if (mesh) {
                    selectedMesh = mesh.get();
                    break;
                }
            }
        }

        if (selectedMesh != nullptr) {
            GPUDrawPushConstants pushConstants{};
            pushConstants.worldMatrix = projection * view * model;
            pushConstants.vertexBuffer = selectedMesh->meshBuffers.vertexBufferAddress;

            vkCmdPushConstants(
                cmd, _meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);
            vkCmdBindIndexBuffer(cmd, selectedMesh->meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

            for (const auto& surface : selectedMesh->surfaces) {
                vkCmdDrawIndexed(cmd, surface.count, 1, surface.startIndex, 0, 0);
            }
        }
    }

    vkCmdEndRendering(cmd);
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
    _swapchainImageLayouts.clear();

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

    const VkResult uploadCommandPoolResult =
        vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_immContext._commandPool);
    if (uploadCommandPoolResult != VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to create immediate command pool: {}", string_VkResult(uploadCommandPoolResult));
        return false;
    }

    VkCommandBufferAllocateInfo uploadCmdAllocInfo = vkinit::command_buffer_allocate_info(_immContext._commandPool, 1);
    const VkResult uploadCommandBufferResult =
        vkAllocateCommandBuffers(_device, &uploadCmdAllocInfo, &_immContext._commandBuffer);
    if (uploadCommandBufferResult != VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to allocate immediate command buffer: {}", string_VkResult(uploadCommandBufferResult));
        return false;
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

    const VkResult uploadFenceResult = vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immContext._fence);
    if (uploadFenceResult != VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to create immediate submit fence: {}", string_VkResult(uploadFenceResult));
        return false;
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
    init_triangle_pipeline();
    init_mesh_pipeline();
    return init_background_pipelines();
}

void VulkanEngine::init_triangle_pipeline()
{
    VkShaderModule triangleFragShader;
    if (!vkutil::load_shader_module("../Shaders/Internal/colored_triangle.frag.spv", _device, &triangleFragShader)) {
        fmt::print("Error when building the triangle fragment shader module");
    } else {
        fmt::print("Triangle fragment shader succesfully loaded");
    }

    VkShaderModule triangleVertexShader;
    if (!vkutil::load_shader_module("../Shaders/Internal/colored_triangle.vert.spv", _device, &triangleVertexShader)) {
        fmt::print("Error when building the triangle vertex shader module");
    } else {
        fmt::print("Triangle vertex shader succesfully loaded");
    }

    // build the pipeline layout that controls the inputs/outputs of the shader
    // we are not using descriptor sets or other systems yet, so no need to use anything other than empty default
    VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
    VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_trianglePipelineLayout));

    PipelineBuilder pipelineBuilder;

    // use the triangle layout we created
    pipelineBuilder._pipelineLayout = _trianglePipelineLayout;
    // connecting the vertex and pixel shaders to the pipeline
    pipelineBuilder.set_shaders(triangleVertexShader, triangleFragShader);
    // it will draw triangles
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    // filled triangles
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    // no backface culling
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    // no multisampling
    pipelineBuilder.set_multisampling_none();
    // no blending
    pipelineBuilder.disable_blending();
    // no depth testing
    pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL);

    // connect the image format we will draw into, from draw image
    pipelineBuilder.set_color_attachment_format(_drawImage.imageFormat);
    pipelineBuilder.set_depth_format(_depthImage.imageFormat);

    // finally build the pipeline
    _trianglePipeline = pipelineBuilder.build_pipeline(_device);

    // clean structures
    vkDestroyShaderModule(_device, triangleFragShader, nullptr);
    vkDestroyShaderModule(_device, triangleVertexShader, nullptr);

    _mainDeletionQueue.push_function([&]() {
        vkDestroyPipelineLayout(_device, _trianglePipelineLayout, nullptr);
        vkDestroyPipeline(_device, _trianglePipeline, nullptr);
    });
}

void VulkanEngine::init_mesh_pipeline()
{
    VkShaderModule triangleFragShader;
    if (!vkutil::load_shader_module("../Shaders/Internal/colored_triangle.frag.spv", _device, &triangleFragShader)) {
        fmt::print("Error when building the triangle fragment shader module");
    } else {
        fmt::print("Triangle fragment shader succesfully loaded");
    }

    VkShaderModule triangleVertexShader;
    if (!vkutil::load_shader_module(
            "../Shaders/Internal/colored_triangle_mesh.vert.spv", _device, &triangleVertexShader)) {
        fmt::print("Error when building the triangle vertex shader module");
    } else {
        fmt::print("Triangle vertex shader succesfully loaded");
    }

    VkPushConstantRange bufferRange{};
    bufferRange.offset = 0;
    bufferRange.size = sizeof(GPUDrawPushConstants);
    bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
    pipeline_layout_info.pPushConstantRanges = &bufferRange;
    pipeline_layout_info.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_meshPipelineLayout));
    PipelineBuilder pipelineBuilder;

    // use the triangle layout we created
    pipelineBuilder._pipelineLayout = _meshPipelineLayout;
    // connecting the vertex and pixel shaders to the pipeline
    pipelineBuilder.set_shaders(triangleVertexShader, triangleFragShader);
    // it will draw triangles
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    // filled triangles
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    // no backface culling
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    // no multisampling
    pipelineBuilder.set_multisampling_none();
    // no blending
    pipelineBuilder.disable_blending();

    pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL);

    // connect the image format we will draw into, from draw image
    pipelineBuilder.set_color_attachment_format(_drawImage.imageFormat);
    pipelineBuilder.set_depth_format(_depthImage.imageFormat);

    // finally build the pipeline
    _meshPipeline = pipelineBuilder.build_pipeline(_device);

    // clean structures
    vkDestroyShaderModule(_device, triangleFragShader, nullptr);
    vkDestroyShaderModule(_device, triangleVertexShader, nullptr);

    _mainDeletionQueue.push_function([&]() {
        vkDestroyPipelineLayout(_device, _meshPipelineLayout, nullptr);
        vkDestroyPipeline(_device, _meshPipeline, nullptr);
    });
}

bool VulkanEngine::init_background_pipelines()
{

    VkPipelineLayoutCreateInfo computeLayout{};
    computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayout.pSetLayouts = &_drawImageDescriptorLayout;
    computeLayout.setLayoutCount = 1;

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(ComputePushConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    computeLayout.pPushConstantRanges = &pushConstant;
    computeLayout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr, &_gradientPipelineLayout));

    VkShaderModule computeDrawShader;
    vkutil::load_shader_module("../Shaders/Internal/gradient.spv", _device, &computeDrawShader);
    VkShaderModule skyShader;
    vkutil::load_shader_module("../Shaders/Internal/sky.spv", _device, &skyShader);

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.layout = _gradientPipelineLayout;
    computePipelineCreateInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computePipelineCreateInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computePipelineCreateInfo.stage.pName = "main";

    computePipelineCreateInfo.stage.module = computeDrawShader;
    VK_CHECK(
        vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &_gradientPipeline));

    ComputeEffect gradient;
    gradient.layout = _gradientPipelineLayout;
    gradient.name = "gradient";
    gradient.pipeline = _gradientPipeline;
    gradient.data.data1 = glm::vec4(1, 0, 0, 1);
    gradient.data.data2 = glm::vec4(0, 0, 1, 1);

    computePipelineCreateInfo.stage.module = skyShader;
    VkPipeline skyPipeline;
    VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &skyPipeline));

    ComputeEffect sky;
    sky.layout = _gradientPipelineLayout;
    sky.name = "sky";
    sky.pipeline = skyPipeline;
    sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

    backgroundEffects.push_back(gradient);
    backgroundEffects.push_back(sky);

    vkDestroyShaderModule(_device, computeDrawShader, nullptr);
    vkDestroyShaderModule(_device, skyShader, nullptr);

    VkPipeline p1 = _gradientPipeline;
    VkPipeline p2 = skyPipeline;
    VkPipelineLayout layout = _gradientPipelineLayout;

    _mainDeletionQueue.push_function([=]() {
        vkDestroyPipelineLayout(_device, layout, nullptr);
        vkDestroyPipeline(_device, p1, nullptr);
        vkDestroyPipeline(_device, p2, nullptr);
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
    _drawImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    _depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
    _depthImage.imageExtent = drawImageExtent;
    _depthImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

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

    VkImageUsageFlags depthImageUsages{};
    depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    const VkImageCreateInfo depthImageInfo =
        vkinit::image_create_info(_depthImage.imageFormat, depthImageUsages, drawImageExtent);
    const VkResult depthImageResult =
        vmaCreateImage(_allocator, &depthImageInfo, &allocationInfo, &_depthImage.image, &_depthImage.allocation, nullptr);
    if (depthImageResult != VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to create depth image: {}", string_VkResult(depthImageResult));
        vkDestroyImageView(_device, _drawImage.imageView, nullptr);
        _drawImage.imageView = VK_NULL_HANDLE;
        vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
        _drawImage.image = VK_NULL_HANDLE;
        _drawImage.allocation = VK_NULL_HANDLE;
        return false;
    }

    const VkImageViewCreateInfo depthImageViewInfo =
        vkinit::imageview_create_info(_depthImage.imageFormat, _depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);
    const VkResult depthImageViewResult =
        vkCreateImageView(_device, &depthImageViewInfo, nullptr, &_depthImage.imageView);
    if (depthImageViewResult != VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to create depth image view: {}", string_VkResult(depthImageViewResult));
        vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);
        _depthImage.image = VK_NULL_HANDLE;
        _depthImage.allocation = VK_NULL_HANDLE;
        vkDestroyImageView(_device, _drawImage.imageView, nullptr);
        _drawImage.imageView = VK_NULL_HANDLE;
        vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
        _drawImage.image = VK_NULL_HANDLE;
        _drawImage.allocation = VK_NULL_HANDLE;
        return false;
    }

    LUNA_CORE_INFO(
        "Created draw image: {}x{}, format={}", extent.width, extent.height, string_VkFormat(_drawImage.imageFormat));
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

    if (_depthImage.imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(_device, _depthImage.imageView, nullptr);
        _depthImage.imageView = VK_NULL_HANDLE;
    }

    if (_drawImage.image != VK_NULL_HANDLE && _drawImage.allocation != VK_NULL_HANDLE) {
        vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
        _drawImage.image = VK_NULL_HANDLE;
        _drawImage.allocation = VK_NULL_HANDLE;
    }

    if (_depthImage.image != VK_NULL_HANDLE && _depthImage.allocation != VK_NULL_HANDLE) {
        vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);
        _depthImage.image = VK_NULL_HANDLE;
        _depthImage.allocation = VK_NULL_HANDLE;
    }

    _drawImage.imageExtent = {};
    _drawImage.imageFormat = VK_FORMAT_UNDEFINED;
    _drawImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    _depthImage.imageExtent = {};
    _depthImage.imageFormat = VK_FORMAT_UNDEFINED;
    _depthImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void VulkanEngine::update_draw_image_descriptors()
{
    if (_device == VK_NULL_HANDLE || _drawImageDescriptors == VK_NULL_HANDLE ||
        _drawImage.imageView == VK_NULL_HANDLE) {
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

AllocatedBuffer VulkanEngine::create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
    // allocate buffer
    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;

    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = memoryUsage;
    vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    AllocatedBuffer newBuffer;

    // allocate the buffer
    VK_CHECK(vmaCreateBuffer(
        _allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));

    return newBuffer;
}

void VulkanEngine::destroy_buffer(const AllocatedBuffer& buffer)
{
    vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
}

void VulkanEngine::immediate_submit(const std::function<void(VkCommandBuffer cmd)>& function)
{
    VK_CHECK(vkResetFences(_device, 1, &_immContext._fence));
    VK_CHECK(vkResetCommandBuffer(_immContext._commandBuffer, 0));

    VkCommandBuffer cmd = _immContext._commandBuffer;
    VkCommandBufferBeginInfo cmdBeginInfo =
        vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdInfo = vkinit::command_buffer_submit_info(cmd);
    VkSubmitInfo2 submit = vkinit::submit_info(&cmdInfo, nullptr, nullptr);
    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, _immContext._fence));
    VK_CHECK(vkWaitForFences(_device, 1, &_immContext._fence, true, 1'000'000'000));
}

GPUMeshBuffers VulkanEngine::uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
{
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    GPUMeshBuffers newSurface;

    // create vertex buffer
    newSurface.vertexBuffer = create_buffer(vertexBufferSize,
                                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                            VMA_MEMORY_USAGE_GPU_ONLY);

    // find the adress of the vertex buffer
    VkBufferDeviceAddressInfo deviceAdressInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                               .buffer = newSurface.vertexBuffer.buffer};
    newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(_device, &deviceAdressInfo);

    // create index buffer
    newSurface.indexBuffer = create_buffer(indexBufferSize,
                                           VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                           VMA_MEMORY_USAGE_GPU_ONLY);
    AllocatedBuffer staging =
        create_buffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* data = staging.allocation->GetMappedData();

    // copy vertex buffer
    memcpy(data, vertices.data(), vertexBufferSize);
    // copy index buffer
    memcpy((char*) data + vertexBufferSize, indices.data(), indexBufferSize);

    immediate_submit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy{0};
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = vertexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

        VkBufferCopy indexCopy{0};
        indexCopy.dstOffset = 0;
        indexCopy.srcOffset = vertexBufferSize;
        indexCopy.size = indexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
    });

    destroy_buffer(staging);

    return newSurface;
}
