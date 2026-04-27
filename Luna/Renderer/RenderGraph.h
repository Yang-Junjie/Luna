#pragma once

// Defines the compiled render-graph representation and pass execution contexts.
// These types describe what a frame will execute after graph building is complete,
// including pass metadata, texture access, and command-buffer facing callbacks.

#include <cstdint>

#include <Barrier.h>
#include <CommandBufferEncoder.h>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <Texture.h>
#include <vector>

namespace luna::RHI {
class Device;
class Texture;
} // namespace luna::RHI

namespace luna {

enum class RenderGraphPassType : uint8_t {
    Raster,
    Compute,
};

struct RenderGraphTextureHandle {
    uint32_t Index{UINT32_MAX};

    bool isValid() const
    {
        return Index != UINT32_MAX;
    }

    explicit operator bool() const
    {
        return isValid();
    }
};

struct RenderGraphTextureDesc {
    std::string Name;
    luna::RHI::TextureType Type{luna::RHI::TextureType::Texture2D};
    uint32_t Width{1};
    uint32_t Height{1};
    uint32_t Depth{1};
    uint32_t ArrayLayers{1};
    uint32_t MipLevels{1};
    luna::RHI::Format Format{luna::RHI::Format::UNDEFINED};
    luna::RHI::TextureUsageFlags Usage{luna::RHI::TextureUsageFlags::None};
    luna::RHI::ResourceState InitialState{luna::RHI::ResourceState::Undefined};
    luna::RHI::SampleCount SampleCount{luna::RHI::SampleCount::Count1};
};

struct RenderGraphPass {
    std::string Name;
    RenderGraphPassType Type{RenderGraphPassType::Raster};
};

struct RenderGraphPassProfile {
    std::string Name;
    RenderGraphPassType Type{RenderGraphPassType::Raster};
    double CpuTimeMs{0.0};
    uint32_t FramebufferWidth{0};
    uint32_t FramebufferHeight{0};
    uint32_t ReadTextureCount{0};
    uint32_t WriteTextureCount{0};
    uint32_t ColorAttachmentCount{0};
    bool HasDepthAttachment{false};
    uint32_t PreBarrierCount{0};
};

struct RenderGraphProfileSnapshot {
    double TotalCpuTimeMs{0.0};
    uint32_t TextureCount{0};
    uint32_t FinalBarrierCount{0};
    std::vector<RenderGraphPassProfile> Passes;
};

class RenderGraphPassContext {
public:
    const luna::RHI::Ref<luna::RHI::Device>& device() const;
    luna::RHI::CommandBufferEncoder& commandBuffer() const;
    const luna::RHI::Ref<luna::RHI::Texture>& getTexture(RenderGraphTextureHandle handle) const;
    uint32_t framebufferWidth() const;
    uint32_t framebufferHeight() const;

protected:
    RenderGraphPassContext(luna::RHI::Ref<luna::RHI::Device> device,
                           luna::RHI::Ref<luna::RHI::CommandBufferEncoder> command_buffer,
                           const std::vector<luna::RHI::Ref<luna::RHI::Texture>>* textures,
                           uint32_t framebuffer_width,
                           uint32_t framebuffer_height);

private:
    luna::RHI::Ref<luna::RHI::Device> m_device;
    luna::RHI::Ref<luna::RHI::CommandBufferEncoder> m_command_buffer;
    const std::vector<luna::RHI::Ref<luna::RHI::Texture>>* m_textures{nullptr};
    uint32_t m_framebuffer_width{0};
    uint32_t m_framebuffer_height{0};
};

class RenderGraphRasterPassContext final : public RenderGraphPassContext {
public:
    const luna::RHI::RenderingInfo& renderingInfo() const;
    void beginRendering();
    void endRendering();

private:
    friend class RenderGraph;

    RenderGraphRasterPassContext(luna::RHI::Ref<luna::RHI::Device> device,
                                 luna::RHI::Ref<luna::RHI::CommandBufferEncoder> command_buffer,
                                 const std::vector<luna::RHI::Ref<luna::RHI::Texture>>* textures,
                                 uint32_t framebuffer_width,
                                 uint32_t framebuffer_height,
                                 const luna::RHI::RenderingInfo* rendering_info);

    const luna::RHI::RenderingInfo* m_rendering_info{nullptr};
};

class RenderGraphComputePassContext final : public RenderGraphPassContext {
private:
    friend class RenderGraph;

    RenderGraphComputePassContext(luna::RHI::Ref<luna::RHI::Device> device,
                                  luna::RHI::Ref<luna::RHI::CommandBufferEncoder> command_buffer,
                                  const std::vector<luna::RHI::Ref<luna::RHI::Texture>>* textures,
                                  uint32_t framebuffer_width,
                                  uint32_t framebuffer_height);
};

struct RenderGraphCompiledPass {
    RenderGraphPass Pass;
    std::vector<luna::RHI::TextureBarrier> PreTextureBarriers;
    luna::RHI::RenderingInfo RenderingInfo;
    uint32_t FramebufferWidth{0};
    uint32_t FramebufferHeight{0};
    uint32_t ReadTextureCount{0};
    uint32_t WriteTextureCount{0};
    std::function<void(RenderGraphRasterPassContext&)> ExecuteRaster;
    std::function<void(RenderGraphComputePassContext&)> ExecuteCompute;
};

class RenderGraph {
public:
    using PassList = std::vector<RenderGraphPass>;
    using CompiledPassList = std::vector<RenderGraphCompiledPass>;

    struct FrameContext {
        luna::RHI::Ref<luna::RHI::Device> device;
        luna::RHI::Ref<luna::RHI::CommandBufferEncoder> command_buffer;
        uint32_t framebuffer_width{0};
        uint32_t framebuffer_height{0};
    };

    RenderGraph() = default;
    RenderGraph(FrameContext frame_context,
                PassList passes,
                std::vector<luna::RHI::Ref<luna::RHI::Texture>> textures,
                CompiledPassList compiled_passes,
                std::vector<luna::RHI::TextureBarrier> final_texture_barriers);

    void execute() const;
    const RenderGraphProfileSnapshot& profile() const;

    const PassList& passes() const
    {
        return m_passes;
    }

private:
    FrameContext m_frame_context;
    PassList m_passes;
    std::vector<luna::RHI::Ref<luna::RHI::Texture>> m_textures;
    CompiledPassList m_compiled_passes;
    std::vector<luna::RHI::TextureBarrier> m_final_texture_barriers;
    mutable RenderGraphProfileSnapshot m_profile;
};

} // namespace luna




