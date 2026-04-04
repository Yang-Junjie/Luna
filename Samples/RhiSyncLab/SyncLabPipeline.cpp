#include "SyncLabPipeline.h"

#include "Core/log.h"
#include "RHI/CommandContext.h"
#include "Vulkan/vk_rhi_device.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string_view>
#include <vector>

namespace sync_lab {
namespace {

constexpr uint32_t kHistoryImageSize = 128;
constexpr uint32_t kReadbackImageSize = 16;
constexpr uint32_t kIndirectImageSize = 256;
constexpr uint32_t kReadbackBlockSize = 4;
constexpr size_t kTimelineLimit = 32;

struct alignas(16) GenerateArgsPushConstants {
    uint32_t requested[4] = {};
};

std::string shader_path(const std::string& root, std::string_view fileName)
{
    return (std::filesystem::path(root) / fileName).lexically_normal().generic_string();
}

size_t frame_slot(uint32_t frameIndex, size_t framesInFlight)
{
    return framesInFlight == 0 ? 0 : static_cast<size_t>(frameIndex % static_cast<uint32_t>(framesInFlight));
}

uint8_t to_u8(float value)
{
    return static_cast<uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
}

void write_rgba(std::vector<uint8_t>& pixels, size_t offset, float r, float g, float b, float a)
{
    pixels[offset + 0] = to_u8(r);
    pixels[offset + 1] = to_u8(g);
    pixels[offset + 2] = to_u8(b);
    pixels[offset + 3] = to_u8(a);
}

std::vector<uint8_t> build_history_pixels(uint32_t width, uint32_t height, int frameIndex)
{
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4, 255);
    const float phase = static_cast<float>(frameIndex % 11) / 10.0f;

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const float fx = static_cast<float>(x) / static_cast<float>(std::max(1u, width - 1));
            const float fy = static_cast<float>(y) / static_cast<float>(std::max(1u, height - 1));
            const float stripe = std::fmod((fx * 7.0f + fy * 5.0f + phase * 3.0f), 1.0f);
            const float marker = (x / 16u + y / 16u + static_cast<uint32_t>(frameIndex)) % 2u == 0 ? 1.0f : 0.25f;
            const size_t offset = (static_cast<size_t>(y) * width + x) * 4;
            write_rgba(pixels,
                       offset,
                       0.15f + 0.70f * phase,
                       0.18f + 0.62f * stripe,
                       0.18f + 0.55f * marker,
                       1.0f);
        }
    }

    return pixels;
}

std::vector<uint8_t> build_readback_pixels(uint32_t width, uint32_t height, uint32_t seed)
{
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4, 255);

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const size_t offset = (static_cast<size_t>(y) * width + x) * 4;
            pixels[offset + 0] = static_cast<uint8_t>((x * 17u + seed * 13u) & 0xFFu);
            pixels[offset + 1] = static_cast<uint8_t>((y * 29u + seed * 7u) & 0xFFu);
            pixels[offset + 2] = static_cast<uint8_t>(((x + y) * 11u + seed * 5u) & 0xFFu);
            pixels[offset + 3] = 255u;
        }
    }

    return pixels;
}

uint32_t pack_rgba_u32(const uint8_t* bytes)
{
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8u) |
           (static_cast<uint32_t>(bytes[2]) << 16u) |
           (static_cast<uint32_t>(bytes[3]) << 24u);
}

} // namespace

struct RhiSyncLabRenderPipeline::Impl {
    luna::SamplerHandle linearSampler{};

    luna::ResourceLayoutHandle historyPreviewLayout{};
    luna::ResourceSetHandle historyPreviewSet{};
    luna::PipelineHandle historyPreviewPipeline{};

    luna::ResourceLayoutHandle singlePreviewLayout{};
    luna::ResourceSetHandle readbackPreviewSet{};
    luna::ResourceSetHandle indirectPreviewSet{};
    luna::PipelineHandle singlePreviewPipeline{};
    luna::PixelFormat previewBackbufferFormat = luna::PixelFormat::Undefined;

    uint32_t framesInFlight = 0;
    uint64_t timelineSerial = 0;
    uint32_t historyAutoAdvanceTick = 0;

    luna::BufferHandle historyUploadBuffer{};
    luna::ImageHandle historyCurrentImage{};
    luna::ImageHandle historyImage{};
    luna::ImageHandle historyBarrierProbeImage{};

    luna::BufferHandle readbackUploadBuffer{};
    luna::ImageHandle readbackImage{};
    std::vector<luna::BufferHandle> readbackBuffers;
    std::vector<bool> readbackPending;
    std::vector<int> readbackPendingX;
    std::vector<int> readbackPendingY;

    luna::ResourceLayoutHandle indirectArgsLayout{};
    luna::ResourceSetHandle indirectArgsSet{};
    luna::PipelineHandle indirectGeneratePipeline{};
    luna::ResourceLayoutHandle indirectPaintLayout{};
    luna::ResourceSetHandle indirectPaintSet{};
    luna::PipelineHandle indirectPaintPipeline{};
    luna::BufferHandle indirectArgsBuffer{};
    luna::ImageHandle indirectImage{};
    std::vector<luna::BufferHandle> indirectReadbackBuffers;
    std::vector<bool> indirectReadbackPending;
};

static void push_timeline(const std::shared_ptr<State>& state, uint64_t* serial, const std::string& label)
{
    if (state == nullptr || serial == nullptr) {
        return;
    }

    const uint64_t entrySerial = ++(*serial);
    state->timeline.push_back({entrySerial, label});
    LUNA_CORE_INFO("RhiSyncLab timeline #{} {}", entrySerial, label);
    if (state->timeline.size() > kTimelineLimit) {
        state->timeline.erase(state->timeline.begin(), state->timeline.begin() + 1);
    }
}

RhiSyncLabRenderPipeline::RhiSyncLabRenderPipeline(std::shared_ptr<State> state)
    : m_state(std::move(state)),
      m_impl(std::make_unique<Impl>())
{}

RhiSyncLabRenderPipeline::~RhiSyncLabRenderPipeline() = default;

bool RhiSyncLabRenderPipeline::init(luna::IRHIDevice& device)
{
    m_vulkanDevice = dynamic_cast<luna::VulkanRHIDevice*>(&device);
    if (m_vulkanDevice == nullptr) {
        LUNA_CORE_ERROR("RhiSyncLab requires the Vulkan RHI backend");
        return false;
    }

    m_shaderRoot = std::filesystem::path{RHI_SYNC_LAB_SHADER_ROOT}.lexically_normal().generic_string();
    return true;
}

void RhiSyncLabRenderPipeline::shutdown(luna::IRHIDevice& device)
{
    destroy_indirect_resources(device);
    destroy_readback_resources(device);
    destroy_history_resources(device);
    destroy_shared_resources(device);
    m_vulkanDevice = nullptr;
}

bool RhiSyncLabRenderPipeline::render(luna::IRHIDevice& device, const luna::FrameContext& frameContext)
{
    if (m_state == nullptr || m_impl == nullptr || frameContext.commandContext == nullptr || !frameContext.backbuffer.isValid()) {
        return false;
    }

    if (!ensure_shared_resources(device, frameContext.backbufferFormat)) {
        return false;
    }

    switch (m_state->page) {
        case Page::HistoryCopy:
            return render_history_copy(device, frameContext);
        case Page::Readback:
            return render_readback(device, frameContext);
        case Page::Indirect:
            return render_indirect(device, frameContext);
        default:
            return false;
    }
}

bool RhiSyncLabRenderPipeline::ensure_shared_resources(luna::IRHIDevice& device, luna::PixelFormat backbufferFormat)
{
    if (!m_impl->linearSampler.isValid()) {
        luna::SamplerDesc samplerDesc{};
        samplerDesc.debugName = "RhiSyncLabLinearSampler";
        if (device.createSampler(samplerDesc, &m_impl->linearSampler) != luna::RHIResult::Success) {
            return false;
        }
    }

    if (!m_impl->historyPreviewLayout.isValid()) {
        luna::ResourceLayoutDesc layoutDesc{};
        layoutDesc.debugName = "RhiSyncLabHistoryPreviewLayout";
        layoutDesc.bindings.push_back({0, luna::ResourceType::CombinedImageSampler, 1, luna::ShaderType::Fragment});
        layoutDesc.bindings.push_back({1, luna::ResourceType::CombinedImageSampler, 1, luna::ShaderType::Fragment});
        if (device.createResourceLayout(layoutDesc, &m_impl->historyPreviewLayout) != luna::RHIResult::Success ||
            device.createResourceSet(m_impl->historyPreviewLayout, &m_impl->historyPreviewSet) != luna::RHIResult::Success) {
            return false;
        }
    }

    if (!m_impl->singlePreviewLayout.isValid()) {
        luna::ResourceLayoutDesc layoutDesc{};
        layoutDesc.debugName = "RhiSyncLabSinglePreviewLayout";
        layoutDesc.bindings.push_back({0, luna::ResourceType::CombinedImageSampler, 1, luna::ShaderType::Fragment});
        if (device.createResourceLayout(layoutDesc, &m_impl->singlePreviewLayout) != luna::RHIResult::Success ||
            device.createResourceSet(m_impl->singlePreviewLayout, &m_impl->readbackPreviewSet) != luna::RHIResult::Success ||
            device.createResourceSet(m_impl->singlePreviewLayout, &m_impl->indirectPreviewSet) != luna::RHIResult::Success) {
            return false;
        }
    }

    if (m_impl->previewBackbufferFormat == backbufferFormat && m_impl->historyPreviewPipeline.isValid() &&
        m_impl->singlePreviewPipeline.isValid()) {
        return true;
    }

    if (m_impl->historyPreviewPipeline.isValid() || m_impl->singlePreviewPipeline.isValid()) {
        if (device.waitIdle() != luna::RHIResult::Success) {
            return false;
        }
        if (m_impl->historyPreviewPipeline.isValid()) {
            device.destroyPipeline(m_impl->historyPreviewPipeline);
            m_impl->historyPreviewPipeline = {};
        }
        if (m_impl->singlePreviewPipeline.isValid()) {
            device.destroyPipeline(m_impl->singlePreviewPipeline);
            m_impl->singlePreviewPipeline = {};
        }
    }

    const std::string vertexShaderPath = shader_path(m_shaderRoot, "sync_lab_fullscreen.vert.spv");
    const std::string historyFragmentShaderPath = shader_path(m_shaderRoot, "sync_lab_history_preview.frag.spv");
    const std::string singleFragmentShaderPath = shader_path(m_shaderRoot, "sync_lab_single_preview.frag.spv");

    luna::GraphicsPipelineDesc baseDesc{};
    baseDesc.vertexShader = {.stage = luna::ShaderType::Vertex, .filePath = vertexShaderPath};
    baseDesc.cullMode = luna::CullMode::None;
    baseDesc.frontFace = luna::FrontFace::Clockwise;
    baseDesc.colorAttachments.push_back({backbufferFormat, false});

    luna::GraphicsPipelineDesc historyDesc = baseDesc;
    historyDesc.debugName = "RhiSyncLabHistoryPreview";
    historyDesc.fragmentShader = {.stage = luna::ShaderType::Fragment, .filePath = historyFragmentShaderPath};
    historyDesc.resourceLayouts = {m_impl->historyPreviewLayout};
    if (device.createGraphicsPipeline(historyDesc, &m_impl->historyPreviewPipeline) != luna::RHIResult::Success) {
        return false;
    }

    luna::GraphicsPipelineDesc singleDesc = baseDesc;
    singleDesc.debugName = "RhiSyncLabSinglePreview";
    singleDesc.fragmentShader = {.stage = luna::ShaderType::Fragment, .filePath = singleFragmentShaderPath};
    singleDesc.resourceLayouts = {m_impl->singlePreviewLayout};
    if (device.createGraphicsPipeline(singleDesc, &m_impl->singlePreviewPipeline) != luna::RHIResult::Success) {
        return false;
    }

    m_impl->previewBackbufferFormat = backbufferFormat;
    return true;
}

void RhiSyncLabRenderPipeline::destroy_shared_resources(luna::IRHIDevice& device)
{
    if (m_impl == nullptr) {
        return;
    }

    if (m_impl->historyPreviewPipeline.isValid()) device.destroyPipeline(m_impl->historyPreviewPipeline);
    if (m_impl->singlePreviewPipeline.isValid()) device.destroyPipeline(m_impl->singlePreviewPipeline);
    if (m_impl->historyPreviewSet.isValid()) device.destroyResourceSet(m_impl->historyPreviewSet);
    if (m_impl->readbackPreviewSet.isValid()) device.destroyResourceSet(m_impl->readbackPreviewSet);
    if (m_impl->indirectPreviewSet.isValid()) device.destroyResourceSet(m_impl->indirectPreviewSet);
    if (m_impl->historyPreviewLayout.isValid()) device.destroyResourceLayout(m_impl->historyPreviewLayout);
    if (m_impl->singlePreviewLayout.isValid()) device.destroyResourceLayout(m_impl->singlePreviewLayout);
    if (m_impl->linearSampler.isValid()) device.destroySampler(m_impl->linearSampler);

    *m_impl = Impl{};
}

bool RhiSyncLabRenderPipeline::ensure_history_resources(luna::IRHIDevice& device)
{
    if (m_impl->historyCurrentImage.isValid() && m_impl->historyImage.isValid() && m_impl->historyUploadBuffer.isValid() &&
        m_impl->historyBarrierProbeImage.isValid()) {
        return true;
    }

    const std::vector<uint8_t> blackPixels(static_cast<size_t>(kHistoryImageSize) * kHistoryImageSize * 4, 0);

    luna::BufferDesc uploadDesc{};
    uploadDesc.size = blackPixels.size();
    uploadDesc.usage = luna::BufferUsage::TransferSrc;
    uploadDesc.memoryUsage = luna::MemoryUsage::Upload;
    uploadDesc.debugName = "RhiSyncLabHistoryUploadBuffer";
    if (!m_impl->historyUploadBuffer.isValid() &&
        device.createBuffer(uploadDesc, &m_impl->historyUploadBuffer, blackPixels.data()) != luna::RHIResult::Success) {
        return false;
    }

    luna::ImageDesc imageDesc{};
    imageDesc.width = kHistoryImageSize;
    imageDesc.height = kHistoryImageSize;
    imageDesc.depth = 1;
    imageDesc.mipLevels = 1;
    imageDesc.arrayLayers = 1;
    imageDesc.type = luna::ImageType::Image2D;
    imageDesc.format = luna::PixelFormat::RGBA8Unorm;
    imageDesc.usage = luna::ImageUsage::Sampled | luna::ImageUsage::TransferSrc | luna::ImageUsage::TransferDst;

    if (!m_impl->historyCurrentImage.isValid()) {
        imageDesc.debugName = "RhiSyncLabHistoryCurrent";
        if (device.createImage(imageDesc, &m_impl->historyCurrentImage, blackPixels.data()) != luna::RHIResult::Success) {
            return false;
        }
    }
    if (!m_impl->historyImage.isValid()) {
        imageDesc.debugName = "RhiSyncLabHistoryImage";
        if (device.createImage(imageDesc, &m_impl->historyImage, blackPixels.data()) != luna::RHIResult::Success) {
            return false;
        }
    }
    if (!m_impl->historyBarrierProbeImage.isValid()) {
        imageDesc.debugName = "RhiSyncLabHistoryBarrierProbe";
        if (device.createImage(imageDesc, &m_impl->historyBarrierProbeImage, blackPixels.data()) != luna::RHIResult::Success) {
            return false;
        }
    }

    luna::ResourceSetWriteDesc writeDesc{};
    writeDesc.images.push_back(
        {.binding = 0, .image = m_impl->historyCurrentImage, .sampler = m_impl->linearSampler, .type = luna::ResourceType::CombinedImageSampler});
    writeDesc.images.push_back(
        {.binding = 1, .image = m_impl->historyImage, .sampler = m_impl->linearSampler, .type = luna::ResourceType::CombinedImageSampler});
    return device.updateResourceSet(m_impl->historyPreviewSet, writeDesc) == luna::RHIResult::Success;
}

bool RhiSyncLabRenderPipeline::ensure_readback_resources(luna::IRHIDevice& device)
{
    if (m_impl->framesInFlight == 0) {
        m_impl->framesInFlight = std::max(1u, device.getCapabilities().framesInFlight);
    }

    if (!m_impl->readbackImage.isValid()) {
        const std::vector<uint8_t> pixels = build_readback_pixels(kReadbackImageSize, kReadbackImageSize, 1);

        luna::BufferDesc uploadDesc{};
        uploadDesc.size = pixels.size();
        uploadDesc.usage = luna::BufferUsage::TransferSrc;
        uploadDesc.memoryUsage = luna::MemoryUsage::Upload;
        uploadDesc.debugName = "RhiSyncLabReadbackUpload";
        if (device.createBuffer(uploadDesc, &m_impl->readbackUploadBuffer, pixels.data()) != luna::RHIResult::Success) {
            return false;
        }

        luna::ImageDesc imageDesc{};
        imageDesc.width = kReadbackImageSize;
        imageDesc.height = kReadbackImageSize;
        imageDesc.depth = 1;
        imageDesc.format = luna::PixelFormat::RGBA8Unorm;
        imageDesc.usage = luna::ImageUsage::Sampled | luna::ImageUsage::TransferSrc | luna::ImageUsage::TransferDst;
        imageDesc.debugName = "RhiSyncLabReadbackImage";
        if (device.createImage(imageDesc, &m_impl->readbackImage, pixels.data()) != luna::RHIResult::Success) {
            return false;
        }

        luna::ResourceSetWriteDesc previewWrite{};
        previewWrite.images.push_back(
            {.binding = 0, .image = m_impl->readbackImage, .sampler = m_impl->linearSampler, .type = luna::ResourceType::CombinedImageSampler});
        if (device.updateResourceSet(m_impl->readbackPreviewSet, previewWrite) != luna::RHIResult::Success) {
            return false;
        }
    }

    if (!m_impl->readbackBuffers.empty()) {
        return true;
    }

    m_impl->readbackBuffers.assign(m_impl->framesInFlight, {});
    m_impl->readbackPending.assign(m_impl->framesInFlight, false);
    m_impl->readbackPendingX.assign(m_impl->framesInFlight, 0);
    m_impl->readbackPendingY.assign(m_impl->framesInFlight, 0);

    luna::BufferDesc readbackDesc{};
    readbackDesc.size = kReadbackBlockSize * kReadbackBlockSize * 4;
    readbackDesc.usage = luna::BufferUsage::TransferDst;
    readbackDesc.memoryUsage = luna::MemoryUsage::Readback;
    readbackDesc.debugName = "RhiSyncLabReadbackBuffer";

    for (uint32_t frame = 0; frame < m_impl->framesInFlight; ++frame) {
        if (device.createBuffer(readbackDesc, &m_impl->readbackBuffers[frame]) != luna::RHIResult::Success) {
            return false;
        }
    }

    return true;
}

bool RhiSyncLabRenderPipeline::ensure_indirect_resources(luna::IRHIDevice& device)
{
    if (m_impl->framesInFlight == 0) {
        m_impl->framesInFlight = std::max(1u, device.getCapabilities().framesInFlight);
    }

    if (!m_impl->indirectArgsLayout.isValid()) {
        luna::ResourceLayoutDesc layoutDesc{};
        layoutDesc.debugName = "RhiSyncLabIndirectArgsLayout";
        layoutDesc.bindings.push_back({0, luna::ResourceType::StorageBuffer, 1, luna::ShaderType::Compute});
        if (device.createResourceLayout(layoutDesc, &m_impl->indirectArgsLayout) != luna::RHIResult::Success ||
            device.createResourceSet(m_impl->indirectArgsLayout, &m_impl->indirectArgsSet) != luna::RHIResult::Success) {
            return false;
        }
    }

    if (!m_impl->indirectPaintLayout.isValid()) {
        luna::ResourceLayoutDesc layoutDesc{};
        layoutDesc.debugName = "RhiSyncLabIndirectPaintLayout";
        layoutDesc.bindings.push_back({0, luna::ResourceType::StorageImage, 1, luna::ShaderType::Compute});
        if (device.createResourceLayout(layoutDesc, &m_impl->indirectPaintLayout) != luna::RHIResult::Success ||
            device.createResourceSet(m_impl->indirectPaintLayout, &m_impl->indirectPaintSet) != luna::RHIResult::Success) {
            return false;
        }
    }

    if (!m_impl->indirectArgsBuffer.isValid()) {
        const std::array<uint32_t, 4> initialArgs = {1u, 1u, 1u, 0u};
        luna::BufferDesc bufferDesc{};
        bufferDesc.size = sizeof(initialArgs);
        bufferDesc.usage = luna::BufferUsage::Storage | luna::BufferUsage::Indirect | luna::BufferUsage::TransferSrc;
        bufferDesc.memoryUsage = luna::MemoryUsage::Default;
        bufferDesc.debugName = "RhiSyncLabIndirectArgsBuffer";
        if (device.createBuffer(bufferDesc, &m_impl->indirectArgsBuffer, initialArgs.data()) != luna::RHIResult::Success) {
            return false;
        }

        luna::ResourceSetWriteDesc argsWrite{};
        argsWrite.buffers.push_back({0, m_impl->indirectArgsBuffer, 0, sizeof(initialArgs), luna::ResourceType::StorageBuffer});
        if (device.updateResourceSet(m_impl->indirectArgsSet, argsWrite) != luna::RHIResult::Success) {
            return false;
        }
    }

    if (!m_impl->indirectImage.isValid()) {
        luna::ImageDesc imageDesc{};
        imageDesc.width = kIndirectImageSize;
        imageDesc.height = kIndirectImageSize;
        imageDesc.depth = 1;
        imageDesc.format = luna::PixelFormat::RGBA8Unorm;
        imageDesc.usage = luna::ImageUsage::Storage | luna::ImageUsage::Sampled;
        imageDesc.debugName = "RhiSyncLabIndirectImage";
        if (device.createImage(imageDesc, &m_impl->indirectImage) != luna::RHIResult::Success) {
            return false;
        }

        luna::ResourceSetWriteDesc paintWrite{};
        paintWrite.images.push_back(
            {.binding = 0, .image = m_impl->indirectImage, .sampler = {}, .type = luna::ResourceType::StorageImage});
        if (device.updateResourceSet(m_impl->indirectPaintSet, paintWrite) != luna::RHIResult::Success) {
            return false;
        }

        luna::ResourceSetWriteDesc previewWrite{};
        previewWrite.images.push_back(
            {.binding = 0, .image = m_impl->indirectImage, .sampler = m_impl->linearSampler, .type = luna::ResourceType::CombinedImageSampler});
        if (device.updateResourceSet(m_impl->indirectPreviewSet, previewWrite) != luna::RHIResult::Success) {
            return false;
        }
    }

    if (!m_impl->indirectGeneratePipeline.isValid()) {
        const std::string generateShaderPath = shader_path(m_shaderRoot, "sync_lab_generate_args.comp.spv");
        luna::ComputePipelineDesc pipelineDesc{};
        pipelineDesc.debugName = "RhiSyncLabGenerateArgs";
        pipelineDesc.computeShader = {.stage = luna::ShaderType::Compute,
                                      .filePath = generateShaderPath};
        pipelineDesc.resourceLayouts = {m_impl->indirectArgsLayout};
        pipelineDesc.pushConstantSize = sizeof(GenerateArgsPushConstants);
        if (device.createComputePipeline(pipelineDesc, &m_impl->indirectGeneratePipeline) != luna::RHIResult::Success) {
            return false;
        }
    }

    if (!m_impl->indirectPaintPipeline.isValid()) {
        const std::string paintShaderPath = shader_path(m_shaderRoot, "sync_lab_paint_indirect.comp.spv");
        luna::ComputePipelineDesc pipelineDesc{};
        pipelineDesc.debugName = "RhiSyncLabPaintIndirect";
        pipelineDesc.computeShader = {.stage = luna::ShaderType::Compute,
                                      .filePath = paintShaderPath};
        pipelineDesc.resourceLayouts = {m_impl->indirectPaintLayout};
        if (device.createComputePipeline(pipelineDesc, &m_impl->indirectPaintPipeline) != luna::RHIResult::Success) {
            return false;
        }
    }

    if (!m_impl->indirectReadbackBuffers.empty()) {
        return true;
    }

    m_impl->indirectReadbackBuffers.assign(m_impl->framesInFlight, {});
    m_impl->indirectReadbackPending.assign(m_impl->framesInFlight, false);
    luna::BufferDesc readbackDesc{};
    readbackDesc.size = sizeof(uint32_t) * 4;
    readbackDesc.usage = luna::BufferUsage::TransferDst;
    readbackDesc.memoryUsage = luna::MemoryUsage::Readback;
    readbackDesc.debugName = "RhiSyncLabIndirectReadback";

    for (uint32_t frame = 0; frame < m_impl->framesInFlight; ++frame) {
        if (device.createBuffer(readbackDesc, &m_impl->indirectReadbackBuffers[frame]) != luna::RHIResult::Success) {
            return false;
        }
    }

    return true;
}

bool RhiSyncLabRenderPipeline::render_history_copy(luna::IRHIDevice& device, const luna::FrameContext& frameContext)
{
    if (!ensure_history_resources(device)) {
        return false;
    }

    auto& history = m_state->history;
    if (history.autoAdvance) {
        ++m_impl->historyAutoAdvanceTick;
        if ((m_impl->historyAutoAdvanceTick % 24u) == 0u) {
            history.advanceFrameRequested = true;
        }
    }

    std::ostringstream summary;
    summary << "Old=" << static_cast<int>(history.barrierOldLayout) << " -> New=" << static_cast<int>(history.barrierNewLayout)
            << ", SrcStage=" << static_cast<int>(history.barrierSrcStage) << ", DstStage=" << static_cast<int>(history.barrierDstStage)
            << ", SrcAccess=" << static_cast<int>(history.barrierSrcAccess) << ", DstAccess=" << static_cast<int>(history.barrierDstAccess);
    history.barrierSummary = summary.str();

    if (history.runBarrierOnlyRequested) {
        history.runBarrierOnlyRequested = false;
        const luna::RHIResult result = frameContext.commandContext->imageBarrier({.image = m_impl->historyBarrierProbeImage,
                                                                                  .oldLayout = history.barrierOldLayout,
                                                                                  .newLayout = history.barrierNewLayout,
                                                                                  .srcStage = history.barrierSrcStage,
                                                                                  .dstStage = history.barrierDstStage,
                                                                                  .srcAccess = history.barrierSrcAccess,
                                                                                  .dstAccess = history.barrierDstAccess,
                                                                                  .aspect = luna::ImageAspect::Color});
        history.status = result == luna::RHIResult::Success ? "Barrier Only executed successfully."
                                                            : "Barrier Only rejected the selected state combination.";
        if (result == luna::RHIResult::Success) {
            LUNA_CORE_INFO("RhiSyncLab barrier-only success: {}", history.barrierSummary);
        } else {
            LUNA_CORE_ERROR("RhiSyncLab barrier-only rejected: {}", history.barrierSummary);
        }
        push_timeline(m_state, &m_impl->timelineSerial, "History: barrier-only probe");
        if (result == luna::RHIResult::Success) {
            history.barrierOldLayout = history.barrierNewLayout;
        }
    }

    const bool advanceFrame = history.advanceFrameRequested;
    if (history.advanceFrameRequested) {
        history.advanceFrameRequested = false;
        ++history.sampleFrame;
    }

    if (advanceFrame || history.sampleFrame == 0) {
        const std::vector<uint8_t> pixels = build_history_pixels(kHistoryImageSize, kHistoryImageSize, history.sampleFrame);
        if (device.writeBuffer(m_impl->historyUploadBuffer, pixels.data(), pixels.size(), 0) != luna::RHIResult::Success) {
            return false;
        }

        if (!history.pauseCopy) {
            if (frameContext.commandContext->imageBarrier({.image = m_impl->historyCurrentImage,
                                                           .newLayout = luna::ImageLayout::TransferSrc,
                                                           .srcStage = luna::PipelineStage::FragmentShader,
                                                           .dstStage = luna::PipelineStage::Transfer,
                                                           .srcAccess = luna::ResourceAccess::ShaderRead,
                                                           .dstAccess = luna::ResourceAccess::TransferRead,
                                                           .aspect = luna::ImageAspect::Color}) != luna::RHIResult::Success ||
                frameContext.commandContext->imageBarrier({.image = m_impl->historyImage,
                                                           .newLayout = luna::ImageLayout::TransferDst,
                                                           .srcStage = luna::PipelineStage::FragmentShader,
                                                           .dstStage = luna::PipelineStage::Transfer,
                                                           .srcAccess = luna::ResourceAccess::ShaderRead,
                                                           .dstAccess = luna::ResourceAccess::TransferWrite,
                                                           .aspect = luna::ImageAspect::Color}) != luna::RHIResult::Success ||
                frameContext.commandContext->copyImage({m_impl->historyCurrentImage,
                                                        m_impl->historyImage,
                                                        kHistoryImageSize,
                                                        kHistoryImageSize,
                                                        kHistoryImageSize,
                                                        kHistoryImageSize}) != luna::RHIResult::Success ||
                frameContext.commandContext->imageBarrier({.image = m_impl->historyImage,
                                                           .oldLayout = luna::ImageLayout::TransferDst,
                                                           .newLayout = luna::ImageLayout::ShaderReadOnly,
                                                           .srcStage = luna::PipelineStage::Transfer,
                                                           .dstStage = luna::PipelineStage::FragmentShader,
                                                           .srcAccess = luna::ResourceAccess::TransferWrite,
                                                           .dstAccess = luna::ResourceAccess::ShaderRead,
                                                           .aspect = luna::ImageAspect::Color}) != luna::RHIResult::Success) {
                return false;
            }
            push_timeline(m_state, &m_impl->timelineSerial, "History: image barrier + copy current -> history");
        }

        if (frameContext.commandContext->bufferBarrier({.buffer = m_impl->historyUploadBuffer,
                                                        .srcStage = luna::PipelineStage::Host,
                                                        .dstStage = luna::PipelineStage::Transfer,
                                                        .srcAccess = luna::ResourceAccess::HostWrite,
                                                        .dstAccess = luna::ResourceAccess::TransferRead}) != luna::RHIResult::Success ||
            frameContext.commandContext->imageBarrier({.image = m_impl->historyCurrentImage,
                                                       .newLayout = luna::ImageLayout::TransferDst,
                                                       .srcStage = luna::PipelineStage::AllCommands,
                                                       .dstStage = luna::PipelineStage::Transfer,
                                                       .srcAccess = luna::ResourceAccess::MemoryRead | luna::ResourceAccess::MemoryWrite,
                                                       .dstAccess = luna::ResourceAccess::TransferWrite,
                                                       .aspect = luna::ImageAspect::Color}) != luna::RHIResult::Success ||
            frameContext.commandContext->copyBufferToImage({.buffer = m_impl->historyUploadBuffer,
                                                            .image = m_impl->historyCurrentImage,
                                                            .aspect = luna::ImageAspect::Color,
                                                            .imageExtentWidth = kHistoryImageSize,
                                                            .imageExtentHeight = kHistoryImageSize,
                                                            .imageExtentDepth = 1}) != luna::RHIResult::Success ||
            frameContext.commandContext->imageBarrier({.image = m_impl->historyCurrentImage,
                                                       .oldLayout = luna::ImageLayout::TransferDst,
                                                       .newLayout = luna::ImageLayout::ShaderReadOnly,
                                                       .srcStage = luna::PipelineStage::Transfer,
                                                       .dstStage = luna::PipelineStage::FragmentShader,
                                                       .srcAccess = luna::ResourceAccess::TransferWrite,
                                                       .dstAccess = luna::ResourceAccess::ShaderRead,
                                                       .aspect = luna::ImageAspect::Color}) != luna::RHIResult::Success) {
            return false;
        }
        push_timeline(m_state, &m_impl->timelineSerial, "History: buffer barrier + copy buffer -> current");
    }

    history.status = history.pauseCopy ? "History copy paused. Current continues to update when you advance the frame."
                                       : "History image shows the previous Current frame after GPU copy.";

    return frameContext.commandContext->beginRendering({.width = frameContext.renderWidth,
                                                        .height = frameContext.renderHeight,
                                                        .colorAttachments = {{frameContext.backbuffer,
                                                                              frameContext.backbufferFormat,
                                                                              {0.03f, 0.04f, 0.06f, 1.0f}}}}) ==
               luna::RHIResult::Success &&
           frameContext.commandContext->bindGraphicsPipeline(m_impl->historyPreviewPipeline) == luna::RHIResult::Success &&
           frameContext.commandContext->bindResourceSet(m_impl->historyPreviewSet) == luna::RHIResult::Success &&
           frameContext.commandContext->draw({3, 1, 0, 0}) == luna::RHIResult::Success &&
           frameContext.commandContext->endRendering() == luna::RHIResult::Success;
}

bool RhiSyncLabRenderPipeline::render_readback(luna::IRHIDevice& device, const luna::FrameContext& frameContext)
{
    if (!ensure_readback_resources(device)) {
        return false;
    }

    auto& readback = m_state->readback;
    const size_t slot = frame_slot(frameContext.frameIndex, m_impl->readbackBuffers.size());

    if (slot < m_impl->readbackPending.size() && m_impl->readbackPending[slot]) {
        std::array<uint8_t, kReadbackBlockSize * kReadbackBlockSize * 4> bytes{};
        if (device.readBuffer(m_impl->readbackBuffers[slot], bytes.data(), bytes.size(), 0) != luna::RHIResult::Success) {
            return false;
        }

        for (size_t index = 0; index < readback.pixels.size(); ++index) {
            readback.pixels[index] = pack_rgba_u32(bytes.data() + index * 4);
        }
        readback.hasReadbackData = true;
        readback.status = "CPU readback completed for the selected 4x4 region.";
        LUNA_CORE_INFO("RhiSyncLab readback CPU consume: region=({}, {}), firstPixel=0x{:08X}",
                       m_impl->readbackPendingX[slot],
                       m_impl->readbackPendingY[slot],
                       readback.pixels[0]);
        m_impl->readbackPending[slot] = false;
        push_timeline(m_state, &m_impl->timelineSerial, "Readback: CPU consumed image-to-buffer copy");
    }

    if (readback.copyBufferToImageRequested) {
        readback.copyBufferToImageRequested = false;
        const uint32_t seed = static_cast<uint32_t>(readback.regionX + readback.regionY * 17);
        const std::vector<uint8_t> pixels = build_readback_pixels(kReadbackImageSize, kReadbackImageSize, seed);
        if (device.writeBuffer(m_impl->readbackUploadBuffer, pixels.data(), pixels.size(), 0) != luna::RHIResult::Success) {
            return false;
        }

        if (frameContext.commandContext->bufferBarrier({.buffer = m_impl->readbackUploadBuffer,
                                                        .srcStage = luna::PipelineStage::Host,
                                                        .dstStage = luna::PipelineStage::Transfer,
                                                        .srcAccess = luna::ResourceAccess::HostWrite,
                                                        .dstAccess = luna::ResourceAccess::TransferRead}) != luna::RHIResult::Success ||
            frameContext.commandContext->imageBarrier({.image = m_impl->readbackImage,
                                                       .newLayout = luna::ImageLayout::TransferDst,
                                                       .srcStage = luna::PipelineStage::FragmentShader,
                                                       .dstStage = luna::PipelineStage::Transfer,
                                                       .srcAccess = luna::ResourceAccess::ShaderRead,
                                                       .dstAccess = luna::ResourceAccess::TransferWrite,
                                                       .aspect = luna::ImageAspect::Color}) != luna::RHIResult::Success ||
            frameContext.commandContext->copyBufferToImage({.buffer = m_impl->readbackUploadBuffer,
                                                            .image = m_impl->readbackImage,
                                                            .aspect = luna::ImageAspect::Color,
                                                            .imageExtentWidth = kReadbackImageSize,
                                                            .imageExtentHeight = kReadbackImageSize,
                                                            .imageExtentDepth = 1}) != luna::RHIResult::Success ||
            frameContext.commandContext->imageBarrier({.image = m_impl->readbackImage,
                                                       .oldLayout = luna::ImageLayout::TransferDst,
                                                       .newLayout = luna::ImageLayout::ShaderReadOnly,
                                                       .srcStage = luna::PipelineStage::Transfer,
                                                       .dstStage = luna::PipelineStage::FragmentShader,
                                                       .srcAccess = luna::ResourceAccess::TransferWrite,
                                                       .dstAccess = luna::ResourceAccess::ShaderRead,
                                                       .aspect = luna::ImageAspect::Color}) != luna::RHIResult::Success) {
            return false;
        }

        readback.status = "Copy Buffer To Image succeeded and the preview image was refreshed.";
        LUNA_CORE_INFO("RhiSyncLab copy buffer to image: seed={}, region=({}, {})",
                       seed,
                       readback.regionX,
                       readback.regionY);
        push_timeline(m_state, &m_impl->timelineSerial, "Readback: buffer barrier + copy buffer -> image");
    }

    if (readback.copyImageToBufferRequested) {
        readback.copyImageToBufferRequested = false;
        if (frameContext.commandContext->imageBarrier({.image = m_impl->readbackImage,
                                                       .newLayout = luna::ImageLayout::TransferSrc,
                                                       .srcStage = luna::PipelineStage::FragmentShader,
                                                       .dstStage = luna::PipelineStage::Transfer,
                                                       .srcAccess = luna::ResourceAccess::ShaderRead,
                                                       .dstAccess = luna::ResourceAccess::TransferRead,
                                                       .aspect = luna::ImageAspect::Color}) != luna::RHIResult::Success ||
            frameContext.commandContext->copyImageToBuffer({.buffer = m_impl->readbackBuffers[slot],
                                                            .image = m_impl->readbackImage,
                                                            .aspect = luna::ImageAspect::Color,
                                                            .imageOffsetX = static_cast<uint32_t>(readback.regionX),
                                                            .imageOffsetY = static_cast<uint32_t>(readback.regionY),
                                                            .imageExtentWidth = kReadbackBlockSize,
                                                            .imageExtentHeight = kReadbackBlockSize,
                                                            .imageExtentDepth = 1}) != luna::RHIResult::Success ||
            frameContext.commandContext->bufferBarrier({.buffer = m_impl->readbackBuffers[slot],
                                                        .srcStage = luna::PipelineStage::Transfer,
                                                        .dstStage = luna::PipelineStage::Host,
                                                        .srcAccess = luna::ResourceAccess::TransferWrite,
                                                        .dstAccess = luna::ResourceAccess::HostRead}) != luna::RHIResult::Success ||
            frameContext.commandContext->imageBarrier({.image = m_impl->readbackImage,
                                                       .oldLayout = luna::ImageLayout::TransferSrc,
                                                       .newLayout = luna::ImageLayout::ShaderReadOnly,
                                                       .srcStage = luna::PipelineStage::Transfer,
                                                       .dstStage = luna::PipelineStage::FragmentShader,
                                                       .srcAccess = luna::ResourceAccess::TransferRead,
                                                       .dstAccess = luna::ResourceAccess::ShaderRead,
                                                       .aspect = luna::ImageAspect::Color}) != luna::RHIResult::Success) {
            return false;
        }

        m_impl->readbackPending[slot] = true;
        m_impl->readbackPendingX[slot] = readback.regionX;
        m_impl->readbackPendingY[slot] = readback.regionY;
        readback.status = "Image to buffer copy recorded. Readback data will appear when this frame slot returns.";
        LUNA_CORE_INFO("RhiSyncLab copy image to buffer: region=({}, {}), size={}x{}",
                       readback.regionX,
                       readback.regionY,
                       kReadbackBlockSize,
                       kReadbackBlockSize);
        push_timeline(m_state, &m_impl->timelineSerial, "Readback: image barrier + copy image -> buffer");
    }

    return frameContext.commandContext->beginRendering({.width = frameContext.renderWidth,
                                                        .height = frameContext.renderHeight,
                                                        .colorAttachments = {{frameContext.backbuffer,
                                                                              frameContext.backbufferFormat,
                                                                              {0.04f, 0.03f, 0.02f, 1.0f}}}}) ==
               luna::RHIResult::Success &&
           frameContext.commandContext->bindGraphicsPipeline(m_impl->singlePreviewPipeline) == luna::RHIResult::Success &&
           frameContext.commandContext->bindResourceSet(m_impl->readbackPreviewSet) == luna::RHIResult::Success &&
           frameContext.commandContext->draw({3, 1, 0, 0}) == luna::RHIResult::Success &&
           frameContext.commandContext->endRendering() == luna::RHIResult::Success;
}

bool RhiSyncLabRenderPipeline::render_indirect(luna::IRHIDevice& device, const luna::FrameContext& frameContext)
{
    if (!ensure_indirect_resources(device)) {
        return false;
    }

    auto& indirect = m_state->indirect;
    const size_t slot = frame_slot(frameContext.frameIndex, m_impl->indirectReadbackBuffers.size());
    if (slot < m_impl->indirectReadbackPending.size() && m_impl->indirectReadbackPending[slot]) {
        std::array<uint32_t, 4> args{};
        if (device.readBuffer(m_impl->indirectReadbackBuffers[slot], args.data(), sizeof(args), 0) != luna::RHIResult::Success) {
            return false;
        }
        indirect.gpuArgs = {args[0], args[1], args[2]};
        m_impl->indirectReadbackPending[slot] = false;
        push_timeline(m_state, &m_impl->timelineSerial, "Indirect: CPU consumed GPU-generated args");
    }

    bool generatedThisFrame = false;
    if (indirect.generateArgsRequested) {
        indirect.generateArgsRequested = false;
        GenerateArgsPushConstants pushConstants{};
        pushConstants.requested[0] = static_cast<uint32_t>(std::max(1, indirect.desiredGroupCountX));
        pushConstants.requested[1] = static_cast<uint32_t>(std::max(1, indirect.desiredGroupCountY));
        pushConstants.requested[2] = 1u;

        if (frameContext.commandContext->bindComputePipeline(m_impl->indirectGeneratePipeline) != luna::RHIResult::Success ||
            frameContext.commandContext->bindResourceSet(m_impl->indirectArgsSet) != luna::RHIResult::Success ||
            frameContext.commandContext->pushConstants(
                &pushConstants, sizeof(pushConstants), 0, luna::ShaderType::Compute) != luna::RHIResult::Success ||
            frameContext.commandContext->dispatch(1, 1, 1) != luna::RHIResult::Success ||
            frameContext.commandContext->bufferBarrier({.buffer = m_impl->indirectArgsBuffer,
                                                        .srcStage = luna::PipelineStage::ComputeShader,
                                                        .dstStage = luna::PipelineStage::Transfer | luna::PipelineStage::DrawIndirect,
                                                        .srcAccess = luna::ResourceAccess::ShaderWrite,
                                                        .dstAccess = luna::ResourceAccess::TransferRead |
                                                                     luna::ResourceAccess::IndirectCommandRead}) !=
                luna::RHIResult::Success ||
            frameContext.commandContext->copyBuffer({m_impl->indirectArgsBuffer,
                                                    m_impl->indirectReadbackBuffers[slot],
                                                    0,
                                                    0,
                                                    sizeof(uint32_t) * 4}) != luna::RHIResult::Success ||
            frameContext.commandContext->bufferBarrier({.buffer = m_impl->indirectReadbackBuffers[slot],
                                                        .srcStage = luna::PipelineStage::Transfer,
                                                        .dstStage = luna::PipelineStage::Host,
                                                        .srcAccess = luna::ResourceAccess::TransferWrite,
                                                        .dstAccess = luna::ResourceAccess::HostRead}) != luna::RHIResult::Success) {
            return false;
        }

        generatedThisFrame = true;
        m_impl->indirectReadbackPending[slot] = true;
        indirect.status = "GPU generated a new indirect args buffer.";
        LUNA_CORE_INFO("RhiSyncLab indirect args generated on GPU: requested=({}, {})",
                       indirect.desiredGroupCountX,
                       indirect.desiredGroupCountY);
        push_timeline(m_state, &m_impl->timelineSerial, "Indirect: compute wrote args + copy buffer -> readback");
    }

    if (indirect.runRequested) {
        indirect.runRequested = false;
        if (frameContext.commandContext->imageBarrier({.image = m_impl->indirectImage,
                                                       .newLayout = luna::ImageLayout::General,
                                                       .srcStage = luna::PipelineStage::FragmentShader,
                                                       .dstStage = luna::PipelineStage::ComputeShader,
                                                       .srcAccess = luna::ResourceAccess::ShaderRead,
                                                       .dstAccess = luna::ResourceAccess::ShaderWrite,
                                                       .aspect = luna::ImageAspect::Color}) != luna::RHIResult::Success ||
            frameContext.commandContext->bindComputePipeline(m_impl->indirectPaintPipeline) != luna::RHIResult::Success ||
            frameContext.commandContext->bindResourceSet(m_impl->indirectPaintSet) != luna::RHIResult::Success) {
            return false;
        }

        const bool dispatchOk = indirect.useIndirect
                                    ? frameContext.commandContext->dispatchIndirect(m_impl->indirectArgsBuffer, 0) ==
                                          luna::RHIResult::Success
                                    : frameContext.commandContext->dispatch(
                                          static_cast<uint32_t>(std::max(1, indirect.desiredGroupCountX)),
                                          static_cast<uint32_t>(std::max(1, indirect.desiredGroupCountY)),
                                          1) == luna::RHIResult::Success;
        if (!dispatchOk ||
            frameContext.commandContext->imageBarrier({.image = m_impl->indirectImage,
                                                       .oldLayout = luna::ImageLayout::General,
                                                       .newLayout = luna::ImageLayout::ShaderReadOnly,
                                                       .srcStage = luna::PipelineStage::ComputeShader,
                                                       .dstStage = luna::PipelineStage::FragmentShader,
                                                       .srcAccess = luna::ResourceAccess::ShaderWrite,
                                                       .dstAccess = luna::ResourceAccess::ShaderRead,
                                                       .aspect = luna::ImageAspect::Color}) != luna::RHIResult::Success) {
            return false;
        }

        indirect.status = indirect.useIndirect ? "Indirect dispatch executed using GPU-generated args."
                                               : "CPU direct dispatch executed for comparison.";
        LUNA_CORE_INFO("RhiSyncLab indirect run: path={}, requested=({}, {})",
                       indirect.useIndirect ? "GPU indirect" : "CPU direct",
                       indirect.desiredGroupCountX,
                       indirect.desiredGroupCountY);
        push_timeline(m_state,
                      &m_impl->timelineSerial,
                      generatedThisFrame ? "Indirect: generated args + ran indirect dispatch"
                                         : (indirect.useIndirect ? "Indirect: ran indirect dispatch"
                                                                 : "Indirect: ran CPU direct dispatch"));
    }

    return frameContext.commandContext->beginRendering({.width = frameContext.renderWidth,
                                                        .height = frameContext.renderHeight,
                                                        .colorAttachments = {{frameContext.backbuffer,
                                                                              frameContext.backbufferFormat,
                                                                              {0.02f, 0.03f, 0.05f, 1.0f}}}}) ==
               luna::RHIResult::Success &&
           frameContext.commandContext->bindGraphicsPipeline(m_impl->singlePreviewPipeline) == luna::RHIResult::Success &&
           frameContext.commandContext->bindResourceSet(m_impl->indirectPreviewSet) == luna::RHIResult::Success &&
           frameContext.commandContext->draw({3, 1, 0, 0}) == luna::RHIResult::Success &&
           frameContext.commandContext->endRendering() == luna::RHIResult::Success;
}

void RhiSyncLabRenderPipeline::destroy_history_resources(luna::IRHIDevice& device)
{
    if (m_impl == nullptr) {
        return;
    }

    if (m_impl->historyUploadBuffer.isValid()) device.destroyBuffer(m_impl->historyUploadBuffer);
    if (m_impl->historyCurrentImage.isValid()) device.destroyImage(m_impl->historyCurrentImage);
    if (m_impl->historyImage.isValid()) device.destroyImage(m_impl->historyImage);
    if (m_impl->historyBarrierProbeImage.isValid()) device.destroyImage(m_impl->historyBarrierProbeImage);

    m_impl->historyUploadBuffer = {};
    m_impl->historyCurrentImage = {};
    m_impl->historyImage = {};
    m_impl->historyBarrierProbeImage = {};
}

void RhiSyncLabRenderPipeline::destroy_readback_resources(luna::IRHIDevice& device)
{
    if (m_impl == nullptr) {
        return;
    }

    if (m_impl->readbackUploadBuffer.isValid()) device.destroyBuffer(m_impl->readbackUploadBuffer);
    if (m_impl->readbackImage.isValid()) device.destroyImage(m_impl->readbackImage);
    for (luna::BufferHandle& buffer : m_impl->readbackBuffers) {
        if (buffer.isValid()) {
            device.destroyBuffer(buffer);
        }
    }

    m_impl->readbackUploadBuffer = {};
    m_impl->readbackImage = {};
    m_impl->readbackBuffers.clear();
    m_impl->readbackPending.clear();
    m_impl->readbackPendingX.clear();
    m_impl->readbackPendingY.clear();
}

void RhiSyncLabRenderPipeline::destroy_indirect_resources(luna::IRHIDevice& device)
{
    if (m_impl == nullptr) {
        return;
    }

    if (m_impl->indirectGeneratePipeline.isValid()) device.destroyPipeline(m_impl->indirectGeneratePipeline);
    if (m_impl->indirectPaintPipeline.isValid()) device.destroyPipeline(m_impl->indirectPaintPipeline);
    if (m_impl->indirectArgsSet.isValid()) device.destroyResourceSet(m_impl->indirectArgsSet);
    if (m_impl->indirectPaintSet.isValid()) device.destroyResourceSet(m_impl->indirectPaintSet);
    if (m_impl->indirectArgsLayout.isValid()) device.destroyResourceLayout(m_impl->indirectArgsLayout);
    if (m_impl->indirectPaintLayout.isValid()) device.destroyResourceLayout(m_impl->indirectPaintLayout);
    if (m_impl->indirectArgsBuffer.isValid()) device.destroyBuffer(m_impl->indirectArgsBuffer);
    if (m_impl->indirectImage.isValid()) device.destroyImage(m_impl->indirectImage);
    for (luna::BufferHandle& buffer : m_impl->indirectReadbackBuffers) {
        if (buffer.isValid()) {
            device.destroyBuffer(buffer);
        }
    }

    m_impl->indirectGeneratePipeline = {};
    m_impl->indirectPaintPipeline = {};
    m_impl->indirectArgsSet = {};
    m_impl->indirectPaintSet = {};
    m_impl->indirectArgsLayout = {};
    m_impl->indirectPaintLayout = {};
    m_impl->indirectArgsBuffer = {};
    m_impl->indirectImage = {};
    m_impl->indirectReadbackBuffers.clear();
    m_impl->indirectReadbackPending.clear();
}

} // namespace sync_lab
