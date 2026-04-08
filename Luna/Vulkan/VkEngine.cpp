#include "VkEngine.h"

#include <Core/Log.h>

#define GLFW_INCLUDE_NONE
#include "VkImages.h"
#include "VkInitializers.h"
#include "VkPipelines.h"
#include "VkShader.h"
#include "VkTypes.h"
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
#include <utility>

VulkanEngine* loaded_engine = nullptr;

#ifndef NDEBUG
constexpr bool b_use_validation_layers = true;
#else
constexpr bool b_use_validation_layers = false;
#endif

namespace {
constexpr uint32_t k_frame_descriptor_set_count = 1000;
constexpr vk::DeviceSize k_frame_uniform_arena_size = 256 * 1024;
constexpr vk::DeviceSize k_frame_staging_arena_size = 4 * 1024 * 1024;

uint32_t packRgba8(float r, float g, float b, float a)
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

void logSwapchainResult(const char* step, vk::Result result, vk::Extent2D swapchain_extent, vk::Extent2D framebuffer_extent)
{
    LUNA_CORE_WARN("{} returned {}. swapchain={}x{}, framebuffer={}x{}",
                   step,
                   vk::to_string(result),
                   swapchain_extent.width,
                   swapchain_extent.height,
                   framebuffer_extent.width,
                   framebuffer_extent.height);
}

void logSwapchainResult(const char* step, VkResult result, vk::Extent2D swapchain_extent, vk::Extent2D framebuffer_extent)
{
    logSwapchainResult(step, static_cast<vk::Result>(result), swapchain_extent, framebuffer_extent);
}

std::optional<std::vector<uint32_t>> loadSpirvCode(const std::filesystem::path& file_path)
{
    std::ifstream file(file_path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        return std::nullopt;
    }

    const size_t file_size = static_cast<size_t>(file.tellg());
    if (file_size == 0 || (file_size % sizeof(uint32_t)) != 0) {
        return std::nullopt;
    }

    std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(file_size));
    file.close();

    return buffer;
}

bool reflectShaderBindings(const std::filesystem::path& file_path,
                             luna::ShaderType shader_type,
                             vk::ShaderStageFlags shader_stages,
                             DescriptorLayoutBuilder& builder)
{
    const auto spirv_code = loadSpirvCode(file_path);
    if (!spirv_code.has_value()) {
        LUNA_CORE_ERROR("Failed to read SPIR-V shader for reflection: {}", file_path.string());
        return false;
    }

    const luna::VulkanShader shader(*spirv_code, shader_type);
    builder.addBindingsFromReflection(shader.getReflectionMap(), 0, shader_stages);
    return true;
}

bool reflectShaderBindings(const std::filesystem::path& file_path,
                             luna::ShaderType shader_type,
                             VkShaderStageFlags shader_stages,
                             DescriptorLayoutBuilder& builder)
{
    return reflectShaderBindings(file_path, shader_type, static_cast<vk::ShaderStageFlags>(shader_stages), builder);
}

std::vector<DescriptorAllocator::PoolSizeRatio> buildPoolRatios(const DescriptorLayoutBuilder& builder)
{
    std::map<vk::DescriptorType, float> descriptor_counts;
    for (const auto& binding : builder.m_bindings) {
        descriptor_counts[binding.descriptorType] += static_cast<float>(std::max(1u, binding.descriptorCount));
    }

    std::vector<DescriptorAllocator::PoolSizeRatio> pool_ratios;
    pool_ratios.reserve(descriptor_counts.size());
    for (const auto& [type, count] : descriptor_counts) {
        pool_ratios.push_back(DescriptorAllocator::PoolSizeRatio{type, count});
    }

    return pool_ratios;
}

bool loadShaderModuleWithFallback(vk::Device device,
                                      vk::ShaderModule* out_shader_module,
                                      std::initializer_list<const char*> paths,
                                      const char* stage_label)
{
    for (const char* path : paths) {
        if (vkutil::loadShaderModule(path, device, out_shader_module)) {
            LUNA_CORE_INFO("Loaded {} shader module from '{}'", stage_label, path);
            return true;
        }
    }

    LUNA_CORE_ERROR("Failed to load {} shader module", stage_label);
    return false;
}

bool loadShaderModuleWithFallback(VkDevice device,
                                       VkShaderModule* out_shader_module,
                                       std::initializer_list<const char*> paths,
                                       const char* stage_label)
{
    vk::ShaderModule shader_module{};
    const bool loaded =
        loadShaderModuleWithFallback(vk::Device(device), &shader_module, paths, stage_label);
    if (loaded) {
        *out_shader_module = static_cast<VkShaderModule>(shader_module);
    }
    return loaded;
}

template <typename T> std::shared_ptr<T> retainResource(T&& resource)
{
    return std::make_shared<T>(std::move(resource));
}

bool renderObjectSort(const RenderObject& a, const RenderObject& b)
{
    const MaterialPipeline* a_pipeline = a.m_material != nullptr ? a.m_material->m_pipeline : nullptr;
    const MaterialPipeline* b_pipeline = b.m_material != nullptr ? b.m_material->m_pipeline : nullptr;
    if (a_pipeline != b_pipeline) {
        return a_pipeline < b_pipeline;
    }

    if (a.m_material != b.m_material) {
        return a.m_material < b.m_material;
    }

    if (a.m_index_buffer != b.m_index_buffer) {
        return a.m_index_buffer < b.m_index_buffer;
    }

    if (a.m_vertex_buffer_address != b.m_vertex_buffer_address) {
        return a.m_vertex_buffer_address < b.m_vertex_buffer_address;
    }

    return a.m_first_index < b.m_first_index;
}

} // namespace

void GltfMetallicRoughness::buildPipelines(VulkanEngine* engine)
{
    const vk::Device device = engine->getDeviceHandle();

    DescriptorLayoutBuilder material_layout_builder;
    material_layout_builder.addBinding(0, vk::DescriptorType::eUniformBuffer);
    material_layout_builder.addBinding(1, vk::DescriptorType::eCombinedImageSampler);
    material_layout_builder.addBinding(2, vk::DescriptorType::eCombinedImageSampler);
    m_material_layout = material_layout_builder.build(device, vk::ShaderStageFlagBits::eFragment);

    vk::ShaderModule mesh_frag_shader{};
    vk::ShaderModule mesh_vertex_shader{};
    const bool loaded_frag = loadShaderModuleWithFallback(device,
                                                             &mesh_frag_shader,
                                                             {"../Shaders/Internal/mesh.frag.spv"},
                                                             "mesh fragment");
    const bool loaded_vert = loadShaderModuleWithFallback(device,
                                                             &mesh_vertex_shader,
                                                             {"../Shaders/Internal/mesh.vert.spv"},
                                                             "mesh vertex");
    if (!loaded_frag || !loaded_vert) {
        if (mesh_frag_shader) {
            device.destroyShaderModule(mesh_frag_shader, nullptr);
        }
        if (mesh_vertex_shader) {
            device.destroyShaderModule(mesh_vertex_shader, nullptr);
        }
        return;
    }

    vk::PushConstantRange matrix_range{};
    matrix_range.offset = 0;
    matrix_range.size = sizeof(GPUDrawPushConstants);
    matrix_range.stageFlags = vk::ShaderStageFlagBits::eVertex;

    std::array<vk::DescriptorSetLayout, 2> layouts = {engine->m_gpu_scene_data_descriptor_layout, m_material_layout};
    vk::PipelineLayoutCreateInfo mesh_layout_info = vkinit::pipelineLayoutCreateInfo();
    mesh_layout_info.setLayoutCount = static_cast<uint32_t>(layouts.size());
    mesh_layout_info.pSetLayouts = layouts.data();
    mesh_layout_info.pushConstantRangeCount = 1;
    mesh_layout_info.pPushConstantRanges = &matrix_range;

    vk::PipelineLayout new_layout{};
    VK_CHECK(device.createPipelineLayout(&mesh_layout_info, nullptr, &new_layout));

    m_opaque_pipeline.m_layout = new_layout;
    m_transparent_pipeline.m_layout = new_layout;

    PipelineBuilder pipeline_builder;
    pipeline_builder.setShaders(mesh_vertex_shader, mesh_frag_shader);
    pipeline_builder.setInputTopology(vk::PrimitiveTopology::eTriangleList);
    pipeline_builder.setPolygonMode(vk::PolygonMode::eFill);
    pipeline_builder.setCullMode(vk::CullModeFlagBits::eNone, vk::FrontFace::eClockwise);
    pipeline_builder.setMultisamplingNone();
    pipeline_builder.disableBlending();
    pipeline_builder.enableDepthtest(true, vk::CompareOp::eLessOrEqual);
    pipeline_builder.setColorAttachmentFormat(engine->getRenderTarget().getColorFormat());
    pipeline_builder.setDepthFormat(engine->getRenderTarget().getDepthFormat());
    pipeline_builder.m_pipeline_layout = new_layout;

    m_opaque_pipeline.m_pipeline = pipeline_builder.buildPipeline(device);

    pipeline_builder.enableBlendingAlphablend();
    pipeline_builder.enableDepthtest(false, vk::CompareOp::eLessOrEqual);
    m_transparent_pipeline.m_pipeline = pipeline_builder.buildPipeline(device);

    device.destroyShaderModule(mesh_frag_shader, nullptr);
    device.destroyShaderModule(mesh_vertex_shader, nullptr);
}

void GltfMetallicRoughness::clearResources(vk::Device device)
{
    if (m_opaque_pipeline.m_pipeline) {
        device.destroyPipeline(m_opaque_pipeline.m_pipeline, nullptr);
        m_opaque_pipeline.m_pipeline = nullptr;
    }

    if (m_transparent_pipeline.m_pipeline) {
        device.destroyPipeline(m_transparent_pipeline.m_pipeline, nullptr);
        m_transparent_pipeline.m_pipeline = nullptr;
    }

    if (m_opaque_pipeline.m_layout) {
        device.destroyPipelineLayout(m_opaque_pipeline.m_layout, nullptr);
        m_opaque_pipeline.m_layout = nullptr;
        m_transparent_pipeline.m_layout = nullptr;
    }

    if (m_material_layout) {
        device.destroyDescriptorSetLayout(m_material_layout, nullptr);
        m_material_layout = nullptr;
    }
}

MaterialInstance GltfMetallicRoughness::writeMaterial(vk::Device device,
                                                        MaterialPass pass,
                                                        const MaterialResources& resources,
                                                        DescriptorAllocator& descriptor_allocator)
{
    MaterialInstance material_instance;
    material_instance.m_pass_type = pass;
    material_instance.m_pipeline = pass == MaterialPass::Transparent ? &m_transparent_pipeline : &m_opaque_pipeline;
    material_instance.m_material_set = descriptor_allocator.allocate(device, m_material_layout);

    DescriptorWriter writer;
    writer.writeBuffer(0,
                        resources.m_data_buffer->get(),
                        sizeof(MaterialConstants),
                        resources.m_data_buffer_offset,
                        vk::DescriptorType::eUniformBuffer);
    writer.writeImage(1,
                       resources.m_color_image->getView(),
                       resources.m_color_sampler->get(),
                       vk::ImageLayout::eShaderReadOnlyOptimal,
                       vk::DescriptorType::eCombinedImageSampler);
    writer.writeImage(2,
                       resources.m_metal_rough_image->getView(),
                       resources.m_metal_rough_sampler->get(),
                       vk::ImageLayout::eShaderReadOnlyOptimal,
                       vk::DescriptorType::eCombinedImageSampler);
    writer.updateSet(device, material_instance.m_material_set);

    return material_instance;
}

VulkanEngine& VulkanEngine::get()
{
    return *loaded_engine;
}

bool VulkanEngine::init(luna::Window& window_ref)
{
    if (loaded_engine != nullptr) {
        LUNA_CORE_ERROR("VulkanEngine already initialized");
        return false;
    }

    loaded_engine = this;
    LUNA_CORE_INFO("Initializing Vulkan engine");

    m_frame_number = 0;
    m_resize_requested = false;
    m_draw_extent = {};
    m_window = static_cast<GLFWwindow*>(window_ref.getNativeWindow());
    if (m_window == nullptr) {
        LUNA_CORE_ERROR("Failed to acquire native GLFW window");
        loaded_engine = nullptr;
        return false;
    }

    m_window_extent = {window_ref.getWidth(), window_ref.getHeight()};
    const vk::Extent2D framebuffer_extent = getFramebufferExtent();
    m_deferred_release.initialize(frame_overlap);
    LUNA_CORE_INFO("Created GLFW window: logical={}x{}, framebuffer={}x{}",
                   m_window_extent.width,
                   m_window_extent.height,
                   framebuffer_extent.width,
                   framebuffer_extent.height);

    if (!initVulkan() || !initSwapchain() || !initCommands() || !initSyncStructures() || !initDescriptors() ||
        !initPipelines()) {
        cleanup();
        return false;
    }

    m_is_initialized = true;
    initDefaultData();
    LUNA_CORE_INFO("Vulkan engine initialized");

    return true;
}

void VulkanEngine::initDefaultData()
{
    m_main_camera.m_position = glm::vec3(0.0f, 0.0f, 5.0f);
    m_main_camera.m_pitch = 0.0f;
    m_main_camera.m_yaw = 0.0f;

    uint32_t white = packRgba8(1.0f, 1.0f, 1.0f, 1.0f);
    uint32_t grey = packRgba8(0.66f, 0.66f, 0.66f, 1.0f);
    uint32_t black = packRgba8(0.0f, 0.0f, 0.0f, 0.0f);

    m_white_image = createImage(
        &white, vk::Extent3D{1, 1, 1}, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled);
    m_grey_image =
        createImage(&grey, vk::Extent3D{1, 1, 1}, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled);
    m_black_image = createImage(
        &black, vk::Extent3D{1, 1, 1}, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled);

    std::array<uint32_t, 16 * 16> pixels;
    for (uint32_t x = 0; x < 16; x++) {
        for (uint32_t y = 0; y < 16; y++) {
            pixels[y * 16 + x] = (x % 2 == y % 2) ? 0xFF000000 : 0xFFFF00FF;
        }
    }
    m_error_checkerboard_image = createImage(
        pixels.data(), VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    vk::SamplerCreateInfo sampl{};
    sampl.magFilter = vk::Filter::eNearest;
    sampl.minFilter = vk::Filter::eNearest;
    VK_CHECK(m_default_sampler_nearest.create(m_device, sampl) ? vk::Result::eSuccess : vk::Result::eErrorInitializationFailed);

    sampl.magFilter = vk::Filter::eLinear;
    sampl.minFilter = vk::Filter::eLinear;
    VK_CHECK(m_default_sampler_linear.create(m_device, sampl) ? vk::Result::eSuccess : vk::Result::eErrorInitializationFailed);

    MaterialConstants material_constants{};
    material_constants.m_color_factors = glm::vec4(1.0f);
    material_constants.m_metal_rough_factors = glm::vec4(1.0f);

    AllocatedBuffer material_constants_buffer =
        createBuffer(sizeof(MaterialConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    std::memcpy(material_constants_buffer.getMappedData(), &material_constants, sizeof(MaterialConstants));

    MaterialResources material_resources{};
    material_resources.m_color_image = &m_white_image;
    material_resources.m_color_sampler = &m_default_sampler_linear;
    material_resources.m_metal_rough_image = &m_white_image;
    material_resources.m_metal_rough_sampler = &m_default_sampler_linear;
    material_resources.m_data_buffer = &material_constants_buffer;
    material_resources.m_data_buffer_offset = 0;

    m_default_material_instance =
        m_metal_rough_material.writeMaterial(m_device, MaterialPass::MainColor, material_resources, m_global_descriptor_allocator);

    auto retained_material_constants_buffer = std::make_shared<AllocatedBuffer>(std::move(material_constants_buffer));
    m_shutdown_release.push([this, retained_material_constants_buffer]() mutable {
        m_loaded_scenes.clear();
        m_main_draw_context.clear();
        destroyImageImmediate(m_white_image);
        destroyImageImmediate(m_grey_image);
        destroyImageImmediate(m_black_image);
        destroyImageImmediate(m_error_checkerboard_image);
        retained_material_constants_buffer.reset();

        m_default_sampler_linear.destroy(m_device);
        m_default_sampler_nearest.destroy(m_device);
    });

    auto loaded_scene = loadGltf(this, "../assets/basicmesh.glb");
    if (!loaded_scene.has_value()) {
        LUNA_CORE_WARN("Falling back to empty scene set because basicmesh.glb failed to load");
        return;
    }

    m_loaded_scenes.clear();
    m_main_draw_context.clear();
    m_loaded_scenes["basicmesh"] = std::move(loaded_scene.value());

    m_shutdown_release.push([this]() {
        m_loaded_scenes.clear();
        m_main_draw_context.clear();
    });
}

void VulkanEngine::cleanup()
{
    LUNA_CORE_INFO("Cleaning up Vulkan engine");

    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);

        m_shutdown_release.flush();
        m_deferred_release.flushAll();

        destroySwapchain();
        destroyRenderTarget();

        for (int i = 0; i < frame_overlap; i++) {
            m_frames[i].destroy(m_device);
        }

        if (m_imm_context.m_command_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_device, m_imm_context.m_command_pool, nullptr);
            m_imm_context.m_command_pool = VK_NULL_HANDLE;
            m_imm_context.m_command_buffer = VK_NULL_HANDLE;
        }

        if (m_imm_context.m_fence != VK_NULL_HANDLE) {
            vkDestroyFence(m_device, m_imm_context.m_fence, nullptr);
            m_imm_context.m_fence = VK_NULL_HANDLE;
        }

        destroyBufferImmediate(m_rectangle.m_index_buffer);
        destroyBufferImmediate(m_rectangle.m_vertex_buffer);
        m_allocator.destroy();
    }

    m_device_context.destroy();
    m_graphics_queue_context.reset();
    m_physical_device_context.reset();
    m_instance_context.destroy();

    m_window = nullptr;
    m_instance = VK_NULL_HANDLE;
    m_debug_messenger = VK_NULL_HANDLE;
    m_chosen_gpu = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
    m_surface = VK_NULL_HANDLE;
    m_graphics_queue = VK_NULL_HANDLE;
    m_graphics_queue_family = 0;
    m_uniform_buffer_alignment = 1;
    m_frame_number = 0;
    m_resize_requested = false;
    m_draw_extent = {};
    m_draw_image_descriptors = VK_NULL_HANDLE;
    m_draw_image_descriptor_layout = VK_NULL_HANDLE;
    m_gpu_scene_data_descriptor_layout = VK_NULL_HANDLE;
    m_gradient_pipeline = VK_NULL_HANDLE;
    m_gradient_pipeline_layout = VK_NULL_HANDLE;
    m_default_sampler_linear.reset();
    m_default_sampler_nearest.reset();
    destroyImageImmediate(m_white_image);
    destroyImageImmediate(m_grey_image);
    destroyImageImmediate(m_black_image);
    destroyImageImmediate(m_error_checkerboard_image);
    m_default_material_instance = {};
    m_metal_rough_material = {};
    m_shutdown_release.clear();
    m_deferred_release.reset();
    m_is_initialized = false;
    loaded_engine = nullptr;

    LUNA_CORE_INFO("Vulkan engine cleanup complete");
}

void VulkanEngine::draw(const RenderFunction& render_function,
                        const OverlayRenderFunction& overlay_renderer,
                        const BeforePresentFunction& before_present)
{
    if (m_device == VK_NULL_HANDLE || !m_swapchain_context.isValid()) {
        LUNA_CORE_WARN("Skipping frame because Vulkan device or swapchain is not ready");
        return;
    }

    const vk::Extent2D swapchain_extent = m_swapchain_context.getExtent();

    if (m_frame_number == 0) {
        LUNA_CORE_INFO("Starting render loop: swapchain={}x{}, draw={}x{}",
                       swapchain_extent.width,
                       swapchain_extent.height,
                       m_render_target.getExtent().width,
                       m_render_target.getExtent().height);
    }

    auto& current_frame = getCurrentFrame();
    current_frame.beginFrame(m_device, m_deferred_release, static_cast<uint64_t>(m_frame_number));

    uint32_t swapchain_image_index = 0;
    bool recreate_after_present = false;
    const vk::Result acquire_result = m_device.acquireNextImageKHR(
        m_swapchain_context.get(), 1'000'000'000, current_frame.m_swapchain_semaphore, nullptr, &swapchain_image_index);
    if (acquire_result == vk::Result::eErrorOutOfDateKHR) {
        logSwapchainResult("vkAcquireNextImageKHR", acquire_result, swapchain_extent, getFramebufferExtent());
        m_resize_requested = true;
        return;
    }
    if (acquire_result == vk::Result::eSuboptimalKHR) {
        m_resize_requested = true;
        recreate_after_present = true;
        logSwapchainResult("vkAcquireNextImageKHR", acquire_result, swapchain_extent, getFramebufferExtent());
    } else if (acquire_result != vk::Result::eSuccess) {
        LUNA_CORE_FATAL("Vulkan call failed: vkAcquireNextImageKHR returned {}", vk::to_string(acquire_result));
        std::abort();
    }

    VK_CHECK(m_device.resetFences(1, &current_frame.m_render_fence));

    vk::CommandBuffer cmd = current_frame.m_main_command_buffer;

    vk::CommandBufferBeginInfo cmd_begin_info =
        vkinit::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    m_draw_extent.width = std::max(
        1u,
        static_cast<uint32_t>(std::min(swapchain_extent.width, m_render_target.getExtent().width) * m_render_scale));
    m_draw_extent.height = std::max(
        1u,
        static_cast<uint32_t>(std::min(swapchain_extent.height, m_render_target.getExtent().height) * m_render_scale));

    VK_CHECK(cmd.begin(&cmd_begin_info));

    auto& swapchain_images = m_swapchain_context.getImages();
    auto& swapchain_image_views = m_swapchain_context.getImageViews();
    auto& swapchain_image_layouts = m_swapchain_context.getImageLayouts();

    FrameRenderContext render_context;
    render_context.m_engine = this;
    render_context.m_frame = &current_frame;
    render_context.m_render_target = &m_render_target;
    render_context.m_command_list.m_handle = cmd;
    render_context.m_draw_extent = fromVk(m_draw_extent);
    render_context.m_swapchain_extent = fromVk(swapchain_extent);
    render_context.m_swapchain_image_index = swapchain_image_index;
    render_context.m_swapchain_image = swapchain_images[swapchain_image_index];
    render_context.m_swapchain_image_view = &swapchain_image_views[swapchain_image_index];
    render_context.m_swapchain_image_layout = &swapchain_image_layouts[swapchain_image_index];
    render_context.m_copy_render_target_to_swapchain = true;

    if (render_function) {
        render_function(render_context);
    } else {
        recordDefaultScene(render_context);
    }

    if (render_context.shouldCopyRenderTargetToSwapchain()) {
        vkutil::transitionImage(
            cmd,
            m_render_target.getColorImage().get(),
            m_render_target.getColorLayout(),
            vk::ImageLayout::eTransferSrcOptimal);
        m_render_target.setColorLayout(vk::ImageLayout::eTransferSrcOptimal);
        render_context.transitionSwapchainImage(luna::render::ImageLayout::TransferDst);

        vkutil::copyImageToImage(cmd,
                                 m_render_target.getColorImage().get(),
                                 render_context.m_swapchain_image,
                                 m_draw_extent,
                                 swapchain_extent);
    }

    if (overlay_renderer) {
        render_context.transitionSwapchainImage(luna::render::ImageLayout::ColorAttachment);

        overlay_renderer(render_context.getCommandList(), render_context.getSwapchainImageView(), render_context.getSwapchainExtent());

        render_context.transitionSwapchainImage(luna::render::ImageLayout::Present);
    } else {
        render_context.transitionSwapchainImage(luna::render::ImageLayout::Present);
    }

    // finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(cmd.end());

    vk::CommandBufferSubmitInfo cmdinfo = vkinit::commandBufferSubmitInfo(cmd);

    vk::SemaphoreSubmitInfo wait_info = vkinit::semaphoreSubmitInfo(
        vk::PipelineStageFlagBits2::eColorAttachmentOutput, current_frame.m_swapchain_semaphore);
    vk::SemaphoreSubmitInfo signal_info =
        vkinit::semaphoreSubmitInfo(vk::PipelineStageFlagBits2::eAllGraphics, current_frame.m_render_semaphore);

    vk::SubmitInfo2 submit = vkinit::submitInfo(&cmdinfo, &signal_info, &wait_info);

    VK_CHECK(m_graphics_queue.submit2(1, &submit, current_frame.m_render_fence));

    if (before_present) {
        before_present();
    }

    vk::PresentInfoKHR present_info{};
    const vk::SwapchainKHR swapchain = m_swapchain_context.get();
    present_info.pSwapchains = &swapchain;
    present_info.swapchainCount = 1;

    present_info.pWaitSemaphores = &current_frame.m_render_semaphore;
    present_info.waitSemaphoreCount = 1;

    present_info.pImageIndices = &swapchain_image_index;

    const vk::Result present_result = m_graphics_queue.presentKHR(&present_info);
    if (present_result == vk::Result::eErrorOutOfDateKHR || present_result == vk::Result::eSuboptimalKHR) {
        m_resize_requested = true;
        logSwapchainResult("vkQueuePresentKHR", present_result, swapchain_extent, getFramebufferExtent());
    } else if (present_result != vk::Result::eSuccess) {
        LUNA_CORE_FATAL("Vulkan call failed: vkQueuePresentKHR returned {}", vk::to_string(present_result));
        std::abort();
    } else if (recreate_after_present) {
        m_resize_requested = true;
    }

    m_frame_number++;
}

void VulkanEngine::recordDefaultScene(FrameRenderContext& render_context)
{
    updateScene();

    vk::CommandBuffer cmd = render_context.getCommandList().m_handle;

    vkutil::transitionImage(
        cmd, m_render_target.getColorImage().get(), m_render_target.getColorLayout(), vk::ImageLayout::eGeneral);
    m_render_target.setColorLayout(vk::ImageLayout::eGeneral);

    drawBackground(cmd);

    vkutil::transitionImage(cmd,
                            m_render_target.getColorImage().get(),
                            m_render_target.getColorLayout(),
                            vk::ImageLayout::eColorAttachmentOptimal);
    m_render_target.setColorLayout(vk::ImageLayout::eColorAttachmentOptimal);
    drawGeometry(cmd);
}

bool VulkanEngine::initVulkan()
{
    if (!m_instance_context.create("Example Vulkan Application", m_window, b_use_validation_layers)) {
        return false;
    }

    m_instance = m_instance_context.get();
    m_debug_messenger = m_instance_context.getDebugMessenger();
    m_surface = m_instance_context.getSurface();

    VkPhysicalDeviceVulkan13Features features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features.dynamicRendering = true;
    features.synchronization2 = true;

    VkPhysicalDeviceVulkan12Features features12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    if (!m_physical_device_context.select(m_instance_context, features, features12)) {
        return false;
    }

    m_chosen_gpu = m_physical_device_context.get();

    if (!m_device_context.create(m_physical_device_context)) {
        return false;
    }

    m_device = m_device_context.get();
    m_uniform_buffer_alignment =
        std::max<vk::DeviceSize>(1, m_physical_device_context.getProperties().limits.minUniformBufferOffsetAlignment);

    if (!m_graphics_queue_context.initialize(m_device_context, vkb::QueueType::graphics)) {
        return false;
    }

    m_graphics_queue = m_graphics_queue_context.get();
    m_graphics_queue_family = m_graphics_queue_context.getFamilyIndex();

    LUNA_CORE_INFO("Selected GPU: {}", m_physical_device_context.getProperties().deviceName.data());

    if (!m_allocator.create(m_instance_context.get(), m_physical_device_context.get(), m_device)) {
        LUNA_CORE_ERROR("Failed to create VMA allocator");
        return false;
    }

    return true;
}

bool VulkanEngine::createSwapchain(uint32_t width, uint32_t height)
{
    LUNA_CORE_INFO("Creating swapchain for framebuffer {}x{}", width, height);
    if (!m_swapchain_context.create(m_physical_device_context, m_device_context, m_surface, width, height)) {
        return false;
    }

    LUNA_CORE_INFO("Created swapchain: {}x{}, images={}",
                   m_swapchain_context.getExtent().width,
                   m_swapchain_context.getExtent().height,
                   m_swapchain_context.getImageCount());

    return true;
}

void VulkanEngine::drawBackground(vk::CommandBuffer cmd)
{

    ComputeEffect& effect = m_background_effects[m_current_background_effect];

    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, effect.m_pipeline);
    cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute, m_gradient_pipeline_layout, 0, 1, &m_draw_image_descriptors, 0, nullptr);
    cmd.pushConstants(
        m_gradient_pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(ComputePushConstants), &effect.m_data);
    cmd.dispatch(static_cast<uint32_t>(std::ceil(m_draw_extent.width / 16.0f)),
                 static_cast<uint32_t>(std::ceil(m_draw_extent.height / 16.0f)),
                 1);
}

void VulkanEngine::drawGeometry(vk::CommandBuffer cmd)
{
    // begin a render pass  connected to our draw image
    vk::RenderingAttachmentInfo color_attachment =
        vkinit::attachmentInfo(m_render_target.getColorImage().getView(), nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vk::RenderingAttachmentInfo depth_attachment =
        vkinit::depthAttachmentInfo(m_render_target.getDepthImage().getView(), VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    vkutil::transitionImage(
        cmd,
        m_render_target.getDepthImage().get(),
        m_render_target.getDepthLayout(),
        vk::ImageLayout::eDepthAttachmentOptimal);
    m_render_target.setDepthLayout(vk::ImageLayout::eDepthAttachmentOptimal);

    vk::RenderingInfo render_info = vkinit::renderingInfo(m_draw_extent, &color_attachment, &depth_attachment);
    cmd.beginRendering(&render_info);

    // set dynamic viewport and scissor
    vk::Viewport viewport{};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<float>(m_draw_extent.width);
    viewport.height = static_cast<float>(m_draw_extent.height);
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    cmd.setViewport(0, 1, &viewport);

    vk::Rect2D scissor{};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = m_draw_extent.width;
    scissor.extent.height = m_draw_extent.height;

    cmd.setScissor(0, 1, &scissor);

    auto scene_data_allocation =
        getCurrentFrame().m_uniform_arena.allocate(sizeof(GPUSceneData), m_uniform_buffer_alignment);
    if (!scene_data_allocation.has_value() || scene_data_allocation->m_mapped_data == nullptr) {
        LUNA_CORE_FATAL("Failed to allocate frame-local scene data buffer slice");
        std::abort();
    }
    std::memcpy(scene_data_allocation->m_mapped_data, &m_scene_data, sizeof(GPUSceneData));

    const vk::DescriptorSet global_descriptor =
        getCurrentFrame().m_frame_descriptors.allocate(m_device, m_gpu_scene_data_descriptor_layout);

    DescriptorWriter writer;
    writer.writeBuffer(0,
                       scene_data_allocation->m_buffer,
                       sizeof(GPUSceneData),
                       static_cast<size_t>(scene_data_allocation->m_offset),
                       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.updateSet(m_device, global_descriptor);

    std::sort(m_main_draw_context.m_opaque_surfaces.begin(), m_main_draw_context.m_opaque_surfaces.end(), renderObjectSort);

    auto draw_objects = [&](const std::vector<RenderObject>& objects) {
        MaterialPipeline* last_pipeline = nullptr;
        MaterialInstance* last_material = nullptr;
        vk::Buffer last_index_buffer{};

        for (const RenderObject& draw : objects) {
            if (draw.m_material == nullptr || draw.m_material->m_pipeline == nullptr ||
                !draw.m_material->m_pipeline->m_pipeline || !draw.m_material->m_pipeline->m_layout) {
                continue;
            }

            if (draw.m_material->m_pipeline != last_pipeline) {
                last_pipeline = draw.m_material->m_pipeline;
                cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, last_pipeline->m_pipeline);
            }

            if (draw.m_material != last_material) {
                last_material = draw.m_material;
                std::array<vk::DescriptorSet, 2> mesh_descriptors = {global_descriptor, draw.m_material->m_material_set};
                cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                       last_material->m_pipeline->m_layout,
                                       0,
                                       static_cast<uint32_t>(mesh_descriptors.size()),
                                       mesh_descriptors.data(),
                                       0,
                                       nullptr);
            }

            GPUDrawPushConstants push_constants{};
            push_constants.m_world_matrix = draw.m_transform;
            push_constants.m_vertex_buffer = draw.m_vertex_buffer_address;
            cmd.pushConstants(draw.m_material->m_pipeline->m_layout,
                              vk::ShaderStageFlagBits::eVertex,
                              0,
                              sizeof(GPUDrawPushConstants),
                              &push_constants);

            if (draw.m_index_buffer != last_index_buffer) {
                last_index_buffer = draw.m_index_buffer;
                cmd.bindIndexBuffer(draw.m_index_buffer, 0, vk::IndexType::eUint32);
            }

            cmd.drawIndexed(draw.m_index_count, 1, draw.m_first_index, 0, 0);
        }
    };

    draw_objects(m_main_draw_context.m_opaque_surfaces);
    draw_objects(m_main_draw_context.m_transparent_surfaces);

    cmd.endRendering();
}

void VulkanEngine::updateScene()
{
    const float aspect = m_draw_extent.height == 0 ? 1.0f
                                                 : static_cast<float>(m_draw_extent.width) /
                                                       static_cast<float>(m_draw_extent.height);

    m_scene_data.m_view = m_main_camera.getViewMatrix();
    m_scene_data.m_proj = glm::perspectiveRH_ZO(glm::radians(70.0f), aspect, 0.1f, 10'000.0f);
    m_scene_data.m_proj[1][1] *= -1.0f;
    m_scene_data.m_viewproj = m_scene_data.m_proj * m_scene_data.m_view;

    m_scene_data.m_ambient_color = glm::vec4(0.1f);
    m_scene_data.m_sunlight_direction = glm::vec4(0.0f, 1.0f, 0.5f, 1.0f);
    m_scene_data.m_sunlight_color = glm::vec4(1.0f);

    m_main_draw_context.clear();

    for (auto& [_, scene] : m_loaded_scenes) {
        if (!scene) {
            continue;
        }

        scene->draw(glm::mat4{1.0f}, m_main_draw_context);
    }
}

void VulkanEngine::destroySwapchain()
{
    if (!m_swapchain_context.isValid()) {
        return;
    }

    m_swapchain_context.destroy();
}

bool VulkanEngine::initSwapchain()
{
    const vk::Extent2D framebuffer_extent = getFramebufferExtent();
    if (framebuffer_extent.width == 0 || framebuffer_extent.height == 0) {
        LUNA_CORE_ERROR("Cannot create swapchain because framebuffer extent is {}x{}",
                        framebuffer_extent.width,
                        framebuffer_extent.height);
        return false;
    }

    LUNA_CORE_INFO("Preparing swapchain using logical window {}x{} and framebuffer {}x{}",
                   m_window_extent.width,
                   m_window_extent.height,
                   framebuffer_extent.width,
                   framebuffer_extent.height);
    if (!createSwapchain(framebuffer_extent.width, framebuffer_extent.height)) {
        return false;
    }

    return createRenderTarget(framebuffer_extent);
}

bool VulkanEngine::initCommands()
{
    std::array<DescriptorAllocatorGrowable::PoolSizeRatio, 2> frame_sizes = {
        DescriptorAllocatorGrowable::PoolSizeRatio{vk::DescriptorType::eUniformBuffer, 3.f},
        DescriptorAllocatorGrowable::PoolSizeRatio{vk::DescriptorType::eCombinedImageSampler, 3.f},
    };

    for (int i = 0; i < frame_overlap; i++) {
        if (!m_frames[i].initialize(m_device,
                                    m_allocator,
                                    m_graphics_queue_family,
                                    k_frame_descriptor_set_count,
                                    frame_sizes,
                                    k_frame_uniform_arena_size,
                                    k_frame_staging_arena_size)) {
            LUNA_CORE_ERROR("Failed to initialize frame context {}", i);
            return false;
        }
    }

    vk::CommandPoolCreateInfo command_pool_info =
        vkinit::commandPoolCreateInfo(m_graphics_queue_family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    const vk::Result upload_command_pool_result =
        m_device.createCommandPool(&command_pool_info, nullptr, &m_imm_context.m_command_pool);
    if (upload_command_pool_result != vk::Result::eSuccess) {
        LUNA_CORE_ERROR("Failed to create immediate command pool: {}", stringVkResult(upload_command_pool_result));
        return false;
    }

    vk::CommandBufferAllocateInfo upload_cmd_alloc_info = vkinit::commandBufferAllocateInfo(m_imm_context.m_command_pool, 1);
    const vk::Result upload_command_buffer_result =
        m_device.allocateCommandBuffers(&upload_cmd_alloc_info, &m_imm_context.m_command_buffer);
    if (upload_command_buffer_result != vk::Result::eSuccess) {
        LUNA_CORE_ERROR("Failed to allocate immediate command buffer: {}", stringVkResult(upload_command_buffer_result));
        return false;
    }

    LUNA_CORE_INFO("Initialized command pools and command buffers for {} frames", frame_overlap);
    return true;
}

bool VulkanEngine::initSyncStructures()
{
    vk::FenceCreateInfo fence_create_info = vkinit::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    const vk::Result upload_fence_result = m_device.createFence(&fence_create_info, nullptr, &m_imm_context.m_fence);
    if (upload_fence_result != vk::Result::eSuccess) {
        LUNA_CORE_ERROR("Failed to create immediate submit fence: {}", stringVkResult(upload_fence_result));
        return false;
    }

    LUNA_CORE_INFO("Initialized synchronization primitives for {} frames", frame_overlap);
    return true;
}

bool VulkanEngine::initDescriptors()
{
    DescriptorLayoutBuilder builder;
    if (!reflectShaderBindings(
            "../Shaders/Internal/gradient.spv", luna::ShaderType::Compute, VK_SHADER_STAGE_COMPUTE_BIT, builder) ||
        !reflectShaderBindings(
            "../Shaders/Internal/sky.spv", luna::ShaderType::Compute, VK_SHADER_STAGE_COMPUTE_BIT, builder)) {
        return false;
    }

    if (builder.m_bindings.empty()) {
        LUNA_CORE_ERROR("No descriptor bindings were reflected for compute shaders");
        return false;
    }

    const auto draw_image_binding_it =
        std::find_if(builder.m_bindings.begin(), builder.m_bindings.end(), [](const vk::DescriptorSetLayoutBinding& binding) {
            return binding.descriptorType == vk::DescriptorType::eStorageImage;
        });
    if (draw_image_binding_it == builder.m_bindings.end()) {
        LUNA_CORE_ERROR("Failed to find reflected storage image binding for background compute shaders");
        return false;
    }

    m_draw_image_descriptor_binding = draw_image_binding_it->binding;
    m_draw_image_descriptor_count = draw_image_binding_it->descriptorCount;
    m_draw_image_descriptor_type = draw_image_binding_it->descriptorType;

    if (m_draw_image_descriptor_type != vk::DescriptorType::eStorageImage) {
        LUNA_CORE_ERROR("Background compute shaders must expose a storage image descriptor in set 0");
        return false;
    }

    if (m_draw_image_descriptor_count != 1) {
        LUNA_CORE_ERROR("Only single storage image descriptors are currently supported, reflected count={}",
                        m_draw_image_descriptor_count);
        return false;
    }

    DescriptorLayoutBuilder scene_data_builder;
    scene_data_builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    m_gpu_scene_data_descriptor_layout =
        scene_data_builder.build(m_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    auto sizes = buildPoolRatios(builder);
    sizes.push_back(DescriptorAllocator::PoolSizeRatio{vk::DescriptorType::eUniformBuffer, 3.0f});
    sizes.push_back(DescriptorAllocator::PoolSizeRatio{vk::DescriptorType::eCombinedImageSampler, 3.0f});
    m_global_descriptor_allocator.initPool(m_device, 10, sizes);
    m_draw_image_descriptor_layout = builder.build(m_device);

    // allocate a descriptor set for our draw image
    m_draw_image_descriptors = m_global_descriptor_allocator.allocate(m_device, m_draw_image_descriptor_layout);

    updateDrawImageDescriptors();

    // make sure both the descriptor allocator and the new layout get cleaned up properly
    m_shutdown_release.push([this]() {
        m_global_descriptor_allocator.destroyPool(m_device);

        vkDestroyDescriptorSetLayout(m_device, m_draw_image_descriptor_layout, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_gpu_scene_data_descriptor_layout, nullptr);
    });

    LUNA_CORE_INFO("Initialized descriptor layouts and allocators");
    return true;
}

bool VulkanEngine::initPipelines()
{
    initTrianglePipeline();
    initMeshPipeline();
    return initBackgroundPipelines();
}

void VulkanEngine::initTrianglePipeline()
{
    vk::ShaderModule triangle_frag_shader{};
    if (!vkutil::loadShaderModule("../Shaders/Internal/colored_triangle.frag.spv", m_device, &triangle_frag_shader)) {
        LUNA_CORE_ERROR("Error when building the triangle fragment shader module");
    } else {
        LUNA_CORE_INFO("Triangle fragment shader succesfully loaded");
    }

    vk::ShaderModule triangle_vertex_shader{};
    if (!vkutil::loadShaderModule("../Shaders/Internal/colored_triangle.vert.spv", m_device, &triangle_vertex_shader)) {
        LUNA_CORE_ERROR("Error when building the triangle vertex shader module");
    } else {
        LUNA_CORE_INFO("Triangle vertex shader succesfully loaded");
    }

    // build the pipeline layout that controls the inputs/outputs of the shader
    // we are not using descriptor sets or other systems yet, so no need to use anything other than empty default
    vk::PipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipelineLayoutCreateInfo();
    VK_CHECK(m_device.createPipelineLayout(&pipeline_layout_info, nullptr, &m_triangle_pipeline_layout));

    PipelineBuilder pipeline_builder;

    // use the triangle layout we created
    pipeline_builder.m_pipeline_layout = m_triangle_pipeline_layout;
    // connecting the vertex and pixel shaders to the pipeline
    pipeline_builder.setShaders(triangle_vertex_shader, triangle_frag_shader);
    // it will draw triangles
    pipeline_builder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    // filled triangles
    pipeline_builder.setPolygonMode(VK_POLYGON_MODE_FILL);
    // no backface culling
    pipeline_builder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    // no multisampling
    pipeline_builder.setMultisamplingNone();
    // no blending
    pipeline_builder.disableBlending();
    // no depth testing
    pipeline_builder.enableDepthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL);

    // connect the image format we will draw into, from draw image
    pipeline_builder.setColorAttachmentFormat(m_render_target.getColorFormat());
    pipeline_builder.setDepthFormat(m_render_target.getDepthFormat());

    // finally build the pipeline
    m_triangle_pipeline = pipeline_builder.buildPipeline(m_device);

    // clean structures
    m_device.destroyShaderModule(triangle_frag_shader, nullptr);
    m_device.destroyShaderModule(triangle_vertex_shader, nullptr);

    m_shutdown_release.push([this]() {
        m_device.destroyPipelineLayout(m_triangle_pipeline_layout, nullptr);
        m_device.destroyPipeline(m_triangle_pipeline, nullptr);
    });
}

void VulkanEngine::initMeshPipeline()
{
    m_metal_rough_material.buildPipelines(this);

    m_shutdown_release.push([this]() {
        m_metal_rough_material.clearResources(m_device);
    });
}

bool VulkanEngine::initBackgroundPipelines()
{

    vk::PipelineLayoutCreateInfo compute_layout{};
    compute_layout.pSetLayouts = &m_draw_image_descriptor_layout;
    compute_layout.setLayoutCount = 1;

    vk::PushConstantRange push_constant{};
    push_constant.offset = 0;
    push_constant.size = sizeof(ComputePushConstants);
    push_constant.stageFlags = vk::ShaderStageFlagBits::eCompute;
    compute_layout.pPushConstantRanges = &push_constant;
    compute_layout.pushConstantRangeCount = 1;

    VK_CHECK(m_device.createPipelineLayout(&compute_layout, nullptr, &m_gradient_pipeline_layout));

    vk::ShaderModule compute_draw_shader{};
    vkutil::loadShaderModule("../Shaders/Internal/gradient.spv", m_device, &compute_draw_shader);
    vk::ShaderModule sky_shader{};
    vkutil::loadShaderModule("../Shaders/Internal/sky.spv", m_device, &sky_shader);

    vk::ComputePipelineCreateInfo compute_pipeline_create_info{};
    compute_pipeline_create_info.layout = m_gradient_pipeline_layout;
    compute_pipeline_create_info.stage =
        vkinit::pipelineShaderStageCreateInfo(vk::ShaderStageFlagBits::eCompute, compute_draw_shader);

    compute_pipeline_create_info.stage.module = compute_draw_shader;
    VK_CHECK(m_device.createComputePipelines({}, 1, &compute_pipeline_create_info, nullptr, &m_gradient_pipeline));

    ComputeEffect gradient;
    gradient.m_layout = m_gradient_pipeline_layout;
    gradient.m_name = "gradient";
    gradient.m_pipeline = m_gradient_pipeline;
    gradient.m_data.m_data1 = glm::vec4(1, 0, 0, 1);
    gradient.m_data.m_data2 = glm::vec4(0, 0, 1, 1);

    compute_pipeline_create_info.stage.module = sky_shader;
    vk::Pipeline sky_pipeline{};
    VK_CHECK(m_device.createComputePipelines({}, 1, &compute_pipeline_create_info, nullptr, &sky_pipeline));

    ComputeEffect sky;
    sky.m_layout = m_gradient_pipeline_layout;
    sky.m_name = "sky";
    sky.m_pipeline = sky_pipeline;
    sky.m_data.m_data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

    m_background_effects.push_back(gradient);
    m_background_effects.push_back(sky);

    m_device.destroyShaderModule(compute_draw_shader, nullptr);
    m_device.destroyShaderModule(sky_shader, nullptr);

    vk::Pipeline p1 = m_gradient_pipeline;
    vk::Pipeline p2 = sky_pipeline;
    vk::PipelineLayout layout = m_gradient_pipeline_layout;

    m_shutdown_release.push([=, this]() {
        m_device.destroyPipelineLayout(layout, nullptr);
        m_device.destroyPipeline(p1, nullptr);
        m_device.destroyPipeline(p2, nullptr);
    });

    LUNA_CORE_INFO("Initialized background compute pipeline");
    return true;
}

bool VulkanEngine::resizeSwapchain()
{
    const vk::Extent2D framebuffer_extent = getFramebufferExtent();
    if (framebuffer_extent.width == 0 || framebuffer_extent.height == 0) {
        LUNA_CORE_WARN("Skipping swapchain recreation because framebuffer extent is {}x{}",
                       framebuffer_extent.width,
                       framebuffer_extent.height);
        return false;
    }

    const vk::Extent2D current_swapchain_extent = m_swapchain_context.getExtent();
    LUNA_CORE_INFO("Recreating swapchain: old={}x{}, new framebuffer={}x{}",
                   current_swapchain_extent.width,
                   current_swapchain_extent.height,
                   framebuffer_extent.width,
                   framebuffer_extent.height);
    VK_CHECK(vkDeviceWaitIdle(m_device));
    m_deferred_release.flushAll();
    destroySwapchain();
    destroyRenderTarget();
    if (!createSwapchain(framebuffer_extent.width, framebuffer_extent.height)) {
        LUNA_CORE_ERROR("Swapchain recreation failed");
        return false;
    }

    if (!createRenderTarget(framebuffer_extent)) {
        LUNA_CORE_ERROR("Render target recreation failed");
        return false;
    }

    if (m_draw_image_descriptors) {
        updateDrawImageDescriptors();
    }

    m_resize_requested = false;
    return true;
}

bool VulkanEngine::createRenderTarget(vk::Extent2D extent)
{
    if (extent.width == 0 || extent.height == 0) {
        LUNA_CORE_ERROR("Cannot create draw resources for extent {}x{}", extent.width, extent.height);
        return false;
    }

    if (!m_render_target.create(m_allocator, extent, vk::Format::eR16G16B16A16Sfloat, vk::Format::eD32Sfloat)) {
        LUNA_CORE_ERROR("Failed to create render target for extent {}x{}", extent.width, extent.height);
        return false;
    }

    LUNA_CORE_INFO(
        "Created render target: {}x{}, format={}", extent.width, extent.height, stringVkFormat(m_render_target.getColorFormat()));
    return true;
}

void VulkanEngine::destroyRenderTarget()
{
    m_render_target.destroy();
    m_draw_extent = {};
}

void VulkanEngine::updateDrawImageDescriptors()
{
    if (!m_device || !m_draw_image_descriptors || !m_render_target.getColorImage().getView()) {
        return;
    }

    DescriptorWriter writer;
    writer.writeImage(m_draw_image_descriptor_binding,
                       m_render_target.getColorImage().getView(),
                       vk::Sampler{},
                       vk::ImageLayout::eGeneral,
                       m_draw_image_descriptor_type);
    writer.updateSet(m_device, m_draw_image_descriptors);
}

vk::Extent2D VulkanEngine::getFramebufferExtent() const
{
    if (m_window == nullptr) {
        return {};
    }

    int framebuffer_width = 0;
    int framebuffer_height = 0;
    glfwGetFramebufferSize(m_window, &framebuffer_width, &framebuffer_height);

    return {static_cast<uint32_t>(framebuffer_width > 0 ? framebuffer_width : 0),
            static_cast<uint32_t>(framebuffer_height > 0 ? framebuffer_height : 0)};
}

AllocatedImage VulkanEngine::createImage(luna::render::Extent3D size,
                                          luna::render::PixelFormat format,
                                          luna::render::ImageUsage usage,
                                          bool mipmapped)
{
    return createImage(toVk(size), toVk(format), toVk(usage), mipmapped);
}

AllocatedImage VulkanEngine::createImage(vk::Extent3D size,
                                          vk::Format format,
                                          vk::ImageUsageFlags usage,
                                          bool mipmapped)
{
    return m_allocator.createImage(size, format, usage, mipmapped);
}

AllocatedImage VulkanEngine::createImage(void* data,
                                          luna::render::Extent3D size,
                                          luna::render::PixelFormat format,
                                          luna::render::ImageUsage usage,
                                          bool mipmapped)
{
    return createImage(data, toVk(size), toVk(format), toVk(usage), mipmapped);
}

AllocatedImage VulkanEngine::createImage(void* data,
                                          vk::Extent3D size,
                                          vk::Format format,
                                          vk::ImageUsageFlags usage,
                                          bool mipmapped)
{
    const size_t data_size = size.depth * size.width * size.height * 4;
    AllocatedBuffer uploadbuffer = createBuffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    std::memcpy(uploadbuffer.getMappedData(), data, data_size);

    AllocatedImage new_image =
        createImage(size,
                     format,
                     usage | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc,
                     mipmapped);

    immediateSubmitVk([&](vk::CommandBuffer cmd) {
        vkutil::transitionImage(cmd, new_image.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copy_region{};
        copy_region.bufferOffset = 0;
        copy_region.bufferRowLength = 0;
        copy_region.bufferImageHeight = 0;

        copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_region.imageSubresource.mipLevel = 0;
        copy_region.imageSubresource.baseArrayLayer = 0;
        copy_region.imageSubresource.layerCount = 1;
        copy_region.imageExtent = size;

        cmd.copyBufferToImage(uploadbuffer.get(),
                              new_image.get(),
                              vk::ImageLayout::eTransferDstOptimal,
                              1,
                              reinterpret_cast<const vk::BufferImageCopy*>(&copy_region));

        vkutil::transitionImage(
            cmd, new_image.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    });

    return new_image;
}

void VulkanEngine::destroyImage(AllocatedImage& image)
{
    if (!image.isValid()) {
        return;
    }

    auto retained_image = retainResource(std::move(image));
    deferRelease([retained_image]() mutable {
        retained_image.reset();
    });
}

AllocatedBuffer VulkanEngine::createBuffer(size_t alloc_size, luna::render::BufferUsage usage, luna::render::MemoryUsage memory_usage)
{
    return createBuffer(alloc_size, toVk(usage), toVk(memory_usage));
}

AllocatedBuffer VulkanEngine::createBuffer(size_t alloc_size, vk::BufferUsageFlags usage, VmaMemoryUsage memory_usage)
{
    return m_allocator.createBuffer(alloc_size, usage, memory_usage);
}

void VulkanEngine::destroyBuffer(AllocatedBuffer& buffer)
{
    if (!buffer.isValid()) {
        return;
    }

    auto retained_buffer = retainResource(std::move(buffer));
    deferRelease([retained_buffer]() mutable {
        retained_buffer.reset();
    });
}

void VulkanEngine::deferRelease(std::function<void()>&& function)
{
    if (!function) {
        return;
    }

    if (!m_deferred_release.isInitialized()) {
        function();
        return;
    }

    m_deferred_release.defer(static_cast<uint64_t>(m_frame_number), std::move(function));
}

void VulkanEngine::immediateSubmit(const std::function<void(RenderCommandList&)>& function)
{
    immediateSubmitVk([&](vk::CommandBuffer cmd) {
        RenderCommandList command_list;
        command_list.m_handle = cmd;
        function(command_list);
    });
}

void VulkanEngine::immediateSubmitVk(const std::function<void(vk::CommandBuffer cmd)>& function)
{
    VK_CHECK(m_device.resetFences(1, &m_imm_context.m_fence));
    VK_CHECK(m_imm_context.m_command_buffer.reset({}));

    vk::CommandBuffer cmd = m_imm_context.m_command_buffer;
    vk::CommandBufferBeginInfo cmd_begin_info =
        vkinit::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(cmd.begin(&cmd_begin_info));

    function(cmd);

    VK_CHECK(cmd.end());

    vk::CommandBufferSubmitInfo cmd_info = vkinit::commandBufferSubmitInfo(cmd);
    vk::SubmitInfo2 submit = vkinit::submitInfo(&cmd_info, nullptr, nullptr);
    VK_CHECK(m_graphics_queue.submit2(1, &submit, m_imm_context.m_fence));
    VK_CHECK(m_device.waitForFences(1, &m_imm_context.m_fence, VK_TRUE, 1'000'000'000));
}

void VulkanEngine::destroyImageImmediate(AllocatedImage& image)
{
    image.reset();
}

void VulkanEngine::destroyBufferImmediate(AllocatedBuffer& buffer)
{
    buffer.reset();
}

GPUMeshBuffers VulkanEngine::uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
{
    const size_t vertex_buffer_size = vertices.size() * sizeof(Vertex);
    const size_t index_buffer_size = indices.size() * sizeof(uint32_t);

    GPUMeshBuffers new_surface;

    // create vertex buffer
    new_surface.m_vertex_buffer = createBuffer(vertex_buffer_size,
                                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                            VMA_MEMORY_USAGE_GPU_ONLY);

    // find the adress of the vertex buffer
    VkBufferDeviceAddressInfo device_adress_info{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                               .buffer = new_surface.m_vertex_buffer.get()};
    new_surface.m_vertex_buffer_address = vkGetBufferDeviceAddress(m_device, &device_adress_info);

    // create index buffer
    new_surface.m_index_buffer = createBuffer(index_buffer_size,
                                           VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                           VMA_MEMORY_USAGE_GPU_ONLY);
    AllocatedBuffer staging =
        createBuffer(vertex_buffer_size + index_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* data = staging.getMappedData();

    // copy vertex buffer
    memcpy(data, vertices.data(), vertex_buffer_size);
    // copy index buffer
    memcpy((char*) data + vertex_buffer_size, indices.data(), index_buffer_size);

    immediateSubmitVk([&](vk::CommandBuffer cmd) {
        vk::BufferCopy vertex_copy{};
        vertex_copy.dstOffset = 0;
        vertex_copy.srcOffset = 0;
        vertex_copy.size = vertex_buffer_size;

        cmd.copyBuffer(staging.get(), new_surface.m_vertex_buffer.get(), 1, &vertex_copy);

        vk::BufferCopy index_copy{};
        index_copy.dstOffset = 0;
        index_copy.srcOffset = vertex_buffer_size;
        index_copy.size = index_buffer_size;

        cmd.copyBuffer(staging.get(), new_surface.m_index_buffer.get(), 1, &index_copy);
    });

    return new_surface;
}

