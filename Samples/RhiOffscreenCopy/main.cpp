#include "Core/application.h"
#include "Core/log.h"
#include "RHI/CommandContext.h"
#include "Renderer/RenderPipeline.h"

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace {

enum class CopyPattern : uint8_t {
    Gradient = 0,
    Checker
};

CopyPattern parse_pattern(int argc, char** argv)
{
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--pattern=checker" || argument == "--variant=checker") {
            return CopyPattern::Checker;
        }
    }

    return CopyPattern::Gradient;
}

std::string_view to_string(CopyPattern pattern)
{
    switch (pattern) {
        case CopyPattern::Checker:
            return "checker";
        case CopyPattern::Gradient:
        default:
            return "gradient";
    }
}

class OffscreenCopyRenderPipeline final : public luna::IRenderPipeline {
public:
    explicit OffscreenCopyRenderPipeline(CopyPattern pattern)
        : m_pattern(pattern)
    {}

    bool init(luna::IRHIDevice&) override
    {
        return true;
    }

    void shutdown(luna::IRHIDevice& device) override
    {
        destroy_offscreen_image(device);
    }

    bool render(luna::IRHIDevice& device, const luna::FrameContext& frameContext) override
    {
        if (frameContext.commandContext == nullptr || !frameContext.backbuffer.isValid()) {
            return false;
        }

        if (!ensure_offscreen_image(device, frameContext.renderWidth, frameContext.renderHeight, frameContext.backbufferFormat)) {
            return false;
        }

        luna::ImageCopyInfo copyInfo{};
        copyInfo.source = m_offscreenImage;
        copyInfo.destination = frameContext.backbuffer;
        copyInfo.sourceWidth = m_imageWidth;
        copyInfo.sourceHeight = m_imageHeight;
        copyInfo.destinationWidth = frameContext.renderWidth;
        copyInfo.destinationHeight = frameContext.renderHeight;

        const bool ok =
            frameContext.commandContext->transitionImage(m_offscreenImage, luna::ImageLayout::TransferSrc) ==
                luna::RHIResult::Success &&
            frameContext.commandContext->transitionImage(frameContext.backbuffer, luna::ImageLayout::TransferDst) ==
                luna::RHIResult::Success &&
            frameContext.commandContext->copyImage(copyInfo) == luna::RHIResult::Success &&
            frameContext.commandContext->transitionImage(frameContext.backbuffer, luna::ImageLayout::Present) ==
                luna::RHIResult::Success;

        if (ok && !m_loggedPass) {
            LUNA_CORE_INFO("barrier PASS");
            LUNA_CORE_INFO("copy PASS");
            LUNA_CORE_INFO("Offscreen copy pattern={}", to_string(m_pattern));
            m_loggedPass = true;
        }

        return ok;
    }

private:
    bool ensure_offscreen_image(luna::IRHIDevice& device,
                                uint32_t width,
                                uint32_t height,
                                luna::PixelFormat format)
    {
        if (width == 0 || height == 0 || format == luna::PixelFormat::Undefined) {
            return false;
        }

        if (m_offscreenImage.isValid() && m_imageWidth == width && m_imageHeight == height && m_imageFormat == format) {
            return true;
        }

        destroy_offscreen_image(device);

        luna::ImageDesc imageDesc{};
        imageDesc.width = width;
        imageDesc.height = height;
        imageDesc.format = format;
        imageDesc.usage = luna::ImageUsage::TransferSrc | luna::ImageUsage::Sampled;
        imageDesc.debugName = "OffscreenCopySource";

        const std::vector<uint8_t> pixels = build_pixels(width, height, format);
        if (pixels.empty()) {
            LUNA_CORE_ERROR("Failed to build offscreen copy pixels for format={}", static_cast<uint32_t>(format));
            return false;
        }

        if (device.createImage(imageDesc, &m_offscreenImage, pixels.data()) != luna::RHIResult::Success) {
            return false;
        }

        m_imageWidth = width;
        m_imageHeight = height;
        m_imageFormat = format;
        return true;
    }

    void destroy_offscreen_image(luna::IRHIDevice& device)
    {
        if (m_offscreenImage.isValid()) {
            device.destroyImage(m_offscreenImage);
            m_offscreenImage = {};
        }

        m_imageWidth = 0;
        m_imageHeight = 0;
        m_imageFormat = luna::PixelFormat::Undefined;
    }

    std::vector<uint8_t> build_pixels(uint32_t width, uint32_t height, luna::PixelFormat format) const
    {
        if (format != luna::PixelFormat::BGRA8Unorm && format != luna::PixelFormat::RGBA8Unorm) {
            return {};
        }

        std::vector<uint8_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4, 255);
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                const size_t offset = (static_cast<size_t>(y) * width + x) * 4;
                uint8_t r = 0;
                uint8_t g = 0;
                uint8_t b = 0;

                if (m_pattern == CopyPattern::Checker) {
                    const bool evenTile = ((x / 48) + (y / 48)) % 2 == 0;
                    r = evenTile ? 250 : 24;
                    g = evenTile ? 180 : 96;
                    b = evenTile ? 40 : 220;
                } else {
                    r = static_cast<uint8_t>((255u * x) / (width > 1 ? width - 1 : 1));
                    g = static_cast<uint8_t>((255u * y) / (height > 1 ? height - 1 : 1));
                    b = static_cast<uint8_t>(140u + ((x + y) % 96u));
                }

                if (format == luna::PixelFormat::BGRA8Unorm) {
                    pixels[offset + 0] = b;
                    pixels[offset + 1] = g;
                    pixels[offset + 2] = r;
                    pixels[offset + 3] = 255;
                } else {
                    pixels[offset + 0] = r;
                    pixels[offset + 1] = g;
                    pixels[offset + 2] = b;
                    pixels[offset + 3] = 255;
                }
            }
        }

        return pixels;
    }

private:
    CopyPattern m_pattern = CopyPattern::Gradient;
    luna::ImageHandle m_offscreenImage{};
    uint32_t m_imageWidth = 0;
    uint32_t m_imageHeight = 0;
    luna::PixelFormat m_imageFormat = luna::PixelFormat::Undefined;
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

    const CopyPattern pattern = parse_pattern(argc, argv);
    LUNA_CORE_INFO("Selected offscreen copy pattern={}", to_string(pattern));

    std::shared_ptr<luna::IRenderPipeline> renderPipeline = std::make_shared<OffscreenCopyRenderPipeline>(pattern);

    luna::ApplicationSpecification specification{
        .name = "RhiOffscreenCopy",
        .windowWidth = 1280,
        .windowHeight = 720,
        .maximized = false,
        .enableImGui = false,
        .enableMultiViewport = false,
        .renderService =
            {
                .applicationName = "RhiOffscreenCopy",
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
