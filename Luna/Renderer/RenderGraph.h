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
class QueryPool;
class Texture;
} // namespace luna::RHI

namespace luna {

enum class RenderGraphPassType : uint8_t {
    Raster,
    Compute,
    Copy,
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
    RHI::TextureType Type{RHI::TextureType::Texture2D};
    uint32_t Width{1};
    uint32_t Height{1};
    uint32_t Depth{1};
    uint32_t ArrayLayers{1};
    uint32_t MipLevels{1};
    RHI::Format Format{RHI::Format::UNDEFINED};
    RHI::TextureUsageFlags Usage{RHI::TextureUsageFlags::None};
    RHI::ResourceState InitialState{RHI::ResourceState::Undefined};
    RHI::SampleCount SampleCount{RHI::SampleCount::Count1};
};

struct RenderGraphPass {
    std::string Name;
    RenderGraphPassType Type{RenderGraphPassType::Raster};
};

struct RenderGraphPassProfile {
    std::string Name;
    RenderGraphPassType Type{RenderGraphPassType::Raster};
    double CpuStartMs{0.0};
    double CpuTimeMs{0.0};
    double GpuStartMs{0.0};
    double GpuTimeMs{0.0};
    bool HasGpuTime{false};
    uint32_t FramebufferWidth{0};
    uint32_t FramebufferHeight{0};
    uint32_t ReadTextureCount{0};
    uint32_t WriteTextureCount{0};
    uint32_t ColorAttachmentCount{0};
    bool HasDepthAttachment{false};
    uint32_t PreBarrierCount{0};
};

struct RenderGraphProfileSnapshot {
    uint64_t FrameIndex{0};
    double TotalCpuTimeMs{0.0};
    double TotalGpuTimeMs{0.0};
    bool GpuTimingSupported{false};
    bool GpuTimingPending{false};
    uint32_t TextureCount{0};
    uint32_t FinalBarrierCount{0};
    std::vector<RenderGraphPassProfile> Passes;
};

class RenderGraphPassContext {
public:
    const RHI::Ref<RHI::Device>& device() const;
    RHI::CommandBufferEncoder& commandBuffer() const;
    const RHI::Ref<RHI::Texture>& getTexture(RenderGraphTextureHandle handle) const;
    uint32_t framebufferWidth() const;
    uint32_t framebufferHeight() const;

protected:
    RenderGraphPassContext(RHI::Ref<RHI::Device> device,
                           RHI::Ref<RHI::CommandBufferEncoder> command_buffer,
                           const std::vector<RHI::Ref<RHI::Texture>>* textures,
                           uint32_t framebuffer_width,
                           uint32_t framebuffer_height);

private:
    RHI::Ref<RHI::Device> m_device;
    RHI::Ref<RHI::CommandBufferEncoder> m_command_buffer;
    const std::vector<RHI::Ref<RHI::Texture>>* m_textures{nullptr};
    uint32_t m_framebuffer_width{0};
    uint32_t m_framebuffer_height{0};
};

class RenderGraphRasterPassContext final : public RenderGraphPassContext {
public:
    const RHI::RenderingInfo& renderingInfo() const;
    void beginRendering();
    void endRendering();

private:
    friend class RenderGraph;

    RenderGraphRasterPassContext(RHI::Ref<RHI::Device> device,
                                 RHI::Ref<RHI::CommandBufferEncoder> command_buffer,
                                 const std::vector<RHI::Ref<RHI::Texture>>* textures,
                                 uint32_t framebuffer_width,
                                 uint32_t framebuffer_height,
                                 const RHI::RenderingInfo* rendering_info);

    const RHI::RenderingInfo* m_rendering_info{nullptr};
};

class RenderGraphComputePassContext final : public RenderGraphPassContext {
private:
    friend class RenderGraph;

    RenderGraphComputePassContext(RHI::Ref<RHI::Device> device,
                                  RHI::Ref<RHI::CommandBufferEncoder> command_buffer,
                                  const std::vector<RHI::Ref<RHI::Texture>>* textures,
                                  uint32_t framebuffer_width,
                                  uint32_t framebuffer_height);
};

struct RenderGraphCompiledPass {
    RenderGraphPass Pass;
    std::vector<RHI::TextureBarrier> PreTextureBarriers;
    RHI::RenderingInfo RenderingInfo;
    uint32_t FramebufferWidth{0};
    uint32_t FramebufferHeight{0};
    uint32_t ReadTextureCount{0};
    uint32_t WriteTextureCount{0};
    std::function<void(RenderGraphRasterPassContext&)> ExecuteRaster;
    std::function<void(RenderGraphComputePassContext&)> ExecuteCompute;
    RenderGraphTextureHandle CopySource{};
    RenderGraphTextureHandle CopyDestination{};
};

class RenderGraph {
public:
    using PassList = std::vector<RenderGraphPass>;
    using CompiledPassList = std::vector<RenderGraphCompiledPass>;

    struct FrameContext {
        RHI::Ref<RHI::Device> device;
        RHI::Ref<RHI::CommandBufferEncoder> command_buffer;
        RHI::Ref<RHI::QueryPool> timestamp_query_pool;
        RHI::Ref<RHI::QueryPool> timestamp_disjoint_query_pool;
        uint32_t timestamp_query_capacity{0};
        uint64_t frame_index{0};
        bool profiling_enabled{false};
        uint32_t framebuffer_width{0};
        uint32_t framebuffer_height{0};
    };

    RenderGraph() = default;
    RenderGraph(FrameContext frame_context,
                PassList passes,
                std::vector<RHI::Ref<RHI::Texture>> textures,
                CompiledPassList compiled_passes,
                std::vector<RHI::TextureBarrier> final_texture_barriers);

    void execute() const;
    const RenderGraphProfileSnapshot& profile() const;

    const PassList& passes() const
    {
        return m_passes;
    }

private:
    FrameContext m_frame_context;
    PassList m_passes;
    std::vector<RHI::Ref<RHI::Texture>> m_textures;
    CompiledPassList m_compiled_passes;
    std::vector<RHI::TextureBarrier> m_final_texture_barriers;
    mutable RenderGraphProfileSnapshot m_profile;
};

} // namespace luna
