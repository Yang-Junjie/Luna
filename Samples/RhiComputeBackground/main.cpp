#include "Core/application.h"
#include "Core/log.h"
#include "RHI/CommandContext.h"
#include "RHI/ResourceLayout.h"
#include "Renderer/RenderPipeline.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>

namespace {

enum class ComputeEffectMode : uint8_t {
    Gradient = 0,
    Rings
};

struct alignas(16) BackgroundPushConstants {
    float colorA[4];
    float colorB[4];
    float params[4];
};

static_assert(sizeof(BackgroundPushConstants) == 48);

ComputeEffectMode parse_effect_mode(int argc, char** argv)
{
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--effect=rings" || argument == "--variant=rings") {
            return ComputeEffectMode::Rings;
        }
    }

    return ComputeEffectMode::Gradient;
}

std::string_view to_string(ComputeEffectMode mode)
{
    switch (mode) {
        case ComputeEffectMode::Rings:
            return "rings";
        case ComputeEffectMode::Gradient:
        default:
            return "gradient";
    }
}

std::filesystem::path shader_root()
{
    return std::filesystem::path{RHI_COMPUTE_BACKGROUND_SHADER_ROOT}.lexically_normal();
}

class ComputeBackgroundRenderPipeline final : public luna::IRenderPipeline {
public:
    ComputeBackgroundRenderPipeline(std::filesystem::path shaderPath, ComputeEffectMode effectMode)
        : m_shaderPath(shaderPath.lexically_normal().generic_string()),
          m_effectMode(effectMode)
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
        if (m_resourceSet.isValid()) {
            device.destroyResourceSet(m_resourceSet);
            m_resourceSet = {};
        }
        if (m_resourceLayout.isValid()) {
            device.destroyResourceLayout(m_resourceLayout);
            m_resourceLayout = {};
        }
        if (m_storageImage.isValid()) {
            device.destroyImage(m_storageImage);
            m_storageImage = {};
        }

        m_storageWidth = 0;
        m_storageHeight = 0;
    }

    bool render(luna::IRHIDevice& device, const luna::FrameContext& frameContext) override
    {
        if (frameContext.commandContext == nullptr || !frameContext.backbuffer.isValid()) {
            return false;
        }

        if (!ensure_resources(device, frameContext.renderWidth, frameContext.renderHeight)) {
            return false;
        }

        const BackgroundPushConstants pushConstants = build_push_constants(frameContext.renderWidth, frameContext.renderHeight);
        const uint32_t groupCountX = (frameContext.renderWidth + 7) / 8;
        const uint32_t groupCountY = (frameContext.renderHeight + 7) / 8;

        luna::ImageCopyInfo copyInfo{};
        copyInfo.source = m_storageImage;
        copyInfo.destination = frameContext.backbuffer;
        copyInfo.sourceWidth = frameContext.renderWidth;
        copyInfo.sourceHeight = frameContext.renderHeight;
        copyInfo.destinationWidth = frameContext.renderWidth;
        copyInfo.destinationHeight = frameContext.renderHeight;

        const bool ok =
            frameContext.commandContext->transitionImage(m_storageImage, luna::ImageLayout::General) ==
                luna::RHIResult::Success &&
            frameContext.commandContext->bindComputePipeline(m_pipeline) == luna::RHIResult::Success &&
            frameContext.commandContext->bindResourceSet(m_resourceSet) == luna::RHIResult::Success &&
            frameContext.commandContext->pushConstants(
                &pushConstants, sizeof(pushConstants), 0, luna::ShaderType::Compute) == luna::RHIResult::Success &&
            frameContext.commandContext->dispatch(groupCountX, groupCountY, 1) == luna::RHIResult::Success &&
            frameContext.commandContext->transitionImage(m_storageImage, luna::ImageLayout::TransferSrc) ==
                luna::RHIResult::Success &&
            frameContext.commandContext->transitionImage(frameContext.backbuffer, luna::ImageLayout::TransferDst) ==
                luna::RHIResult::Success &&
            frameContext.commandContext->copyImage(copyInfo) == luna::RHIResult::Success &&
            frameContext.commandContext->transitionImage(frameContext.backbuffer, luna::ImageLayout::Present) ==
                luna::RHIResult::Success;

        if (ok && !m_loggedPass) {
            LUNA_CORE_INFO("dispatch PASS");
            LUNA_CORE_INFO("barrier PASS");
            LUNA_CORE_INFO("copy PASS");
            LUNA_CORE_INFO("Compute effect={}", to_string(m_effectMode));
            m_loggedPass = true;
        }

        return ok;
    }

private:
    bool ensure_resources(luna::IRHIDevice& device, uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0) {
            return false;
        }

        if (!m_resourceLayout.isValid()) {
            luna::ResourceLayoutDesc layoutDesc{};
            layoutDesc.debugName = "ComputeBackgroundLayout";
            layoutDesc.bindings.push_back({0, luna::ResourceType::StorageImage, 1, luna::ShaderType::Compute});
            if (device.createResourceLayout(layoutDesc, &m_resourceLayout) != luna::RHIResult::Success) {
                return false;
            }
        }

        if (!m_resourceSet.isValid() &&
            device.createResourceSet(m_resourceLayout, &m_resourceSet) != luna::RHIResult::Success) {
            return false;
        }

        if (!m_pipeline.isValid()) {
            luna::ComputePipelineDesc pipelineDesc{};
            pipelineDesc.debugName = "ComputeBackgroundPipeline";
            pipelineDesc.computeShader = {.stage = luna::ShaderType::Compute, .filePath = m_shaderPath};
            pipelineDesc.resourceLayouts.push_back(m_resourceLayout);
            pipelineDesc.pushConstantSize = sizeof(BackgroundPushConstants);
            pipelineDesc.pushConstantVisibility = luna::ShaderType::Compute;
            if (device.createComputePipeline(pipelineDesc, &m_pipeline) != luna::RHIResult::Success) {
                return false;
            }
        }

        if (m_storageImage.isValid() && m_storageWidth == width && m_storageHeight == height) {
            return true;
        }

        if (m_storageImage.isValid()) {
            device.destroyImage(m_storageImage);
            m_storageImage = {};
        }

        luna::ImageDesc imageDesc{};
        imageDesc.width = width;
        imageDesc.height = height;
        imageDesc.format = luna::PixelFormat::RGBA16Float;
        imageDesc.usage = luna::ImageUsage::Storage | luna::ImageUsage::TransferSrc;
        imageDesc.debugName = "ComputeBackgroundOutput";
        if (device.createImage(imageDesc, &m_storageImage) != luna::RHIResult::Success) {
            return false;
        }

        luna::ResourceSetWriteDesc writeDesc{};
        writeDesc.images.push_back(
            {.binding = 0, .image = m_storageImage, .sampler = {}, .type = luna::ResourceType::StorageImage});
        if (device.updateResourceSet(m_resourceSet, writeDesc) != luna::RHIResult::Success) {
            return false;
        }

        m_storageWidth = width;
        m_storageHeight = height;
        return true;
    }

    BackgroundPushConstants build_push_constants(uint32_t width, uint32_t height) const
    {
        BackgroundPushConstants pushConstants{};
        pushConstants.colorA[3] = 1.0f;
        pushConstants.colorB[3] = 1.0f;
        pushConstants.params[0] = static_cast<float>(m_effectMode == ComputeEffectMode::Rings ? 1 : 0);
        pushConstants.params[1] = static_cast<float>(width);
        pushConstants.params[2] = static_cast<float>(height);

        if (m_effectMode == ComputeEffectMode::Rings) {
            pushConstants.colorA[0] = 1.0f;
            pushConstants.colorA[1] = 0.45f;
            pushConstants.colorA[2] = 0.12f;
            pushConstants.colorB[0] = 0.08f;
            pushConstants.colorB[1] = 0.14f;
            pushConstants.colorB[2] = 0.95f;
            return pushConstants;
        }

        pushConstants.colorA[0] = 0.04f;
        pushConstants.colorA[1] = 0.08f;
        pushConstants.colorA[2] = 0.18f;
        pushConstants.colorB[0] = 0.22f;
        pushConstants.colorB[1] = 0.72f;
        pushConstants.colorB[2] = 0.96f;
        return pushConstants;
    }

private:
    std::string m_shaderPath;
    ComputeEffectMode m_effectMode = ComputeEffectMode::Gradient;
    luna::ResourceLayoutHandle m_resourceLayout{};
    luna::ResourceSetHandle m_resourceSet{};
    luna::PipelineHandle m_pipeline{};
    luna::ImageHandle m_storageImage{};
    uint32_t m_storageWidth = 0;
    uint32_t m_storageHeight = 0;
    bool m_loggedPass = false;
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

    const ComputeEffectMode effectMode = parse_effect_mode(argc, argv);
    const std::filesystem::path computeShaderRoot = shader_root();
    LUNA_CORE_INFO("Selected compute effect={}", to_string(effectMode));

    std::shared_ptr<luna::IRenderPipeline> renderPipeline = std::make_shared<ComputeBackgroundRenderPipeline>(
        computeShaderRoot / "background.comp.spv", effectMode);

    luna::ApplicationSpecification specification{
        .name = "RhiComputeBackground",
        .windowWidth = 1280,
        .windowHeight = 720,
        .maximized = false,
        .enableImGui = false,
        .enableMultiViewport = false,
        .renderService =
            {
                .applicationName = "RhiComputeBackground",
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
