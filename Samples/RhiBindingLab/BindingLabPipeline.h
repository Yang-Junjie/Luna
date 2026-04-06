#pragma once

#include "BindingLabState.h"
#include "Renderer/RenderPipeline.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace binding_lab {

class RhiBindingLabRenderPipeline final : public luna::IRenderPipeline {
public:
    explicit RhiBindingLabRenderPipeline(std::shared_ptr<State> state);

    bool init(luna::IRHIDevice& device) override;
    void shutdown(luna::IRHIDevice& device) override;
    bool render(luna::IRHIDevice& device, const luna::FrameContext& frameContext) override;

private:
    bool ensure_shader_handles(luna::IRHIDevice& device);
    bool ensure_shared_resources(luna::IRHIDevice& device);
    bool ensure_multi_set_resources(luna::IRHIDevice& device, luna::PixelFormat backbufferFormat);
    bool ensure_descriptor_array_resources(luna::IRHIDevice& device, luna::PixelFormat backbufferFormat);
    bool ensure_dynamic_uniform_resources(luna::IRHIDevice& device, luna::PixelFormat backbufferFormat);

    bool build_probe_pipeline(luna::IRHIDevice& device, luna::PixelFormat backbufferFormat);
    bool run_conflict_probe(luna::IRHIDevice& device, luna::PixelFormat backbufferFormat);
    bool update_multi_set_buffers(luna::IRHIDevice& device);
    bool update_descriptor_array_set(luna::IRHIDevice& device);

    bool render_multi_set(luna::IRHIDevice& device, const luna::FrameContext& frameContext);
    bool render_descriptor_array(luna::IRHIDevice& device, const luna::FrameContext& frameContext);
    bool render_dynamic_uniform(luna::IRHIDevice& device, const luna::FrameContext& frameContext);

    void destroy_probe_pipeline(luna::IRHIDevice& device);
    void destroy_multi_set_resources(luna::IRHIDevice& device);
    void destroy_descriptor_array_resources(luna::IRHIDevice& device);
    void destroy_dynamic_uniform_resources(luna::IRHIDevice& device);
    void destroy_shared_resources(luna::IRHIDevice& device);
    void destroy_shader_handles(luna::IRHIDevice& device);

    std::vector<uint8_t> build_texture_pixels(uint32_t width, uint32_t height, int variant) const;

private:
    std::shared_ptr<State> m_state;
    std::string m_shaderRoot;
    luna::ShaderHandle m_fullscreenVertexShader{};
    luna::ShaderHandle m_probeFragmentShader{};
    luna::ShaderHandle m_multiSetFragmentShader{};
    luna::ShaderHandle m_descriptorArrayFragmentShader{};
    luna::ShaderHandle m_dynamicUniformFragmentShader{};

    luna::SamplerHandle m_linearSampler{};
    luna::ImageHandle m_dummyTexture{};

    luna::ResourceLayoutHandle m_globalLayout{};
    luna::ResourceLayoutHandle m_materialLayout{};
    luna::ResourceLayoutHandle m_objectLayout{};
    luna::PipelineHandle m_probePipeline{};
    luna::PipelineHandle m_multiSetPipeline{};
    luna::PixelFormat m_multiSetBackbufferFormat = luna::PixelFormat::Undefined;
    luna::BufferHandle m_globalBuffer{};
    luna::BufferHandle m_globalNeutralBuffer{};
    luna::BufferHandle m_materialBuffer{};
    luna::BufferHandle m_materialNeutralBuffer{};
    luna::BufferHandle m_objectBuffer{};
    luna::BufferHandle m_objectNeutralBuffer{};
    luna::ResourceSetHandle m_globalSet{};
    luna::ResourceSetHandle m_globalNeutralSet{};
    luna::ResourceSetHandle m_materialSet{};
    luna::ResourceSetHandle m_materialNeutralSet{};
    luna::ResourceSetHandle m_objectSet{};
    luna::ResourceSetHandle m_objectNeutralSet{};

    luna::ResourceLayoutHandle m_descriptorArrayLayout{};
    luna::PipelineHandle m_descriptorArrayPipeline{};
    luna::PixelFormat m_descriptorArrayBackbufferFormat = luna::PixelFormat::Undefined;
    std::array<luna::ImageHandle, 4> m_descriptorArrayTextures{};
    luna::ImageHandle m_descriptorArrayAlternateTexture{};
    luna::ResourceSetHandle m_descriptorArraySet{};

    luna::ResourceLayoutHandle m_dynamicUniformLayout{};
    luna::PipelineHandle m_dynamicUniformPipeline{};
    luna::PixelFormat m_dynamicUniformBackbufferFormat = luna::PixelFormat::Undefined;
    luna::BufferHandle m_dynamicUniformBuffer{};
    luna::ResourceSetHandle m_dynamicUniformSet{};
    uint32_t m_dynamicUniformStride = 0;
};

} // namespace binding_lab
