#pragma once

#include "ImageViewLabState.h"
#include "Renderer/RenderPipeline.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace luna {
class VulkanRHIDevice;
}

namespace image_view_lab {

class RhiImageViewLabRenderPipeline final : public luna::IRenderPipeline {
public:
    explicit RhiImageViewLabRenderPipeline(std::shared_ptr<State> state);

    bool init(luna::IRHIDevice& device) override;
    void shutdown(luna::IRHIDevice& device) override;
    bool render(luna::IRHIDevice& device, const luna::FrameContext& frameContext) override;

private:
    bool ensure_shared_resources(luna::IRHIDevice& device);
    bool ensure_present_pipelines(luna::IRHIDevice& device, luna::PixelFormat backbufferFormat);

    bool ensure_mip_source(luna::IRHIDevice& device);
    bool ensure_array_source(luna::IRHIDevice& device);
    bool ensure_volume_source(luna::IRHIDevice& device);

    bool render_mip_view(luna::IRHIDevice& device, const luna::FrameContext& frameContext);
    bool render_array_layer_view(luna::IRHIDevice& device, const luna::FrameContext& frameContext);
    bool render_slice_3d_view(luna::IRHIDevice& device, const luna::FrameContext& frameContext);
    bool render_textured_2d_preview(const luna::FrameContext& frameContext, float lod);
    bool render_array_preview(const luna::FrameContext& frameContext, float layer, float lod);
    bool render_volume_preview(const luna::FrameContext& frameContext, float slice, float lod);
    bool render_placeholder(const luna::FrameContext& frameContext, const std::array<float, 4>& clearColor);

    bool update_2d_texture_set(luna::IRHIDevice& device, uint32_t frameIndex, luna::ImageViewHandle view);
    bool update_array_texture_set(luna::IRHIDevice& device, uint32_t frameIndex, luna::ImageViewHandle view);
    bool update_volume_texture_set(luna::IRHIDevice& device, uint32_t frameIndex, luna::ImageViewHandle view);

    bool create_mip_view_record(luna::IRHIDevice& device, uint32_t baseMip, uint32_t mipCount, bool selectNewView);
    bool create_array_view_record(luna::IRHIDevice& device,
                                  luna::ImageViewType type,
                                  uint32_t baseMip,
                                  uint32_t mipCount,
                                  uint32_t baseLayer,
                                  uint32_t layerCount,
                                  bool selectNewView);
    bool create_volume_view_record(luna::IRHIDevice& device, uint32_t baseMip, uint32_t mipCount, bool selectNewView);

    void destroy_view_records(luna::IRHIDevice& device, std::vector<ViewRecord>& views);
    void erase_view_record(luna::IRHIDevice& device, std::vector<ViewRecord>& views, int index, int* selectedIndex);
    void destroy_shared_resources(luna::IRHIDevice& device);
    void destroy_present_pipelines(luna::IRHIDevice& device);
    void destroy_mip_source(luna::IRHIDevice& device);
    void destroy_array_source(luna::IRHIDevice& device);
    void destroy_volume_source(luna::IRHIDevice& device);

    std::vector<uint8_t> build_mip_texture_data(uint32_t width, uint32_t height) const;
    std::vector<uint8_t> build_array_texture_data(uint32_t width, uint32_t height, uint32_t layers) const;
    std::vector<uint8_t> build_volume_texture_data(uint32_t width, uint32_t height, uint32_t depth) const;

private:
    std::shared_ptr<State> m_state;
    luna::VulkanRHIDevice* m_vulkanDevice = nullptr;
    std::string m_shaderRoot;
    uint32_t m_framesInFlight = 0;

    luna::SamplerHandle m_linearSampler{};
    luna::ResourceLayoutHandle m_texture2DLayout{};
    luna::ResourceLayoutHandle m_textureArrayLayout{};
    luna::ResourceLayoutHandle m_texture3DLayout{};
    std::vector<luna::ResourceSetHandle> m_texture2DSets;
    std::vector<luna::ResourceSetHandle> m_textureArraySets;
    std::vector<luna::ResourceSetHandle> m_texture3DSets;
    std::vector<luna::ImageViewHandle> m_bound2DViews;
    std::vector<luna::ImageViewHandle> m_boundArrayViews;
    std::vector<luna::ImageViewHandle> m_bound3DViews;

    luna::PipelineHandle m_present2DPipeline{};
    luna::PipelineHandle m_presentArrayPipeline{};
    luna::PipelineHandle m_present3DPipeline{};
    luna::PixelFormat m_presentBackbufferFormat = luna::PixelFormat::Undefined;

    luna::ImageHandle m_mipImage{};
    luna::ImageHandle m_arrayImage{};
    luna::ImageHandle m_volumeImage{};
};

} // namespace image_view_lab
