#include "Core/application.h"
#include "Core/log.h"
#include "RHI/CommandContext.h"
#include "Renderer/RenderPipeline.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>

namespace {

enum class SampleRendererMode : uint8_t {
    Triangle = 0,
    Clear
};

struct TriangleVertex {
    float position[2];
    float color[3];
};

SampleRendererMode parse_renderer_mode(int argc, char** argv)
{
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--renderer=clear" || argument == "--mode=clear") {
            return SampleRendererMode::Clear;
        }
        if (argument == "--renderer=triangle" || argument == "--mode=triangle") {
            return SampleRendererMode::Triangle;
        }
    }

    return SampleRendererMode::Triangle;
}

class ClearColorPipeline final : public luna::IRenderPipeline {
public:
    explicit ClearColorPipeline(luna::ClearColorValue clearColor)
        : m_clearColor(clearColor)
    {}

    bool init(luna::IRHIDevice&) override
    {
        return true;
    }

    void shutdown(luna::IRHIDevice&) override {}

    bool render(luna::IRHIDevice&, const luna::FrameContext& frameContext) override
    {
        if (frameContext.commandContext == nullptr || !frameContext.backbuffer.isValid()) {
            return false;
        }

        luna::RenderingInfo renderingInfo{};
        renderingInfo.width = frameContext.renderWidth;
        renderingInfo.height = frameContext.renderHeight;
        renderingInfo.colorAttachments.push_back(
            {.image = frameContext.backbuffer, .format = frameContext.backbufferFormat, .clearColor = m_clearColor});

        return frameContext.commandContext->beginRendering(renderingInfo) == luna::RHIResult::Success &&
               frameContext.commandContext->endRendering() == luna::RHIResult::Success;
    }

private:
    luna::ClearColorValue m_clearColor{};
};

class TriangleRenderPipeline final : public luna::IRenderPipeline {
public:
    TriangleRenderPipeline(std::filesystem::path vertexShaderPath, std::filesystem::path fragmentShaderPath)
        : m_vertexShaderPath(vertexShaderPath.lexically_normal().generic_string()),
          m_fragmentShaderPath(fragmentShaderPath.lexically_normal().generic_string())
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
        if (m_vertexBuffer.isValid()) {
            device.destroyBuffer(m_vertexBuffer);
            m_vertexBuffer = {};
        }
        if (m_fragmentShader.isValid()) {
            device.destroyShader(m_fragmentShader);
            m_fragmentShader = {};
        }
        if (m_vertexShader.isValid()) {
            device.destroyShader(m_vertexShader);
            m_vertexShader = {};
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

        luna::RenderingInfo renderingInfo{};
        renderingInfo.width = frameContext.renderWidth;
        renderingInfo.height = frameContext.renderHeight;
        renderingInfo.colorAttachments.push_back(
            {.image = frameContext.backbuffer,
             .format = frameContext.backbufferFormat,
             .clearColor = {0.08f, 0.12f, 0.18f, 1.0f}});

        return frameContext.commandContext->beginRendering(renderingInfo) == luna::RHIResult::Success &&
               frameContext.commandContext->bindGraphicsPipeline(m_pipeline) == luna::RHIResult::Success &&
               frameContext.commandContext->bindVertexBuffer(m_vertexBuffer, 0) == luna::RHIResult::Success &&
               frameContext.commandContext->draw({3, 1, 0, 0}) == luna::RHIResult::Success &&
               frameContext.commandContext->endRendering() == luna::RHIResult::Success;
    }

private:
    bool ensure_resources(luna::IRHIDevice& device, luna::PixelFormat backbufferFormat)
    {
        if (backbufferFormat == luna::PixelFormat::Undefined) {
            return false;
        }

        if (!m_vertexShader.isValid() &&
            device.createShader({.stage = luna::ShaderType::Vertex, .filePath = m_vertexShaderPath}, &m_vertexShader) !=
                luna::RHIResult::Success) {
            return false;
        }

        if (!m_fragmentShader.isValid() &&
            device.createShader({.stage = luna::ShaderType::Fragment, .filePath = m_fragmentShaderPath},
                                &m_fragmentShader) != luna::RHIResult::Success) {
            return false;
        }

        if (!m_vertexBuffer.isValid()) {
            luna::BufferDesc bufferDesc{};
            bufferDesc.size = static_cast<uint64_t>(sizeof(TriangleVertex) * m_vertices.size());
            bufferDesc.usage = luna::BufferUsage::Vertex;
            bufferDesc.memoryUsage = luna::MemoryUsage::Default;
            bufferDesc.debugName = "MinimalTriangleVertices";

            if (device.createBuffer(bufferDesc, &m_vertexBuffer, m_vertices.data()) != luna::RHIResult::Success) {
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
        pipelineDesc.debugName = "MinimalTrianglePipeline";
        pipelineDesc.vertexShader = {.stage = luna::ShaderType::Vertex, .filePath = m_vertexShaderPath};
        pipelineDesc.fragmentShader = {.stage = luna::ShaderType::Fragment, .filePath = m_fragmentShaderPath};
        pipelineDesc.vertexLayout.stride = sizeof(TriangleVertex);
        pipelineDesc.vertexLayout.attributes.push_back({0, 0, luna::VertexFormat::Float2});
        pipelineDesc.vertexLayout.attributes.push_back({1, sizeof(float) * 2, luna::VertexFormat::Float3});
        pipelineDesc.cullMode = luna::CullMode::None;
        pipelineDesc.frontFace = luna::FrontFace::Clockwise;
        pipelineDesc.colorAttachments.push_back({backbufferFormat, false});

        if (device.createGraphicsPipeline(pipelineDesc, &m_pipeline) != luna::RHIResult::Success) {
            return false;
        }

        m_pipelineFormat = backbufferFormat;
        return true;
    }

private:
    std::string m_vertexShaderPath;
    std::string m_fragmentShaderPath;
    luna::ShaderHandle m_vertexShader{};
    luna::ShaderHandle m_fragmentShader{};
    luna::BufferHandle m_vertexBuffer{};
    luna::PipelineHandle m_pipeline{};
    luna::PixelFormat m_pipelineFormat = luna::PixelFormat::Undefined;
    std::array<TriangleVertex, 3> m_vertices{{
        TriangleVertex{{0.0f, -0.6f}, {1.0f, 0.2f, 0.2f}},
        TriangleVertex{{0.6f, 0.45f}, {0.2f, 1.0f, 0.3f}},
        TriangleVertex{{-0.6f, 0.45f}, {0.2f, 0.4f, 1.0f}},
    }};
};

std::filesystem::path shader_root()
{
    return std::filesystem::path{LUNA_MINIMAL_RHI_TRIANGLE_SHADER_ROOT}.lexically_normal();
}

} // namespace

int main(int argc, char** argv)
{
#ifndef NDEBUG
    constexpr luna::Logger::Level kLogLevel = luna::Logger::Level::Trace;
#else
    constexpr luna::Logger::Level kLogLevel = luna::Logger::Level::Info;
#endif

    luna::Logger::init("logs/luna.log", kLogLevel);

    const std::filesystem::path triangleShaderRoot = shader_root();
    const SampleRendererMode rendererMode = parse_renderer_mode(argc, argv);
    std::shared_ptr<luna::IRenderPipeline> renderPipeline;
    if (rendererMode == SampleRendererMode::Clear) {
        renderPipeline = std::make_shared<ClearColorPipeline>(luna::ClearColorValue{0.08f, 0.12f, 0.18f, 1.0f});
        LUNA_CORE_INFO("Registered ClearColorPipeline");
    } else {
        renderPipeline = std::make_shared<TriangleRenderPipeline>(
            triangleShaderRoot / "triangle.vert.spv", triangleShaderRoot / "triangle.frag.spv");
        LUNA_CORE_INFO("Registered TrianglePipeline");
    }

    luna::ApplicationSpecification specification{
        .name = "Luna Minimal RHI Triangle",
        .windowWidth = 1280,
        .windowHeight = 720,
        .maximized = false,
        .enableImGui = false,
        .enableMultiViewport = false,
        .renderService =
            {
                .applicationName = "Luna Minimal RHI Triangle",
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
