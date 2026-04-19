#include "backends/imgui_impl_glfw.h"
#include "Core/Log.h"
#include "Imgui/ImGuiContext.h"
#include "Renderer/Renderer.h"

#include <cstddef>
#include <cstring>

#include <algorithm>
#include <array>
#include <Barrier.h>
#include <Buffer.h>
#include <Builders.h>
#include <CommandBufferEncoder.h>
#include <DescriptorPool.h>
#include <DescriptorSet.h>
#include <DescriptorSetLayout.h>
#include <Device.h>
#include <filesystem>
#include <Pipeline.h>
#include <PipelineLayout.h>
#include <Queue.h>
#include <Sampler.h>
#include <ShaderCompiler.h>
#include <string>
#include <Texture.h>
#include <unordered_map>
#include <vector>

namespace luna::rhi {
namespace {

constexpr uint32_t kImGuiDescriptorPoolSize = 2'048;
constexpr const char* kImGuiShaderRelativePath = "Luna/Imgui/Shaders/ImGui.slang";

struct ImGuiPushConstants {
    float scale[2]{};
    float translate[2]{};
};

struct ImGuiTextureKey {
    const void* view{nullptr};
    const void* sampler{nullptr};

    bool operator==(const ImGuiTextureKey& other) const
    {
        return view == other.view && sampler == other.sampler;
    }
};

struct ImGuiTextureKeyHash {
    size_t operator()(const ImGuiTextureKey& key) const
    {
        return std::hash<const void*>()(key.view) ^ (std::hash<const void*>()(key.sampler) << 1);
    }
};

struct ImGuiTextureBinding {
    luna::RHI::Ref<luna::RHI::TextureView> view;
    luna::RHI::Ref<luna::RHI::Sampler> sampler;
    luna::RHI::Ref<luna::RHI::DescriptorSet> descriptorSet;
};

struct FrameResources {
    luna::RHI::Ref<luna::RHI::Buffer> vertexBuffer;
    uint64_t vertexBufferSize = 0;
    luna::RHI::Ref<luna::RHI::Buffer> indexBuffer;
    uint64_t indexBufferSize = 0;
};

luna::RHI::Ref<luna::RHI::Device> g_device;
luna::RHI::Ref<luna::RHI::Queue> g_graphics_queue;
luna::RHI::Ref<luna::RHI::ShaderCompiler> g_shader_compiler;
luna::RHI::Ref<luna::RHI::DescriptorSetLayout> g_texture_layout;
luna::RHI::Ref<luna::RHI::DescriptorPool> g_descriptor_pool;
luna::RHI::Ref<luna::RHI::PipelineLayout> g_pipeline_layout;
luna::RHI::Ref<luna::RHI::ShaderModule> g_vertex_shader;
luna::RHI::Ref<luna::RHI::ShaderModule> g_fragment_shader;
luna::RHI::Ref<luna::RHI::GraphicsPipeline> g_pipeline;
luna::RHI::Ref<luna::RHI::Sampler> g_default_sampler;
luna::RHI::Ref<luna::RHI::Texture> g_font_texture;
luna::RHI::Format g_pipeline_color_format = luna::RHI::Format::UNDEFINED;
std::unordered_map<ImGuiTextureKey, std::shared_ptr<ImGuiTextureBinding>, ImGuiTextureKeyHash> g_texture_bindings;
std::vector<FrameResources> g_frame_resources;
ImTextureID g_font_texture_id = 0;
luna::RHI::BackendType g_backend_type = luna::RHI::BackendType::Vulkan;
bool g_initialized = false;

float imguiTopClipY(luna::RHI::BackendType backend_type)
{
    switch (backend_type) {
        case luna::RHI::BackendType::Vulkan:
        case luna::RHI::BackendType::DirectX12:
            return -1.0f;
        case luna::RHI::BackendType::DirectX11:
        case luna::RHI::BackendType::OpenGL:
        case luna::RHI::BackendType::OpenGLES:
        default:
            return 1.0f;
    }
}

std::filesystem::path projectRoot()
{
    return std::filesystem::path(LUNA_PROJECT_ROOT);
}

std::filesystem::path imguiShaderPath()
{
    return projectRoot() / kImGuiShaderRelativePath;
}

bool writeBufferData(const luna::RHI::Ref<luna::RHI::Buffer>& buffer, const void* data, uint64_t size)
{
    if (!buffer || (size > 0 && data == nullptr)) {
        return false;
    }
    if (size == 0) {
        return true;
    }

    void* mapped = buffer->Map();
    if (mapped == nullptr) {
        return false;
    }

    std::memcpy(mapped, data, static_cast<size_t>(size));
    buffer->Flush(0, size);
    buffer->Unmap();
    return true;
}

luna::RHI::Ref<luna::RHI::Buffer> createCpuToGpuBuffer(const luna::RHI::Ref<luna::RHI::Device>& device,
                                                       uint64_t size,
                                                       luna::RHI::BufferUsageFlags usage,
                                                       const std::string& name)
{
    if (!device || size == 0) {
        return {};
    }

    return device->CreateBuffer(luna::RHI::BufferBuilder()
                                    .SetSize(size)
                                    .SetUsage(usage)
                                    .SetMemoryUsage(luna::RHI::BufferMemoryUsage::CpuToGpu)
                                    .SetName(name)
                                    .Build());
}

std::shared_ptr<ImGuiTextureBinding> createTextureBinding(const luna::RHI::Ref<luna::RHI::TextureView>& view,
                                                          const luna::RHI::Ref<luna::RHI::Sampler>& sampler)
{
    if (!view || !sampler || !g_descriptor_pool || !g_texture_layout) {
        return {};
    }

    auto descriptor_set = g_descriptor_pool->AllocateDescriptorSet(g_texture_layout);
    if (!descriptor_set) {
        return {};
    }

    descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 0,
        .TextureView = view,
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
        .Binding = 1,
        .Sampler = sampler,
    });
    descriptor_set->Update();

    auto binding = std::make_shared<ImGuiTextureBinding>();
    binding->view = view;
    binding->sampler = sampler;
    binding->descriptorSet = descriptor_set;
    return binding;
}

std::shared_ptr<ImGuiTextureBinding> getOrCreateTextureBinding(const luna::RHI::Ref<luna::RHI::TextureView>& view,
                                                               const luna::RHI::Ref<luna::RHI::Sampler>& sampler)
{
    if (!view) {
        return {};
    }

    const auto effective_sampler = sampler ? sampler : g_default_sampler;
    if (!effective_sampler) {
        return {};
    }

    const ImGuiTextureKey key{
        .view = view.get(),
        .sampler = effective_sampler.get(),
    };

    const auto existing = g_texture_bindings.find(key);
    if (existing != g_texture_bindings.end()) {
        return existing->second;
    }

    auto binding = createTextureBinding(view, effective_sampler);
    if (!binding) {
        return {};
    }

    g_texture_bindings.emplace(key, binding);
    return binding;
}

bool createFontTexture()
{
    if (!g_device) {
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    if (pixels == nullptr || width <= 0 || height <= 0) {
        LUNA_IMGUI_ERROR("Failed to build ImGui font atlas");
        return false;
    }

    const uint64_t upload_size = static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * 4ull;
    auto staging_buffer =
        createCpuToGpuBuffer(g_device, upload_size, luna::RHI::BufferUsageFlags::TransferSrc, "ImGuiFontAtlasUpload");
    if (!staging_buffer || !writeBufferData(staging_buffer, pixels, upload_size)) {
        LUNA_IMGUI_ERROR("Failed to upload ImGui font atlas staging data");
        return false;
    }

    g_font_texture = g_device->CreateTexture(
        luna::RHI::TextureBuilder()
            .SetSize(static_cast<uint32_t>(width), static_cast<uint32_t>(height))
            .SetFormat(luna::RHI::Format::RGBA8_UNORM)
            .SetUsage(luna::RHI::TextureUsageFlags::Sampled | luna::RHI::TextureUsageFlags::TransferDst)
            .SetInitialState(luna::RHI::ResourceState::Undefined)
            .SetName("ImGuiFontAtlas")
            .Build());
    if (!g_font_texture) {
        LUNA_IMGUI_ERROR("Failed to create ImGui font atlas texture");
        return false;
    }

    auto upload_commands = g_device->CreateCommandBufferEncoder();
    if (!upload_commands) {
        LUNA_IMGUI_ERROR("Failed to create command buffer for ImGui font upload");
        return false;
    }

    upload_commands->Begin();
    upload_commands->TransitionImage(g_font_texture, luna::RHI::ImageTransition::UndefinedToTransferDst);

    const luna::RHI::BufferImageCopy region{
        .BufferOffset = 0,
        .BufferRowLength = 0,
        .BufferImageHeight = 0,
        .ImageSubresource =
            {
                .AspectMask = luna::RHI::ImageAspectFlags::Color,
                .MipLevel = 0,
                .BaseArrayLayer = 0,
                .LayerCount = 1,
            },
        .ImageOffsetX = 0,
        .ImageOffsetY = 0,
        .ImageOffsetZ = 0,
        .ImageExtentWidth = static_cast<uint32_t>(width),
        .ImageExtentHeight = static_cast<uint32_t>(height),
        .ImageExtentDepth = 1,
    };
    const std::array<luna::RHI::BufferImageCopy, 1> copy_regions{region};
    upload_commands->CopyBufferToImage(
        staging_buffer, g_font_texture, luna::RHI::ResourceState::CopyDest, copy_regions);
    upload_commands->TransitionImage(g_font_texture, luna::RHI::ImageTransition::TransferDstToShaderRead);
    upload_commands->End();

    if (g_graphics_queue) {
        g_graphics_queue->Submit(upload_commands);
        g_graphics_queue->WaitIdle();
    }

    upload_commands->ReturnToPool();
    g_font_texture_id = ImGuiRhiContext::GetTextureId(g_font_texture);
    if (g_font_texture_id == 0) {
        LUNA_IMGUI_ERROR("Failed to create ImGui font atlas texture binding");
        return false;
    }

    io.Fonts->SetTexID(g_font_texture_id);
    io.Fonts->ClearTexData();
    return true;
}

bool ensureShaders()
{
    if (g_vertex_shader && g_fragment_shader) {
        return true;
    }

    if (!g_device || !g_shader_compiler) {
        return false;
    }

    const auto shader_path = imguiShaderPath();
    if (!std::filesystem::exists(shader_path)) {
        LUNA_IMGUI_ERROR("ImGui shader file is missing: {}", shader_path.string());
        return false;
    }

    g_vertex_shader = g_shader_compiler->CompileOrLoad(g_device,
                                                       luna::RHI::ShaderCreateInfo{
                                                           .SourcePath = shader_path.string(),
                                                           .EntryPoint = "imguiVertexMain",
                                                           .Stage = luna::RHI::ShaderStage::Vertex,
                                                       });
    g_fragment_shader = g_shader_compiler->CompileOrLoad(g_device,
                                                         luna::RHI::ShaderCreateInfo{
                                                             .SourcePath = shader_path.string(),
                                                             .EntryPoint = "imguiFragmentMain",
                                                             .Stage = luna::RHI::ShaderStage::Fragment,
                                                         });
    if (!g_vertex_shader || !g_fragment_shader) {
        LUNA_IMGUI_ERROR("Failed to compile ImGui shaders from '{}'", shader_path.string());
        return false;
    }

    return true;
}

bool ensurePipeline(luna::RHI::Format color_format)
{
    if (g_pipeline && g_pipeline_color_format == color_format) {
        return true;
    }

    if (!ensureShaders() || !g_texture_layout || !g_pipeline_layout || color_format == luna::RHI::Format::UNDEFINED) {
        return false;
    }

    luna::RHI::ColorBlendAttachmentState blend_attachment{};
    blend_attachment.BlendEnable = true;
    blend_attachment.SrcColorBlendFactor = luna::RHI::BlendFactor::SrcAlpha;
    blend_attachment.DstColorBlendFactor = luna::RHI::BlendFactor::OneMinusSrcAlpha;
    blend_attachment.ColorBlendOp = luna::RHI::BlendOp::Add;
    blend_attachment.SrcAlphaBlendFactor = luna::RHI::BlendFactor::One;
    blend_attachment.DstAlphaBlendFactor = luna::RHI::BlendFactor::OneMinusSrcAlpha;
    blend_attachment.AlphaBlendOp = luna::RHI::BlendOp::Add;
    blend_attachment.ColorWriteMask = luna::RHI::ColorComponentFlags::All;

    g_pipeline = g_device->CreateGraphicsPipeline(
        luna::RHI::GraphicsPipelineBuilder()
            .SetShaders({g_vertex_shader, g_fragment_shader})
            .AddVertexBinding(0, sizeof(ImDrawVert), luna::RHI::VertexInputRate::Vertex)
            .AddVertexAttribute(0, 0, luna::RHI::Format::RG32_FLOAT, offsetof(ImDrawVert, pos), "POSITION")
            .AddVertexAttribute(1, 0, luna::RHI::Format::RG32_FLOAT, offsetof(ImDrawVert, uv), "TEXCOORD")
            .AddVertexAttribute(2, 0, luna::RHI::Format::RGBA8_UNORM, offsetof(ImDrawVert, col), "COLOR")
            .SetTopology(luna::RHI::PrimitiveTopology::TriangleList)
            .SetCullMode(luna::RHI::CullMode::None)
            .SetFrontFace(luna::RHI::FrontFace::CounterClockwise)
            .SetDepthTest(false, false, luna::RHI::CompareOp::Always)
            .AddColorAttachment(blend_attachment)
            .AddColorFormat(color_format)
            .SetLayout(g_pipeline_layout)
            .Build());
    g_pipeline_color_format = g_pipeline ? color_format : luna::RHI::Format::UNDEFINED;
    return g_pipeline != nullptr;
}

bool ensureFrameBuffers(uint32_t frame_index, uint64_t vertex_buffer_size, uint64_t index_buffer_size)
{
    if (!g_device) {
        return false;
    }

    if (frame_index >= g_frame_resources.size()) {
        g_frame_resources.resize(static_cast<size_t>(frame_index) + 1);
    }

    auto& frame = g_frame_resources[frame_index];
    if (vertex_buffer_size > frame.vertexBufferSize) {
        frame.vertexBuffer = createCpuToGpuBuffer(g_device,
                                                  vertex_buffer_size,
                                                  luna::RHI::BufferUsageFlags::VertexBuffer,
                                                  "ImGuiVertexBuffer_Frame" + std::to_string(frame_index));
        frame.vertexBufferSize = frame.vertexBuffer ? vertex_buffer_size : 0;
    }
    if (index_buffer_size > frame.indexBufferSize) {
        frame.indexBuffer = createCpuToGpuBuffer(g_device,
                                                 index_buffer_size,
                                                 luna::RHI::BufferUsageFlags::IndexBuffer,
                                                 "ImGuiIndexBuffer_Frame" + std::to_string(frame_index));
        frame.indexBufferSize = frame.indexBuffer ? index_buffer_size : 0;
    }

    return frame.vertexBuffer != nullptr && frame.indexBuffer != nullptr;
}

bool uploadDrawBuffers(ImDrawData* draw_data, uint32_t frame_index)
{
    if (!draw_data || draw_data->TotalVtxCount <= 0 || draw_data->TotalIdxCount <= 0) {
        return false;
    }

    const uint64_t vertex_buffer_size =
        static_cast<uint64_t>(draw_data->TotalVtxCount) * static_cast<uint64_t>(sizeof(ImDrawVert));
    const uint64_t index_buffer_size =
        static_cast<uint64_t>(draw_data->TotalIdxCount) * static_cast<uint64_t>(sizeof(ImDrawIdx));
    if (!ensureFrameBuffers(frame_index, vertex_buffer_size, index_buffer_size)) {
        LUNA_IMGUI_ERROR("Failed to allocate ImGui frame buffers");
        return false;
    }

    auto& frame = g_frame_resources[frame_index];
    auto* vertex_dst = static_cast<ImDrawVert*>(frame.vertexBuffer->Map());
    auto* index_dst = static_cast<ImDrawIdx*>(frame.indexBuffer->Map());
    if (vertex_dst == nullptr || index_dst == nullptr) {
        if (frame.vertexBuffer) {
            frame.vertexBuffer->Unmap();
        }
        if (frame.indexBuffer) {
            frame.indexBuffer->Unmap();
        }
        LUNA_IMGUI_ERROR("Failed to map ImGui frame buffers");
        return false;
    }

    for (int list_index = 0; list_index < draw_data->CmdListsCount; ++list_index) {
        const ImDrawList* draw_list = draw_data->CmdLists[list_index];
        std::memcpy(vertex_dst, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
        std::memcpy(index_dst, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vertex_dst += draw_list->VtxBuffer.Size;
        index_dst += draw_list->IdxBuffer.Size;
    }

    frame.vertexBuffer->Flush(0, vertex_buffer_size);
    frame.indexBuffer->Flush(0, index_buffer_size);
    frame.vertexBuffer->Unmap();
    frame.indexBuffer->Unmap();
    return true;
}

void setupRenderState(luna::RHI::CommandBufferEncoder& command_buffer,
                      uint32_t framebuffer_width,
                      uint32_t framebuffer_height,
                      const ImGuiPushConstants& push_constants,
                      const FrameResources& frame)
{
    command_buffer.BindGraphicsPipeline(g_pipeline);
    command_buffer.SetViewport(
        {0.0f, 0.0f, static_cast<float>(framebuffer_width), static_cast<float>(framebuffer_height), 0.0f, 1.0f});
    command_buffer.BindVertexBuffer(0, frame.vertexBuffer);
    command_buffer.BindIndexBuffer(
        frame.indexBuffer, 0, sizeof(ImDrawIdx) == 2 ? luna::RHI::IndexType::UInt16 : luna::RHI::IndexType::UInt32);
    command_buffer.PushConstants(
        g_pipeline, luna::RHI::ShaderStage::Vertex, 0, sizeof(ImGuiPushConstants), &push_constants);
}

ImGuiTextureBinding* resolveTextureBinding(ImTextureID texture_id)
{
    auto resolve_binding = [](ImTextureID id) -> ImGuiTextureBinding* {
        return reinterpret_cast<ImGuiTextureBinding*>(static_cast<uintptr_t>(id));
    };

    if (texture_id == 0) {
        texture_id = g_font_texture_id;
    }

    auto* binding = resolve_binding(texture_id);
    if (binding != nullptr && binding->view && binding->descriptorSet && binding->view->GetTexture()) {
        return binding;
    }

    if (texture_id != g_font_texture_id && g_font_texture_id != 0) {
        binding = resolve_binding(g_font_texture_id);
        if (binding != nullptr && binding->view && binding->descriptorSet && binding->view->GetTexture()) {
            return binding;
        }
    }

    return nullptr;
}

void clearState()
{
    g_initialized = false;
    g_font_texture_id = 0;
    g_backend_type = luna::RHI::BackendType::Vulkan;
    g_texture_bindings.clear();
    g_frame_resources.clear();
    g_pipeline.reset();
    g_pipeline_color_format = luna::RHI::Format::UNDEFINED;
    g_vertex_shader.reset();
    g_fragment_shader.reset();
    g_pipeline_layout.reset();
    g_descriptor_pool.reset();
    g_texture_layout.reset();
    g_default_sampler.reset();
    g_font_texture.reset();
    g_shader_compiler.reset();
    g_graphics_queue.reset();
    g_device.reset();
}

} // namespace

bool ImGuiRhiContext::Init(luna::Renderer& renderer)
{
    if (g_initialized) {
        return true;
    }

    try {
        g_device = renderer.getDevice();
        g_graphics_queue = renderer.getGraphicsQueue();
        g_shader_compiler = renderer.getShaderCompiler();
        g_backend_type = renderer.getInstance() ? renderer.getInstance()->GetType() : luna::RHI::BackendType::Vulkan;
        if (!g_device || !g_graphics_queue || !g_shader_compiler || renderer.getNativeWindow() == nullptr) {
            LUNA_IMGUI_ERROR("Cannot initialize ImGui because renderer state is incomplete");
            clearState();
            return false;
        }

        if (!ImGui_ImplGlfw_InitForOther(renderer.getNativeWindow(), true)) {
            LUNA_IMGUI_ERROR("Failed to initialize ImGui GLFW platform backend");
            clearState();
            return false;
        }

        g_texture_layout = g_device->CreateDescriptorSetLayout(
            luna::RHI::DescriptorSetLayoutBuilder()
                .AddBinding(0, luna::RHI::DescriptorType::SampledImage, 1, luna::RHI::ShaderStage::Fragment)
                .AddBinding(1, luna::RHI::DescriptorType::Sampler, 1, luna::RHI::ShaderStage::Fragment)
                .Build());
        g_descriptor_pool = g_device->CreateDescriptorPool(
            luna::RHI::DescriptorPoolBuilder()
                .SetMaxSets(kImGuiDescriptorPoolSize)
                .AddPoolSize(luna::RHI::DescriptorType::SampledImage, kImGuiDescriptorPoolSize)
                .AddPoolSize(luna::RHI::DescriptorType::Sampler, kImGuiDescriptorPoolSize)
                .Build());
        g_pipeline_layout = g_device->CreatePipelineLayout(
            luna::RHI::PipelineLayoutBuilder()
                .AddSetLayout(g_texture_layout)
                .AddPushConstant(luna::RHI::ShaderStage::Vertex, 0, sizeof(ImGuiPushConstants))
                .Build());
        g_default_sampler = g_device->CreateSampler(luna::RHI::SamplerBuilder()
                                                        .SetFilter(luna::RHI::Filter::Linear, luna::RHI::Filter::Linear)
                                                        .SetAddressMode(luna::RHI::SamplerAddressMode::ClampToEdge)
                                                        .SetMipmapMode(luna::RHI::SamplerMipmapMode::Linear)
                                                        .SetAnisotropy(false)
                                                        .SetName("ImGuiDefaultSampler")
                                                        .Build());

        if (!g_texture_layout || !g_descriptor_pool || !g_pipeline_layout || !g_default_sampler || !ensureShaders() ||
            !createFontTexture()) {
            ImGui_ImplGlfw_Shutdown();
            clearState();
            return false;
        }

        NotifyFrameResourcesChanged(renderer.getFramesInFlight());

        ImGuiIO& io = ImGui::GetIO();
        io.BackendRendererName = "luna_rhi_imgui";
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

        g_initialized = true;
        return true;
    } catch (const std::exception& exception) {
        LUNA_IMGUI_ERROR("ImGui RHI initialization threw an exception: {}", exception.what());
    } catch (...) {
        LUNA_IMGUI_ERROR("ImGui RHI initialization hit an unknown exception");
    }

    ImGui_ImplGlfw_Shutdown();
    clearState();
    return false;
}

void ImGuiRhiContext::Destroy()
{
    if (!g_device && !ImGui::GetCurrentContext()) {
        clearState();
        return;
    }

    if (g_graphics_queue) {
        try {
            g_graphics_queue->WaitIdle();
        } catch (...) {
        }
    }

    if (ImGui::GetCurrentContext() != nullptr) {
        ImGuiIO& io = ImGui::GetIO();
        io.BackendRendererName = nullptr;
        io.BackendFlags &= ~ImGuiBackendFlags_RendererHasVtxOffset;
        io.Fonts->SetTexID(nullptr);
    }

    ImGui_ImplGlfw_Shutdown();
    clearState();
}

void ImGuiRhiContext::StartFrame()
{
    if (!g_initialized) {
        return;
    }

    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiRhiContext::RenderDrawData(luna::RHI::CommandBufferEncoder& command_buffer,
                                     const luna::RHI::Ref<luna::RHI::Texture>& color_target,
                                     uint32_t framebuffer_width,
                                     uint32_t framebuffer_height,
                                     uint32_t frame_index)
{
    if (!g_initialized || color_target == nullptr) {
        return;
    }

    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    if (draw_data == nullptr || draw_data->TotalVtxCount <= 0 || draw_data->TotalIdxCount <= 0) {
        return;
    }

    const int draw_fb_width = static_cast<int>(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    const int draw_fb_height = static_cast<int>(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if (draw_fb_width <= 0 || draw_fb_height <= 0) {
        return;
    }

    if (!ensurePipeline(color_target->GetFormat()) || !uploadDrawBuffers(draw_data, frame_index)) {
        return;
    }

    if (frame_index >= g_frame_resources.size()) {
        return;
    }

    const uint32_t target_width = framebuffer_width > 0 ? framebuffer_width : static_cast<uint32_t>(draw_fb_width);
    const uint32_t target_height = framebuffer_height > 0 ? framebuffer_height : static_cast<uint32_t>(draw_fb_height);
    auto& frame = g_frame_resources[frame_index];

    const ImVec2 clip_off = draw_data->DisplayPos;
    const ImVec2 clip_scale = draw_data->FramebufferScale;
    const float top_clip_y = imguiTopClipY(g_backend_type);
    const float scale_x = 2.0f / draw_data->DisplaySize.x;
    const float scale_y = (-2.0f * top_clip_y) / draw_data->DisplaySize.y;
    const ImGuiPushConstants push_constants{
        .scale =
            {
                scale_x,
                scale_y,
            },
        .translate =
            {
                -1.0f - clip_off.x * scale_x,
                top_clip_y - clip_off.y * scale_y,
            },
    };

    setupRenderState(command_buffer, target_width, target_height, push_constants, frame);

    int global_vtx_offset = 0;
    int global_idx_offset = 0;
    ImTextureID last_texture_id = 0;

    for (int list_index = 0; list_index < draw_data->CmdListsCount; ++list_index) {
        const ImDrawList* draw_list = draw_data->CmdLists[list_index];
        for (const ImDrawCmd& draw_command : draw_list->CmdBuffer) {
            if (draw_command.UserCallback != nullptr) {
                if (draw_command.UserCallback == ImDrawCallback_ResetRenderState) {
                    setupRenderState(command_buffer, target_width, target_height, push_constants, frame);
                } else {
                    draw_command.UserCallback(draw_list, &draw_command);
                }
                continue;
            }

            const ImVec2 clip_min{
                (draw_command.ClipRect.x - clip_off.x) * clip_scale.x,
                (draw_command.ClipRect.y - clip_off.y) * clip_scale.y,
            };
            const ImVec2 clip_max{
                (draw_command.ClipRect.z - clip_off.x) * clip_scale.x,
                (draw_command.ClipRect.w - clip_off.y) * clip_scale.y,
            };

            const int32_t scissor_x =
                static_cast<int32_t>(std::clamp(clip_min.x, 0.0f, static_cast<float>(target_width)));
            const int32_t scissor_y =
                static_cast<int32_t>(std::clamp(clip_min.y, 0.0f, static_cast<float>(target_height)));
            const int32_t scissor_max_x =
                static_cast<int32_t>(std::clamp(clip_max.x, 0.0f, static_cast<float>(target_width)));
            const int32_t scissor_max_y =
                static_cast<int32_t>(std::clamp(clip_max.y, 0.0f, static_cast<float>(target_height)));
            if (scissor_max_x <= scissor_x || scissor_max_y <= scissor_y) {
                continue;
            }

            command_buffer.SetScissor({
                .OffsetX = scissor_x,
                .OffsetY = scissor_y,
                .Width = static_cast<uint32_t>(scissor_max_x - scissor_x),
                .Height = static_cast<uint32_t>(scissor_max_y - scissor_y),
            });

            if (draw_command.GetTexID() != last_texture_id) {
                last_texture_id = draw_command.GetTexID();
                if (auto* binding = resolveTextureBinding(last_texture_id);
                    binding != nullptr && binding->descriptorSet) {
                    const std::array<luna::RHI::Ref<luna::RHI::DescriptorSet>, 1> descriptor_sets{
                        binding->descriptorSet};
                    command_buffer.BindDescriptorSets(g_pipeline, 0, descriptor_sets);
                }
            }

            command_buffer.DrawIndexed(draw_command.ElemCount,
                                       1,
                                       static_cast<uint32_t>(draw_command.IdxOffset + global_idx_offset),
                                       static_cast<int32_t>(draw_command.VtxOffset + global_vtx_offset),
                                       0);
        }

        global_idx_offset += draw_list->IdxBuffer.Size;
        global_vtx_offset += draw_list->VtxBuffer.Size;
    }
}

ImTextureID ImGuiRhiContext::GetTextureId(const luna::RHI::Ref<luna::RHI::Texture>& texture)
{
    if (!texture) {
        return 0;
    }

    auto default_view = texture->GetDefaultView();
    return GetTextureId(default_view, g_default_sampler);
}

ImTextureID ImGuiRhiContext::GetTextureId(const luna::RHI::Ref<luna::RHI::TextureView>& view,
                                          const luna::RHI::Ref<luna::RHI::Sampler>& sampler)
{
    auto binding = getOrCreateTextureBinding(view, sampler);
    return binding ? static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(binding.get())) : 0;
}

void ImGuiRhiContext::EndFrame() {}

void ImGuiRhiContext::NotifyFrameResourcesChanged(uint32_t frames_in_flight)
{
    g_frame_resources.resize((std::max)(frames_in_flight, 1u));
}

} // namespace luna::rhi
