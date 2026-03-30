#include "vk_engine.h"

#include <Core/log.h>

#define GLFW_INCLUDE_NONE
#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"
#include "vk_shader.h"
#include "vk_types.h"
#include "VkBootstrap.h"
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <cmath>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <GLFW/glfw3.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <map>
#include <optional>

VulkanEngine* loadedEngine = nullptr;

#ifndef NDEBUG
constexpr bool bUseValidationLayers = true;
#else
constexpr bool bUseValidationLayers = false;
#endif

namespace {
uint32_t pack_rgba8(float r, float g, float b, float a)
{
    const auto to_u8 = [](float v) -> uint32_t {
        return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
    };

    return to_u8(r) | (to_u8(g) << 8) | (to_u8(b) << 16) | (to_u8(a) << 24);
}

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
    file.close();

    return buffer;
}

bool reflect_shader_bindings(const std::filesystem::path& filePath,
                             luna::ShaderType shaderType,
                             VkShaderStageFlags shaderStages,
                             DescriptorLayoutBuilder& builder)
{
    const auto spirvCode = load_spirv_code(filePath);
    if (!spirvCode.has_value()) {
        LUNA_CORE_ERROR("Failed to read SPIR-V shader for reflection: {}", filePath.string());
        return false;
    }

    const luna::VulkanShader shader(*spirvCode, shaderType);
    builder.add_bindings_from_reflection(shader.getReflectionMap(), 0, shaderStages);
    return true;
}

std::vector<DescriptorAllocator::PoolSizeRatio> build_pool_ratios(const DescriptorLayoutBuilder& builder)
{
    std::map<VkDescriptorType, float> descriptorCounts;
    for (const auto& binding : builder.bindings) {
        descriptorCounts[binding.descriptorType] += static_cast<float>(std::max(1u, binding.descriptorCount));
    }

    std::vector<DescriptorAllocator::PoolSizeRatio> poolRatios;
    poolRatios.reserve(descriptorCounts.size());
    for (const auto& [type, count] : descriptorCounts) {
        poolRatios.push_back({type, count});
    }

    return poolRatios;
}

bool load_shader_module_with_fallback(VkDevice device,
                                      VkShaderModule* outShaderModule,
                                      std::initializer_list<const char*> paths,
                                      const char* stageLabel)
{
    for (const char* path : paths) {
        if (vkutil::load_shader_module(path, device, outShaderModule)) {
            LUNA_CORE_INFO("Loaded {} shader module from '{}'", stageLabel, path);
            return true;
        }
    }

    LUNA_CORE_ERROR("Failed to load {} shader module", stageLabel);
    return false;
}

bool render_object_sort(const RenderObject& a, const RenderObject& b)
{
    const MaterialPipeline* aPipeline = a.material != nullptr ? a.material->pipeline : nullptr;
    const MaterialPipeline* bPipeline = b.material != nullptr ? b.material->pipeline : nullptr;
    if (aPipeline != bPipeline) {
        return aPipeline < bPipeline;
    }

    if (a.material != b.material) {
        return a.material < b.material;
    }

    if (a.indexBuffer != b.indexBuffer) {
        return a.indexBuffer < b.indexBuffer;
    }

    if (a.vertexBufferAddress != b.vertexBufferAddress) {
        return a.vertexBufferAddress < b.vertexBufferAddress;
    }

    return a.firstIndex < b.firstIndex;
}

} // namespace

void GLTFMetallic_Roughness::build_pipelines(VulkanEngine* engine)
{
    DescriptorLayoutBuilder materialLayoutBuilder;
    materialLayoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    materialLayoutBuilder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    materialLayoutBuilder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    materialLayout = materialLayoutBuilder.build(engine->_device, VK_SHADER_STAGE_FRAGMENT_BIT);

    VkShaderModule meshFragShader{VK_NULL_HANDLE};
    VkShaderModule meshVertexShader{VK_NULL_HANDLE};
    const bool loadedFrag = load_shader_module_with_fallback(engine->_device,
                                                             &meshFragShader,
                                                             {"../Shaders/Internal/mesh.frag.spv"},
                                                             "mesh fragment");
    const bool loadedVert = load_shader_module_with_fallback(engine->_device,
                                                             &meshVertexShader,
                                                             {"../Shaders/Internal/mesh.vert.spv"},
                                                             "mesh vertex");
    if (!loadedFrag || !loadedVert) {
        if (meshFragShader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(engine->_device, meshFragShader, nullptr);
        }
        if (meshVertexShader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(engine->_device, meshVertexShader, nullptr);
        }
        return;
    }

    VkPushConstantRange matrixRange{};
    matrixRange.offset = 0;
    matrixRange.size = sizeof(GPUDrawPushConstants);
    matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    std::array<VkDescriptorSetLayout, 2> layouts = {engine->_gpuSceneDataDescriptorLayout, materialLayout};
    VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
    mesh_layout_info.setLayoutCount = static_cast<uint32_t>(layouts.size());
    mesh_layout_info.pSetLayouts = layouts.data();
    mesh_layout_info.pushConstantRangeCount = 1;
    mesh_layout_info.pPushConstantRanges = &matrixRange;

    VkPipelineLayout newLayout{VK_NULL_HANDLE};
    VK_CHECK(vkCreatePipelineLayout(engine->_device, &mesh_layout_info, nullptr, &newLayout));

    opaquePipeline.layout = newLayout;
    transparentPipeline.layout = newLayout;

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.set_shaders(meshVertexShader, meshFragShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_blending();
    pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL);
    pipelineBuilder.set_color_attachment_format(engine->_drawImage.imageFormat);
    pipelineBuilder.set_depth_format(engine->_depthImage.imageFormat);
    pipelineBuilder._pipelineLayout = newLayout;

    opaquePipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device);

    pipelineBuilder.enable_blending_alphablend();
    pipelineBuilder.enable_depthtest(false, VK_COMPARE_OP_LESS_OR_EQUAL);
    transparentPipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device);

    vkDestroyShaderModule(engine->_device, meshFragShader, nullptr);
    vkDestroyShaderModule(engine->_device, meshVertexShader, nullptr);
}

void GLTFMetallic_Roughness::clear_resources(VkDevice device)
{
    if (opaquePipeline.pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, opaquePipeline.pipeline, nullptr);
        opaquePipeline.pipeline = VK_NULL_HANDLE;
    }

    if (transparentPipeline.pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, transparentPipeline.pipeline, nullptr);
        transparentPipeline.pipeline = VK_NULL_HANDLE;
    }

    if (opaquePipeline.layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, opaquePipeline.layout, nullptr);
        opaquePipeline.layout = VK_NULL_HANDLE;
        transparentPipeline.layout = VK_NULL_HANDLE;
    }

    if (materialLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, materialLayout, nullptr);
        materialLayout = VK_NULL_HANDLE;
    }
}

MaterialInstance GLTFMetallic_Roughness::write_material(VkDevice device,
                                                        MaterialPass pass,
                                                        const MaterialResources& resources,
                                                        DescriptorAllocator& descriptorAllocator)
{
    MaterialInstance materialInstance;
    materialInstance.passType = pass;
    materialInstance.pipeline = pass == MaterialPass::Transparent ? &transparentPipeline : &opaquePipeline;
    materialInstance.materialSet = descriptorAllocator.allocate(device, materialLayout);

    DescriptorWriter writer;
    writer.write_buffer(0,
                        resources.dataBuffer.buffer,
                        sizeof(MaterialConstants),
                        resources.dataBufferOffset,
                        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.write_image(1,
                       resources.colorImage.imageView,
                       resources.colorSampler,
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.write_image(2,
                       resources.metalRoughImage.imageView,
                       resources.metalRoughSampler,
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.update_set(device, materialInstance.materialSet);

    return materialInstance;
}

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
    mainCamera.position = glm::vec3(0.0f, 0.0f, 5.0f);
    mainCamera.pitch = 0.0f;
    mainCamera.yaw = 0.0f;

    uint32_t white = pack_rgba8(1.0f, 1.0f, 1.0f, 1.0f);
    uint32_t grey = pack_rgba8(0.66f, 0.66f, 0.66f, 1.0f);
    uint32_t black = pack_rgba8(0.0f, 0.0f, 0.0f, 0.0f);

    _whiteImage = create_image(
        &white, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    _greyImage =
        create_image(&grey, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    _blackImage = create_image(
        &black, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    std::array<uint32_t, 16 * 16> pixels;
    for (uint32_t x = 0; x < 16; x++) {
        for (uint32_t y = 0; y < 16; y++) {
            pixels[y * 16 + x] = (x % 2 == y % 2) ? 0xFF000000 : 0xFFFF00FF;
        }
    }
    _errorCheckerboardImage = create_image(
        pixels.data(), VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampl.magFilter = VK_FILTER_NEAREST;
    sampl.minFilter = VK_FILTER_NEAREST;
    VK_CHECK(vkCreateSampler(_device, &sampl, nullptr, &_defaultSamplerNearest));

    sampl.magFilter = VK_FILTER_LINEAR;
    sampl.minFilter = VK_FILTER_LINEAR;
    VK_CHECK(vkCreateSampler(_device, &sampl, nullptr, &_defaultSamplerLinear));

    MaterialConstants materialConstants{};
    materialConstants.colorFactors = glm::vec4(1.0f);
    materialConstants.metal_rough_factors = glm::vec4(1.0f);

    AllocatedBuffer materialConstantsBuffer =
        create_buffer(sizeof(MaterialConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    std::memcpy(materialConstantsBuffer.info.pMappedData, &materialConstants, sizeof(MaterialConstants));

    MaterialResources materialResources{};
    materialResources.colorImage = _whiteImage;
    materialResources.colorSampler = _defaultSamplerLinear;
    materialResources.metalRoughImage = _whiteImage;
    materialResources.metalRoughSampler = _defaultSamplerLinear;
    materialResources.dataBuffer = materialConstantsBuffer;
    materialResources.dataBufferOffset = 0;

    _defaultMaterialInstance =
        metalRoughMaterial.write_material(_device, MaterialPass::MainColor, materialResources, globalDescriptorAllocator);

    _mainDeletionQueue.push_function([this, materialConstantsBuffer]() {
        loadedScenes.clear();
        mainDrawContext.clear();
        destroy_image(_whiteImage);
        destroy_image(_greyImage);
        destroy_image(_blackImage);
        destroy_image(_errorCheckerboardImage);
        destroy_buffer(materialConstantsBuffer);

        if (_defaultSamplerLinear != VK_NULL_HANDLE) {
            vkDestroySampler(_device, _defaultSamplerLinear, nullptr);
            _defaultSamplerLinear = VK_NULL_HANDLE;
        }

        if (_defaultSamplerNearest != VK_NULL_HANDLE) {
            vkDestroySampler(_device, _defaultSamplerNearest, nullptr);
            _defaultSamplerNearest = VK_NULL_HANDLE;
        }
    });

    auto loadedScene = loadGltf(this, "../assets/basicmesh.glb");
    if (!loadedScene.has_value()) {
        LUNA_CORE_WARN("Falling back to empty scene set because basicmesh.glb failed to load");
        return;
    }

    loadedScenes.clear();
    mainDrawContext.clear();
    loadedScenes["basicmesh"] = std::move(loadedScene.value());

    _mainDeletionQueue.push_function([this]() {
        loadedScenes.clear();
        mainDrawContext.clear();
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
        destroy_draw_resources();
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
    _gpuSceneDataDescriptorLayout = VK_NULL_HANDLE;
    _gradientPipeline = VK_NULL_HANDLE;
    _gradientPipelineLayout = VK_NULL_HANDLE;
    _defaultSamplerLinear = VK_NULL_HANDLE;
    _defaultSamplerNearest = VK_NULL_HANDLE;
    _whiteImage = {};
    _greyImage = {};
    _blackImage = {};
    _errorCheckerboardImage = {};
    _defaultMaterialInstance = {};
    metalRoughMaterial = {};
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
    get_current_frame()._frameDescriptors.clear_pools(_device);

    VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._renderFence));

    uint32_t swapchainImageIndex = 0;
    bool recreateAfterPresent = false;
    const VkResult acquireResult = vkAcquireNextImageKHR(
        _device, _swapchain, 1'000'000'000, get_current_frame()._swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        logSwapchainResult("vkAcquireNextImageKHR", acquireResult, _swapchainExtent, get_framebuffer_extent());
        resize_requested = true;
        return;
    }
    if (acquireResult == VK_SUBOPTIMAL_KHR) {
        resize_requested = true;
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

    _drawExtent.width = std::max(
        1u, static_cast<uint32_t>(std::min(_swapchainExtent.width, _drawImage.imageExtent.width) * renderScale));
    _drawExtent.height = std::max(
        1u, static_cast<uint32_t>(std::min(_swapchainExtent.height, _drawImage.imageExtent.height) * renderScale));

    update_scene();

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
        resize_requested = true;
        logSwapchainResult("vkQueuePresentKHR", presentResult, _swapchainExtent, get_framebuffer_extent());
    } else if (presentResult != VK_SUCCESS) {
        LUNA_CORE_FATAL("Vulkan call failed: vkQueuePresentKHR returned {}", string_VkResult(presentResult));
        std::abort();
    } else if (recreateAfterPresent) {
        resize_requested = true;
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

    return true;
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

    vkutil::transition_image(cmd, _depthImage.image, _depthImageLayout, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
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

    AllocatedBuffer sceneDataBuffer =
        create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    std::memcpy(sceneDataBuffer.info.pMappedData, &sceneData, sizeof(GPUSceneData));

    get_current_frame()._deletionQueue.push_function([=, this]() {
        destroy_buffer(sceneDataBuffer);
    });

    const VkDescriptorSet globalDescriptor =
        get_current_frame()._frameDescriptors.allocate(_device, _gpuSceneDataDescriptorLayout);

    DescriptorWriter writer;
    writer.write_buffer(0, sceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.update_set(_device, globalDescriptor);

    std::sort(mainDrawContext.opaqueSurfaces.begin(), mainDrawContext.opaqueSurfaces.end(), render_object_sort);

    auto draw_objects = [&](const std::vector<RenderObject>& objects) {
        MaterialPipeline* lastPipeline = nullptr;
        MaterialInstance* lastMaterial = nullptr;
        VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

        for (const RenderObject& draw : objects) {
            if (draw.material == nullptr || draw.material->pipeline == nullptr ||
                draw.material->pipeline->pipeline == VK_NULL_HANDLE || draw.material->pipeline->layout == VK_NULL_HANDLE) {
                continue;
            }

            if (draw.material->pipeline != lastPipeline) {
                lastPipeline = draw.material->pipeline;
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, lastPipeline->pipeline);
            }

            if (draw.material != lastMaterial) {
                lastMaterial = draw.material;
                std::array<VkDescriptorSet, 2> meshDescriptors = {globalDescriptor, draw.material->materialSet};
                vkCmdBindDescriptorSets(cmd,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        lastMaterial->pipeline->layout,
                                        0,
                                        static_cast<uint32_t>(meshDescriptors.size()),
                                        meshDescriptors.data(),
                                        0,
                                        nullptr);
            }

            GPUDrawPushConstants pushConstants{};
            pushConstants.worldMatrix = draw.transform;
            pushConstants.vertexBuffer = draw.vertexBufferAddress;
            vkCmdPushConstants(cmd,
                               draw.material->pipeline->layout,
                               VK_SHADER_STAGE_VERTEX_BIT,
                               0,
                               sizeof(GPUDrawPushConstants),
                               &pushConstants);

            if (draw.indexBuffer != lastIndexBuffer) {
                lastIndexBuffer = draw.indexBuffer;
                vkCmdBindIndexBuffer(cmd, draw.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
            }

            vkCmdDrawIndexed(cmd, draw.indexCount, 1, draw.firstIndex, 0, 0);
        }
    };

    draw_objects(mainDrawContext.opaqueSurfaces);
    draw_objects(mainDrawContext.transparentSurfaces);

    vkCmdEndRendering(cmd);
}

void VulkanEngine::update_scene()
{
    const float aspect = _drawExtent.height == 0 ? 1.0f
                                                 : static_cast<float>(_drawExtent.width) /
                                                       static_cast<float>(_drawExtent.height);

    sceneData.view = mainCamera.get_view_matrix();
    sceneData.proj = glm::perspectiveRH_ZO(glm::radians(70.0f), aspect, 0.1f, 10'000.0f);
    sceneData.proj[1][1] *= -1.0f;
    sceneData.viewproj = sceneData.proj * sceneData.view;

    sceneData.ambientColor = glm::vec4(0.1f);
    sceneData.sunlightDirection = glm::vec4(0.0f, 1.0f, 0.5f, 1.0f);
    sceneData.sunlightColor = glm::vec4(1.0f);

    mainDrawContext.clear();

    for (auto& [_, scene] : loadedScenes) {
        if (!scene) {
            continue;
        }

        scene->Draw(glm::mat4{1.0f}, mainDrawContext);
    }
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
    if (!create_swapchain(framebufferExtent.width, framebufferExtent.height)) {
        return false;
    }

    return create_draw_resources(framebufferExtent);
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
    DescriptorLayoutBuilder builder;
    if (!reflect_shader_bindings(
            "../Shaders/Internal/gradient.spv", luna::ShaderType::Compute, VK_SHADER_STAGE_COMPUTE_BIT, builder) ||
        !reflect_shader_bindings(
            "../Shaders/Internal/sky.spv", luna::ShaderType::Compute, VK_SHADER_STAGE_COMPUTE_BIT, builder)) {
        return false;
    }

    if (builder.bindings.empty()) {
        LUNA_CORE_ERROR("No descriptor bindings were reflected for compute shaders");
        return false;
    }

    const auto drawImageBindingIt =
        std::find_if(builder.bindings.begin(), builder.bindings.end(), [](const VkDescriptorSetLayoutBinding& binding) {
            return binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        });
    if (drawImageBindingIt == builder.bindings.end()) {
        LUNA_CORE_ERROR("Failed to find reflected storage image binding for background compute shaders");
        return false;
    }

    _drawImageDescriptorBinding = drawImageBindingIt->binding;
    _drawImageDescriptorCount = drawImageBindingIt->descriptorCount;
    _drawImageDescriptorType = drawImageBindingIt->descriptorType;

    if (_drawImageDescriptorType != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
        LUNA_CORE_ERROR("Background compute shaders must expose a storage image descriptor in set 0");
        return false;
    }

    if (_drawImageDescriptorCount != 1) {
        LUNA_CORE_ERROR("Only single storage image descriptors are currently supported, reflected count={}",
                        _drawImageDescriptorCount);
        return false;
    }

    DescriptorLayoutBuilder sceneDataBuilder;
    sceneDataBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    _gpuSceneDataDescriptorLayout =
        sceneDataBuilder.build(_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    std::array<DescriptorAllocatorGrowable::PoolSizeRatio, 2> frameSizes = {
        DescriptorAllocatorGrowable::PoolSizeRatio{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3.f},
        DescriptorAllocatorGrowable::PoolSizeRatio{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3.f},
    };
    for (int i = 0; i < FRAME_OVERLAP; i++) {
        _frames[i]._frameDescriptors.init(_device, 1000, frameSizes);
    }

    auto sizes = build_pool_ratios(builder);
    sizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3.0f});
    sizes.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3.0f});
    globalDescriptorAllocator.init_pool(_device, 10, sizes);
    _drawImageDescriptorLayout = builder.build(_device);

    // allocate a descriptor set for our draw image
    _drawImageDescriptors = globalDescriptorAllocator.allocate(_device, _drawImageDescriptorLayout);

    update_draw_image_descriptors();

    // make sure both the descriptor allocator and the new layout get cleaned up properly
    _mainDeletionQueue.push_function([&]() {
        globalDescriptorAllocator.destroy_pool(_device);

        vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout, nullptr);
        vkDestroyDescriptorSetLayout(_device, _gpuSceneDataDescriptorLayout, nullptr);
    });

    _mainDeletionQueue.push_function([&]() {
        for (int i = 0; i < FRAME_OVERLAP; i++) {
            _frames[i]._frameDescriptors.destroy_pools(_device);
        }
    });

    LUNA_CORE_INFO("Initialized descriptor layouts and allocators");
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
        LUNA_CORE_ERROR("Error when building the triangle fragment shader module");
    } else {
        LUNA_CORE_INFO("Triangle fragment shader succesfully loaded");
    }

    VkShaderModule triangleVertexShader;
    if (!vkutil::load_shader_module("../Shaders/Internal/colored_triangle.vert.spv", _device, &triangleVertexShader)) {
        LUNA_CORE_ERROR("Error when building the triangle vertex shader module");
    } else {
        LUNA_CORE_INFO("Triangle vertex shader succesfully loaded");
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
    metalRoughMaterial.build_pipelines(this);

    _mainDeletionQueue.push_function([this]() {
        metalRoughMaterial.clear_resources(_device);
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

    _mainDeletionQueue.push_function([=, this]() {
        vkDestroyPipelineLayout(_device, layout, nullptr);
        vkDestroyPipeline(_device, p1, nullptr);
        vkDestroyPipeline(_device, p2, nullptr);
    });

    LUNA_CORE_INFO("Initialized background compute pipeline");
    return true;
}

bool VulkanEngine::resize_swapchain()
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

    resize_requested = false;
    return true;
}

bool VulkanEngine::create_draw_resources(VkExtent2D extent)
{
    if (extent.width == 0 || extent.height == 0) {
        LUNA_CORE_ERROR("Cannot create draw resources for extent {}x{}", extent.width, extent.height);
        return false;
    }

    const VkExtent3D drawImageExtent = {extent.width, extent.height, 1};

    _drawImage = create_image(drawImageExtent,
                              VK_FORMAT_R16G16B16A16_SFLOAT,
                              VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                  VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    _drawImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    _depthImage =
        create_image(drawImageExtent, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    _depthImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    LUNA_CORE_INFO(
        "Created draw image: {}x{}, format={}", extent.width, extent.height, string_VkFormat(_drawImage.imageFormat));
    return true;
}

void VulkanEngine::destroy_draw_resources()
{
    if (_device == VK_NULL_HANDLE || _allocator == VK_NULL_HANDLE) {
        _drawImage = {};
        _depthImage = {};
        return;
    }

    destroy_image(_drawImage);
    destroy_image(_depthImage);
    _drawImage = {};
    _depthImage = {};
    _drawImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    _depthImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void VulkanEngine::update_draw_image_descriptors()
{
    if (_device == VK_NULL_HANDLE || _drawImageDescriptors == VK_NULL_HANDLE ||
        _drawImage.imageView == VK_NULL_HANDLE) {
        return;
    }

    DescriptorWriter writer;
    writer.write_image(_drawImageDescriptorBinding,
                       _drawImage.imageView,
                       VK_NULL_HANDLE,
                       VK_IMAGE_LAYOUT_GENERAL,
                       _drawImageDescriptorType);
    writer.update_set(_device, _drawImageDescriptors);
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

AllocatedImage VulkanEngine::create_image(VkExtent3D size,
                                          VkFormat format,
                                          VkImageUsageFlags usage,
                                          bool mipmapped)
{
    AllocatedImage newImage;
    newImage.imageFormat = format;
    newImage.imageExtent = size;

    VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size);
    if (mipmapped) {
        img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }

    VmaAllocationCreateInfo allocinfo = {};
    allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vmaCreateImage(_allocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT) {
        aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlags);
    view_info.subresourceRange.levelCount = img_info.mipLevels;

    VK_CHECK(vkCreateImageView(_device, &view_info, nullptr, &newImage.imageView));
    return newImage;
}

AllocatedImage VulkanEngine::create_image(void* data,
                                          VkExtent3D size,
                                          VkFormat format,
                                          VkImageUsageFlags usage,
                                          bool mipmapped)
{
    const size_t dataSize = size.depth * size.width * size.height * 4;
    AllocatedBuffer uploadbuffer = create_buffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    std::memcpy(uploadbuffer.info.pMappedData, data, dataSize);

    AllocatedImage newImage =
        create_image(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

    immediate_submit([&](VkCommandBuffer cmd) {
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

        vkCmdCopyBufferToImage(
            cmd, uploadbuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        vkutil::transition_image(
            cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    });

    destroy_buffer(uploadbuffer);
    return newImage;
}

void VulkanEngine::destroy_image(const AllocatedImage& image)
{
    if (_device == VK_NULL_HANDLE || _allocator == VK_NULL_HANDLE) {
        return;
    }

    if (image.imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(_device, image.imageView, nullptr);
    }

    if (image.image != VK_NULL_HANDLE && image.allocation != VK_NULL_HANDLE) {
        vmaDestroyImage(_allocator, image.image, image.allocation);
    }
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
