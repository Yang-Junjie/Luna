#include "ImageViewLabPipeline.h"

#include "Core/log.h"
#include "RHI/CommandContext.h"
#include "Vulkan/vk_rhi_device.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <string_view>

namespace image_view_lab {
namespace {

struct alignas(16) PreviewPushConstants {
    float params[4] = {};
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

std::string mip_view_label(const ViewRecord& view)
{
    std::ostringstream builder;
    builder << "Mip View [" << view.desc.baseMipLevel << " .. "
            << (view.desc.baseMipLevel + view.desc.mipCount - 1) << "]";
    return builder.str();
}

std::string array_view_label(const ViewRecord& view)
{
    std::ostringstream builder;
    builder << luna::to_string(view.desc.type) << " [mip " << view.desc.baseMipLevel << " .. "
            << (view.desc.baseMipLevel + view.desc.mipCount - 1) << ", layer " << view.desc.baseArrayLayer << " .. "
            << (view.desc.baseArrayLayer + view.desc.layerCount - 1) << "]";
    return builder.str();
}

std::string volume_view_label(const ViewRecord& view)
{
    std::ostringstream builder;
    builder << "3D View [mip " << view.desc.baseMipLevel << " .. "
            << (view.desc.baseMipLevel + view.desc.mipCount - 1) << "]";
    return builder.str();
}

int clamp_selected_index(int value, size_t count)
{
    if (count == 0) {
        return 0;
    }
    return std::clamp(value, 0, static_cast<int>(count - 1));
}

} // namespace

RhiImageViewLabRenderPipeline::RhiImageViewLabRenderPipeline(std::shared_ptr<State> state)
    : m_state(std::move(state))
{}

bool RhiImageViewLabRenderPipeline::init(luna::IRHIDevice& device)
{
    m_vulkanDevice = dynamic_cast<luna::VulkanRHIDevice*>(&device);
    if (m_vulkanDevice == nullptr) {
        LUNA_CORE_ERROR("RhiImageViewLab requires the Vulkan RHI backend");
        return false;
    }

    m_shaderRoot = std::filesystem::path{RHI_IMAGE_VIEW_LAB_SHADER_ROOT}.lexically_normal().generic_string();
    return true;
}

void RhiImageViewLabRenderPipeline::shutdown(luna::IRHIDevice& device)
{
    destroy_mip_source(device);
    destroy_array_source(device);
    destroy_volume_source(device);
    destroy_present_pipelines(device);
    destroy_shared_resources(device);
    m_vulkanDevice = nullptr;
}

bool RhiImageViewLabRenderPipeline::render(luna::IRHIDevice& device, const luna::FrameContext& frameContext)
{
    if (m_state == nullptr || frameContext.commandContext == nullptr || !frameContext.backbuffer.isValid()) {
        return false;
    }

    if (!ensure_shared_resources(device) || !ensure_present_pipelines(device, frameContext.backbufferFormat)) {
        return false;
    }

    switch (m_state->page) {
        case Page::MipView:
            return render_mip_view(device, frameContext);
        case Page::ArrayLayerView:
            return render_array_layer_view(device, frameContext);
        case Page::Slice3DView:
            return render_slice_3d_view(device, frameContext);
        default:
            return render_placeholder(frameContext, {0.04f, 0.04f, 0.05f, 1.0f});
    }
}

bool RhiImageViewLabRenderPipeline::ensure_shared_resources(luna::IRHIDevice& device)
{
    const uint32_t desiredFramesInFlight = std::max(1u, device.getCapabilities().framesInFlight);
    bool ready = m_linearSampler.isValid() && m_texture2DLayout.isValid() && m_textureArrayLayout.isValid() &&
                 m_texture3DLayout.isValid() && m_framesInFlight == desiredFramesInFlight &&
                 m_texture2DSets.size() == desiredFramesInFlight && m_textureArraySets.size() == desiredFramesInFlight &&
                 m_texture3DSets.size() == desiredFramesInFlight;
    if (ready) {
        for (uint32_t frame = 0; frame < desiredFramesInFlight; ++frame) {
            if (!m_texture2DSets[frame].isValid() || !m_textureArraySets[frame].isValid() ||
                !m_texture3DSets[frame].isValid()) {
                ready = false;
                break;
            }
        }
    }
    if (ready) {
        return true;
    }

    if (m_linearSampler.isValid() || !m_texture2DSets.empty() || !m_textureArraySets.empty() || !m_texture3DSets.empty()) {
        if (device.waitIdle() != luna::RHIResult::Success) {
            return false;
        }
        destroy_shared_resources(device);
    }

    if (!m_linearSampler.isValid()) {
        luna::SamplerDesc samplerDesc{};
        samplerDesc.debugName = "RhiImageViewLabLinearSampler";
        if (device.createSampler(samplerDesc, &m_linearSampler) != luna::RHIResult::Success) {
            return false;
        }
    }

    luna::ResourceLayoutDesc layoutDesc{};
    if (!m_texture2DLayout.isValid()) {
        layoutDesc.debugName = "RhiImageViewLabTexture2DLayout";
        layoutDesc.bindings.push_back({0, luna::ResourceType::CombinedImageSampler, 1, luna::ShaderType::Fragment});
        if (device.createResourceLayout(layoutDesc, &m_texture2DLayout) != luna::RHIResult::Success) {
            return false;
        }
    }

    if (!m_textureArrayLayout.isValid()) {
        layoutDesc = {};
        layoutDesc.debugName = "RhiImageViewLabTextureArrayLayout";
        layoutDesc.bindings.push_back({0, luna::ResourceType::CombinedImageSampler, 1, luna::ShaderType::Fragment});
        if (device.createResourceLayout(layoutDesc, &m_textureArrayLayout) != luna::RHIResult::Success) {
            return false;
        }
    }

    if (!m_texture3DLayout.isValid()) {
        layoutDesc = {};
        layoutDesc.debugName = "RhiImageViewLabTexture3DLayout";
        layoutDesc.bindings.push_back({0, luna::ResourceType::CombinedImageSampler, 1, luna::ShaderType::Fragment});
        if (device.createResourceLayout(layoutDesc, &m_texture3DLayout) != luna::RHIResult::Success) {
            return false;
        }
    }

    m_framesInFlight = desiredFramesInFlight;
    m_texture2DSets.assign(m_framesInFlight, {});
    m_textureArraySets.assign(m_framesInFlight, {});
    m_texture3DSets.assign(m_framesInFlight, {});
    m_bound2DViews.assign(m_framesInFlight, {});
    m_boundArrayViews.assign(m_framesInFlight, {});
    m_bound3DViews.assign(m_framesInFlight, {});

    for (uint32_t frame = 0; frame < m_framesInFlight; ++frame) {
        if (device.createResourceSet(m_texture2DLayout, &m_texture2DSets[frame]) != luna::RHIResult::Success ||
            device.createResourceSet(m_textureArrayLayout, &m_textureArraySets[frame]) != luna::RHIResult::Success ||
            device.createResourceSet(m_texture3DLayout, &m_texture3DSets[frame]) != luna::RHIResult::Success) {
            destroy_shared_resources(device);
            return false;
        }
    }

    return true;
}

bool RhiImageViewLabRenderPipeline::ensure_present_pipelines(luna::IRHIDevice& device, luna::PixelFormat backbufferFormat)
{
    if (backbufferFormat == luna::PixelFormat::Undefined) {
        return false;
    }

    if (m_presentBackbufferFormat == backbufferFormat && m_present2DPipeline.isValid() && m_presentArrayPipeline.isValid() &&
        m_present3DPipeline.isValid()) {
        return true;
    }

    if (m_present2DPipeline.isValid() || m_presentArrayPipeline.isValid() || m_present3DPipeline.isValid()) {
        if (device.waitIdle() != luna::RHIResult::Success) {
            return false;
        }
        destroy_present_pipelines(device);
    }

    const std::string vertexShaderPath = shader_path(m_shaderRoot, "image_view_lab_fullscreen.vert.spv");
    const std::string present2DFragmentShaderPath = shader_path(m_shaderRoot, "image_view_lab_present_2d.frag.spv");
    const std::string presentArrayFragmentShaderPath = shader_path(m_shaderRoot, "image_view_lab_present_array.frag.spv");
    const std::string present3DFragmentShaderPath = shader_path(m_shaderRoot, "image_view_lab_present_3d.frag.spv");

    luna::GraphicsPipelineDesc pipelineDesc{};
    pipelineDesc.vertexShader = {.stage = luna::ShaderType::Vertex, .filePath = vertexShaderPath};
    pipelineDesc.cullMode = luna::CullMode::None;
    pipelineDesc.frontFace = luna::FrontFace::Clockwise;
    pipelineDesc.pushConstantSize = sizeof(PreviewPushConstants);
    pipelineDesc.pushConstantVisibility = luna::ShaderType::Fragment;
    pipelineDesc.colorAttachments.push_back({backbufferFormat, false});

    luna::GraphicsPipelineDesc present2DDesc = pipelineDesc;
    present2DDesc.debugName = "RhiImageViewLabPresent2D";
    present2DDesc.fragmentShader = {.stage = luna::ShaderType::Fragment, .filePath = present2DFragmentShaderPath};
    present2DDesc.resourceLayouts.push_back(m_texture2DLayout);
    if (device.createGraphicsPipeline(present2DDesc, &m_present2DPipeline) != luna::RHIResult::Success) {
        return false;
    }

    luna::GraphicsPipelineDesc presentArrayDesc = pipelineDesc;
    presentArrayDesc.debugName = "RhiImageViewLabPresentArray";
    presentArrayDesc.fragmentShader = {.stage = luna::ShaderType::Fragment, .filePath = presentArrayFragmentShaderPath};
    presentArrayDesc.resourceLayouts.push_back(m_textureArrayLayout);
    if (device.createGraphicsPipeline(presentArrayDesc, &m_presentArrayPipeline) != luna::RHIResult::Success) {
        return false;
    }

    luna::GraphicsPipelineDesc present3DDesc = pipelineDesc;
    present3DDesc.debugName = "RhiImageViewLabPresent3D";
    present3DDesc.fragmentShader = {.stage = luna::ShaderType::Fragment, .filePath = present3DFragmentShaderPath};
    present3DDesc.resourceLayouts.push_back(m_texture3DLayout);
    if (device.createGraphicsPipeline(present3DDesc, &m_present3DPipeline) != luna::RHIResult::Success) {
        return false;
    }

    m_presentBackbufferFormat = backbufferFormat;
    return true;
}

bool RhiImageViewLabRenderPipeline::update_2d_texture_set(luna::IRHIDevice& device,
                                                          uint32_t frameIndex,
                                                          luna::ImageViewHandle view)
{
    if (m_texture2DSets.empty() || !view.isValid()) {
        return false;
    }

    const size_t slot = frame_slot(frameIndex, m_texture2DSets.size());
    if (m_bound2DViews[slot] == view) {
        return true;
    }

    luna::ResourceSetWriteDesc writeDesc{};
    writeDesc.images.push_back(
        {.binding = 0, .imageView = view, .sampler = m_linearSampler, .type = luna::ResourceType::CombinedImageSampler});
    if (device.updateResourceSet(m_texture2DSets[slot], writeDesc) != luna::RHIResult::Success) {
        return false;
    }

    m_bound2DViews[slot] = view;
    return true;
}

bool RhiImageViewLabRenderPipeline::update_array_texture_set(luna::IRHIDevice& device,
                                                             uint32_t frameIndex,
                                                             luna::ImageViewHandle view)
{
    if (m_textureArraySets.empty() || !view.isValid()) {
        return false;
    }

    const size_t slot = frame_slot(frameIndex, m_textureArraySets.size());
    if (m_boundArrayViews[slot] == view) {
        return true;
    }

    luna::ResourceSetWriteDesc writeDesc{};
    writeDesc.images.push_back(
        {.binding = 0, .imageView = view, .sampler = m_linearSampler, .type = luna::ResourceType::CombinedImageSampler});
    if (device.updateResourceSet(m_textureArraySets[slot], writeDesc) != luna::RHIResult::Success) {
        return false;
    }

    m_boundArrayViews[slot] = view;
    return true;
}

bool RhiImageViewLabRenderPipeline::update_volume_texture_set(luna::IRHIDevice& device,
                                                              uint32_t frameIndex,
                                                              luna::ImageViewHandle view)
{
    if (m_texture3DSets.empty() || !view.isValid()) {
        return false;
    }

    const size_t slot = frame_slot(frameIndex, m_texture3DSets.size());
    if (m_bound3DViews[slot] == view) {
        return true;
    }

    luna::ResourceSetWriteDesc writeDesc{};
    writeDesc.images.push_back(
        {.binding = 0, .imageView = view, .sampler = m_linearSampler, .type = luna::ResourceType::CombinedImageSampler});
    if (device.updateResourceSet(m_texture3DSets[slot], writeDesc) != luna::RHIResult::Success) {
        return false;
    }

    m_bound3DViews[slot] = view;
    return true;
}

bool RhiImageViewLabRenderPipeline::render_textured_2d_preview(const luna::FrameContext& frameContext, float lod)
{
    PreviewPushConstants pushConstants{};
    pushConstants.params[0] = lod;

    return frameContext.commandContext->beginRendering({.width = frameContext.renderWidth,
                                                        .height = frameContext.renderHeight,
                                                        .colorAttachments = {{frameContext.backbuffer,
                                                                              frameContext.backbufferFormat,
                                                                              {0.03f, 0.03f, 0.035f, 1.0f}}}}) ==
               luna::RHIResult::Success &&
           frameContext.commandContext->bindGraphicsPipeline(m_present2DPipeline) == luna::RHIResult::Success &&
           frameContext.commandContext->bindResourceSet(
               m_texture2DSets[frame_slot(frameContext.frameIndex, m_texture2DSets.size())]) == luna::RHIResult::Success &&
           frameContext.commandContext->pushConstants(
               &pushConstants, sizeof(pushConstants), 0, luna::ShaderType::Fragment) == luna::RHIResult::Success &&
           frameContext.commandContext->draw({3, 1, 0, 0}) == luna::RHIResult::Success &&
           frameContext.commandContext->endRendering() == luna::RHIResult::Success;
}

bool RhiImageViewLabRenderPipeline::render_array_preview(const luna::FrameContext& frameContext, float layer, float lod)
{
    PreviewPushConstants pushConstants{};
    pushConstants.params[0] = layer;
    pushConstants.params[1] = lod;

    return frameContext.commandContext->beginRendering({.width = frameContext.renderWidth,
                                                        .height = frameContext.renderHeight,
                                                        .colorAttachments = {{frameContext.backbuffer,
                                                                              frameContext.backbufferFormat,
                                                                              {0.03f, 0.04f, 0.06f, 1.0f}}}}) ==
               luna::RHIResult::Success &&
           frameContext.commandContext->bindGraphicsPipeline(m_presentArrayPipeline) == luna::RHIResult::Success &&
           frameContext.commandContext->bindResourceSet(
               m_textureArraySets[frame_slot(frameContext.frameIndex, m_textureArraySets.size())]) ==
               luna::RHIResult::Success &&
           frameContext.commandContext->pushConstants(
               &pushConstants, sizeof(pushConstants), 0, luna::ShaderType::Fragment) == luna::RHIResult::Success &&
           frameContext.commandContext->draw({3, 1, 0, 0}) == luna::RHIResult::Success &&
           frameContext.commandContext->endRendering() == luna::RHIResult::Success;
}

bool RhiImageViewLabRenderPipeline::render_volume_preview(const luna::FrameContext& frameContext, float slice, float lod)
{
    PreviewPushConstants pushConstants{};
    pushConstants.params[0] = slice;
    pushConstants.params[1] = lod;

    return frameContext.commandContext->beginRendering({.width = frameContext.renderWidth,
                                                        .height = frameContext.renderHeight,
                                                        .colorAttachments = {{frameContext.backbuffer,
                                                                              frameContext.backbufferFormat,
                                                                              {0.02f, 0.03f, 0.04f, 1.0f}}}}) ==
               luna::RHIResult::Success &&
           frameContext.commandContext->bindGraphicsPipeline(m_present3DPipeline) == luna::RHIResult::Success &&
           frameContext.commandContext->bindResourceSet(
               m_texture3DSets[frame_slot(frameContext.frameIndex, m_texture3DSets.size())]) == luna::RHIResult::Success &&
           frameContext.commandContext->pushConstants(
               &pushConstants, sizeof(pushConstants), 0, luna::ShaderType::Fragment) == luna::RHIResult::Success &&
           frameContext.commandContext->draw({3, 1, 0, 0}) == luna::RHIResult::Success &&
           frameContext.commandContext->endRendering() == luna::RHIResult::Success;
}

bool RhiImageViewLabRenderPipeline::render_placeholder(const luna::FrameContext& frameContext,
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

bool RhiImageViewLabRenderPipeline::create_mip_view_record(luna::IRHIDevice& device,
                                                           uint32_t baseMip,
                                                           uint32_t mipCount,
                                                           bool selectNewView)
{
    luna::ImageViewDesc desc{};
    desc.image = m_mipImage;
    desc.type = luna::ImageViewType::Image2D;
    desc.aspect = luna::ImageAspect::Color;
    desc.baseMipLevel = baseMip;
    desc.mipCount = mipCount;
    desc.baseArrayLayer = 0;
    desc.layerCount = 1;
    desc.debugName = "RhiImageViewLabMipView";

    luna::ImageViewHandle view{};
    if (device.createImageView(desc, &view) != luna::RHIResult::Success) {
        m_state->mip.status = "Create View failed.";
        return false;
    }

    ViewRecord record{};
    record.handle = view;
    record.desc = desc;
    record.label = mip_view_label(record);
    m_state->mip.views.push_back(record);
    if (selectNewView) {
        m_state->mip.selectedView = static_cast<int>(m_state->mip.views.size() - 1);
    }

    std::ostringstream status;
    status << "Created view: mip " << baseMip << " .. " << (baseMip + mipCount - 1);
    m_state->mip.status = status.str();
    return true;
}

bool RhiImageViewLabRenderPipeline::create_array_view_record(luna::IRHIDevice& device,
                                                             luna::ImageViewType type,
                                                             uint32_t baseMip,
                                                             uint32_t mipCount,
                                                             uint32_t baseLayer,
                                                             uint32_t layerCount,
                                                             bool selectNewView)
{
    luna::ImageViewDesc desc{};
    desc.image = m_arrayImage;
    desc.type = type;
    desc.aspect = luna::ImageAspect::Color;
    desc.baseMipLevel = baseMip;
    desc.mipCount = mipCount;
    desc.baseArrayLayer = baseLayer;
    desc.layerCount = layerCount;
    desc.debugName = "RhiImageViewLabArrayView";

    luna::ImageViewHandle view{};
    if (device.createImageView(desc, &view) != luna::RHIResult::Success) {
        m_state->array.status = "Create View failed.";
        return false;
    }

    ViewRecord record{};
    record.handle = view;
    record.desc = desc;
    record.label = array_view_label(record);
    m_state->array.views.push_back(record);
    if (selectNewView) {
        m_state->array.selectedView = static_cast<int>(m_state->array.views.size() - 1);
    }

    std::ostringstream status;
    status << "Created " << luna::to_string(type) << " view: mip " << baseMip << " .. " << (baseMip + mipCount - 1)
           << ", layer " << baseLayer << " .. " << (baseLayer + layerCount - 1);
    m_state->array.status = status.str();
    return true;
}

bool RhiImageViewLabRenderPipeline::create_volume_view_record(luna::IRHIDevice& device,
                                                              uint32_t baseMip,
                                                              uint32_t mipCount,
                                                              bool selectNewView)
{
    luna::ImageViewDesc desc{};
    desc.image = m_volumeImage;
    desc.type = luna::ImageViewType::Image3D;
    desc.aspect = luna::ImageAspect::Color;
    desc.baseMipLevel = baseMip;
    desc.mipCount = mipCount;
    desc.baseArrayLayer = 0;
    desc.layerCount = 1;
    desc.debugName = "RhiImageViewLabVolumeView";

    luna::ImageViewHandle view{};
    if (device.createImageView(desc, &view) != luna::RHIResult::Success) {
        m_state->volume.status = "Create View failed.";
        return false;
    }

    ViewRecord record{};
    record.handle = view;
    record.desc = desc;
    record.label = volume_view_label(record);
    m_state->volume.views.push_back(record);
    if (selectNewView) {
        m_state->volume.selectedView = static_cast<int>(m_state->volume.views.size() - 1);
    }

    std::ostringstream status;
    status << "Created 3D view: mip " << baseMip << " .. " << (baseMip + mipCount - 1);
    m_state->volume.status = status.str();
    return true;
}

void RhiImageViewLabRenderPipeline::destroy_view_records(luna::IRHIDevice& device, std::vector<ViewRecord>& views)
{
    if (!views.empty()) {
        device.waitIdle();
    }

    for (ViewRecord& view : views) {
        if (view.handle.isValid()) {
            device.destroyImageView(view.handle);
            view.handle = {};
        }
    }
    views.clear();
    std::fill(m_bound2DViews.begin(), m_bound2DViews.end(), luna::ImageViewHandle{});
    std::fill(m_boundArrayViews.begin(), m_boundArrayViews.end(), luna::ImageViewHandle{});
    std::fill(m_bound3DViews.begin(), m_bound3DViews.end(), luna::ImageViewHandle{});
}

void RhiImageViewLabRenderPipeline::erase_view_record(luna::IRHIDevice& device,
                                                      std::vector<ViewRecord>& views,
                                                      int index,
                                                      int* selectedIndex)
{
    if (selectedIndex == nullptr || index < 0 || index >= static_cast<int>(views.size())) {
        return;
    }

    device.waitIdle();
    device.destroyImageView(views[static_cast<size_t>(index)].handle);
    views.erase(views.begin() + index);
    *selectedIndex = views.empty() ? 0 : std::clamp(*selectedIndex, 0, static_cast<int>(views.size() - 1));
    std::fill(m_bound2DViews.begin(), m_bound2DViews.end(), luna::ImageViewHandle{});
    std::fill(m_boundArrayViews.begin(), m_boundArrayViews.end(), luna::ImageViewHandle{});
    std::fill(m_bound3DViews.begin(), m_bound3DViews.end(), luna::ImageViewHandle{});
}

bool RhiImageViewLabRenderPipeline::ensure_mip_source(luna::IRHIDevice& device)
{
    auto& mip = m_state->mip;
    mip.mipLevels = calculate_theoretical_mip_count(mip.width, mip.height);

    if (mip.recreateImageRequested || !m_mipImage.isValid()) {
        destroy_mip_source(device);

        const std::vector<uint8_t> pixels = build_mip_texture_data(mip.width, mip.height);
        luna::ImageDesc desc{};
        desc.width = mip.width;
        desc.height = mip.height;
        desc.depth = 1;
        desc.mipLevels = mip.mipLevels;
        desc.arrayLayers = 1;
        desc.type = luna::ImageType::Image2D;
        desc.format = luna::PixelFormat::RGBA8Unorm;
        desc.usage = luna::ImageUsage::Sampled;
        desc.debugName = "RhiImageViewLabMipSource";
        if (device.createImage(desc, &m_mipImage, pixels.data()) != luna::RHIResult::Success) {
            mip.status = "Create source image failed.";
            return false;
        }

        mip.recreateImageRequested = false;
        mip.previewLod = 0.0f;
        if (!create_mip_view_record(device, 0, mip.mipLevels, true)) {
            return false;
        }
    }

    if (mip.createViewRequested) {
        mip.createViewRequested = false;
        if (!create_mip_view_record(device, mip.createBaseMip, mip.createMipCount, true)) {
            return false;
        }
    }

    if (mip.deleteView >= 0) {
        erase_view_record(device, mip.views, mip.deleteView, &mip.selectedView);
        mip.deleteView = -1;
        mip.status = "Deleted selected view.";
    }

    mip.selectedView = clamp_selected_index(mip.selectedView, mip.views.size());
    return true;
}

bool RhiImageViewLabRenderPipeline::ensure_array_source(luna::IRHIDevice& device)
{
    auto& array = m_state->array;
    array.mipLevels = calculate_theoretical_mip_count(array.width, array.height);

    if (array.recreateImageRequested || !m_arrayImage.isValid()) {
        destroy_array_source(device);

        const std::vector<uint8_t> pixels = build_array_texture_data(array.width, array.height, array.arrayLayers);
        luna::ImageDesc desc{};
        desc.width = array.width;
        desc.height = array.height;
        desc.depth = 1;
        desc.mipLevels = array.mipLevels;
        desc.arrayLayers = array.arrayLayers;
        desc.type = luna::ImageType::Image2DArray;
        desc.format = luna::PixelFormat::RGBA8Unorm;
        desc.usage = luna::ImageUsage::Sampled;
        desc.debugName = "RhiImageViewLabArraySource";
        if (device.createImage(desc, &m_arrayImage, pixels.data()) != luna::RHIResult::Success) {
            array.status = "Create source image failed.";
            return false;
        }

        array.recreateImageRequested = false;
        array.previewLayer = 0;
        array.previewLod = 0.0f;
        if (!create_array_view_record(device, luna::ImageViewType::Image2DArray, 0, array.mipLevels, 0, array.arrayLayers, true)) {
            return false;
        }
    }

    if (array.createViewRequested) {
        array.createViewRequested = false;
        if (!create_array_view_record(device,
                                      array.createType,
                                      array.createBaseMip,
                                      array.createMipCount,
                                      array.createBaseLayer,
                                      array.createLayerCount,
                                      true)) {
            return false;
        }
    }

    if (array.deleteView >= 0) {
        erase_view_record(device, array.views, array.deleteView, &array.selectedView);
        array.deleteView = -1;
        array.status = "Deleted selected view.";
    }

    array.selectedView = clamp_selected_index(array.selectedView, array.views.size());
    return true;
}

bool RhiImageViewLabRenderPipeline::ensure_volume_source(luna::IRHIDevice& device)
{
    auto& volume = m_state->volume;
    volume.mipLevels = calculate_theoretical_mip_count(volume.width, volume.height, volume.depth);

    if (volume.recreateImageRequested || !m_volumeImage.isValid()) {
        destroy_volume_source(device);

        const std::vector<uint8_t> pixels = build_volume_texture_data(volume.width, volume.height, volume.depth);
        luna::ImageDesc desc{};
        desc.width = volume.width;
        desc.height = volume.height;
        desc.depth = volume.depth;
        desc.mipLevels = volume.mipLevels;
        desc.arrayLayers = 1;
        desc.type = luna::ImageType::Image3D;
        desc.format = luna::PixelFormat::RGBA8Unorm;
        desc.usage = luna::ImageUsage::Sampled;
        desc.debugName = "RhiImageViewLabVolumeSource";
        if (device.createImage(desc, &m_volumeImage, pixels.data()) != luna::RHIResult::Success) {
            volume.status = "Create source image failed.";
            return false;
        }

        volume.recreateImageRequested = false;
        volume.previewSlice = 0;
        volume.previewLod = 0.0f;
        if (!create_volume_view_record(device, 0, volume.mipLevels, true)) {
            return false;
        }
    }

    if (volume.createViewRequested) {
        volume.createViewRequested = false;
        if (!create_volume_view_record(device, volume.createBaseMip, volume.createMipCount, true)) {
            return false;
        }
    }

    if (volume.deleteView >= 0) {
        erase_view_record(device, volume.views, volume.deleteView, &volume.selectedView);
        volume.deleteView = -1;
        volume.status = "Deleted selected view.";
    }

    volume.selectedView = clamp_selected_index(volume.selectedView, volume.views.size());
    return true;
}

bool RhiImageViewLabRenderPipeline::render_mip_view(luna::IRHIDevice& device, const luna::FrameContext& frameContext)
{
    if (!ensure_mip_source(device) || m_state->mip.views.empty()) {
        return render_placeholder(frameContext, {0.05f, 0.05f, 0.06f, 1.0f});
    }

    auto& mip = m_state->mip;
    const ViewRecord& view = mip.views[static_cast<size_t>(mip.selectedView)];
    mip.previewLod = std::clamp(mip.previewLod, 0.0f, static_cast<float>(std::max(1u, view.desc.mipCount) - 1u));

    if (!update_2d_texture_set(device, frameContext.frameIndex, view.handle)) {
        return false;
    }

    return render_textured_2d_preview(frameContext, mip.previewLod);
}

bool RhiImageViewLabRenderPipeline::render_array_layer_view(luna::IRHIDevice& device,
                                                            const luna::FrameContext& frameContext)
{
    if (!ensure_array_source(device) || m_state->array.views.empty()) {
        return render_placeholder(frameContext, {0.04f, 0.05f, 0.07f, 1.0f});
    }

    auto& array = m_state->array;
    const ViewRecord& view = array.views[static_cast<size_t>(array.selectedView)];
    array.previewLod = std::clamp(array.previewLod, 0.0f, static_cast<float>(std::max(1u, view.desc.mipCount) - 1u));

    if (view.desc.type == luna::ImageViewType::Image2D) {
        array.previewLayer = 0;
        if (!update_2d_texture_set(device, frameContext.frameIndex, view.handle)) {
            return false;
        }
        return render_textured_2d_preview(frameContext, array.previewLod);
    }

    array.previewLayer = std::clamp(array.previewLayer, 0, static_cast<int>(std::max(1u, view.desc.layerCount) - 1u));
    if (!update_array_texture_set(device, frameContext.frameIndex, view.handle)) {
        return false;
    }
    return render_array_preview(frameContext, static_cast<float>(array.previewLayer), array.previewLod);
}

bool RhiImageViewLabRenderPipeline::render_slice_3d_view(luna::IRHIDevice& device, const luna::FrameContext& frameContext)
{
    if (!ensure_volume_source(device) || m_state->volume.views.empty()) {
        return render_placeholder(frameContext, {0.03f, 0.04f, 0.06f, 1.0f});
    }

    auto& volume = m_state->volume;
    const ViewRecord& view = volume.views[static_cast<size_t>(volume.selectedView)];
    volume.previewLod = std::clamp(volume.previewLod, 0.0f, static_cast<float>(std::max(1u, view.desc.mipCount) - 1u));
    volume.previewSlice = std::clamp(volume.previewSlice, 0, static_cast<int>(std::max(1u, volume.depth) - 1u));

    if (!update_volume_texture_set(device, frameContext.frameIndex, view.handle)) {
        return false;
    }

    const float slice = volume.depth > 1 ? static_cast<float>(volume.previewSlice) / static_cast<float>(volume.depth - 1) : 0.0f;
    return render_volume_preview(frameContext, slice, volume.previewLod);
}

void RhiImageViewLabRenderPipeline::destroy_shared_resources(luna::IRHIDevice& device)
{
    for (luna::ResourceSetHandle& set : m_texture2DSets) {
        if (set.isValid()) {
            device.destroyResourceSet(set);
        }
    }
    for (luna::ResourceSetHandle& set : m_textureArraySets) {
        if (set.isValid()) {
            device.destroyResourceSet(set);
        }
    }
    for (luna::ResourceSetHandle& set : m_texture3DSets) {
        if (set.isValid()) {
            device.destroyResourceSet(set);
        }
    }
    if (m_texture2DLayout.isValid()) device.destroyResourceLayout(m_texture2DLayout);
    if (m_textureArrayLayout.isValid()) device.destroyResourceLayout(m_textureArrayLayout);
    if (m_texture3DLayout.isValid()) device.destroyResourceLayout(m_texture3DLayout);
    if (m_linearSampler.isValid()) device.destroySampler(m_linearSampler);

    m_framesInFlight = 0;
    m_texture2DSets.clear();
    m_textureArraySets.clear();
    m_texture3DSets.clear();
    m_bound2DViews.clear();
    m_boundArrayViews.clear();
    m_bound3DViews.clear();
    m_texture2DLayout = {};
    m_textureArrayLayout = {};
    m_texture3DLayout = {};
    m_linearSampler = {};
}

void RhiImageViewLabRenderPipeline::destroy_present_pipelines(luna::IRHIDevice& device)
{
    if (m_present2DPipeline.isValid()) device.destroyPipeline(m_present2DPipeline);
    if (m_presentArrayPipeline.isValid()) device.destroyPipeline(m_presentArrayPipeline);
    if (m_present3DPipeline.isValid()) device.destroyPipeline(m_present3DPipeline);

    m_present2DPipeline = {};
    m_presentArrayPipeline = {};
    m_present3DPipeline = {};
    m_presentBackbufferFormat = luna::PixelFormat::Undefined;
}

void RhiImageViewLabRenderPipeline::destroy_mip_source(luna::IRHIDevice& device)
{
    destroy_view_records(device, m_state->mip.views);
    if (m_mipImage.isValid()) {
        device.destroyImage(m_mipImage);
        m_mipImage = {};
    }
    m_state->mip.selectedView = 0;
}

void RhiImageViewLabRenderPipeline::destroy_array_source(luna::IRHIDevice& device)
{
    destroy_view_records(device, m_state->array.views);
    if (m_arrayImage.isValid()) {
        device.destroyImage(m_arrayImage);
        m_arrayImage = {};
    }
    m_state->array.selectedView = 0;
}

void RhiImageViewLabRenderPipeline::destroy_volume_source(luna::IRHIDevice& device)
{
    destroy_view_records(device, m_state->volume.views);
    if (m_volumeImage.isValid()) {
        device.destroyImage(m_volumeImage);
        m_volumeImage = {};
    }
    m_state->volume.selectedView = 0;
}

std::vector<uint8_t> RhiImageViewLabRenderPipeline::build_mip_texture_data(uint32_t width, uint32_t height) const
{
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4, 255);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const float fx = static_cast<float>(x) / static_cast<float>(std::max(1u, width - 1));
            const float fy = static_cast<float>(y) / static_cast<float>(std::max(1u, height - 1));
            const float rings = 0.5f + 0.5f * std::sin(std::sqrt((fx - 0.5f) * (fx - 0.5f) + (fy - 0.5f) * (fy - 0.5f)) * 96.0f);
            const float checker = ((x / 8u) + (y / 8u)) % 2u == 0 ? 1.0f : 0.12f;
            const size_t offset = (static_cast<size_t>(y) * width + x) * 4;
            write_rgba(pixels, offset, checker, rings, fx * 0.6f + fy * 0.4f, 1.0f);
        }
    }
    return pixels;
}

std::vector<uint8_t> RhiImageViewLabRenderPipeline::build_array_texture_data(uint32_t width,
                                                                              uint32_t height,
                                                                              uint32_t layers) const
{
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(layers) * 4,
                                255);
    const std::array<std::array<float, 3>, 4> colors = {{
        {0.90f, 0.18f, 0.16f},
        {0.18f, 0.78f, 0.25f},
        {0.16f, 0.34f, 0.92f},
        {0.92f, 0.78f, 0.14f},
    }};

    for (uint32_t layer = 0; layer < layers; ++layer) {
        const auto& color = colors[layer % colors.size()];
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                const float fx = static_cast<float>(x) / static_cast<float>(std::max(1u, width - 1));
                const float fy = static_cast<float>(y) / static_cast<float>(std::max(1u, height - 1));
                const float stripe = std::fmod(static_cast<float>(x + y + layer * 13u) * 0.12f, 1.0f);
                const size_t offset =
                    ((static_cast<size_t>(layer) * height + static_cast<size_t>(y)) * width + static_cast<size_t>(x)) * 4;
                write_rgba(pixels, offset, color[0] * (0.45f + 0.55f * fx), color[1] * (0.45f + 0.55f * fy), color[2] * stripe, 1.0f);
            }
        }
    }
    return pixels;
}

std::vector<uint8_t> RhiImageViewLabRenderPipeline::build_volume_texture_data(uint32_t width,
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
                const float wave = 0.5f + 0.5f * std::sin((fx * 9.0f + fy * 5.0f + fz * 11.0f) * 3.1415926f);
                const size_t texelIndex =
                    ((static_cast<size_t>(z) * height + static_cast<size_t>(y)) * width + static_cast<size_t>(x)) * 4;
                write_rgba(pixels, texelIndex, fx, fy, fz, wave);
            }
        }
    }
    return pixels;
}

} // namespace image_view_lab
