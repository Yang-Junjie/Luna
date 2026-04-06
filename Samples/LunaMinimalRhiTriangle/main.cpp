#include "Core/application.h"
#include "Core/log.h"
#include "RHI/CommandContext.h"
#include "Renderer/RenderPipeline.h"

#include <filesystem>
#include <memory>
#include <string>

namespace {

constexpr const char* kSampleName = "Luna Minimal RHI Triangle";

class MinimalTrianglePipeline final : public luna::IRenderPipeline {
public:
    explicit MinimalTrianglePipeline(const std::filesystem::path& shaderRoot)
        : m_vertexShaderPath((shaderRoot / "triangle.vert.spv").lexically_normal().generic_string()),
          m_fragmentShaderPath((shaderRoot / "triangle.frag.spv").lexically_normal().generic_string())
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
        m_backbufferFormat = luna::PixelFormat::Undefined;
    }

    bool render(luna::IRHIDevice& device, const luna::FrameContext& frameContext) override
    {
        if (frameContext.commandContext == nullptr || !frameContext.backbuffer.isValid()) {
            return false;
        }

        if (!ensure_pipeline(device, frameContext.backbufferFormat)) {
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
               frameContext.commandContext->draw({3, 1, 0, 0}) == luna::RHIResult::Success &&
               frameContext.commandContext->endRendering() == luna::RHIResult::Success;
    }

private:
    bool ensure_pipeline(luna::IRHIDevice& device, luna::PixelFormat backbufferFormat)
    {
        if (backbufferFormat == luna::PixelFormat::Undefined) {
            return false;
        }

        if (m_pipeline.isValid() && m_backbufferFormat == backbufferFormat) {
            return true;
        }

        if (m_pipeline.isValid()) {
            device.destroyPipeline(m_pipeline);
            m_pipeline = {};
        }

        luna::GraphicsPipelineDesc pipelineDesc{};
        pipelineDesc.debugName = "LunaMinimalTrianglePipeline";
        pipelineDesc.vertexShader = {.stage = luna::ShaderType::Vertex, .filePath = m_vertexShaderPath};
        pipelineDesc.fragmentShader = {.stage = luna::ShaderType::Fragment, .filePath = m_fragmentShaderPath};
        pipelineDesc.cullMode = luna::CullMode::None;
        pipelineDesc.colorAttachments.push_back({backbufferFormat, false});

        if (device.createGraphicsPipeline(pipelineDesc, &m_pipeline) != luna::RHIResult::Success) {
            return false;
        }

        m_backbufferFormat = backbufferFormat;
        return true;
    }

private:
    std::string m_vertexShaderPath;
    std::string m_fragmentShaderPath;
    luna::PipelineHandle m_pipeline{};
    luna::PixelFormat m_backbufferFormat = luna::PixelFormat::Undefined;
};

std::filesystem::path shader_root()
{
    return std::filesystem::path{LUNA_MINIMAL_RHI_TRIANGLE_SHADER_ROOT}.lexically_normal();
}

} // namespace

int main()
{
#ifndef NDEBUG
    constexpr luna::Logger::Level kLogLevel = luna::Logger::Level::Trace;
#else
    constexpr luna::Logger::Level kLogLevel = luna::Logger::Level::Info;
#endif

    luna::Logger::init("logs/luna.log", kLogLevel);

    const luna::ApplicationSpecification specification{
        .name = kSampleName,
        .windowWidth = 1280,
        .windowHeight = 720,
        .maximized = false,
        .enableImGui = false,
        .enableMultiViewport = false,
        .renderService =
            {
                .applicationName = kSampleName,
                .backend = luna::RHIBackend::Vulkan,
                .renderPipeline = std::make_shared<MinimalTrianglePipeline>(shader_root()),
            },
    };

    {
        luna::Application app(specification);
        if (!app.isInitialized()) {
            luna::Logger::shutdown();
            return 1;
        }
        app.run();
    }

    luna::Logger::shutdown();
    return 0;
}
