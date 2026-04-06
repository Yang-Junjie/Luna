#include "ImageLabPipeline.h"

#include "Core/log.h"
#include "RHI/CommandContext.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <sstream>

namespace image_lab {
namespace {

struct alignas(16) PreviewPushConstants {
    float params[4] = {};
};

std::string shader_path(const std::string& root, std::string_view fileName)
{
    return (std::filesystem::path(root) / fileName).lexically_normal().generic_string();
}

std::string join_formats(const std::array<luna::PixelFormat, 4>& formats, int count)
{
    std::ostringstream builder;
    for (int index = 0; index < count; ++index) {
        if (index > 0) {
            builder << ", ";
        }
        builder << luna::to_string(formats[static_cast<size_t>(index)]);
    }
    return builder.str();
}

const char* attachment_mode_name(AttachmentMode mode)
{
    switch (mode) {
        case AttachmentMode::DepthOnly:
            return "Depth Only";
        case AttachmentMode::ColorOnly:
        default:
            return "Color Only";
    }
}

bool is_probe_accepted(luna::PixelFormat format, const luna::RHIFormatSupport& support)
{
    return luna::is_depth_format(format) ? support.depthStencilAttachment : support.sampled && support.colorAttachment;
}

std::string build_format_support_details(const luna::RHIFormatSupport& support)
{
    std::ostringstream builder;
    builder << "sampled=" << (support.sampled ? "yes" : "no") << ", colorAttachment="
            << (support.colorAttachment ? "yes" : "no") << ", depthStencilAttachment="
            << (support.depthStencilAttachment ? "yes" : "no") << ", storage="
            << (support.storage ? "yes" : "no");
    return builder.str();
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

std::array<luna::ImageHandle, 4> fill_image_array(luna::ImageHandle image)
{
    return {image, image, image, image};
}

size_t frame_slot(uint32_t frameIndex, size_t framesInFlight)
{
    return framesInFlight == 0 ? 0 : static_cast<size_t>(frameIndex % static_cast<uint32_t>(framesInFlight));
}

} // namespace

RhiImageLabRenderPipeline::RhiImageLabRenderPipeline(std::shared_ptr<State> state)
    : m_state(std::move(state))
{}

bool RhiImageLabRenderPipeline::init(luna::IRHIDevice& device)
{
    (void)device;
    m_shaderRoot = std::filesystem::path{RHI_IMAGE_LAB_SHADER_ROOT}.lexically_normal().generic_string();
    return true;
}

void RhiImageLabRenderPipeline::shutdown(luna::IRHIDevice& device)
{
    destroy_attachment_ops_resources(device);
    destroy_mrt_resources(device);
    destroy_mip_resources(device);
    destroy_array3d_resources(device);
    destroy_present_pipelines(device);
    destroy_shared_resources(device);
}

bool RhiImageLabRenderPipeline::render(luna::IRHIDevice& device, const luna::FrameContext& frameContext)
{
    if (m_state == nullptr || frameContext.commandContext == nullptr || !frameContext.backbuffer.isValid()) {
        return false;
    }

    if (!ensure_shared_resources(device) || !ensure_present_pipelines(device, frameContext.backbufferFormat)) {
        return false;
    }

    switch (m_state->page) {
        case Page::AttachmentOps:
            return render_attachment_ops(device, frameContext);
        case Page::MRTPreview:
            return render_mrt_preview(device, frameContext);
        case Page::MipPreview:
            return render_mip_preview(device, frameContext);
        case Page::Array3DPreview:
            return render_array3d_preview(device, frameContext);
        case Page::FormatProbe:
        default:
            return render_format_probe(device, frameContext);
    }
}

bool RhiImageLabRenderPipeline::ensure_shared_resources(luna::IRHIDevice& device)
{
    const uint32_t desiredFramesInFlight = std::max(1u, device.getCapabilities().framesInFlight);
    bool sharedReady = m_linearSampler.isValid() && m_presentTextureLayout.isValid() && m_arrayTextureLayout.isValid() &&
                       m_volumeTextureLayout.isValid() && m_framesInFlight == desiredFramesInFlight &&
                       m_presentTextureSets.size() == desiredFramesInFlight &&
                       m_arrayTextureSets.size() == desiredFramesInFlight &&
                       m_volumeTextureSets.size() == desiredFramesInFlight;
    if (sharedReady) {
        for (uint32_t frame = 0; frame < desiredFramesInFlight; ++frame) {
            if (!m_presentTextureSets[frame].isValid() || !m_arrayTextureSets[frame].isValid() ||
                !m_volumeTextureSets[frame].isValid()) {
                sharedReady = false;
                break;
            }
        }
    }
    if (sharedReady) {
        return true;
    }

    if (m_linearSampler.isValid() || m_presentTextureLayout.isValid() || m_arrayTextureLayout.isValid() ||
        m_volumeTextureLayout.isValid() || !m_presentTextureSets.empty() || !m_arrayTextureSets.empty() ||
        !m_volumeTextureSets.empty()) {
        destroy_shared_resources(device);
    }

    if (!m_linearSampler.isValid()) {
        luna::SamplerDesc samplerDesc{};
        samplerDesc.debugName = "RhiImageLabLinearSampler";
        if (device.createSampler(samplerDesc, &m_linearSampler) != luna::RHIResult::Success) {
            return false;
        }
    }

    luna::ResourceLayoutDesc layoutDesc{};
    if (!m_presentTextureLayout.isValid()) {
        layoutDesc = {};
        layoutDesc.debugName = "RhiImageLabPresentTextureLayout";
        for (uint32_t binding = 0; binding < 4; ++binding) {
            layoutDesc.bindings.push_back({binding, luna::ResourceType::CombinedImageSampler, 1, luna::ShaderType::Fragment});
        }
        if (device.createResourceLayout(layoutDesc, &m_presentTextureLayout) != luna::RHIResult::Success) {
            return false;
        }
    }

    if (!m_arrayTextureLayout.isValid()) {
        layoutDesc = {};
        layoutDesc.debugName = "RhiImageLabArrayTextureLayout";
        layoutDesc.bindings.push_back({0, luna::ResourceType::CombinedImageSampler, 1, luna::ShaderType::Fragment});
        if (device.createResourceLayout(layoutDesc, &m_arrayTextureLayout) != luna::RHIResult::Success) {
            return false;
        }
    }

    if (!m_volumeTextureLayout.isValid()) {
        layoutDesc = {};
        layoutDesc.debugName = "RhiImageLabVolumeTextureLayout";
        layoutDesc.bindings.push_back({0, luna::ResourceType::CombinedImageSampler, 1, luna::ShaderType::Fragment});
        if (device.createResourceLayout(layoutDesc, &m_volumeTextureLayout) != luna::RHIResult::Success) {
            return false;
        }
    }

    m_framesInFlight = desiredFramesInFlight;
    m_presentTextureSets.assign(m_framesInFlight, {});
    m_arrayTextureSets.assign(m_framesInFlight, {});
    m_volumeTextureSets.assign(m_framesInFlight, {});
    m_presentTextureSetImages.assign(m_framesInFlight, {});
    m_arrayTextureSetImages.assign(m_framesInFlight, {});
    m_volumeTextureSetImages.assign(m_framesInFlight, {});

    for (uint32_t frame = 0; frame < m_framesInFlight; ++frame) {
        if (device.createResourceSet(m_presentTextureLayout, &m_presentTextureSets[frame]) != luna::RHIResult::Success ||
            device.createResourceSet(m_arrayTextureLayout, &m_arrayTextureSets[frame]) != luna::RHIResult::Success ||
            device.createResourceSet(m_volumeTextureLayout, &m_volumeTextureSets[frame]) != luna::RHIResult::Success) {
            destroy_shared_resources(device);
            return false;
        }
    }

    return true;
}

bool RhiImageLabRenderPipeline::ensure_present_pipelines(luna::IRHIDevice& device, luna::PixelFormat backbufferFormat)
{
    if (backbufferFormat == luna::PixelFormat::Undefined) {
        return false;
    }

    if (m_presentBackbufferFormat == backbufferFormat && m_present2DPipeline.isValid() && m_presentDepthPipeline.isValid() &&
        m_presentArrayPipeline.isValid() && m_presentVolumePipeline.isValid()) {
        return true;
    }

    if (m_present2DPipeline.isValid() || m_presentDepthPipeline.isValid() || m_presentArrayPipeline.isValid() ||
        m_presentVolumePipeline.isValid()) {
        destroy_present_pipelines(device);
    }

    const std::string vertexShaderPath = shader_path(m_shaderRoot, "image_lab_fullscreen.vert.spv");
    const std::string present2DFragmentShaderPath = shader_path(m_shaderRoot, "image_lab_present_2d.frag.spv");
    const std::string presentDepthFragmentShaderPath = shader_path(m_shaderRoot, "image_lab_present_depth.frag.spv");
    const std::string presentArrayFragmentShaderPath = shader_path(m_shaderRoot, "image_lab_present_array.frag.spv");
    const std::string presentVolumeFragmentShaderPath = shader_path(m_shaderRoot, "image_lab_present_3d.frag.spv");

    luna::GraphicsPipelineDesc present2DDesc{};
    present2DDesc.debugName = "RhiImageLabPresent2D";
    present2DDesc.vertexShader = {.stage = luna::ShaderType::Vertex, .filePath = vertexShaderPath};
    present2DDesc.fragmentShader = {.stage = luna::ShaderType::Fragment, .filePath = present2DFragmentShaderPath};
    present2DDesc.resourceLayouts.push_back(m_presentTextureLayout);
    present2DDesc.pushConstantSize = sizeof(PreviewPushConstants);
    present2DDesc.pushConstantVisibility = luna::ShaderType::Fragment;
    present2DDesc.cullMode = luna::CullMode::None;
    present2DDesc.frontFace = luna::FrontFace::Clockwise;
    present2DDesc.colorAttachments.push_back({backbufferFormat, false});
    if (device.createGraphicsPipeline(present2DDesc, &m_present2DPipeline) != luna::RHIResult::Success) {
        return false;
    }

    luna::GraphicsPipelineDesc presentDepthDesc = present2DDesc;
    presentDepthDesc.debugName = "RhiImageLabPresentDepth";
    presentDepthDesc.resourceLayouts = {m_arrayTextureLayout};
    presentDepthDesc.fragmentShader.filePath = presentDepthFragmentShaderPath;
    if (device.createGraphicsPipeline(presentDepthDesc, &m_presentDepthPipeline) != luna::RHIResult::Success) {
        return false;
    }

    luna::GraphicsPipelineDesc presentArrayDesc = present2DDesc;
    presentArrayDesc.debugName = "RhiImageLabPresentArray";
    presentArrayDesc.resourceLayouts = {m_arrayTextureLayout};
    presentArrayDesc.fragmentShader.filePath = presentArrayFragmentShaderPath;
    if (device.createGraphicsPipeline(presentArrayDesc, &m_presentArrayPipeline) != luna::RHIResult::Success) {
        return false;
    }

    luna::GraphicsPipelineDesc presentVolumeDesc = present2DDesc;
    presentVolumeDesc.debugName = "RhiImageLabPresentVolume";
    presentVolumeDesc.resourceLayouts = {m_volumeTextureLayout};
    presentVolumeDesc.fragmentShader.filePath = presentVolumeFragmentShaderPath;
    if (device.createGraphicsPipeline(presentVolumeDesc, &m_presentVolumePipeline) != luna::RHIResult::Success) {
        return false;
    }

    m_presentBackbufferFormat = backbufferFormat;
    return true;
}

bool RhiImageLabRenderPipeline::ensure_attachment_ops_resources(luna::IRHIDevice& device,
                                                                uint32_t width,
                                                                uint32_t height,
                                                                luna::PixelFormat)
{
    const bool needsRebuild = !m_attachmentColorPipeline.isValid() || !m_attachmentDepthPipeline.isValid() ||
                              !m_attachmentColorImage.isValid() || !m_attachmentDepthImage.isValid() ||
                              m_attachmentWidth != width || m_attachmentHeight != height;
    if (!needsRebuild) {
        return true;
    }

    if (m_attachmentColorPipeline.isValid() || m_attachmentDepthPipeline.isValid() || m_attachmentColorImage.isValid() ||
        m_attachmentDepthImage.isValid()) {
        destroy_attachment_ops_resources(device);
    }

    const std::vector<uint8_t> blackPixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u, 0u);
    luna::ImageDesc colorDesc{};
    colorDesc.width = width;
    colorDesc.height = height;
    colorDesc.depth = 1;
    colorDesc.mipLevels = 1;
    colorDesc.arrayLayers = 1;
    colorDesc.type = luna::ImageType::Image2D;
    colorDesc.format = luna::PixelFormat::RGBA8Unorm;
    colorDesc.usage = luna::ImageUsage::ColorAttachment | luna::ImageUsage::Sampled;
    colorDesc.debugName = "RhiImageLabAttachmentOpsColor";
    if (device.createImage(colorDesc, &m_attachmentColorImage, blackPixels.data()) != luna::RHIResult::Success) {
        destroy_attachment_ops_resources(device);
        return false;
    }

    luna::ImageDesc depthDesc = colorDesc;
    depthDesc.format = luna::PixelFormat::D32Float;
    depthDesc.usage = luna::ImageUsage::DepthStencilAttachment | luna::ImageUsage::Sampled;
    depthDesc.debugName = "RhiImageLabAttachmentOpsDepth";
    if (device.createImage(depthDesc, &m_attachmentDepthImage) != luna::RHIResult::Success) {
        destroy_attachment_ops_resources(device);
        return false;
    }

    const std::string sceneVertexShaderPath = shader_path(m_shaderRoot, "image_lab_attachment_scene.vert.spv");
    const std::string sceneColorFragmentShaderPath = shader_path(m_shaderRoot, "image_lab_attachment_color.frag.spv");
    const std::string sceneDepthFragmentShaderPath = shader_path(m_shaderRoot, "image_lab_attachment_depth.frag.spv");

    luna::GraphicsPipelineDesc colorPipelineDesc{};
    colorPipelineDesc.debugName = "RhiImageLabAttachmentColor";
    colorPipelineDesc.vertexShader = {.stage = luna::ShaderType::Vertex, .filePath = sceneVertexShaderPath};
    colorPipelineDesc.fragmentShader = {.stage = luna::ShaderType::Fragment, .filePath = sceneColorFragmentShaderPath};
    colorPipelineDesc.pushConstantSize = sizeof(PreviewPushConstants);
    colorPipelineDesc.pushConstantVisibility = luna::ShaderType::AllGraphics;
    colorPipelineDesc.cullMode = luna::CullMode::None;
    colorPipelineDesc.frontFace = luna::FrontFace::Clockwise;
    colorPipelineDesc.colorAttachments.push_back({colorDesc.format, false});
    if (device.createGraphicsPipeline(colorPipelineDesc, &m_attachmentColorPipeline) != luna::RHIResult::Success) {
        destroy_attachment_ops_resources(device);
        return false;
    }

    luna::GraphicsPipelineDesc depthPipelineDesc = colorPipelineDesc;
    depthPipelineDesc.debugName = "RhiImageLabAttachmentDepth";
    depthPipelineDesc.fragmentShader.filePath = sceneDepthFragmentShaderPath;
    depthPipelineDesc.colorAttachments.clear();
    depthPipelineDesc.depthStencil = {
        luna::PixelFormat::D32Float,
        true,
        true,
        luna::CompareOp::LessOrEqual,
    };
    if (device.createGraphicsPipeline(depthPipelineDesc, &m_attachmentDepthPipeline) != luna::RHIResult::Success) {
        destroy_attachment_ops_resources(device);
        return false;
    }

    m_attachmentWidth = width;
    m_attachmentHeight = height;
    return true;
}

bool RhiImageLabRenderPipeline::render_format_probe(luna::IRHIDevice& device, const luna::FrameContext& frameContext)
{
    update_format_probe(device);
    return render_placeholder(frameContext, {0.08f, 0.09f, 0.11f, 1.0f});
}

bool RhiImageLabRenderPipeline::render_attachment_ops(luna::IRHIDevice& device, const luna::FrameContext& frameContext)
{
    if (!ensure_attachment_ops_resources(device, frameContext.renderWidth, frameContext.renderHeight, frameContext.backbufferFormat)) {
        return render_placeholder(frameContext, {0.08f, 0.07f, 0.05f, 1.0f});
    }

    auto& ops = m_state->attachmentOps;
    ops.viewportScale = std::clamp(ops.viewportScale, 0.2f, 1.0f);
    const int maxInset = std::max(0, static_cast<int>(std::min(m_attachmentWidth, m_attachmentHeight) / 2u) - 1);
    ops.scissorInset = std::clamp(ops.scissorInset, 0, maxInset);

    const float phase = static_cast<float>(ops.renderTick) * 0.13f;
    PreviewPushConstants pushConstants{};
    pushConstants.params[0] = 0.52f * std::sin(phase);
    pushConstants.params[1] = 0.38f * std::cos(phase * 0.71f);
    pushConstants.params[2] = 0.92f;
    pushConstants.params[3] = phase;

    luna::RenderingInfo renderingInfo{};
    renderingInfo.width = m_attachmentWidth;
    renderingInfo.height = m_attachmentHeight;
    if (ops.mode == AttachmentMode::ColorOnly) {
        renderingInfo.colorAttachments.push_back({
            m_attachmentColorImage,
            luna::PixelFormat::RGBA8Unorm,
            {0.02f, 0.02f, 0.025f, 1.0f},
            {},
            ops.colorLoadOp,
            ops.colorStoreOp,
        });
    } else {
        renderingInfo.depthAttachment = {
            m_attachmentDepthImage,
            luna::PixelFormat::D32Float,
            1.0f,
            {},
            ops.depthLoadOp,
            ops.depthStoreOp,
        };
    }

    const float viewportWidth = static_cast<float>(m_attachmentWidth) * ops.viewportScale;
    const float viewportHeight = static_cast<float>(m_attachmentHeight) * ops.viewportScale;
    const luna::Viewport viewport{
        (static_cast<float>(m_attachmentWidth) - viewportWidth) * 0.5f,
        (static_cast<float>(m_attachmentHeight) - viewportHeight) * 0.5f,
        viewportWidth,
        viewportHeight,
        0.0f,
        1.0f,
    };
    const luna::ScissorRect scissor{
        ops.scissorInset,
        ops.scissorInset,
        std::max(1u, m_attachmentWidth - static_cast<uint32_t>(ops.scissorInset * 2)),
        std::max(1u, m_attachmentHeight - static_cast<uint32_t>(ops.scissorInset * 2)),
    };

    const luna::PipelineHandle scenePipeline =
        ops.mode == AttachmentMode::ColorOnly ? m_attachmentColorPipeline : m_attachmentDepthPipeline;
    const bool renderOk =
        frameContext.commandContext->beginRendering(renderingInfo) == luna::RHIResult::Success &&
        frameContext.commandContext->setViewport(viewport) == luna::RHIResult::Success &&
        frameContext.commandContext->setScissor(scissor) == luna::RHIResult::Success &&
        frameContext.commandContext->bindGraphicsPipeline(scenePipeline) == luna::RHIResult::Success &&
        frameContext.commandContext->pushConstants(&pushConstants, sizeof(pushConstants), 0, luna::ShaderType::AllGraphics) ==
            luna::RHIResult::Success &&
        frameContext.commandContext->draw({3, 1, 0, 0}) == luna::RHIResult::Success &&
        frameContext.commandContext->endRendering() == luna::RHIResult::Success;
    if (!renderOk) {
        return false;
    }

    ++ops.renderTick;

    bool previewOk = false;
    if (ops.mode == AttachmentMode::ColorOnly) {
        if (frameContext.commandContext->transitionImage(m_attachmentColorImage, luna::ImageLayout::ShaderReadOnly) !=
                luna::RHIResult::Success ||
            !update_four_texture_set(device, frameContext.frameIndex, fill_image_array(m_attachmentColorImage))) {
            return false;
        }
        previewOk = render_textured_2d_preview(frameContext, 0, 0, 0.0f, 1);
    } else {
        if (frameContext.commandContext->transitionImage(m_attachmentDepthImage, luna::ImageLayout::ShaderReadOnly) !=
                luna::RHIResult::Success ||
            !update_array_texture_set(device, frameContext.frameIndex, m_attachmentDepthImage)) {
            return false;
        }
        previewOk = render_depth_preview(frameContext);
    }

    std::ostringstream status;
    status << "Mode=" << attachment_mode_name(ops.mode) << ", viewportScale=" << ops.viewportScale
           << ", scissorInset=" << ops.scissorInset << ", tick=" << ops.renderTick << ", color "
           << luna::to_string(ops.colorLoadOp) << "/" << luna::to_string(ops.colorStoreOp) << ", depth "
           << luna::to_string(ops.depthLoadOp) << "/" << luna::to_string(ops.depthStoreOp);
    if (ops.mode == AttachmentMode::ColorOnly) {
        status << ". Load keeps previous triangle trails, Clear/Discard resets to the current frame.";
    } else {
        status << ". Depth-only preview is sampling the D32 attachment.";
    }
    ops.status = status.str();
    return previewOk;
}

void RhiImageLabRenderPipeline::update_format_probe(luna::IRHIDevice& device)
{
    if (m_state == nullptr || !m_state->formatProbe.probeRequested) {
        return;
    }

    m_state->formatProbe.probeRequested = false;
    const luna::RHIFormatSupport support = device.queryFormatSupport(m_state->formatProbe.selectedFormat);
    m_state->formatProbe.backendMapping = std::string(support.backendFormatName);
    m_state->formatProbe.details = build_format_support_details(support);
    m_state->formatProbe.accepted = is_probe_accepted(m_state->formatProbe.selectedFormat, support);
}

bool RhiImageLabRenderPipeline::update_four_texture_set(luna::IRHIDevice& device,
                                                        uint32_t frameIndex,
                                                        const std::array<luna::ImageHandle, 4>& images)
{
    if (m_presentTextureSets.empty()) {
        return false;
    }

    const size_t slot = frame_slot(frameIndex, m_presentTextureSets.size());
    if (m_presentTextureSetImages[slot] == images) {
        return true;
    }

    luna::ResourceSetWriteDesc writeDesc{};
    for (uint32_t binding = 0; binding < 4; ++binding) {
        if (!images[binding].isValid()) {
            return false;
        }
        writeDesc.images.push_back({.binding = binding,
                                    .image = images[binding],
                                    .sampler = m_linearSampler,
                                    .type = luna::ResourceType::CombinedImageSampler});
    }
    if (device.updateResourceSet(m_presentTextureSets[slot], writeDesc) != luna::RHIResult::Success) {
        return false;
    }

    m_presentTextureSetImages[slot] = images;
    return true;
}

bool RhiImageLabRenderPipeline::update_array_texture_set(luna::IRHIDevice& device,
                                                         uint32_t frameIndex,
                                                         luna::ImageHandle image)
{
    if (m_arrayTextureSets.empty() || !image.isValid()) {
        return false;
    }

    const size_t slot = frame_slot(frameIndex, m_arrayTextureSets.size());
    if (m_arrayTextureSetImages[slot] == image) {
        return true;
    }

    luna::ResourceSetWriteDesc writeDesc{};
    writeDesc.images.push_back(
        {.binding = 0, .image = image, .sampler = m_linearSampler, .type = luna::ResourceType::CombinedImageSampler});
    if (device.updateResourceSet(m_arrayTextureSets[slot], writeDesc) != luna::RHIResult::Success) {
        return false;
    }

    m_arrayTextureSetImages[slot] = image;
    return true;
}

bool RhiImageLabRenderPipeline::update_volume_texture_set(luna::IRHIDevice& device,
                                                          uint32_t frameIndex,
                                                          luna::ImageHandle image)
{
    if (m_volumeTextureSets.empty() || !image.isValid()) {
        return false;
    }

    const size_t slot = frame_slot(frameIndex, m_volumeTextureSets.size());
    if (m_volumeTextureSetImages[slot] == image) {
        return true;
    }

    luna::ResourceSetWriteDesc writeDesc{};
    writeDesc.images.push_back(
        {.binding = 0, .image = image, .sampler = m_linearSampler, .type = luna::ResourceType::CombinedImageSampler});
    if (device.updateResourceSet(m_volumeTextureSets[slot], writeDesc) != luna::RHIResult::Success) {
        return false;
    }

    m_volumeTextureSetImages[slot] = image;
    return true;
}

bool RhiImageLabRenderPipeline::render_textured_2d_preview(const luna::FrameContext& frameContext,
                                                           int mode,
                                                           int previewIndex,
                                                           float lod,
                                                           int activeTextureCount)
{
    PreviewPushConstants pushConstants{};
    pushConstants.params[0] = static_cast<float>(mode);
    pushConstants.params[1] = static_cast<float>(previewIndex);
    pushConstants.params[2] = lod;
    pushConstants.params[3] = static_cast<float>(activeTextureCount);

    const bool ok =
        frameContext.commandContext->beginRendering({.width = frameContext.renderWidth,
                                                     .height = frameContext.renderHeight,
                                                     .colorAttachments = {{frameContext.backbuffer,
                                                                           frameContext.backbufferFormat,
                                                                           {0.03f, 0.03f, 0.035f, 1.0f}}}}) ==
            luna::RHIResult::Success &&
        frameContext.commandContext->bindGraphicsPipeline(m_present2DPipeline) == luna::RHIResult::Success &&
        frameContext.commandContext->bindResourceSet(
            m_presentTextureSets[frame_slot(frameContext.frameIndex, m_presentTextureSets.size())]) == luna::RHIResult::Success &&
        frameContext.commandContext->pushConstants(
            &pushConstants, sizeof(pushConstants), 0, luna::ShaderType::Fragment) == luna::RHIResult::Success &&
        frameContext.commandContext->draw({3, 1, 0, 0}) == luna::RHIResult::Success &&
        frameContext.commandContext->endRendering() == luna::RHIResult::Success;

    return ok;
}

bool RhiImageLabRenderPipeline::render_depth_preview(const luna::FrameContext& frameContext)
{
    PreviewPushConstants pushConstants{};
    const bool ok =
        frameContext.commandContext->beginRendering({.width = frameContext.renderWidth,
                                                     .height = frameContext.renderHeight,
                                                     .colorAttachments = {{frameContext.backbuffer,
                                                                           frameContext.backbufferFormat,
                                                                           {0.025f, 0.025f, 0.03f, 1.0f}}}}) ==
            luna::RHIResult::Success &&
        frameContext.commandContext->bindGraphicsPipeline(m_presentDepthPipeline) == luna::RHIResult::Success &&
        frameContext.commandContext->bindResourceSet(
            m_arrayTextureSets[frame_slot(frameContext.frameIndex, m_arrayTextureSets.size())]) == luna::RHIResult::Success &&
        frameContext.commandContext->pushConstants(
            &pushConstants, sizeof(pushConstants), 0, luna::ShaderType::Fragment) == luna::RHIResult::Success &&
        frameContext.commandContext->draw({3, 1, 0, 0}) == luna::RHIResult::Success &&
        frameContext.commandContext->endRendering() == luna::RHIResult::Success;

    return ok;
}

bool RhiImageLabRenderPipeline::render_array_preview_pass(const luna::FrameContext& frameContext, float layer)
{
    PreviewPushConstants pushConstants{};
    pushConstants.params[0] = layer;

    const bool ok =
        frameContext.commandContext->beginRendering({.width = frameContext.renderWidth,
                                                     .height = frameContext.renderHeight,
                                                     .colorAttachments = {{frameContext.backbuffer,
                                                                           frameContext.backbufferFormat,
                                                                           {0.03f, 0.035f, 0.05f, 1.0f}}}}) ==
            luna::RHIResult::Success &&
        frameContext.commandContext->bindGraphicsPipeline(m_presentArrayPipeline) == luna::RHIResult::Success &&
        frameContext.commandContext->bindResourceSet(
            m_arrayTextureSets[frame_slot(frameContext.frameIndex, m_arrayTextureSets.size())]) == luna::RHIResult::Success &&
        frameContext.commandContext->pushConstants(
            &pushConstants, sizeof(pushConstants), 0, luna::ShaderType::Fragment) == luna::RHIResult::Success &&
        frameContext.commandContext->draw({3, 1, 0, 0}) == luna::RHIResult::Success &&
        frameContext.commandContext->endRendering() == luna::RHIResult::Success;

    return ok;
}

bool RhiImageLabRenderPipeline::render_volume_preview_pass(const luna::FrameContext& frameContext, float slice)
{
    PreviewPushConstants pushConstants{};
    pushConstants.params[0] = slice;

    const bool ok =
        frameContext.commandContext->beginRendering({.width = frameContext.renderWidth,
                                                     .height = frameContext.renderHeight,
                                                     .colorAttachments = {{frameContext.backbuffer,
                                                                           frameContext.backbufferFormat,
                                                                           {0.02f, 0.03f, 0.04f, 1.0f}}}}) ==
            luna::RHIResult::Success &&
        frameContext.commandContext->bindGraphicsPipeline(m_presentVolumePipeline) == luna::RHIResult::Success &&
        frameContext.commandContext->bindResourceSet(
            m_volumeTextureSets[frame_slot(frameContext.frameIndex, m_volumeTextureSets.size())]) == luna::RHIResult::Success &&
        frameContext.commandContext->pushConstants(
            &pushConstants, sizeof(pushConstants), 0, luna::ShaderType::Fragment) == luna::RHIResult::Success &&
        frameContext.commandContext->draw({3, 1, 0, 0}) == luna::RHIResult::Success &&
        frameContext.commandContext->endRendering() == luna::RHIResult::Success;

    return ok;
}

bool RhiImageLabRenderPipeline::render_placeholder(const luna::FrameContext& frameContext,
                                                   const std::array<float, 4>& clearColor)
{
    return frameContext.commandContext->beginRendering(
               {.width = frameContext.renderWidth,
                .height = frameContext.renderHeight,
                .colorAttachments = {{frameContext.backbuffer,
                                      frameContext.backbufferFormat,
                                      {clearColor[0], clearColor[1], clearColor[2], clearColor[3]}}}}) ==
               luna::RHIResult::Success &&
           frameContext.commandContext->endRendering() == luna::RHIResult::Success;
}

bool RhiImageLabRenderPipeline::ensure_mrt_resources(luna::IRHIDevice& device,
                                                     uint32_t width,
                                                     uint32_t height,
                                                     luna::PixelFormat backbufferFormat)
{
    const int attachmentCount = std::clamp(m_state->mrt.attachmentCount, 1, 4);
    m_state->mrt.attachmentCount = attachmentCount;
    m_state->mrt.previewAttachment = std::clamp(m_state->mrt.previewAttachment, 0, attachmentCount - 1);

    bool needsRebuild = m_state->mrt.rebuildRequested || !m_mrtPipeline.isValid() || width != m_mrtWidth ||
                        height != m_mrtHeight || attachmentCount != m_mrtAttachmentCount;
    if (!needsRebuild) {
        for (int index = 0; index < attachmentCount; ++index) {
            if (m_mrtFormats[static_cast<size_t>(index)] != m_state->mrt.formats[static_cast<size_t>(index)] ||
                !m_mrtImages[static_cast<size_t>(index)].isValid()) {
                needsRebuild = true;
                break;
            }
        }
    }

    if (!needsRebuild) {
        return true;
    }

    if (m_mrtPipeline.isValid() || m_mrtImages[0].isValid()) {
        destroy_mrt_resources(device);
    }

    for (int index = 0; index < attachmentCount; ++index) {
        luna::ImageDesc desc{};
        desc.width = width;
        desc.height = height;
        desc.depth = 1;
        desc.mipLevels = 1;
        desc.arrayLayers = 1;
        desc.type = luna::ImageType::Image2D;
        desc.format = m_state->mrt.formats[static_cast<size_t>(index)];
        desc.usage = luna::ImageUsage::ColorAttachment | luna::ImageUsage::Sampled;
        desc.debugName = "RhiImageLabMRTAttachment";
        if (device.createImage(desc, &m_mrtImages[static_cast<size_t>(index)]) != luna::RHIResult::Success) {
            m_state->mrt.status =
                "Build MRT Pipeline failed: attachment image creation failed for format " +
                std::string(luna::to_string(desc.format));
            destroy_mrt_resources(device);
            return false;
        }
        m_mrtFormats[static_cast<size_t>(index)] = desc.format;
    }

    const std::string mrtVertexShaderPath = shader_path(m_shaderRoot, "image_lab_fullscreen.vert.spv");
    const std::string mrtFragmentShaderPath =
        shader_path(m_shaderRoot, "image_lab_mrt_" + std::to_string(attachmentCount) + ".frag.spv");

    luna::GraphicsPipelineDesc pipelineDesc{};
    pipelineDesc.debugName = "RhiImageLabMRTPipeline";
    pipelineDesc.vertexShader = {.stage = luna::ShaderType::Vertex, .filePath = mrtVertexShaderPath};
    pipelineDesc.fragmentShader = {.stage = luna::ShaderType::Fragment, .filePath = mrtFragmentShaderPath};
    pipelineDesc.cullMode = luna::CullMode::None;
    pipelineDesc.frontFace = luna::FrontFace::Clockwise;
    for (int index = 0; index < attachmentCount; ++index) {
        pipelineDesc.colorAttachments.push_back({m_state->mrt.formats[static_cast<size_t>(index)], false});
    }

    if (device.createGraphicsPipeline(pipelineDesc, &m_mrtPipeline) != luna::RHIResult::Success) {
        m_state->mrt.status = "Build MRT Pipeline failed: graphics pipeline creation failed.";
        destroy_mrt_resources(device);
        return false;
    }

    m_mrtWidth = width;
    m_mrtHeight = height;
    m_mrtAttachmentCount = attachmentCount;
    m_state->mrt.rebuildRequested = false;

    std::ostringstream status;
    status << "Pipeline build success: " << attachmentCount << " attachment(s) ["
           << join_formats(m_state->mrt.formats, attachmentCount) << "]";
    m_state->mrt.status = status.str();
    return true;
}

bool RhiImageLabRenderPipeline::ensure_mip_resources(luna::IRHIDevice& device, luna::PixelFormat)
{
    m_state->mip.mipLevels = calculate_theoretical_mip_count(m_state->mip.width, m_state->mip.height);
    m_state->mip.lod =
        std::clamp(m_state->mip.lod, 0.0f, static_cast<float>(std::max(1u, m_state->mip.mipLevels) - 1u));

    const bool needsRecreate = m_state->mip.createRequested || !m_mipImage.isValid() || m_mipWidth != m_state->mip.width ||
                               m_mipHeight != m_state->mip.height || m_mipLevelCount != m_state->mip.mipLevels;
    if (!needsRecreate) {
        return true;
    }

    if (m_mipImage.isValid()) {
        destroy_mip_resources(device);
    }

    const std::vector<uint8_t> pixels = build_mip_texture_data(m_state->mip.width, m_state->mip.height);
    luna::ImageDesc desc{};
    desc.width = m_state->mip.width;
    desc.height = m_state->mip.height;
    desc.depth = 1;
    desc.mipLevels = m_state->mip.mipLevels;
    desc.arrayLayers = 1;
    desc.type = luna::ImageType::Image2D;
    desc.format = luna::PixelFormat::RGBA8Unorm;
    desc.usage = luna::ImageUsage::Sampled;
    desc.debugName = "RhiImageLabMipTexture";
    if (device.createImage(desc, &m_mipImage, pixels.data()) != luna::RHIResult::Success) {
        m_state->mip.status = "Create Mip Texture failed.";
        return false;
    }

    m_mipWidth = desc.width;
    m_mipHeight = desc.height;
    m_mipLevelCount = desc.mipLevels;
    m_state->mip.createRequested = false;

    std::ostringstream status;
    status << "Created mip texture: " << desc.width << "x" << desc.height << ", mipLevels=" << desc.mipLevels;
    m_state->mip.status = status.str();
    return true;
}

bool RhiImageLabRenderPipeline::ensure_array3d_resources(luna::IRHIDevice& device, luna::PixelFormat)
{
    const uint32_t desiredDepth = m_state->array3d.type == luna::ImageType::Image3D ? m_state->array3d.depth : 1u;
    const uint32_t desiredLayers =
        m_state->array3d.type == luna::ImageType::Image2DArray ? m_state->array3d.arrayLayers : 1u;

    m_state->array3d.mipLevels = std::clamp(
        m_state->array3d.mipLevels,
        1u,
        calculate_theoretical_mip_count(m_state->array3d.width,
                                        m_state->array3d.height,
                                        desiredDepth));

    const bool needsRecreate = m_state->array3d.createRequested || !m_array3dImage.isValid() ||
                               m_array3dImageType != m_state->array3d.type || m_array3dWidth != m_state->array3d.width ||
                               m_array3dHeight != m_state->array3d.height || m_array3dDepth != desiredDepth ||
                               m_array3dMipLevels != m_state->array3d.mipLevels || m_array3dLayers != desiredLayers;
    if (!needsRecreate) {
        return true;
    }

    if (m_array3dImage.isValid()) {
        destroy_array3d_resources(device);
    }

    luna::ImageDesc desc{};
    desc.width = m_state->array3d.width;
    desc.height = m_state->array3d.height;
    desc.depth = desiredDepth;
    desc.mipLevels = m_state->array3d.mipLevels;
    desc.arrayLayers = desiredLayers;
    desc.type = m_state->array3d.type;
    desc.format = luna::PixelFormat::RGBA8Unorm;
    desc.usage = luna::ImageUsage::Sampled;
    desc.debugName = "RhiImageLabArray3D";

    std::vector<uint8_t> pixels;
    switch (desc.type) {
        case luna::ImageType::Image2D:
            pixels = build_2d_texture_data(desc.width, desc.height);
            break;
        case luna::ImageType::Image2DArray:
            pixels = build_array_texture_data(desc.width, desc.height, desc.arrayLayers);
            break;
        case luna::ImageType::Image3D:
            pixels = build_volume_texture_data(desc.width, desc.height, desc.depth);
            break;
    }

    if (device.createImage(desc, &m_array3dImage, pixels.data()) != luna::RHIResult::Success) {
        m_state->array3d.status = "Create Image failed.";
        return false;
    }

    m_array3dImageType = desc.type;
    m_array3dWidth = desc.width;
    m_array3dHeight = desc.height;
    m_array3dDepth = desc.depth;
    m_array3dMipLevels = desc.mipLevels;
    m_array3dLayers = desc.arrayLayers;
    m_state->array3d.createRequested = false;

    if (desc.type == luna::ImageType::Image2DArray) {
        m_state->array3d.layer = std::clamp(m_state->array3d.layer, 0, static_cast<int>(desc.arrayLayers - 1));
        m_state->array3d.previewMode = ArrayPreviewMode::Array;
    } else if (desc.type == luna::ImageType::Image3D) {
        m_state->array3d.previewMode = ArrayPreviewMode::Volume;
    }

    std::ostringstream status;
    status << "Created image: type=" << luna::to_string(desc.type) << ", size=" << desc.width << "x" << desc.height;
    if (desc.type == luna::ImageType::Image3D) {
        status << "x" << desc.depth;
    }
    status << ", mipLevels=" << desc.mipLevels << ", arrayLayers=" << desc.arrayLayers;
    m_state->array3d.status = status.str();

    return true;
}

bool RhiImageLabRenderPipeline::render_mrt_preview(luna::IRHIDevice& device, const luna::FrameContext& frameContext)
{
    if (!ensure_mrt_resources(device, frameContext.renderWidth, frameContext.renderHeight, frameContext.backbufferFormat)) {
        return render_placeholder(frameContext, {0.09f, 0.06f, 0.06f, 1.0f});
    }

    luna::RenderingInfo renderingInfo{};
    renderingInfo.width = frameContext.renderWidth;
    renderingInfo.height = frameContext.renderHeight;
    for (int index = 0; index < m_mrtAttachmentCount; ++index) {
        renderingInfo.colorAttachments.push_back(
            {m_mrtImages[static_cast<size_t>(index)], m_mrtFormats[static_cast<size_t>(index)], {0.0f, 0.0f, 0.0f, 1.0f}});
    }

    const bool drawOk =
        frameContext.commandContext->beginRendering(renderingInfo) == luna::RHIResult::Success &&
        frameContext.commandContext->bindGraphicsPipeline(m_mrtPipeline) == luna::RHIResult::Success &&
        frameContext.commandContext->draw({3, 1, 0, 0}) == luna::RHIResult::Success &&
        frameContext.commandContext->endRendering() == luna::RHIResult::Success;
    if (!drawOk) {
        return false;
    }

    std::array<luna::ImageHandle, 4> previewImages{};
    for (int index = 0; index < m_mrtAttachmentCount; ++index) {
        frameContext.commandContext->transitionImage(m_mrtImages[static_cast<size_t>(index)], luna::ImageLayout::ShaderReadOnly);
        previewImages[static_cast<size_t>(index)] = m_mrtImages[static_cast<size_t>(index)];
    }
    for (int index = m_mrtAttachmentCount; index < 4; ++index) {
        previewImages[static_cast<size_t>(index)] = previewImages[0];
    }

    if (!update_four_texture_set(device, frameContext.frameIndex, previewImages)) {
        return false;
    }

    return render_textured_2d_preview(frameContext,
                                      m_state->mrt.showFourUpView ? 1 : 0,
                                      m_state->mrt.previewAttachment,
                                      0.0f,
                                      m_mrtAttachmentCount);
}

bool RhiImageLabRenderPipeline::render_mip_preview(luna::IRHIDevice& device, const luna::FrameContext& frameContext)
{
    if (!ensure_mip_resources(device, frameContext.backbufferFormat)) {
        return render_placeholder(frameContext, {0.05f, 0.08f, 0.08f, 1.0f});
    }

    if (!update_four_texture_set(device, frameContext.frameIndex, fill_image_array(m_mipImage))) {
        return false;
    }

    return render_textured_2d_preview(frameContext, 0, 0, m_state->mip.lod, 1);
}

bool RhiImageLabRenderPipeline::render_array3d_preview(luna::IRHIDevice& device, const luna::FrameContext& frameContext)
{
    if (!ensure_array3d_resources(device, frameContext.backbufferFormat)) {
        return render_placeholder(frameContext, {0.04f, 0.05f, 0.08f, 1.0f});
    }

    switch (m_array3dImageType) {
        case luna::ImageType::Image2D:
            if (!update_four_texture_set(device, frameContext.frameIndex, fill_image_array(m_array3dImage))) {
                return false;
            }
            return render_textured_2d_preview(frameContext, 0, 0, 0.0f, 1);

        case luna::ImageType::Image2DArray: {
            if (!update_array_texture_set(device, frameContext.frameIndex, m_array3dImage)) {
                return false;
            }
            return render_array_preview_pass(
                frameContext, static_cast<float>(std::clamp(m_state->array3d.layer, 0, static_cast<int>(m_array3dLayers - 1))));
        }

        case luna::ImageType::Image3D: {
            if (!update_volume_texture_set(device, frameContext.frameIndex, m_array3dImage)) {
                return false;
            }
            return render_volume_preview_pass(frameContext, m_state->array3d.slice);
        }
    }

    return false;
}

std::vector<uint8_t> RhiImageLabRenderPipeline::build_mip_texture_data(uint32_t width, uint32_t height) const
{
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4, 255);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const float fx = static_cast<float>(x) / static_cast<float>(std::max(1u, width - 1));
            const float fy = static_cast<float>(y) / static_cast<float>(std::max(1u, height - 1));
            const float dx = fx - 0.5f;
            const float dy = fy - 0.5f;
            const float radial = std::sqrt(dx * dx + dy * dy);
            const float rings = 0.5f + 0.5f * std::sin(radial * 90.0f);
            const float checker = ((x / 8u) + (y / 8u)) % 2u == 0 ? 1.0f : 0.12f;
            const size_t offset = (static_cast<size_t>(y) * width + x) * 4;
            write_rgba(pixels, offset, checker, rings, fx * 0.5f + fy * 0.5f, 1.0f);
        }
    }
    return pixels;
}

std::vector<uint8_t> RhiImageLabRenderPipeline::build_2d_texture_data(uint32_t width, uint32_t height) const
{
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4, 255);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const float fx = static_cast<float>(x) / static_cast<float>(std::max(1u, width - 1));
            const float fy = static_cast<float>(y) / static_cast<float>(std::max(1u, height - 1));
            const float checker = ((x / 16u) + (y / 16u)) % 2u == 0 ? 1.0f : 0.2f;
            const size_t offset = (static_cast<size_t>(y) * width + x) * 4;
            write_rgba(pixels, offset, fx, fy, checker, 1.0f);
        }
    }
    return pixels;
}

std::vector<uint8_t> RhiImageLabRenderPipeline::build_array_texture_data(uint32_t width,
                                                                         uint32_t height,
                                                                         uint32_t layers) const
{
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(layers) * 4,
                                255);
    const std::array<std::array<float, 3>, 4> colors = {{
        {0.92f, 0.18f, 0.16f},
        {0.18f, 0.75f, 0.24f},
        {0.12f, 0.36f, 0.94f},
        {0.92f, 0.74f, 0.12f},
    }};

    for (uint32_t layer = 0; layer < layers; ++layer) {
        const auto& color = colors[layer % colors.size()];
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                const float fx = static_cast<float>(x) / static_cast<float>(std::max(1u, width - 1));
                const float fy = static_cast<float>(y) / static_cast<float>(std::max(1u, height - 1));
                const float stripe = std::fmod(fx * 12.0f + static_cast<float>(layer), 1.0f);
                const float accent = stripe > 0.5f ? 1.0f : 0.22f;
                const size_t texelIndex =
                    ((static_cast<size_t>(layer) * height + static_cast<size_t>(y)) * width + static_cast<size_t>(x)) * 4;
                write_rgba(pixels,
                           texelIndex,
                           color[0] * accent,
                           color[1] * (0.45f + 0.55f * fy),
                           color[2] * (0.45f + 0.55f * fx),
                           1.0f);
            }
        }
    }

    return pixels;
}

std::vector<uint8_t> RhiImageLabRenderPipeline::build_volume_texture_data(uint32_t width,
                                                                          uint32_t height,
                                                                          uint32_t depth) const
{
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth) * 4,
                                255);
    for (uint32_t z = 0; z < depth; ++z) {
        const float fz = static_cast<float>(z) / static_cast<float>(std::max(1u, depth - 1));
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                const float fx = static_cast<float>(x) / static_cast<float>(std::max(1u, width - 1));
                const float fy = static_cast<float>(y) / static_cast<float>(std::max(1u, height - 1));
                const float wave = 0.5f + 0.5f * std::sin((fx + fy + fz) * 18.0f);
                const size_t texelIndex =
                    ((static_cast<size_t>(z) * height + static_cast<size_t>(y)) * width + static_cast<size_t>(x)) * 4;
                write_rgba(pixels, texelIndex, fx, fy, fz, 0.55f + 0.45f * wave);
            }
        }
    }
    return pixels;
}

void RhiImageLabRenderPipeline::destroy_shared_resources(luna::IRHIDevice& device)
{
    for (luna::ResourceSetHandle& resourceSet : m_presentTextureSets) {
        if (resourceSet.isValid()) {
            device.destroyResourceSet(resourceSet);
        }
    }
    for (luna::ResourceSetHandle& resourceSet : m_arrayTextureSets) {
        if (resourceSet.isValid()) {
            device.destroyResourceSet(resourceSet);
        }
    }
    for (luna::ResourceSetHandle& resourceSet : m_volumeTextureSets) {
        if (resourceSet.isValid()) {
            device.destroyResourceSet(resourceSet);
        }
    }
    if (m_presentTextureLayout.isValid()) device.destroyResourceLayout(m_presentTextureLayout);
    if (m_arrayTextureLayout.isValid()) device.destroyResourceLayout(m_arrayTextureLayout);
    if (m_volumeTextureLayout.isValid()) device.destroyResourceLayout(m_volumeTextureLayout);
    if (m_linearSampler.isValid()) device.destroySampler(m_linearSampler);

    m_framesInFlight = 0;
    m_presentTextureSets.clear();
    m_arrayTextureSets.clear();
    m_volumeTextureSets.clear();
    m_presentTextureSetImages.clear();
    m_arrayTextureSetImages.clear();
    m_volumeTextureSetImages.clear();
    m_presentTextureLayout = {};
    m_arrayTextureLayout = {};
    m_volumeTextureLayout = {};
    m_linearSampler = {};
}

void RhiImageLabRenderPipeline::destroy_present_pipelines(luna::IRHIDevice& device)
{
    if (m_present2DPipeline.isValid()) device.destroyPipeline(m_present2DPipeline);
    if (m_presentDepthPipeline.isValid()) device.destroyPipeline(m_presentDepthPipeline);
    if (m_presentArrayPipeline.isValid()) device.destroyPipeline(m_presentArrayPipeline);
    if (m_presentVolumePipeline.isValid()) device.destroyPipeline(m_presentVolumePipeline);

    m_present2DPipeline = {};
    m_presentDepthPipeline = {};
    m_presentArrayPipeline = {};
    m_presentVolumePipeline = {};
    m_presentBackbufferFormat = luna::PixelFormat::Undefined;
}

void RhiImageLabRenderPipeline::destroy_attachment_ops_resources(luna::IRHIDevice& device)
{
    if (m_attachmentColorPipeline.isValid()) {
        device.destroyPipeline(m_attachmentColorPipeline);
        m_attachmentColorPipeline = {};
    }
    if (m_attachmentDepthPipeline.isValid()) {
        device.destroyPipeline(m_attachmentDepthPipeline);
        m_attachmentDepthPipeline = {};
    }
    if (m_attachmentColorImage.isValid()) {
        device.destroyImage(m_attachmentColorImage);
        m_attachmentColorImage = {};
    }
    if (m_attachmentDepthImage.isValid()) {
        device.destroyImage(m_attachmentDepthImage);
        m_attachmentDepthImage = {};
    }

    m_attachmentWidth = 0;
    m_attachmentHeight = 0;
}

void RhiImageLabRenderPipeline::destroy_mrt_resources(luna::IRHIDevice& device)
{
    if (m_mrtPipeline.isValid()) {
        device.destroyPipeline(m_mrtPipeline);
        m_mrtPipeline = {};
    }

    for (luna::ImageHandle& image : m_mrtImages) {
        if (image.isValid()) {
            device.destroyImage(image);
            image = {};
        }
    }

    m_mrtWidth = 0;
    m_mrtHeight = 0;
    m_mrtAttachmentCount = 0;
    m_mrtFormats = {};
}

void RhiImageLabRenderPipeline::destroy_mip_resources(luna::IRHIDevice& device)
{
    if (m_mipImage.isValid()) {
        device.destroyImage(m_mipImage);
        m_mipImage = {};
    }

    m_mipWidth = 0;
    m_mipHeight = 0;
    m_mipLevelCount = 0;
}

void RhiImageLabRenderPipeline::destroy_array3d_resources(luna::IRHIDevice& device)
{
    if (m_array3dImage.isValid()) {
        device.destroyImage(m_array3dImage);
        m_array3dImage = {};
    }

    m_array3dImageType = luna::ImageType::Image2D;
    m_array3dWidth = 0;
    m_array3dHeight = 0;
    m_array3dDepth = 0;
    m_array3dMipLevels = 0;
    m_array3dLayers = 0;
}

} // namespace image_lab
