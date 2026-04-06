#pragma once

#include "ImageLabState.h"
#include "Renderer/RenderPipeline.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace image_lab {

class RhiImageLabRenderPipeline final : public luna::IRenderPipeline {
public:
    explicit RhiImageLabRenderPipeline(std::shared_ptr<State> state);

    bool init(luna::IRHIDevice& device) override;
    void shutdown(luna::IRHIDevice& device) override;
    bool render(luna::IRHIDevice& device, const luna::FrameContext& frameContext) override;

private:
    bool ensure_shared_resources(luna::IRHIDevice& device);
    bool ensure_present_pipelines(luna::IRHIDevice& device, luna::PixelFormat backbufferFormat);
    bool ensure_attachment_ops_resources(luna::IRHIDevice& device,
                                         uint32_t width,
                                         uint32_t height,
                                         luna::PixelFormat backbufferFormat);
    bool ensure_mrt_resources(luna::IRHIDevice& device,
                              uint32_t width,
                              uint32_t height,
                              luna::PixelFormat backbufferFormat);
    bool ensure_mip_resources(luna::IRHIDevice& device, luna::PixelFormat backbufferFormat);
    bool ensure_array3d_resources(luna::IRHIDevice& device, luna::PixelFormat backbufferFormat);

    bool render_format_probe(luna::IRHIDevice& device, const luna::FrameContext& frameContext);
    bool render_attachment_ops(luna::IRHIDevice& device, const luna::FrameContext& frameContext);
    bool render_mrt_preview(luna::IRHIDevice& device, const luna::FrameContext& frameContext);
    bool render_mip_preview(luna::IRHIDevice& device, const luna::FrameContext& frameContext);
    bool render_array3d_preview(luna::IRHIDevice& device, const luna::FrameContext& frameContext);

    void update_format_probe(luna::IRHIDevice& device);
    bool update_four_texture_set(luna::IRHIDevice& device,
                                 uint32_t frameIndex,
                                 const std::array<luna::ImageHandle, 4>& images);
    bool update_array_texture_set(luna::IRHIDevice& device, uint32_t frameIndex, luna::ImageHandle image);
    bool update_volume_texture_set(luna::IRHIDevice& device, uint32_t frameIndex, luna::ImageHandle image);
    bool render_textured_2d_preview(const luna::FrameContext& frameContext,
                                    int mode,
                                    int previewIndex,
                                    float lod,
                                    int activeTextureCount);
    bool render_depth_preview(const luna::FrameContext& frameContext);
    bool render_array_preview_pass(const luna::FrameContext& frameContext, float layer);
    bool render_volume_preview_pass(const luna::FrameContext& frameContext, float slice);
    bool render_placeholder(const luna::FrameContext& frameContext, const std::array<float, 4>& clearColor);

    void destroy_shared_resources(luna::IRHIDevice& device);
    void destroy_present_pipelines(luna::IRHIDevice& device);
    void destroy_attachment_ops_resources(luna::IRHIDevice& device);
    void destroy_mrt_resources(luna::IRHIDevice& device);
    void destroy_mip_resources(luna::IRHIDevice& device);
    void destroy_array3d_resources(luna::IRHIDevice& device);

    std::vector<uint8_t> build_mip_texture_data(uint32_t width, uint32_t height) const;
    std::vector<uint8_t> build_2d_texture_data(uint32_t width, uint32_t height) const;
    std::vector<uint8_t> build_array_texture_data(uint32_t width, uint32_t height, uint32_t layers) const;
    std::vector<uint8_t> build_volume_texture_data(uint32_t width, uint32_t height, uint32_t depth) const;

private:
    std::shared_ptr<State> m_state;
    std::string m_shaderRoot;
    uint32_t m_framesInFlight = 0;

    luna::SamplerHandle m_linearSampler{};
    luna::ResourceLayoutHandle m_presentTextureLayout{};
    luna::ResourceLayoutHandle m_arrayTextureLayout{};
    luna::ResourceLayoutHandle m_volumeTextureLayout{};
    std::vector<luna::ResourceSetHandle> m_presentTextureSets;
    std::vector<luna::ResourceSetHandle> m_arrayTextureSets;
    std::vector<luna::ResourceSetHandle> m_volumeTextureSets;
    std::vector<std::array<luna::ImageHandle, 4>> m_presentTextureSetImages;
    std::vector<luna::ImageHandle> m_arrayTextureSetImages;
    std::vector<luna::ImageHandle> m_volumeTextureSetImages;

    luna::PipelineHandle m_present2DPipeline{};
    luna::PipelineHandle m_presentDepthPipeline{};
    luna::PipelineHandle m_presentArrayPipeline{};
    luna::PipelineHandle m_presentVolumePipeline{};
    luna::PixelFormat m_presentBackbufferFormat = luna::PixelFormat::Undefined;

    luna::PipelineHandle m_attachmentColorPipeline{};
    luna::PipelineHandle m_attachmentDepthPipeline{};
    luna::ImageHandle m_attachmentColorImage{};
    luna::ImageHandle m_attachmentDepthImage{};
    uint32_t m_attachmentWidth = 0;
    uint32_t m_attachmentHeight = 0;

    luna::PipelineHandle m_mrtPipeline{};
    std::array<luna::ImageHandle, 4> m_mrtImages{};
    std::array<luna::PixelFormat, 4> m_mrtFormats{};
    uint32_t m_mrtWidth = 0;
    uint32_t m_mrtHeight = 0;
    int m_mrtAttachmentCount = 0;

    luna::ImageHandle m_mipImage{};
    uint32_t m_mipWidth = 0;
    uint32_t m_mipHeight = 0;
    uint32_t m_mipLevelCount = 0;

    luna::ImageHandle m_array3dImage{};
    luna::ImageType m_array3dImageType = luna::ImageType::Image2D;
    uint32_t m_array3dWidth = 0;
    uint32_t m_array3dHeight = 0;
    uint32_t m_array3dDepth = 0;
    uint32_t m_array3dMipLevels = 0;
    uint32_t m_array3dLayers = 0;
};

} // namespace image_lab
