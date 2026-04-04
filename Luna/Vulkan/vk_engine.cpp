#include "vk_engine.h"

#include "Core/Paths.h"
#include "Imgui/ImGuiLayer.hpp"
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
#include <cstddef>
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
std::filesystem::path shader_path(std::string_view relativePath)
{
    return luna::paths::shader(relativePath);
}

std::filesystem::path asset_path(std::string_view relativePath)
{
    return luna::paths::asset(relativePath);
}

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
        case luna::SamplerAddressMode::ClampToEdge:
            return vk::SamplerAddressMode::eClampToEdge;
        case luna::SamplerAddressMode::MirroredRepeat:
            return vk::SamplerAddressMode::eMirroredRepeat;
        case luna::SamplerAddressMode::Repeat:
        default:
            return vk::SamplerAddressMode::eRepeat;
    }
}

vk::ImageType to_vulkan_image_type(luna::ImageType type)
{
    switch (type) {
        case luna::ImageType::Image3D:
            return vk::ImageType::e3D;
        case luna::ImageType::Image2DArray:
        case luna::ImageType::Image2D:
        default:
            return vk::ImageType::e2D;
    }
}

vk::ImageViewType to_vulkan_image_view_type(luna::ImageType type)
{
    switch (type) {
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
    return desc.type == luna::ImageType::Image2DArray ? desc.arrayLayers : 1u;
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

void logSwapchainResult(const char* step, vk::Result result, vk::Extent2D swapchainExtent, vk::Extent2D framebufferExtent)
{
    LUNA_CORE_WARN("{} returned {}. swapchain={}x{}, framebuffer={}x{}",
                   step,
                   vk::to_string(result),
                   swapchainExtent.width,
                   swapchainExtent.height,
                   framebufferExtent.width,
                   framebufferExtent.height);
}

void logSwapchainResult(const char* step, VkResult result, vk::Extent2D swapchainExtent, vk::Extent2D framebufferExtent)
{
    logSwapchainResult(step, static_cast<vk::Result>(result), swapchainExtent, framebufferExtent);
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
                             vk::ShaderStageFlags shaderStages,
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

bool reflect_shader_bindings(const std::filesystem::path& filePath,
                             luna::ShaderType shaderType,
                             VkShaderStageFlags shaderStages,
                             DescriptorLayoutBuilder& builder)
{
    return reflect_shader_bindings(filePath, shaderType, static_cast<vk::ShaderStageFlags>(shaderStages), builder);
}

std::vector<DescriptorAllocator::PoolSizeRatio> build_pool_ratios(const DescriptorLayoutBuilder& builder)
{
    std::map<vk::DescriptorType, float> descriptorCounts;
    for (const auto& binding : builder.bindings) {
        descriptorCounts[binding.descriptorType] += static_cast<float>(std::max(1u, binding.descriptorCount));
    }

    std::vector<DescriptorAllocator::PoolSizeRatio> poolRatios;
    poolRatios.reserve(descriptorCounts.size());
    for (const auto& [type, count] : descriptorCounts) {
        poolRatios.push_back(DescriptorAllocator::PoolSizeRatio{type, count});
    }

    return poolRatios;
}

bool load_shader_module_with_fallback(vk::Device device,
                                      vk::ShaderModule* outShaderModule,
                                      std::initializer_list<std::filesystem::path> paths,
                                      const char* stageLabel)
{
    for (const auto& path : paths) {
        const std::string pathString = luna::paths::display(path);
        if (vkutil::load_shader_module(pathString.c_str(), device, outShaderModule)) {
            LUNA_CORE_INFO("Loaded {} shader module from '{}'", stageLabel, pathString);
            return true;
        }
    }

    LUNA_CORE_ERROR("Failed to load {} shader module", stageLabel);
    return false;
}

bool load_shader_module_with_fallback(VkDevice device,
                                      VkShaderModule* outShaderModule,
                                      std::initializer_list<std::filesystem::path> paths,
                                      const char* stageLabel)
{
    vk::ShaderModule shaderModule{};
    const bool loaded =
        load_shader_module_with_fallback(vk::Device(device), &shaderModule, paths, stageLabel);
    if (loaded) {
        *outShaderModule = static_cast<VkShaderModule>(shaderModule);
    }
    return loaded;
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
    luna::ResourceLayoutDesc materialLayoutDesc{};
    materialLayoutDesc.debugName = "MaterialLayout";
    materialLayoutDesc.bindings.push_back(
        {0, luna::ResourceType::UniformBuffer, 1, luna::ShaderType::Fragment});
    materialLayoutDesc.bindings.push_back(
        {1, luna::ResourceType::CombinedImageSampler, 1, luna::ShaderType::Fragment});
    materialLayoutDesc.bindings.push_back(
        {2, luna::ResourceType::CombinedImageSampler, 1, luna::ShaderType::Fragment});
    materialLayout = build_resource_layout(engine->_device, materialLayoutDesc);

    vk::PushConstantRange matrixRange{};
    matrixRange.offset = 0;
    matrixRange.size = sizeof(GPUDrawPushConstants);
    matrixRange.stageFlags = vk::ShaderStageFlagBits::eVertex;

    std::array<vk::DescriptorSetLayout, 2> layouts = {engine->_gpuSceneDataDescriptorLayout, materialLayout};
    vk::PipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
    mesh_layout_info.setLayoutCount = static_cast<uint32_t>(layouts.size());
    mesh_layout_info.pSetLayouts = layouts.data();
    mesh_layout_info.pushConstantRangeCount = 1;
    mesh_layout_info.pPushConstantRanges = &matrixRange;

    vk::PipelineLayout newLayout{};
    VK_CHECK(engine->_device.createPipelineLayout(&mesh_layout_info, nullptr, &newLayout));

    opaquePipeline.layout = newLayout;
    transparentPipeline.layout = newLayout;

    const std::string meshVertexPath = luna::paths::display(shader_path("Internal/mesh.vert.spv"));
    const std::string meshFragmentPath = luna::paths::display(shader_path("Internal/mesh.frag.spv"));

    luna::GraphicsPipelineDesc opaquePipelineDesc{};
    opaquePipelineDesc.debugName = "Mesh pipeline";
    opaquePipelineDesc.vertexShader = {luna::ShaderType::Vertex, meshVertexPath};
    opaquePipelineDesc.fragmentShader = {luna::ShaderType::Fragment, meshFragmentPath};
    opaquePipelineDesc.topology = luna::PrimitiveTopology::TriangleList;
    opaquePipelineDesc.polygonMode = luna::PolygonMode::Fill;
    opaquePipelineDesc.cullMode = luna::CullMode::None;
    opaquePipelineDesc.frontFace = luna::FrontFace::Clockwise;
    opaquePipelineDesc.colorAttachments.push_back({luna::PixelFormat::RGBA16Float, false});
    opaquePipelineDesc.depthStencil = {luna::PixelFormat::D32Float, true, true, luna::CompareOp::LessOrEqual};
    opaquePipeline.pipeline = build_graphics_pipeline(engine->_device, opaquePipelineDesc, newLayout);

    luna::GraphicsPipelineDesc transparentPipelineDesc = opaquePipelineDesc;
    transparentPipelineDesc.debugName = {};
    transparentPipelineDesc.colorAttachments.front().blendEnabled = true;
    transparentPipelineDesc.depthStencil.depthWriteEnabled = false;
    transparentPipeline.pipeline = build_graphics_pipeline(engine->_device, transparentPipelineDesc, newLayout);
}

void GLTFMetallic_Roughness::clear_resources(vk::Device device)
{
    if (opaquePipeline.pipeline) {
        device.destroyPipeline(opaquePipeline.pipeline, nullptr);
        opaquePipeline.pipeline = nullptr;
    }

    if (transparentPipeline.pipeline) {
        device.destroyPipeline(transparentPipeline.pipeline, nullptr);
        transparentPipeline.pipeline = nullptr;
    }

    if (opaquePipeline.layout) {
        device.destroyPipelineLayout(opaquePipeline.layout, nullptr);
        opaquePipeline.layout = nullptr;
        transparentPipeline.layout = nullptr;
    }

    if (materialLayout) {
        device.destroyDescriptorSetLayout(materialLayout, nullptr);
        materialLayout = nullptr;
    }
}

MaterialInstance GLTFMetallic_Roughness::write_material(vk::Device device,
                                                        MaterialPass pass,
                                                        const MaterialResources& resources,
                                                        DescriptorAllocator& descriptorAllocator)
{
    MaterialInstance materialInstance;
    materialInstance.passType = pass;
    materialInstance.pipeline = pass == MaterialPass::Transparent ? &transparentPipeline : &opaquePipeline;
    materialInstance.materialSet = descriptorAllocator.allocate(device, materialLayout);

    VulkanResourceBindingRegistry bindingRegistry;
    const luna::BufferHandle materialBuffer = bindingRegistry.register_buffer(resources.dataBuffer.buffer);
    const luna::ImageViewHandle colorImage = bindingRegistry.register_image_view(resources.colorImage.imageView);
    const luna::SamplerHandle colorSampler = bindingRegistry.register_sampler(resources.colorSampler);
    const luna::ImageViewHandle metalRoughImage = bindingRegistry.register_image_view(resources.metalRoughImage.imageView);
    const luna::SamplerHandle metalRoughSampler = bindingRegistry.register_sampler(resources.metalRoughSampler);

    luna::ResourceSetWriteDesc materialWrite{};
    materialWrite.buffers.push_back(
        {0, materialBuffer, resources.dataBufferOffset, sizeof(MaterialConstants), luna::ResourceType::UniformBuffer});
    materialWrite.images.push_back({.binding = 1,
                                    .imageView = colorImage,
                                    .sampler = colorSampler,
                                    .type = luna::ResourceType::CombinedImageSampler});
    materialWrite.images.push_back({.binding = 2,
                                    .imageView = metalRoughImage,
                                    .sampler = metalRoughSampler,
                                    .type = luna::ResourceType::CombinedImageSampler});

    const bool updated = update_resource_set(device, bindingRegistry, materialInstance.materialSet, materialWrite);

    bindingRegistry.unregister_buffer(materialBuffer);
    bindingRegistry.unregister_image_view(colorImage);
    bindingRegistry.unregister_sampler(colorSampler);
    bindingRegistry.unregister_image_view(metalRoughImage);
    bindingRegistry.unregister_sampler(metalRoughSampler);

    if (!updated) {
        LUNA_CORE_ERROR("Failed to update material resource bindings via RHI");
    }

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
    _frameNumber = 0;
    resize_requested = false;
    m_loggedBeginFramePass = false;
    m_loggedPresentPass = false;
    LUNA_CORE_INFO("Initializing Vulkan engine");

    _window = static_cast<GLFWwindow*>(window.getNativeWindow());
    if (_window == nullptr) {
        LUNA_CORE_ERROR("Failed to acquire native GLFW window");
        loadedEngine = nullptr;
        return false;
    }

    _windowExtent = {window.getWidth(), window.getHeight()};
    const vk::Extent2D framebufferExtent = get_framebuffer_extent();
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
    if (m_legacyRendererMode != LegacyRendererMode::LegacyScene) {
        return;
    }

    mainCamera.position = glm::vec3(0.0f, 0.0f, 5.0f);
    mainCamera.pitch = 0.0f;
    mainCamera.yaw = 0.0f;

    uint32_t white = pack_rgba8(1.0f, 1.0f, 1.0f, 1.0f);
    uint32_t grey = pack_rgba8(0.66f, 0.66f, 0.66f, 1.0f);
    uint32_t black = pack_rgba8(0.0f, 0.0f, 0.0f, 0.0f);

    const luna::ImageDesc defaultTextureDesc{
        .width = 1,
        .height = 1,
        .depth = 1,
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = luna::PixelFormat::RGBA8Unorm,
        .usage = luna::ImageUsage::Sampled,
        .debugName = "DefaultTexture",
    };
    _whiteImage = create_image(defaultTextureDesc, &white);
    _greyImage = create_image(defaultTextureDesc, &grey);
    _blackImage = create_image(defaultTextureDesc, &black);

    std::array<uint32_t, 16 * 16> pixels;
    for (uint32_t x = 0; x < 16; x++) {
        for (uint32_t y = 0; y < 16; y++) {
            pixels[y * 16 + x] = (x % 2 == y % 2) ? 0xFF000000 : 0xFFFF00FF;
        }
    }
    const luna::ImageDesc errorTextureDesc{
        .width = 16,
        .height = 16,
        .depth = 1,
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = luna::PixelFormat::RGBA8Unorm,
        .usage = luna::ImageUsage::Sampled,
        .debugName = "ErrorCheckerboard",
    };
    _errorCheckerboardImage = create_image(errorTextureDesc, pixels.data());

    const luna::SamplerDesc nearestSamplerDesc{
        .magFilter = luna::FilterMode::Nearest,
        .minFilter = luna::FilterMode::Nearest,
        .mipmapMode = luna::SamplerMipmapMode::Nearest,
        .addressModeU = luna::SamplerAddressMode::Repeat,
        .addressModeV = luna::SamplerAddressMode::Repeat,
        .addressModeW = luna::SamplerAddressMode::Repeat,
        .debugName = "DefaultNearestSampler",
    };
    _defaultSamplerNearest = create_sampler(nearestSamplerDesc);

    const luna::SamplerDesc linearSamplerDesc{
        .magFilter = luna::FilterMode::Linear,
        .minFilter = luna::FilterMode::Linear,
        .mipmapMode = luna::SamplerMipmapMode::Linear,
        .addressModeU = luna::SamplerAddressMode::Repeat,
        .addressModeV = luna::SamplerAddressMode::Repeat,
        .addressModeW = luna::SamplerAddressMode::Repeat,
        .debugName = "DefaultLinearSampler",
    };
    _defaultSamplerLinear = create_sampler(linearSamplerDesc);

    MaterialConstants materialConstants{};
    materialConstants.colorFactors = glm::vec4(1.0f);
    materialConstants.metal_rough_factors = glm::vec4(1.0f);

    const luna::BufferDesc materialConstantsDesc{
        .size = sizeof(MaterialConstants),
        .usage = luna::BufferUsage::Uniform,
        .memoryUsage = luna::MemoryUsage::Upload,
        .debugName = "DefaultMaterialConstants",
    };
    AllocatedBuffer materialConstantsBuffer = create_buffer(materialConstantsDesc, &materialConstants);

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

    auto loadedScene = loadGltf(this, asset_path("basicmesh.glb"));
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

        if (m_triangleVertexBuffer.buffer != VK_NULL_HANDLE) {
            destroy_buffer(m_triangleVertexBuffer);
            m_triangleVertexBuffer = {};
            m_triangleVertexCount = 0;
        }

        if (_trianglePipeline != VK_NULL_HANDLE) {
            _device.destroyPipeline(_trianglePipeline, nullptr);
            _trianglePipeline = VK_NULL_HANDLE;
        }

        if (_trianglePipelineLayout != VK_NULL_HANDLE) {
            _device.destroyPipelineLayout(_trianglePipelineLayout, nullptr);
            _trianglePipelineLayout = VK_NULL_HANDLE;
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
    _swapchainImageFormat = vk::Format::eUndefined;
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
    LUNA_CORE_INFO("RHI shutdown complete");
}

bool VulkanEngine::uses_background_compute() const
{
    return m_legacyRendererMode == LegacyRendererMode::LegacyScene ||
           m_legacyRendererMode == LegacyRendererMode::ComputeBackground;
}

bool VulkanEngine::uses_scene_renderer() const
{
    return m_legacyRendererMode == LegacyRendererMode::LegacyScene;
}

void VulkanEngine::clear_draw_image(vk::CommandBuffer cmd)
{
    const vk::ImageSubresourceRange colorRange = vkinit::image_subresource_range(vk::ImageAspectFlagBits::eColor);
    cmd.clearColorImage(_drawImage.image, vk::ImageLayout::eGeneral, &m_demoClearColor, 1, &colorRange);
}

void VulkanEngine::record_draw_pass(vk::CommandBuffer cmd)
{
    if (uses_scene_renderer()) {
        update_scene();
    }

    vkutil::transition_image(cmd, _drawImage.image, _drawImageLayout, VK_IMAGE_LAYOUT_GENERAL);
    _drawImageLayout = VK_IMAGE_LAYOUT_GENERAL;

    if (uses_background_compute()) {
        record_compute_background(cmd);
    } else {
        clear_draw_image(cmd);
    }

    if (m_legacyRendererMode != LegacyRendererMode::Triangle && !uses_scene_renderer()) {
        return;
    }

    vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    _drawImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    if (m_legacyRendererMode == LegacyRendererMode::Triangle) {
        record_triangle_geometry(cmd);
        return;
    }

    record_scene_geometry(cmd);
}

void VulkanEngine::draw(const OverlayRenderFunction& overlayRenderer, const BeforePresentFunction& beforePresent)
{
    if (_device == VK_NULL_HANDLE || _swapchain == VK_NULL_HANDLE) {
        LUNA_CORE_WARN("Skipping frame because Vulkan device or swapchain is not ready");
        return;
    }

    const vk::Extent2D framebufferExtent = get_framebuffer_extent();
    if (framebufferExtent.width == 0 || framebufferExtent.height == 0) {
        resize_requested = true;
        return;
    }

    if (_frameNumber == 0) {
        LUNA_CORE_INFO("Starting render loop: swapchain={}x{}, draw={}x{}",
                       _swapchainExtent.width,
                       _swapchainExtent.height,
                       _drawImage.imageExtent.width,
                       _drawImage.imageExtent.height);
    }

    VK_CHECK(_device.waitForFences(1, &get_current_frame()._renderFence, VK_TRUE, 1'000'000'000));
    get_current_frame()._deletionQueue.flush();
    get_current_frame()._frameDescriptors.clear_pools(_device);

    VK_CHECK(_device.resetFences(1, &get_current_frame()._renderFence));

    uint32_t swapchainImageIndex = 0;
    bool recreateAfterPresent = false;
    const vk::Result acquireResult = _device.acquireNextImageKHR(
        _swapchain, 1'000'000'000, get_current_frame()._swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (acquireResult == vk::Result::eErrorOutOfDateKHR) {
        logSwapchainResult("vkAcquireNextImageKHR", acquireResult, _swapchainExtent, get_framebuffer_extent());
        resize_requested = true;
        return;
    }
    if (acquireResult == vk::Result::eSuboptimalKHR) {
        resize_requested = true;
        recreateAfterPresent = true;
        logSwapchainResult("vkAcquireNextImageKHR", acquireResult, _swapchainExtent, get_framebuffer_extent());
    } else if (acquireResult != vk::Result::eSuccess) {
        LUNA_CORE_FATAL("Vulkan call failed: vkAcquireNextImageKHR returned {}", vk::to_string(acquireResult));
        std::abort();
    }

    vk::CommandBuffer cmd = get_current_frame()._mainCommandBuffer;

    VK_CHECK(cmd.reset({}));

    vk::CommandBufferBeginInfo cmdBeginInfo =
        vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    _drawExtent.width = std::max(
        1u, static_cast<uint32_t>(std::min(_swapchainExtent.width, _drawImage.imageExtent.width) * renderScale));
    _drawExtent.height = std::max(
        1u, static_cast<uint32_t>(std::min(_swapchainExtent.height, _drawImage.imageExtent.height) * renderScale));

    VK_CHECK(cmd.begin(&cmdBeginInfo));
    if (!m_loggedBeginFramePass) {
        LUNA_CORE_INFO("BeginFrame PASS");
        m_loggedBeginFramePass = true;
    }

    record_draw_pass(cmd);

    // transition the draw image and the swapchain image into their correct transfer layouts
    const VkImageLayout drawImageSourceLayout =
        _drawImageLayout == VK_IMAGE_LAYOUT_GENERAL ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    vkutil::transition_image(cmd, _drawImage.image, drawImageSourceLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
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
    VK_CHECK(cmd.end());

    vk::CommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

    vk::SemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(
        vk::PipelineStageFlagBits2::eColorAttachmentOutput, get_current_frame()._swapchainSemaphore);
    vk::SemaphoreSubmitInfo signalInfo =
        vkinit::semaphore_submit_info(vk::PipelineStageFlagBits2::eAllGraphics, get_current_frame()._renderSemaphore);

    vk::SubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

    VK_CHECK(_graphicsQueue.submit2(1, &submit, get_current_frame()._renderFence));

    if (beforePresent) {
        beforePresent();
    }

    vk::PresentInfoKHR presentInfo{};
    presentInfo.pSwapchains = &_swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &get_current_frame()._renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    const vk::Result presentResult = _graphicsQueue.presentKHR(&presentInfo);
    if (presentResult == vk::Result::eErrorOutOfDateKHR || presentResult == vk::Result::eSuboptimalKHR) {
        resize_requested = true;
        logSwapchainResult("vkQueuePresentKHR", presentResult, _swapchainExtent, get_framebuffer_extent());
    } else if (presentResult != vk::Result::eSuccess) {
        LUNA_CORE_FATAL("Vulkan call failed: vkQueuePresentKHR returned {}", vk::to_string(presentResult));
        std::abort();
    } else {
        if (!m_loggedPresentPass) {
            LUNA_CORE_INFO("Present PASS");
            if (m_legacyRendererMode == LegacyRendererMode::LegacyScene) {
                LUNA_CORE_INFO("Acquire/Present via RHI PASS");
            }
            m_loggedPresentPass = true;
        }
        if (recreateAfterPresent) {
            resize_requested = true;
        }
    }

    _frameNumber++;
}

void VulkanEngine::drawImGui(luna::ImGuiLayer* imguiLayer)
{
    if (imguiLayer == nullptr) {
        draw();
        return;
    }

    draw([imguiLayer](auto commandBuffer, auto targetImageView, auto targetExtent) {
             imguiLayer->render(commandBuffer, targetImageView, targetExtent);
         },
         [imguiLayer]() {
             imguiLayer->renderPlatformWindows();
         });
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

    VkSurfaceKHR rawSurface = VK_NULL_HANDLE;
    const VkResult surfaceResult = glfwCreateWindowSurface(_instance, _window, nullptr, &rawSurface);
    if (surfaceResult != VK_SUCCESS) {
        LUNA_CORE_ERROR("Failed to create Vulkan surface: {}", string_VkResult(surfaceResult));
        return false;
    }
    _surface = rawSurface;
    LUNA_CORE_INFO("Surface created");

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

bool VulkanEngine::create_swapchain(uint32_t width, uint32_t height)
{
    LUNA_CORE_INFO("Creating swapchain for framebuffer {}x{}", width, height);
    vkb::SwapchainBuilder swapchainBuilder{_chosenGPU, _device, _surface};

    _swapchainImageFormat = vk::Format::eB8G8R8A8Unorm;

    auto swapchain_ret = swapchainBuilder
                             .set_desired_format(VkSurfaceFormatKHR{.format = static_cast<VkFormat>(_swapchainImageFormat),
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
    LUNA_CORE_INFO("Swapchain created, Format={}, Extent={}x{}",
                   string_VkFormat(_swapchainImageFormat),
                   _swapchainExtent.width,
                   _swapchainExtent.height);

    return true;
}

void VulkanEngine::record_compute_background(vk::CommandBuffer cmd)
{
    ComputeEffect& effect = backgroundEffects[currentBackgroundEffect];

    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, effect.pipeline);
    cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute, _gradientPipelineLayout, 0, 1, &_drawImageDescriptors, 0, nullptr);
    cmd.pushConstants(
        _gradientPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(ComputePushConstants), &effect.data);
    cmd.dispatch(static_cast<uint32_t>(std::ceil(_drawExtent.width / 16.0f)),
                 static_cast<uint32_t>(std::ceil(_drawExtent.height / 16.0f)),
                 1);
}

void VulkanEngine::record_scene_geometry(vk::CommandBuffer cmd)
{
    // begin a render pass  connected to our draw image
    vk::RenderingAttachmentInfo colorAttachment =
        vkinit::attachment_info(_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vk::RenderingAttachmentInfo depthAttachment =
        vkinit::depth_attachment_info(_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    vkutil::transition_image(cmd, _depthImage.image, _depthImageLayout, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    _depthImageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

    vk::RenderingInfo renderInfo = vkinit::rendering_info(_drawExtent, &colorAttachment, &depthAttachment);
    cmd.beginRendering(&renderInfo);

    // set dynamic viewport and scissor
    vk::Viewport viewport{};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<float>(_drawExtent.width);
    viewport.height = static_cast<float>(_drawExtent.height);
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    cmd.setViewport(0, 1, &viewport);

    vk::Rect2D scissor{};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = _drawExtent.width;
    scissor.extent.height = _drawExtent.height;

    cmd.setScissor(0, 1, &scissor);

    const luna::BufferDesc sceneDataDesc{
        .size = sizeof(GPUSceneData),
        .usage = luna::BufferUsage::Uniform,
        .memoryUsage = luna::MemoryUsage::Upload,
        .debugName = "SceneDataBuffer",
    };
    AllocatedBuffer sceneDataBuffer = create_buffer(sceneDataDesc, &sceneData);

    get_current_frame()._deletionQueue.push_function([=, this]() {
        destroy_buffer(sceneDataBuffer);
    });

    const vk::DescriptorSet globalDescriptor =
        get_current_frame()._frameDescriptors.allocate(_device, _gpuSceneDataDescriptorLayout);

    VulkanResourceBindingRegistry bindingRegistry;
    const luna::BufferHandle sceneBufferHandle = bindingRegistry.register_buffer(sceneDataBuffer.buffer);

    luna::ResourceSetWriteDesc globalWrite{};
    globalWrite.buffers.push_back({0, sceneBufferHandle, 0, sizeof(GPUSceneData), luna::ResourceType::UniformBuffer});
    const bool updatedGlobalBindings = update_resource_set(_device, bindingRegistry, globalDescriptor, globalWrite);
    bindingRegistry.unregister_buffer(sceneBufferHandle);
    if (!updatedGlobalBindings) {
        LUNA_CORE_ERROR("Failed to update global scene bindings via RHI");
    }

    std::sort(mainDrawContext.opaqueSurfaces.begin(), mainDrawContext.opaqueSurfaces.end(), render_object_sort);

    auto draw_objects = [&](const std::vector<RenderObject>& objects) {
        MaterialPipeline* lastPipeline = nullptr;
        MaterialInstance* lastMaterial = nullptr;
        vk::Buffer lastIndexBuffer{};

        for (const RenderObject& draw : objects) {
            if (draw.material == nullptr || draw.material->pipeline == nullptr ||
                !draw.material->pipeline->pipeline || !draw.material->pipeline->layout) {
                continue;
            }

            if (draw.material->pipeline != lastPipeline) {
                lastPipeline = draw.material->pipeline;
                cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, lastPipeline->pipeline);
            }

            if (draw.material != lastMaterial) {
                lastMaterial = draw.material;
                std::array<vk::DescriptorSet, 2> meshDescriptors = {globalDescriptor, draw.material->materialSet};
                cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
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
            cmd.pushConstants(draw.material->pipeline->layout,
                              vk::ShaderStageFlagBits::eVertex,
                              0,
                              sizeof(GPUDrawPushConstants),
                              &pushConstants);

            if (draw.indexBuffer != lastIndexBuffer) {
                lastIndexBuffer = draw.indexBuffer;
                cmd.bindIndexBuffer(draw.indexBuffer, 0, vk::IndexType::eUint32);
            }

            cmd.drawIndexed(draw.indexCount, 1, draw.firstIndex, 0, 0);
        }
    };

    draw_objects(mainDrawContext.opaqueSurfaces);
    draw_objects(mainDrawContext.transparentSurfaces);

    cmd.endRendering();
}

void VulkanEngine::record_triangle_geometry(vk::CommandBuffer cmd)
{
    if (!_trianglePipeline || !m_triangleVertexBuffer.buffer || m_triangleVertexCount == 0) {
        return;
    }

    vk::RenderingAttachmentInfo colorAttachment =
        vkinit::attachment_info(_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vk::RenderingAttachmentInfo depthAttachment =
        vkinit::depth_attachment_info(_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    vkutil::transition_image(cmd, _depthImage.image, _depthImageLayout, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    _depthImageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

    vk::RenderingInfo renderInfo = vkinit::rendering_info(_drawExtent, &colorAttachment, &depthAttachment);
    cmd.beginRendering(&renderInfo);

    vk::Viewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(_drawExtent.width);
    viewport.height = static_cast<float>(_drawExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    cmd.setViewport(0, 1, &viewport);

    vk::Rect2D scissor{};
    scissor.offset = vk::Offset2D{0, 0};
    scissor.extent = _drawExtent;
    cmd.setScissor(0, 1, &scissor);

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, _trianglePipeline);
    const vk::DeviceSize vertexOffset = 0;
    cmd.bindVertexBuffers(0, 1, &m_triangleVertexBuffer.buffer, &vertexOffset);
    cmd.draw(m_triangleVertexCount, 1, 0, 0);
    cmd.endRendering();
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
    _swapchainImageFormat = vk::Format::eUndefined;
}

bool VulkanEngine::init_swapchain()
{
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
    if (!create_swapchain(framebufferExtent.width, framebufferExtent.height)) {
        return false;
    }

    return create_draw_resources(framebufferExtent);
}

bool VulkanEngine::init_commands()
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

bool VulkanEngine::init_sync_structures()
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

bool VulkanEngine::init_descriptors()
{
    if (!uses_background_compute()) {
        return true;
    }

    DescriptorLayoutBuilder builder;
    if (!reflect_shader_bindings(
            shader_path("Internal/gradient.spv"), luna::ShaderType::Compute, VK_SHADER_STAGE_COMPUTE_BIT, builder) ||
        !reflect_shader_bindings(
            shader_path("Internal/sky.spv"), luna::ShaderType::Compute, VK_SHADER_STAGE_COMPUTE_BIT, builder)) {
        return false;
    }

    if (builder.bindings.empty()) {
        LUNA_CORE_ERROR("No descriptor bindings were reflected for compute shaders");
        return false;
    }

    const auto drawImageBindingIt =
        std::find_if(builder.bindings.begin(), builder.bindings.end(), [](const vk::DescriptorSetLayoutBinding& binding) {
            return binding.descriptorType == vk::DescriptorType::eStorageImage;
        });
    if (drawImageBindingIt == builder.bindings.end()) {
        LUNA_CORE_ERROR("Failed to find reflected storage image binding for background compute shaders");
        return false;
    }

    _drawImageDescriptorBinding = drawImageBindingIt->binding;
    _drawImageDescriptorCount = drawImageBindingIt->descriptorCount;
    _drawImageDescriptorType = drawImageBindingIt->descriptorType;

    if (_drawImageDescriptorType != vk::DescriptorType::eStorageImage) {
        LUNA_CORE_ERROR("Background compute shaders must expose a storage image descriptor in set 0");
        return false;
    }

    if (_drawImageDescriptorCount != 1) {
        LUNA_CORE_ERROR("Only single storage image descriptors are currently supported, reflected count={}",
                        _drawImageDescriptorCount);
        return false;
    }

    luna::ResourceLayoutDesc globalLayoutDesc{};
    globalLayoutDesc.debugName = "GlobalLayout";
    globalLayoutDesc.bindings.push_back(
        {0, luna::ResourceType::UniformBuffer, 1, luna::ShaderType::Vertex | luna::ShaderType::Fragment});
    _gpuSceneDataDescriptorLayout = build_resource_layout(_device, globalLayoutDesc);

    std::array<DescriptorAllocatorGrowable::PoolSizeRatio, 2> frameSizes = {
        DescriptorAllocatorGrowable::PoolSizeRatio{vk::DescriptorType::eUniformBuffer, 3.f},
        DescriptorAllocatorGrowable::PoolSizeRatio{vk::DescriptorType::eCombinedImageSampler, 3.f},
    };
    for (int i = 0; i < FRAME_OVERLAP; i++) {
        _frames[i]._frameDescriptors.init(_device, 1000, frameSizes);
    }

    auto sizes = build_pool_ratios(builder);
    sizes.push_back(DescriptorAllocator::PoolSizeRatio{vk::DescriptorType::eUniformBuffer, 3.0f});
    sizes.push_back(DescriptorAllocator::PoolSizeRatio{vk::DescriptorType::eCombinedImageSampler, 3.0f});
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

    if (uses_scene_renderer()) {
        init_mesh_pipeline();
    }

    if (uses_background_compute()) {
        return init_background_pipelines();
    }

    return true;
}

void VulkanEngine::init_triangle_pipeline()
{
    auto resolve_triangle_shader_path = [&](const std::filesystem::path& overridePath,
                                            std::string_view fallbackRelativePath) -> std::filesystem::path {
        if (!overridePath.empty()) {
            return overridePath.lexically_normal();
        }

        return shader_path(fallbackRelativePath);
    };

    vk::ShaderModule triangleFragShader{};
    const std::string triangleFragmentPath =
        luna::paths::display(resolve_triangle_shader_path(m_triangleFragmentShaderPath, "Internal/colored_triangle.frag.spv"));
    if (!vkutil::load_shader_module(triangleFragmentPath.c_str(), _device, &triangleFragShader)) {
        LUNA_CORE_ERROR("Error when building the triangle fragment shader module");
    } else {
        LUNA_CORE_INFO("Triangle fragment shader succesfully loaded");
    }

    vk::ShaderModule triangleVertexShader{};
    const std::string triangleVertexPath =
        luna::paths::display(resolve_triangle_shader_path(m_triangleVertexShaderPath, "Internal/colored_triangle.vert.spv"));
    if (!vkutil::load_shader_module(triangleVertexPath.c_str(), _device, &triangleVertexShader)) {
        LUNA_CORE_ERROR("Error when building the triangle vertex shader module");
    } else {
        LUNA_CORE_INFO("Triangle vertex shader succesfully loaded");
    }

    if (!triangleFragShader || !triangleVertexShader) {
        if (triangleFragShader) {
            _device.destroyShaderModule(triangleFragShader, nullptr);
        }
        if (triangleVertexShader) {
            _device.destroyShaderModule(triangleVertexShader, nullptr);
        }
        return;
    }

    // build the pipeline layout that controls the inputs/outputs of the shader
    // we are not using descriptor sets or other systems yet, so no need to use anything other than empty default
    vk::PipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
    VK_CHECK(_device.createPipelineLayout(&pipeline_layout_info, nullptr, &_trianglePipelineLayout));

    PipelineBuilder pipelineBuilder;

    // use the triangle layout we created
    pipelineBuilder._pipelineLayout = _trianglePipelineLayout;
    // connecting the vertex and pixel shaders to the pipeline
    pipelineBuilder.set_shaders(triangleVertexShader, triangleFragShader);
    const bool usesCustomTriangleShaders = !m_triangleVertexShaderPath.empty() || !m_triangleFragmentShaderPath.empty();
    if (usesCustomTriangleShaders) {
        const vk::VertexInputBindingDescription vertexBinding{
            0, static_cast<uint32_t>(sizeof(TriangleVertex)), vk::VertexInputRate::eVertex};
        const std::array<vk::VertexInputAttributeDescription, 2> vertexAttributes = {
            vk::VertexInputAttributeDescription{0, 0, vk::Format::eR32G32Sfloat, offsetof(TriangleVertex, position)},
            vk::VertexInputAttributeDescription{1, 0, vk::Format::eR32G32B32Sfloat, offsetof(TriangleVertex, color)},
        };
        pipelineBuilder.set_vertex_input(std::span<const vk::VertexInputBindingDescription>(&vertexBinding, 1),
                                         std::span<const vk::VertexInputAttributeDescription>(vertexAttributes));
    }
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
    // this demo triangle does not need depth testing
    pipelineBuilder.disable_depthtest();

    // connect the image format we will draw into, from draw image
    pipelineBuilder.set_color_attachment_format(_drawImage.imageFormat);
    pipelineBuilder.set_depth_format(_depthImage.imageFormat);

    // finally build the pipeline
    _trianglePipeline = pipelineBuilder.build_pipeline(_device);
    if (_trianglePipeline) {
        LUNA_CORE_INFO("Graphics pipeline created: Triangle");
    }

    // clean structures
    _device.destroyShaderModule(triangleFragShader, nullptr);
    _device.destroyShaderModule(triangleVertexShader, nullptr);
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

    vk::PipelineLayoutCreateInfo computeLayout{};
    computeLayout.pSetLayouts = &_drawImageDescriptorLayout;
    computeLayout.setLayoutCount = 1;

    vk::PushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(ComputePushConstants);
    pushConstant.stageFlags = vk::ShaderStageFlagBits::eCompute;
    computeLayout.pPushConstantRanges = &pushConstant;
    computeLayout.pushConstantRangeCount = 1;

    VK_CHECK(_device.createPipelineLayout(&computeLayout, nullptr, &_gradientPipelineLayout));

    vk::ShaderModule computeDrawShader{};
    const std::string gradientShaderPath = luna::paths::display(shader_path("Internal/gradient.spv"));
    vkutil::load_shader_module(gradientShaderPath.c_str(), _device, &computeDrawShader);
    vk::ShaderModule skyShader{};
    const std::string skyShaderPath = luna::paths::display(shader_path("Internal/sky.spv"));
    vkutil::load_shader_module(skyShaderPath.c_str(), _device, &skyShader);

    vk::ComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.layout = _gradientPipelineLayout;
    computePipelineCreateInfo.stage =
        vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eCompute, computeDrawShader);

    computePipelineCreateInfo.stage.module = computeDrawShader;
    VK_CHECK(_device.createComputePipelines({}, 1, &computePipelineCreateInfo, nullptr, &_gradientPipeline));

    ComputeEffect gradient;
    gradient.layout = _gradientPipelineLayout;
    gradient.name = "gradient";
    gradient.pipeline = _gradientPipeline;
    gradient.data.data1 = glm::vec4(1, 0, 0, 1);
    gradient.data.data2 = glm::vec4(0, 0, 1, 1);

    computePipelineCreateInfo.stage.module = skyShader;
    vk::Pipeline skyPipeline{};
    VK_CHECK(_device.createComputePipelines({}, 1, &computePipelineCreateInfo, nullptr, &skyPipeline));

    ComputeEffect sky;
    sky.layout = _gradientPipelineLayout;
    sky.name = "sky";
    sky.pipeline = skyPipeline;
    sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

    backgroundEffects.push_back(gradient);
    backgroundEffects.push_back(sky);

    _device.destroyShaderModule(computeDrawShader, nullptr);
    _device.destroyShaderModule(skyShader, nullptr);

    vk::Pipeline p1 = _gradientPipeline;
    vk::Pipeline p2 = skyPipeline;
    vk::PipelineLayout layout = _gradientPipelineLayout;

    _mainDeletionQueue.push_function([=, this]() {
        _device.destroyPipelineLayout(layout, nullptr);
        _device.destroyPipeline(p1, nullptr);
        _device.destroyPipeline(p2, nullptr);
    });

    LUNA_CORE_INFO("Initialized background compute pipeline");
    return true;
}

bool VulkanEngine::resize_swapchain()
{
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
    destroy_draw_resources();
    destroy_swapchain();
    if (!create_swapchain(framebufferExtent.width, framebufferExtent.height)) {
        LUNA_CORE_ERROR("Swapchain recreation failed");
        return false;
    }

    if (!create_draw_resources(framebufferExtent)) {
        LUNA_CORE_ERROR("Draw resource recreation failed");
        return false;
    }

    if (_drawImageDescriptors) {
        update_draw_image_descriptors();
    }

    resize_requested = false;
    LUNA_CORE_INFO("Swapchain recreated");
    return true;
}

bool VulkanEngine::create_draw_resources(vk::Extent2D extent)
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
    if (m_legacyRendererMode == LegacyRendererMode::LegacyScene) {
        LUNA_CORE_INFO("Offscreen color/depth images created via RHI: {}x{}", extent.width, extent.height);
    }
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
    if (!_device || !_drawImageDescriptors || !_drawImage.imageView) {
        return;
    }

    DescriptorWriter writer;
    writer.write_image(_drawImageDescriptorBinding,
                       _drawImage.imageView,
                       vk::Sampler{},
                       vk::ImageLayout::eGeneral,
                       _drawImageDescriptorType);
    writer.update_set(_device, _drawImageDescriptors);
}

vk::Extent2D VulkanEngine::get_framebuffer_extent() const
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

AllocatedImage VulkanEngine::create_image(vk::Extent3D size,
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

AllocatedImage VulkanEngine::create_image(void* data,
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

void VulkanEngine::destroy_image(const AllocatedImage& image)
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

AllocatedBuffer VulkanEngine::create_buffer(const luna::BufferDesc& desc, const void* initialData)
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

AllocatedImage VulkanEngine::create_image(const luna::ImageDesc& desc, const void* initialData)
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

    const uint32_t viewLayerCount = desc.type == luna::ImageType::Image2DArray ? desc.arrayLayers : 1u;
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

vk::Sampler VulkanEngine::create_sampler(const luna::SamplerDesc& desc)
{
    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter = to_vulkan_filter(desc.magFilter);
    samplerInfo.minFilter = to_vulkan_filter(desc.minFilter);
    samplerInfo.mipmapMode = to_vulkan_mipmap_mode(desc.mipmapMode);
    samplerInfo.addressModeU = to_vulkan_sampler_address_mode(desc.addressModeU);
    samplerInfo.addressModeV = to_vulkan_sampler_address_mode(desc.addressModeV);
    samplerInfo.addressModeW = to_vulkan_sampler_address_mode(desc.addressModeW);
    samplerInfo.minLod = desc.minLod;
    samplerInfo.maxLod = desc.maxLod;

    vk::Sampler sampler{};
    VK_CHECK(_device.createSampler(&samplerInfo, nullptr, &sampler));
    return sampler;
}

AllocatedBuffer VulkanEngine::create_buffer(size_t allocSize, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
    // allocate buffer
    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;
    bufferInfo.usage = static_cast<VkBufferUsageFlags>(usage);

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = memoryUsage;
    vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    AllocatedBuffer newBuffer;

    // allocate the buffer
    VkBuffer rawBuffer = VK_NULL_HANDLE;
    VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaallocInfo, &rawBuffer, &newBuffer.allocation, &newBuffer.info));
    newBuffer.buffer = rawBuffer;

    return newBuffer;
}

void VulkanEngine::destroy_buffer(const AllocatedBuffer& buffer)
{
    vmaDestroyBuffer(_allocator, static_cast<VkBuffer>(buffer.buffer), buffer.allocation);
}

bool VulkanEngine::uploadBufferData(const AllocatedBuffer& buffer, const void* data, size_t size, size_t offset)
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

bool VulkanEngine::uploadTriangleVertices(std::span<const TriangleVertex> vertices)
{
    if (_device == VK_NULL_HANDLE || _allocator == VK_NULL_HANDLE) {
        LUNA_CORE_ERROR("Cannot upload triangle vertices because Vulkan device is not initialized");
        return false;
    }

    if (vertices.empty()) {
        LUNA_CORE_ERROR("Triangle vertex upload requires at least one vertex");
        return false;
    }

    if (m_triangleVertexBuffer.buffer != VK_NULL_HANDLE) {
        destroy_buffer(m_triangleVertexBuffer);
        m_triangleVertexBuffer = {};
        m_triangleVertexCount = 0;
    }

    const size_t vertexBufferSize = vertices.size() * sizeof(TriangleVertex);
    AllocatedBuffer stagingBuffer =
        create_buffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    std::memcpy(stagingBuffer.info.pMappedData, vertices.data(), vertexBufferSize);

    m_triangleVertexBuffer =
        create_buffer(vertexBufferSize,
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      VMA_MEMORY_USAGE_GPU_ONLY);

    immediate_submit([&](vk::CommandBuffer cmd) {
        vk::BufferCopy vertexCopy{};
        vertexCopy.srcOffset = 0;
        vertexCopy.dstOffset = 0;
        vertexCopy.size = vertexBufferSize;
        cmd.copyBuffer(stagingBuffer.buffer, m_triangleVertexBuffer.buffer, 1, &vertexCopy);
    });

    destroy_buffer(stagingBuffer);
    m_triangleVertexCount = static_cast<uint32_t>(vertices.size());
    LUNA_CORE_INFO("Uploaded {} vertices", m_triangleVertexCount);
    return true;
}

void VulkanEngine::immediate_submit(const std::function<void(vk::CommandBuffer cmd)>& function)
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

    immediate_submit([&](vk::CommandBuffer cmd) {
        vk::BufferCopy vertexCopy{};
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = vertexBufferSize;

        cmd.copyBuffer(staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

        vk::BufferCopy indexCopy{};
        indexCopy.dstOffset = 0;
        indexCopy.srcOffset = vertexBufferSize;
        indexCopy.size = indexBufferSize;

        cmd.copyBuffer(staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
    });

    destroy_buffer(staging);
    if (m_legacyRendererMode == LegacyRendererMode::LegacyScene) {
        LUNA_CORE_INFO("Mesh buffers uploaded via RHI: vertices={}, indices={}", vertices.size(), indices.size());
    }

    return newSurface;
}
