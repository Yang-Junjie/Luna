#include "Core/application.h"
#include "Core/log.h"
#include "RHI/CommandContext.h"
#include "Renderer/RenderPipeline.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>

namespace {

enum class PushMode : uint8_t {
    Neutral = 0,
    Shifted
};

struct QuadVertex {
    float position[2];
    float color[3];
};

struct alignas(16) QuadPushConstants {
    float tint[4];
    float offset[2];
    float scale;
    float mixFactor;
};

static_assert(sizeof(QuadPushConstants) == 32);

PushMode parse_push_mode(int argc, char** argv)
{
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--push-mode=shifted" || argument == "--variant=shifted") {
            return PushMode::Shifted;
        }
    }

    return PushMode::Neutral;
}

std::string_view to_string(PushMode mode)
{
    switch (mode) {
        case PushMode::Shifted:
            return "shifted";
        case PushMode::Neutral:
        default:
            return "neutral";
    }
}

std::filesystem::path shader_root()
{
    return std::filesystem::path{RHI_INDEXED_QUAD_SHADER_ROOT}.lexically_normal();
}

class IndexedQuadRenderPipeline final : public luna::IRenderPipeline {
public:
    IndexedQuadRenderPipeline(std::filesystem::path vertexShaderPath,
                              std::filesystem::path fragmentShaderPath,
                              PushMode pushMode)
        : m_vertexShaderPath(vertexShaderPath.lexically_normal().generic_string()),
          m_fragmentShaderPath(fragmentShaderPath.lexically_normal().generic_string()),
          m_pushMode(pushMode)
    {}

    bool init(luna::IRHIDevice&) override
    {
        return true;
    }

    void shutdown(luna::IRHIDevice& device) override
    {
        if (m_pipeline.isValid()) {
            device.destroyPipeline(m_pipeline);
            m_pipeline = {};
        }
        if (m_indexBuffer.isValid()) {
            device.destroyBuffer(m_indexBuffer);
            m_indexBuffer = {};
        }
        if (m_vertexBuffer.isValid()) {
            device.destroyBuffer(m_vertexBuffer);
            m_vertexBuffer = {};
        }
        m_pipelineFormat = luna::PixelFormat::Undefined;
    }

    bool render(luna::IRHIDevice& device, const luna::FrameContext& frameContext) override
    {
        if (frameContext.commandContext == nullptr || !frameContext.backbuffer.isValid()) {
            return false;
        }

        if (!ensure_resources(device, frameContext.backbufferFormat)) {
            return false;
        }

        const QuadPushConstants pushConstants = build_push_constants();

        luna::RenderingInfo renderingInfo{};
        renderingInfo.width = frameContext.renderWidth;
        renderingInfo.height = frameContext.renderHeight;
        renderingInfo.colorAttachments.push_back(
            {.image = frameContext.backbuffer,
             .format = frameContext.backbufferFormat,
             .clearColor = {0.08f, 0.12f, 0.18f, 1.0f}});

        const bool ok = frameContext.commandContext->beginRendering(renderingInfo) == luna::RHIResult::Success &&
                        frameContext.commandContext->bindGraphicsPipeline(m_pipeline) == luna::RHIResult::Success &&
                        frameContext.commandContext->bindVertexBuffer(m_vertexBuffer, 0) == luna::RHIResult::Success &&
                        frameContext.commandContext->bindIndexBuffer(m_indexBuffer, luna::IndexFormat::UInt16, 0) ==
                            luna::RHIResult::Success &&
                        frameContext.commandContext->pushConstants(
                            &pushConstants, sizeof(pushConstants), 0, luna::ShaderType::AllGraphics) ==
                            luna::RHIResult::Success &&
                        frameContext.commandContext->drawIndexed({6, 1, 0, 0, 0}) == luna::RHIResult::Success &&
                        frameContext.commandContext->endRendering() == luna::RHIResult::Success;

        if (ok && !m_loggedPass) {
            LUNA_CORE_INFO("bind index buffer PASS");
            LUNA_CORE_INFO("push constants PASS");
            LUNA_CORE_INFO("drawIndexed PASS");
            LUNA_CORE_INFO("Push mode={} offset=({}, {}) scale={} mixFactor={}",
                           to_string(m_pushMode),
                           pushConstants.offset[0],
                           pushConstants.offset[1],
                           pushConstants.scale,
                           pushConstants.mixFactor);
            m_loggedPass = true;
        }

        return ok;
    }

private:
    bool ensure_resources(luna::IRHIDevice& device, luna::PixelFormat backbufferFormat)
    {
        if (backbufferFormat == luna::PixelFormat::Undefined) {
            return false;
        }

        if (!m_vertexBuffer.isValid()) {
            luna::BufferDesc vertexBufferDesc{};
            vertexBufferDesc.size = static_cast<uint64_t>(sizeof(QuadVertex) * m_vertices.size());
            vertexBufferDesc.usage = luna::BufferUsage::Vertex | luna::BufferUsage::TransferDst;
            vertexBufferDesc.memoryUsage = luna::MemoryUsage::Default;
            vertexBufferDesc.debugName = "IndexedQuadVertices";
            if (device.createBuffer(vertexBufferDesc, &m_vertexBuffer, m_vertices.data()) != luna::RHIResult::Success) {
                return false;
            }
        }

        if (!m_indexBuffer.isValid()) {
            luna::BufferDesc indexBufferDesc{};
            indexBufferDesc.size = static_cast<uint64_t>(sizeof(uint16_t) * m_indices.size());
            indexBufferDesc.usage = luna::BufferUsage::Index | luna::BufferUsage::TransferDst;
            indexBufferDesc.memoryUsage = luna::MemoryUsage::Default;
            indexBufferDesc.debugName = "IndexedQuadIndices";
            if (device.createBuffer(indexBufferDesc, &m_indexBuffer, m_indices.data()) != luna::RHIResult::Success) {
                return false;
            }
        }

        if (m_pipeline.isValid() && m_pipelineFormat == backbufferFormat) {
            return true;
        }

        if (m_pipeline.isValid()) {
            device.destroyPipeline(m_pipeline);
            m_pipeline = {};
        }

        luna::GraphicsPipelineDesc pipelineDesc{};
        pipelineDesc.debugName = "IndexedQuadPipeline";
        pipelineDesc.vertexShader = {.stage = luna::ShaderType::Vertex, .filePath = m_vertexShaderPath};
        pipelineDesc.fragmentShader = {.stage = luna::ShaderType::Fragment, .filePath = m_fragmentShaderPath};
        pipelineDesc.vertexLayout.stride = sizeof(QuadVertex);
        pipelineDesc.vertexLayout.attributes.push_back({0, 0, luna::VertexFormat::Float2});
        pipelineDesc.vertexLayout.attributes.push_back({1, static_cast<uint32_t>(sizeof(float) * 2), luna::VertexFormat::Float3});
        pipelineDesc.cullMode = luna::CullMode::None;
        pipelineDesc.frontFace = luna::FrontFace::Clockwise;
        pipelineDesc.pushConstantSize = sizeof(QuadPushConstants);
        pipelineDesc.pushConstantVisibility = luna::ShaderType::AllGraphics;
        pipelineDesc.colorAttachments.push_back({backbufferFormat, false});

        if (device.createGraphicsPipeline(pipelineDesc, &m_pipeline) != luna::RHIResult::Success) {
            return false;
        }

        m_pipelineFormat = backbufferFormat;
        return true;
    }

    QuadPushConstants build_push_constants() const
    {
        QuadPushConstants pushConstants{};
        pushConstants.tint[3] = 1.0f;

        if (m_pushMode == PushMode::Shifted) {
            pushConstants.tint[0] = 1.0f;
            pushConstants.tint[1] = 0.45f;
            pushConstants.tint[2] = 0.1f;
            pushConstants.offset[0] = 0.28f;
            pushConstants.offset[1] = -0.12f;
            pushConstants.scale = 0.62f;
            pushConstants.mixFactor = 0.82f;
            return pushConstants;
        }

        pushConstants.tint[0] = 0.12f;
        pushConstants.tint[1] = 0.65f;
        pushConstants.tint[2] = 1.0f;
        pushConstants.offset[0] = 0.0f;
        pushConstants.offset[1] = 0.0f;
        pushConstants.scale = 0.82f;
        pushConstants.mixFactor = 0.18f;
        return pushConstants;
    }

private:
    std::string m_vertexShaderPath;
    std::string m_fragmentShaderPath;
    PushMode m_pushMode = PushMode::Neutral;
    luna::BufferHandle m_vertexBuffer{};
    luna::BufferHandle m_indexBuffer{};
    luna::PipelineHandle m_pipeline{};
    luna::PixelFormat m_pipelineFormat = luna::PixelFormat::Undefined;
    bool m_loggedPass = false;
    std::array<QuadVertex, 4> m_vertices{{
        QuadVertex{{-0.7f, -0.7f}, {1.0f, 0.2f, 0.2f}},
        QuadVertex{{0.7f, -0.7f}, {0.2f, 1.0f, 0.3f}},
        QuadVertex{{0.7f, 0.7f}, {0.2f, 0.4f, 1.0f}},
        QuadVertex{{-0.7f, 0.7f}, {1.0f, 0.9f, 0.2f}},
    }};
    std::array<uint16_t, 6> m_indices{{0, 1, 2, 2, 3, 0}};
};

} // namespace

int main(int argc, char** argv)
{
#ifndef NDEBUG
    constexpr luna::Logger::Level kLogLevel = luna::Logger::Level::Trace;
#else
    constexpr luna::Logger::Level kLogLevel = luna::Logger::Level::Info;
#endif

    luna::Logger::init("logs/luna.log", kLogLevel);

    const std::filesystem::path quadShaderRoot = shader_root();
    const PushMode pushMode = parse_push_mode(argc, argv);
    LUNA_CORE_INFO("Selected push mode={}", to_string(pushMode));

    std::shared_ptr<luna::IRenderPipeline> renderPipeline = std::make_shared<IndexedQuadRenderPipeline>(
        quadShaderRoot / "quad.vert.spv", quadShaderRoot / "quad.frag.spv", pushMode);

    luna::ApplicationSpecification specification{
        .name = "RhiIndexedQuad",
        .windowWidth = 1280,
        .windowHeight = 720,
        .maximized = false,
        .enableImGui = false,
        .enableMultiViewport = false,
        .renderService =
            {
                .applicationName = "RhiIndexedQuad",
                .backend = luna::RHIBackend::Vulkan,
                .renderPipeline = renderPipeline,
            },
    };

    std::unique_ptr<luna::Application> app = std::make_unique<luna::Application>(specification);
    if (!app->isInitialized()) {
        app.reset();
        luna::Logger::shutdown();
        return 1;
    }

    app->run();
    app.reset();
    luna::Logger::shutdown();
    return 0;
}
