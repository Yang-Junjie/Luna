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
    uint32_t Width{1};
    uint32_t Height{1};
    luna::RHI::Format Format{luna::RHI::Format::UNDEFINED};
    luna::RHI::TextureUsageFlags Usage{luna::RHI::TextureUsageFlags::None};
    luna::RHI::ResourceState InitialState{luna::RHI::ResourceState::Undefined};
    luna::RHI::SampleCount SampleCount{luna::RHI::SampleCount::Count1};
};

struct RenderGraphPass {
    std::string Name;
    RenderGraphPassType Type{RenderGraphPassType::Raster};
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

struct RenderGraphCompiledRasterPass {
    RenderGraphPass Pass;
    std::vector<luna::RHI::TextureBarrier> PreTextureBarriers;
    luna::RHI::RenderingInfo RenderingInfo;
    uint32_t FramebufferWidth{0};
    uint32_t FramebufferHeight{0};
    std::function<void(RenderGraphRasterPassContext&)> Execute;
};

class RenderGraph {
public:
    using PassList = std::vector<RenderGraphPass>;
    using CompiledRasterPassList = std::vector<RenderGraphCompiledRasterPass>;

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
                CompiledRasterPassList raster_passes,
                std::vector<luna::RHI::TextureBarrier> final_texture_barriers);

    void execute() const;

    const RenderGraphPass* findPass(std::string_view name) const;
    RenderGraphPass* findPass(std::string_view name);

    const PassList& passes() const
    {
        return m_passes;
    }

private:
    FrameContext m_frame_context;
    PassList m_passes;
    std::vector<luna::RHI::Ref<luna::RHI::Texture>> m_textures;
    CompiledRasterPassList m_raster_passes;
    std::vector<luna::RHI::TextureBarrier> m_final_texture_barriers;
};

} // namespace luna




