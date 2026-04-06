#pragma once

#include "IblLabState.h"
#include "Renderer/RenderPipeline.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace ibl_lab {

class RhiIblLabRenderPipeline final : public luna::IRenderPipeline {
public:
    explicit RhiIblLabRenderPipeline(std::shared_ptr<State> state);

    bool init(luna::IRHIDevice& device) override;
    void shutdown(luna::IRHIDevice& device) override;
    bool render(luna::IRHIDevice& device, const luna::FrameContext& frameContext) override;

private:
    struct RenderTarget {
        luna::ImageHandle image{};
        luna::ImageViewHandle view{};
        luna::PixelFormat format = luna::PixelFormat::Undefined;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    struct CubeTexture {
        luna::ImageHandle image{};
        luna::ImageViewHandle cubeView{};
        std::array<luna::ImageViewHandle, 6> faceViews{};
        std::vector<luna::ImageViewHandle> attachmentViews;
        uint32_t size = 0;
        uint32_t mipLevels = 0;
    };

    struct PendingProbe {
        ProbeKind kind = ProbeKind::None;
        uint32_t face = 0;
        uint32_t mip = 0;
        uint32_t auxFace = 0;
        uint32_t auxMip = 0;
    };

    bool ensure_shared_resources(luna::IRHIDevice& device);
    bool ensure_source_hdr(luna::IRHIDevice& device);
    bool ensure_env_cube(luna::IRHIDevice& device);
    bool ensure_irradiance_cube(luna::IRHIDevice& device);
    bool ensure_prefilter_cube(luna::IRHIDevice& device);
    bool ensure_brdf_lut(luna::IRHIDevice& device);
    bool ensure_present_pipelines(luna::IRHIDevice& device, luna::PixelFormat backbufferFormat);
    bool ensure_generation_pipeline(luna::IRHIDevice& device);
    bool ensure_probe_resources(luna::IRHIDevice& device);

    bool update_cube_texture_set(luna::IRHIDevice& device, uint32_t frameIndex, luna::ImageViewHandle view);
    bool update_face_texture_set(luna::IRHIDevice& device, uint32_t frameIndex, luna::ImageViewHandle view);

    bool render_cube_atlas(const luna::FrameContext& frameContext,
                           const RenderTarget& target,
                           luna::ImageViewHandle cubeView,
                           int selectedFace,
                           float mipLevel);
    bool render_skybox(const luna::FrameContext& frameContext,
                       const RenderTarget& target,
                       luna::ImageViewHandle cubeView,
                       float yaw,
                       float pitch);
    bool render_face_preview(const luna::FrameContext& frameContext,
                             const RenderTarget& target,
                             luna::ImageViewHandle faceView,
                             float mipLevel,
                             bool applyTonemap = true);
    bool render_cube_filter(const luna::FrameContext& frameContext,
                            const RenderTarget& target,
                            luna::ImageViewHandle sourceCubeView,
                            uint32_t faceIndex,
                            float sourceLod);
    bool render_placeholder(const luna::FrameContext& frameContext, const RenderTarget& target);

    bool stamp_selected_face(luna::IRHIDevice& device, const luna::FrameContext& frameContext);
    bool generate_irradiance_cube(luna::IRHIDevice& device, const luna::FrameContext& frameContext);
    bool generate_prefilter_cube(luna::IRHIDevice& device, const luna::FrameContext& frameContext);
    bool execute_requested_probe(luna::IRHIDevice& device, const luna::FrameContext& frameContext);
    bool consume_probe_result(luna::IRHIDevice& device, uint32_t frameIndex);

    bool queue_cube_faces_distinct_probe(luna::IRHIDevice& device, const luna::FrameContext& frameContext, size_t slot);
    bool queue_skybox_rotation_probe(luna::IRHIDevice& device, const luna::FrameContext& frameContext, size_t slot);
    bool queue_face_mip_preview_probe(luna::IRHIDevice& device, const luna::FrameContext& frameContext, size_t slot);
    bool queue_face_isolation_probe(luna::IRHIDevice& device, const luna::FrameContext& frameContext, size_t slot);
    bool queue_env_mips_probe(luna::IRHIDevice& device, const luna::FrameContext& frameContext, size_t slot);
    bool queue_irradiance_probe(luna::IRHIDevice& device, const luna::FrameContext& frameContext, size_t slot);
    bool queue_prefilter_mips_probe(luna::IRHIDevice& device, const luna::FrameContext& frameContext, size_t slot);
    bool queue_brdf_lut_preview_probe(luna::IRHIDevice& device, const luna::FrameContext& frameContext, size_t slot);

    bool create_cube_texture(luna::IRHIDevice& device,
                             CubeTexture& cube,
                             uint32_t size,
                             uint32_t mipLevels,
                             std::string_view debugBaseName,
                             const void* initialData = nullptr);
    void destroy_cube_texture(luna::IRHIDevice& device, CubeTexture& cube);
    luna::ImageViewHandle ensure_attachment_view(luna::IRHIDevice& device, uint32_t faceIndex, uint32_t mipLevel);
    luna::ImageViewHandle ensure_attachment_view(luna::IRHIDevice& device,
                                                 CubeTexture& cube,
                                                 uint32_t faceIndex,
                                                 uint32_t mipLevel,
                                                 std::string_view debugName);
    bool copy_probe_pixel(const luna::FrameContext& frameContext,
                          luna::BufferHandle buffer,
                          uint64_t bufferOffset,
                          uint32_t x,
                          uint32_t y) const;

    void destroy_shared_resources(luna::IRHIDevice& device);
    void destroy_source_hdr(luna::IRHIDevice& device);
    void destroy_env_cube(luna::IRHIDevice& device);
    void destroy_irradiance_cube(luna::IRHIDevice& device);
    void destroy_prefilter_cube(luna::IRHIDevice& device);
    void destroy_brdf_lut(luna::IRHIDevice& device);
    void destroy_present_pipelines(luna::IRHIDevice& device);
    void destroy_probe_resources(luna::IRHIDevice& device);

    bool load_source_hdr_pixels();
    std::string resolve_source_hdr_path() const;
    std::string resolve_brdf_lut_path() const;
    bool load_brdf_lut_pixels(std::vector<uint16_t>* outPixels, uint32_t* outWidth, uint32_t* outHeight, std::string* outPath) const;
    std::vector<uint8_t> build_source_preview_data() const;
    std::vector<uint16_t> build_env_cube_data(uint32_t size) const;
    std::array<float, 4> sample_source_bilinear(float u, float v) const;
    std::array<float, 4> sample_source_direction(uint32_t faceIndex, float u, float v) const;

private:
    std::shared_ptr<State> m_state;
    luna::IRHIDevice* m_device = nullptr;
    std::string m_shaderRoot;
    uint32_t m_framesInFlight = 0;

    std::string m_sourceHdrPath;
    uint32_t m_sourceHdrWidth = 0;
    uint32_t m_sourceHdrHeight = 0;
    uint32_t m_sourceHdrChannels = 0;
    std::vector<float> m_sourceHdrPixels;

    luna::SamplerHandle m_linearSampler{};
    luna::ResourceLayoutHandle m_cubeLayout{};
    luna::ResourceLayoutHandle m_faceLayout{};
    std::vector<luna::ResourceSetHandle> m_cubeSets;
    std::vector<luna::ResourceSetHandle> m_faceSets;
    std::vector<luna::ImageViewHandle> m_boundCubeViews;
    std::vector<luna::ImageViewHandle> m_boundFaceViews;

    luna::PipelineHandle m_cubeAtlasPipeline{};
    luna::PipelineHandle m_skyboxPipeline{};
    luna::PipelineHandle m_facePreviewPipeline{};
    luna::PipelineHandle m_cubeFilterPipeline{};
    luna::PixelFormat m_presentBackbufferFormat = luna::PixelFormat::Undefined;

    luna::ImageHandle m_sourcePreviewImage{};
    luna::ImageViewHandle m_sourcePreviewView{};
    luna::ImageHandle m_envCubeImage{};
    luna::ImageViewHandle m_envCubeView{};
    std::array<luna::ImageViewHandle, 6> m_faceViews{};
    std::vector<luna::ImageViewHandle> m_attachmentViews;
    CubeTexture m_irradianceCube;
    CubeTexture m_prefilterCube;
    luna::ImageHandle m_brdfLutImage{};
    luna::ImageViewHandle m_brdfLutView{};

    luna::ImageHandle m_probeImage{};
    std::vector<luna::BufferHandle> m_probeBuffers;
    std::vector<PendingProbe> m_probePending;
};

} // namespace ibl_lab
