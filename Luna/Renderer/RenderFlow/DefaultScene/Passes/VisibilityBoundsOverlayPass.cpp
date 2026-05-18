#include "Renderer/RenderFlow/DefaultScene/Passes/VisibilityBoundsOverlayPass.h"

#include "Core/Log.h"
#include "Renderer/RendererUtilities.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/PassCommon.h"
#include "Renderer/RenderFlow/DefaultScene/SharedState.h"
#include "Renderer/RenderFlow/RenderBlackboardKeys.h"
#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/Resources/ShaderModuleLoader.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <limits>
#include <optional>
#include <utility>

#include <Buffer.h>
#include <Builders.h>
#include <CommandBufferEncoder.h>
#include <DescriptorPool.h>
#include <DescriptorSet.h>
#include <DescriptorSetLayout.h>
#include <Device.h>
#include <Pipeline.h>
#include <PipelineLayout.h>
#include <ShaderCompiler.h>
#include <ShaderModule.h>

namespace luna::render_flow::default_scene {
namespace {

constexpr std::array<RenderPassResourceUsage, 1> kPassResources{{
    {.name = blackboard::SceneFinalColor.value(),
     .access = RenderPassResourceAccess::ReadWrite,
     .flags = RenderFeatureGraphResourceFlags::External},
}};

constexpr std::array<std::pair<uint8_t, uint8_t>, 12> kBoxEdges{{
    {0, 1},
    {1, 2},
    {2, 3},
    {3, 0},
    {4, 5},
    {5, 6},
    {6, 7},
    {7, 4},
    {0, 4},
    {1, 5},
    {2, 6},
    {3, 7},
}};

struct VisibilityBoundsOverlayGpuParams {
    glm::mat4 view_projection{1.0f};
};

namespace overlay_binding {
constexpr uint32_t Params = 0;
} // namespace overlay_binding

bool isValidTextureHandle(const std::optional<RenderGraphTextureHandle>& handle)
{
    return handle.has_value() && handle->isValid();
}

std::filesystem::path shaderPath()
{
    return std::filesystem::path(LUNA_PROJECT_ROOT) / "Luna" / "Renderer" / "Shaders" /
           "VisibilityBoundsOverlay.slang";
}

RHI::ColorBlendAttachmentState makeAlphaBlendAttachment()
{
    RHI::ColorBlendAttachmentState blend_attachment{};
    blend_attachment.BlendEnable = true;
    blend_attachment.SrcColorBlendFactor = RHI::BlendFactor::SrcAlpha;
    blend_attachment.DstColorBlendFactor = RHI::BlendFactor::OneMinusSrcAlpha;
    blend_attachment.ColorBlendOp = RHI::BlendOp::Add;
    blend_attachment.SrcAlphaBlendFactor = RHI::BlendFactor::One;
    blend_attachment.DstAlphaBlendFactor = RHI::BlendFactor::OneMinusSrcAlpha;
    blend_attachment.AlphaBlendOp = RHI::BlendOp::Add;
    blend_attachment.ColorWriteMask = RHI::ColorComponentFlags::All;
    return blend_attachment;
}

RHI::Ref<RHI::DescriptorSetLayout> createDescriptorSetLayout(
    const RHI::Ref<RHI::Device>& device)
{
    if (!device) {
        return {};
    }
    return device->CreateDescriptorSetLayout(RHI::DescriptorSetLayoutBuilder()
                                                 .AddBinding(overlay_binding::Params,
                                                             RHI::DescriptorType::UniformBuffer,
                                                             1,
                                                             RHI::ShaderStage::Vertex)
                                                 .Build());
}

RHI::Ref<RHI::DescriptorPool> createDescriptorPool(const RHI::Ref<RHI::Device>& device)
{
    if (!device) {
        return {};
    }
    return device->CreateDescriptorPool(RHI::DescriptorPoolBuilder()
                                            .SetMaxSets(1)
                                            .AddPoolSize(RHI::DescriptorType::UniformBuffer, 1)
                                            .Build());
}

RHI::Ref<RHI::PipelineLayout> createPipelineLayout(
    const RHI::Ref<RHI::Device>& device,
    const RHI::Ref<RHI::DescriptorSetLayout>& layout)
{
    if (!device || !layout) {
        return {};
    }
    return device->CreatePipelineLayout(RHI::PipelineLayoutBuilder().AddSetLayout(layout).Build());
}

RHI::Ref<RHI::GraphicsPipeline> createPipeline(
    const RHI::Ref<RHI::Device>& device,
    const RHI::Ref<RHI::PipelineLayout>& layout,
    const RHI::Ref<RHI::ShaderModule>& vertex_shader,
    const RHI::Ref<RHI::ShaderModule>& fragment_shader,
    RHI::Format color_format)
{
    if (!device || !layout || !vertex_shader || !fragment_shader ||
        color_format == RHI::Format::UNDEFINED) {
        return {};
    }

    return device->CreateGraphicsPipeline(
        RHI::GraphicsPipelineBuilder()
            .SetShaders({vertex_shader, fragment_shader})
            .AddVertexBinding(0, sizeof(VisibilityBoundsOverlayVertex), RHI::VertexInputRate::Vertex)
            .AddVertexAttribute(0,
                                0,
                                RHI::Format::RGB32_FLOAT,
                                offsetof(VisibilityBoundsOverlayVertex, world_position),
                                "POSITION",
                                0)
            .AddVertexAttribute(1,
                                0,
                                RHI::Format::RGBA32_FLOAT,
                                offsetof(VisibilityBoundsOverlayVertex, color),
                                "COLOR",
                                0)
            .SetTopology(RHI::PrimitiveTopology::LineList)
            .SetCullMode(RHI::CullMode::None)
            .SetFrontFace(RHI::FrontFace::CounterClockwise)
            .SetDepthTest(false, false, RHI::CompareOp::Always)
            .SetLineWidth(1.0f)
            .AddColorAttachment(makeAlphaBlendAttachment())
            .AddColorFormat(color_format)
            .SetLayout(layout)
            .Build());
}

std::array<glm::vec3, 8> boxCorners(const glm::vec3& min, const glm::vec3& max)
{
    return {{
        {min.x, min.y, min.z},
        {max.x, min.y, min.z},
        {max.x, max.y, min.z},
        {min.x, max.y, min.z},
        {min.x, min.y, max.z},
        {max.x, min.y, max.z},
        {max.x, max.y, max.z},
        {min.x, max.y, max.z},
    }};
}

void appendBoxLineVertices(const std::array<glm::vec3, 8>& corners,
                           const glm::vec4& color,
                           std::vector<VisibilityBoundsOverlayVertex>& vertices)
{
    for (const auto& [first, second] : kBoxEdges) {
        vertices.push_back(VisibilityBoundsOverlayVertex{
            .world_position = corners[first],
            .color = color,
        });
        vertices.push_back(VisibilityBoundsOverlayVertex{
            .world_position = corners[second],
            .color = color,
        });
    }
}

void clearOverlayPass(RenderGraphRasterPassContext& pass_context)
{
    pass_context.beginRendering();
    pass_context.endRendering();
}

} // namespace

glm::vec4 visibilityBoundsOverlayColor(VisibilityDebugClassification classification) noexcept
{
    switch (classification) {
        case VisibilityDebugClassification::CameraVisible:
            return {0.16f, 1.0f, 0.32f, 0.88f};
        case VisibilityDebugClassification::CameraCulled:
            return {1.0f, 0.16f, 0.10f, 0.92f};
        case VisibilityDebugClassification::InvalidBounds:
            return {1.0f, 0.82f, 0.10f, 0.95f};
    }
    return {1.0f, 1.0f, 1.0f, 0.9f};
}

VisibilityBoundsOverlayBuildStats buildVisibilityBoundsOverlayVertices(
    std::span<const VisibilityDebugItem> items,
    std::span<const VisibilityDebugFrustumItem> frustums,
    std::vector<VisibilityBoundsOverlayVertex>& vertices)
{
    vertices.clear();
    vertices.reserve((items.size() + frustums.size()) * kBoxEdges.size() * 2);

    VisibilityBoundsOverlayBuildStats stats{};
    stats.items = static_cast<uint32_t>((std::min) (items.size(),
                                                   static_cast<size_t>((std::numeric_limits<uint32_t>::max)())));
    for (const VisibilityDebugItem& item : items) {
        const glm::vec4 color = visibilityBoundsOverlayColor(item.classification);
        if (item.world_bounds.isValid()) {
            appendBoxLineVertices(boxCorners(item.world_bounds.Min, item.world_bounds.Max), color, vertices);
            ++stats.bounds;
            continue;
        }

        const glm::vec3 extent{kVisibilityBoundsInvalidMarkerHalfExtent};
        appendBoxLineVertices(boxCorners(item.world_origin - extent, item.world_origin + extent), color, vertices);
        ++stats.invalid_markers;
    }

    stats.frustums = static_cast<uint32_t>((std::min) (frustums.size(),
                                                       static_cast<size_t>((std::numeric_limits<uint32_t>::max)())));
    for (const VisibilityDebugFrustumItem& frustum : frustums) {
        appendBoxLineVertices(frustum.corners, frustum.color, vertices);
    }

    stats.vertices = static_cast<uint32_t>((std::min) (vertices.size(),
                                                      static_cast<size_t>((std::numeric_limits<uint32_t>::max)())));
    return stats;
}

struct VisibilityBoundsOverlayResources::State {
    RHI::Ref<RHI::Device> device;
    RHI::Ref<RHI::ShaderModule> vertex_shader;
    RHI::Ref<RHI::ShaderModule> fragment_shader;
    RHI::Ref<RHI::DescriptorSetLayout> descriptor_set_layout;
    RHI::Ref<RHI::DescriptorPool> descriptor_pool;
    RHI::Ref<RHI::DescriptorSet> descriptor_set;
    RHI::Ref<RHI::PipelineLayout> pipeline_layout;
    RHI::Ref<RHI::GraphicsPipeline> pipeline;
    RHI::Ref<RHI::Buffer> params_buffer;
    RHI::Ref<RHI::Buffer> vertex_buffer;
    RHI::Format color_format{RHI::Format::UNDEFINED};
    uint64_t vertex_buffer_bytes{0};
    uint32_t vertex_count{0};
    VisibilityBoundsOverlayBuildStats build_stats{};

    [[nodiscard]] bool pipelineReady() const noexcept
    {
        return device && vertex_shader && fragment_shader && pipeline_layout && pipeline &&
               descriptor_set_layout && descriptor_pool && descriptor_set && params_buffer &&
               color_format != RHI::Format::UNDEFINED;
    }
};

VisibilityBoundsOverlayResources::VisibilityBoundsOverlayResources()
    : m_state(std::make_unique<State>())
{}

VisibilityBoundsOverlayResources::~VisibilityBoundsOverlayResources() = default;

void VisibilityBoundsOverlayResources::resetFrameStats() noexcept
{
    m_state->vertex_count = 0;
    m_state->build_stats = {};
}

void VisibilityBoundsOverlayResources::shutdown()
{
    *m_state = {};
}

bool VisibilityBoundsOverlayResources::ensurePipeline(const SceneRenderContext& context)
{
    if (!context.device || !context.compiler || context.color_format == RHI::Format::UNDEFINED) {
        return false;
    }
    if (m_state->pipelineReady() && m_state->device == context.device &&
        m_state->color_format == context.color_format) {
        return true;
    }

    const VisibilityBoundsOverlayBuildStats current_build_stats = m_state->build_stats;
    const uint32_t current_vertex_count = m_state->vertex_count;
    *m_state = {};
    m_state->build_stats = current_build_stats;
    m_state->vertex_count = current_vertex_count;
    m_state->device = context.device;
    m_state->color_format = context.color_format;

    const std::filesystem::path path = shaderPath();
    m_state->vertex_shader = renderer_detail::loadShaderModule(context.device,
                                                               context.compiler,
                                                               path,
                                                               "visibilityBoundsOverlayVertexMain",
                                                               RHI::ShaderStage::Vertex);
    m_state->fragment_shader = renderer_detail::loadShaderModule(context.device,
                                                                 context.compiler,
                                                                 path,
                                                                 "visibilityBoundsOverlayFragmentMain",
                                                                 RHI::ShaderStage::Fragment);
    m_state->descriptor_set_layout = createDescriptorSetLayout(context.device);
    m_state->descriptor_pool = createDescriptorPool(context.device);
    if (m_state->descriptor_pool && m_state->descriptor_set_layout) {
        m_state->descriptor_set = m_state->descriptor_pool->AllocateDescriptorSet(m_state->descriptor_set_layout);
    }
    m_state->pipeline_layout = createPipelineLayout(context.device, m_state->descriptor_set_layout);
    m_state->pipeline = createPipeline(context.device,
                                       m_state->pipeline_layout,
                                       m_state->vertex_shader,
                                       m_state->fragment_shader,
                                       context.color_format);
    m_state->params_buffer = context.device->CreateBuffer(RHI::BufferBuilder()
                                                              .SetSize(sizeof(VisibilityBoundsOverlayGpuParams))
                                                              .SetUsage(RHI::BufferUsageFlags::UniformBuffer)
                                                              .SetMemoryUsage(RHI::BufferMemoryUsage::CpuToGpu)
                                                              .SetName("VisibilityBoundsOverlayParams")
                                                              .Build());

    if (!m_state->pipelineReady()) {
        LUNA_RENDERER_WARN("Visibility bounds overlay resources incomplete: vertex_shader={} fragment_shader={} "
                           "descriptor_layout={} descriptor_pool={} descriptor_set={} pipeline_layout={} "
                           "pipeline={} params_buffer={}",
                           static_cast<bool>(m_state->vertex_shader),
                           static_cast<bool>(m_state->fragment_shader),
                           static_cast<bool>(m_state->descriptor_set_layout),
                           static_cast<bool>(m_state->descriptor_pool),
                           static_cast<bool>(m_state->descriptor_set),
                           static_cast<bool>(m_state->pipeline_layout),
                           static_cast<bool>(m_state->pipeline),
                           static_cast<bool>(m_state->params_buffer));
        return false;
    }

    LUNA_RENDERER_DEBUG("Visibility bounds overlay pipeline ready");
    return true;
}

void updateOverlayParams(const RHI::Ref<RHI::Buffer>& params_buffer,
                         const RHI::Ref<RHI::DescriptorSet>& descriptor_set,
                         const glm::mat4& view_projection)
{
    if (!params_buffer || !descriptor_set) {
        return;
    }

    const VisibilityBoundsOverlayGpuParams params{
        .view_projection = view_projection,
    };
    if (void* mapped = params_buffer->Map()) {
        std::memcpy(mapped, &params, sizeof(params));
        params_buffer->Flush(0, sizeof(params));
        params_buffer->Unmap();
    }

    descriptor_set->WriteBuffer(RHI::BufferWriteInfo{
        .Binding = overlay_binding::Params,
        .Buffer = params_buffer,
        .Offset = 0,
        .Stride = sizeof(VisibilityBoundsOverlayGpuParams),
        .Size = sizeof(VisibilityBoundsOverlayGpuParams),
        .Type = RHI::DescriptorType::UniformBuffer,
    });
    descriptor_set->Update();
}

bool VisibilityBoundsOverlayResources::ensureVertexBuffer(const SceneRenderContext& context,
                                                          uint64_t required_bytes)
{
    if (required_bytes == 0 || !context.device) {
        return false;
    }
    if (m_state->vertex_buffer && m_state->vertex_buffer_bytes >= required_bytes) {
        return true;
    }

    const uint64_t capacity = (std::max) (required_bytes, m_state->vertex_buffer_bytes * 2);
    m_state->vertex_buffer = context.device->CreateBuffer(RHI::BufferBuilder()
                                                              .SetSize(capacity)
                                                              .SetUsage(RHI::BufferUsageFlags::VertexBuffer)
                                                              .SetMemoryUsage(RHI::BufferMemoryUsage::CpuToGpu)
                                                              .SetName("VisibilityBoundsOverlayVertices")
                                                              .Build());
    m_state->vertex_buffer_bytes = m_state->vertex_buffer ? capacity : 0;
    if (!m_state->vertex_buffer) {
        LUNA_RENDERER_WARN("Failed to create visibility bounds overlay vertex buffer ({} bytes)", required_bytes);
        return false;
    }
    return true;
}

bool VisibilityBoundsOverlayResources::upload(const SceneRenderContext& context,
                                              std::span<const VisibilityDebugItem> items,
                                              std::span<const VisibilityDebugFrustumItem> frustums,
                                              const glm::mat4& view_projection)
{
    std::vector<VisibilityBoundsOverlayVertex> vertices;
    m_state->build_stats = buildVisibilityBoundsOverlayVertices(items, frustums, vertices);
    m_state->vertex_count = 0;
    if (vertices.empty() || vertices.size() > (std::numeric_limits<uint32_t>::max)()) {
        return false;
    }
    if (!ensurePipeline(context)) {
        return false;
    }

    const uint64_t required_bytes = static_cast<uint64_t>(vertices.size()) * sizeof(VisibilityBoundsOverlayVertex);
    if (!ensureVertexBuffer(context, required_bytes)) {
        return false;
    }
    updateOverlayParams(m_state->params_buffer, m_state->descriptor_set, view_projection);

    if (void* mapped = m_state->vertex_buffer->Map()) {
        std::memcpy(mapped, vertices.data(), static_cast<size_t>(required_bytes));
        m_state->vertex_buffer->Flush(0, required_bytes);
        m_state->vertex_buffer->Unmap();
        m_state->vertex_count = static_cast<uint32_t>(vertices.size());
        return true;
    }

    LUNA_RENDERER_WARN("Failed to map visibility bounds overlay vertex buffer");
    return false;
}

void VisibilityBoundsOverlayResources::draw(RenderGraphRasterPassContext& pass_context,
                                            uint32_t vertex_count) const
{
    if (!m_state->pipelineReady() || !m_state->vertex_buffer || vertex_count == 0) {
        clearOverlayPass(pass_context);
        return;
    }

    pass_context.beginRendering();
    auto& commands = pass_context.commandBuffer();
    commands.BindGraphicsPipeline(m_state->pipeline);
    configureViewportAndScissor(commands, pass_context.framebufferWidth(), pass_context.framebufferHeight());
    const std::array<RHI::Ref<RHI::DescriptorSet>, 1> descriptor_sets{m_state->descriptor_set};
    commands.BindDescriptorSets(m_state->pipeline, 0, descriptor_sets);
    commands.BindVertexBuffer(0, m_state->vertex_buffer);
    commands.Draw(vertex_count, 1, 0, 0);
    pass_context.endRendering();
}

uint32_t VisibilityBoundsOverlayResources::vertexCount() const noexcept
{
    return m_state->vertex_count;
}

VisibilityBoundsOverlayStats VisibilityBoundsOverlayResources::stats() const noexcept
{
    return VisibilityBoundsOverlayStats{
        .build = m_state->build_stats,
        .vertex_buffer_bytes = m_state->vertex_buffer_bytes,
        .resources_ready = m_state->pipelineReady(),
    };
}

VisibilityBoundsOverlayPass::VisibilityBoundsOverlayPass(PassSharedState& state,
                                                         VisibilityBoundsOverlayResources& resources)
    : m_state(&state),
      m_resources(&resources)
{}

const char* VisibilityBoundsOverlayPass::name() const noexcept
{
    return "VisibilityBoundsOverlay";
}

std::span<const RenderPassResourceUsage> VisibilityBoundsOverlayPass::resourceUsages() const noexcept
{
    return kPassResources;
}

void VisibilityBoundsOverlayPass::setup(RenderPassContext& context)
{
    if (!m_state || !m_resources) {
        return;
    }
    m_resources->resetFrameStats();

    const auto& draw_queue = m_state->drawQueue();
    const auto& debug_items = draw_queue.visibilityDebugItems();
    const auto& debug_frustums = draw_queue.visibilityDebugFrustums();
    if (debug_items.empty() && debug_frustums.empty()) {
        return;
    }

    const auto scene_color = context.blackboard().get(blackboard::SceneFinalColor);
    if (!isValidTextureHandle(scene_color)) {
        LUNA_RENDERER_WARN("VisibilityBoundsOverlay missing input texture '{}'",
                           blackboard::SceneFinalColor.value());
        return;
    }

    const RenderFeatureFrameContext* frame_context = m_state->frameContext();
    if (frame_context == nullptr) {
        return;
    }

    if (!m_resources->upload(context.sceneContext(),
                             debug_items,
                             debug_frustums,
                             frame_context->view.current.view_projection)) {
        return;
    }

    const uint32_t vertex_count = m_resources->vertexCount();
    if (vertex_count == 0) {
        return;
    }

    blackboard::publishSceneColorStage(context.blackboard(), blackboard::SceneColorStage::Final, *scene_color);

    context.graph().AddRasterPass(
        name(),
        [scene_color = *scene_color](RenderGraphRasterPassBuilder& pass_builder) {
            pass_builder.WriteColor(scene_color,
                                    RHI::AttachmentLoadOp::Load,
                                    RHI::AttachmentStoreOp::Store);
        },
        [this, vertex_count](RenderGraphRasterPassContext& pass_context) {
            if (m_resources == nullptr) {
                clearOverlayPass(pass_context);
                return;
            }
            m_resources->draw(pass_context, vertex_count);
        });
}

} // namespace luna::render_flow::default_scene
