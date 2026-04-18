#include "Renderer/RenderGraph.h"

#include "Core/Log.h"

#include <algorithm>

namespace luna::rhi {
namespace {

const luna::RHI::Ref<luna::RHI::Texture>& emptyTextureRef()
{
    static const luna::RHI::Ref<luna::RHI::Texture> empty_ref{};
    return empty_ref;
}

} // namespace

RenderGraphPassContext::RenderGraphPassContext(luna::RHI::Ref<luna::RHI::Device> device,
                                               luna::RHI::Ref<luna::RHI::CommandBufferEncoder> command_buffer,
                                               const std::vector<luna::RHI::Ref<luna::RHI::Texture>>* textures,
                                               uint32_t framebuffer_width,
                                               uint32_t framebuffer_height)
    : m_device(std::move(device)),
      m_command_buffer(std::move(command_buffer)),
      m_textures(textures),
      m_framebuffer_width(framebuffer_width),
      m_framebuffer_height(framebuffer_height)
{}

const luna::RHI::Ref<luna::RHI::Device>& RenderGraphPassContext::device() const
{
    return m_device;
}

luna::RHI::CommandBufferEncoder& RenderGraphPassContext::commandBuffer() const
{
    return *m_command_buffer;
}

const luna::RHI::Ref<luna::RHI::Texture>& RenderGraphPassContext::getTexture(RenderGraphTextureHandle handle) const
{
    if (m_textures == nullptr || !handle.isValid() || handle.Index >= m_textures->size()) {
        return emptyTextureRef();
    }

    return (*m_textures)[handle.Index];
}

uint32_t RenderGraphPassContext::framebufferWidth() const
{
    return m_framebuffer_width;
}

uint32_t RenderGraphPassContext::framebufferHeight() const
{
    return m_framebuffer_height;
}

RenderGraphRasterPassContext::RenderGraphRasterPassContext(
    luna::RHI::Ref<luna::RHI::Device> device,
    luna::RHI::Ref<luna::RHI::CommandBufferEncoder> command_buffer,
    const std::vector<luna::RHI::Ref<luna::RHI::Texture>>* textures,
    uint32_t framebuffer_width,
    uint32_t framebuffer_height,
    const luna::RHI::RenderingInfo* rendering_info)
    : RenderGraphPassContext(std::move(device),
                             std::move(command_buffer),
                             textures,
                             framebuffer_width,
                             framebuffer_height),
      m_rendering_info(rendering_info)
{}

const luna::RHI::RenderingInfo& RenderGraphRasterPassContext::renderingInfo() const
{
    static const luna::RHI::RenderingInfo empty_rendering_info{};
    return m_rendering_info != nullptr ? *m_rendering_info : empty_rendering_info;
}

void RenderGraphRasterPassContext::beginRendering()
{
    if (m_rendering_info != nullptr) {
        commandBuffer().BeginRendering(*m_rendering_info);
    }
}

void RenderGraphRasterPassContext::endRendering()
{
    if (m_rendering_info != nullptr) {
        commandBuffer().EndRendering();
    }
}

RenderGraph::RenderGraph(FrameContext frame_context,
                         PassList passes,
                         std::vector<luna::RHI::Ref<luna::RHI::Texture>> textures,
                         CompiledRasterPassList raster_passes,
                         std::vector<luna::RHI::TextureBarrier> final_texture_barriers)
    : m_frame_context(std::move(frame_context)),
      m_passes(std::move(passes)),
      m_textures(std::move(textures)),
      m_raster_passes(std::move(raster_passes)),
      m_final_texture_barriers(std::move(final_texture_barriers))
{}

void RenderGraph::execute() const
{
    if (!m_frame_context.command_buffer) {
        return;
    }

    auto& command_buffer = *m_frame_context.command_buffer;
    for (const auto& pass : m_raster_passes) {
        if (!pass.PreTextureBarriers.empty()) {
            command_buffer.PipelineBarrier(
                luna::RHI::SyncScope::AllCommands, luna::RHI::SyncScope::AllCommands, pass.PreTextureBarriers);
        }

        command_buffer.BeginDebugLabel(pass.Pass.Name, 0.20f, 0.55f, 0.85f, 1.0f);

        RenderGraphRasterPassContext pass_context(m_frame_context.device,
                                                  m_frame_context.command_buffer,
                                                  &m_textures,
                                                  pass.FramebufferWidth,
                                                  pass.FramebufferHeight,
                                                  &pass.RenderingInfo);
        if (pass.Execute) {
            pass.Execute(pass_context);
        }

        command_buffer.EndDebugLabel();
    }

    if (!m_final_texture_barriers.empty()) {
        command_buffer.PipelineBarrier(
            luna::RHI::SyncScope::AllCommands, luna::RHI::SyncScope::AllCommands, m_final_texture_barriers);
    }
}

const RenderGraphPass* RenderGraph::findPass(std::string_view name) const
{
    const auto it = std::find_if(m_passes.begin(), m_passes.end(), [name](const RenderGraphPass& pass) {
        return pass.Name == name;
    });
    return it == m_passes.end() ? nullptr : &(*it);
}

RenderGraphPass* RenderGraph::findPass(std::string_view name)
{
    const auto it = std::find_if(m_passes.begin(), m_passes.end(), [name](const RenderGraphPass& pass) {
        return pass.Name == name;
    });
    return it == m_passes.end() ? nullptr : &(*it);
}

} // namespace luna::rhi
