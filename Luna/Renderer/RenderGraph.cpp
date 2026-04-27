#include "Core/Log.h"
#include "Renderer/RenderGraph.h"
#include "Renderer/RendererUtilities.h"

#include <algorithm>
#include <chrono>

namespace luna {
namespace {

const luna::RHI::Ref<luna::RHI::Texture>& emptyTextureRef()
{
    static const luna::RHI::Ref<luna::RHI::Texture> empty_ref{};
    return empty_ref;
}

double elapsedMilliseconds(std::chrono::steady_clock::time_point begin,
                           std::chrono::steady_clock::time_point end)
{
    return std::chrono::duration<double, std::milli>(end - begin).count();
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
    : RenderGraphPassContext(
          std::move(device), std::move(command_buffer), textures, framebuffer_width, framebuffer_height),
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

RenderGraphComputePassContext::RenderGraphComputePassContext(
    luna::RHI::Ref<luna::RHI::Device> device,
    luna::RHI::Ref<luna::RHI::CommandBufferEncoder> command_buffer,
    const std::vector<luna::RHI::Ref<luna::RHI::Texture>>* textures,
    uint32_t framebuffer_width,
    uint32_t framebuffer_height)
    : RenderGraphPassContext(
          std::move(device), std::move(command_buffer), textures, framebuffer_width, framebuffer_height)
{}

RenderGraph::RenderGraph(FrameContext frame_context,
                         PassList passes,
                         std::vector<luna::RHI::Ref<luna::RHI::Texture>> textures,
                         CompiledPassList compiled_passes,
                         std::vector<luna::RHI::TextureBarrier> final_texture_barriers)
    : m_frame_context(std::move(frame_context)),
      m_passes(std::move(passes)),
      m_textures(std::move(textures)),
      m_compiled_passes(std::move(compiled_passes)),
      m_final_texture_barriers(std::move(final_texture_barriers))
{}

void RenderGraph::execute() const
{
    m_profile = {};
    m_profile.TextureCount = static_cast<uint32_t>(m_textures.size());
    m_profile.FinalBarrierCount = static_cast<uint32_t>(m_final_texture_barriers.size());
    m_profile.Passes.reserve(m_compiled_passes.size());

    if (!m_frame_context.command_buffer) {
        LUNA_RENDERER_WARN("Skipping render graph execution because command buffer is null");
        return;
    }

    const size_t raster_pass_count =
        static_cast<size_t>(std::count_if(m_compiled_passes.begin(), m_compiled_passes.end(), [](const auto& pass) {
            return pass.Pass.Type == RenderGraphPassType::Raster;
        }));
    const size_t compute_pass_count = m_compiled_passes.size() - raster_pass_count;
    LUNA_RENDERER_FRAME_DEBUG(
        "Executing render graph with {} pass(es) (raster={}, compute={}), {} texture(s), {} final barrier(s)",
                              m_compiled_passes.size(),
                              raster_pass_count,
                              compute_pass_count,
                              m_textures.size(),
                              m_final_texture_barriers.size());
    auto& command_buffer = *m_frame_context.command_buffer;
    const auto graph_begin = std::chrono::steady_clock::now();
    for (const auto& pass : m_compiled_passes) {
        RenderGraphPassProfile pass_profile{
            .Name = pass.Pass.Name,
            .Type = pass.Pass.Type,
            .CpuTimeMs = 0.0,
            .FramebufferWidth = pass.FramebufferWidth,
            .FramebufferHeight = pass.FramebufferHeight,
            .ReadTextureCount = pass.ReadTextureCount,
            .WriteTextureCount = pass.WriteTextureCount,
            .ColorAttachmentCount = static_cast<uint32_t>(pass.RenderingInfo.ColorAttachments.size()),
            .HasDepthAttachment = static_cast<bool>(pass.RenderingInfo.DepthAttachment),
            .PreBarrierCount = static_cast<uint32_t>(pass.PreTextureBarriers.size()),
        };
        const auto pass_begin = std::chrono::steady_clock::now();

        if (!pass.PreTextureBarriers.empty()) {
            LUNA_RENDERER_FRAME_TRACE("Render graph pass '{}' applying {} pre-texture barrier(s)",
                                      pass.Pass.Name,
                                      pass.PreTextureBarriers.size());
            command_buffer.PipelineBarrier(
                luna::RHI::SyncScope::AllCommands, luna::RHI::SyncScope::AllCommands, pass.PreTextureBarriers);
        }

        if (pass.Pass.Type == RenderGraphPassType::Raster) {
            LUNA_RENDERER_FRAME_TRACE("Executing raster render graph pass '{}' ({}x{}, color_attachments={}, has_depth={})",
                                      pass.Pass.Name,
                                      pass.FramebufferWidth,
                                      pass.FramebufferHeight,
                                      pass.RenderingInfo.ColorAttachments.size(),
                                      static_cast<bool>(pass.RenderingInfo.DepthAttachment));
        } else {
            LUNA_RENDERER_FRAME_TRACE("Executing compute render graph pass '{}'", pass.Pass.Name);
        }
        command_buffer.BeginDebugLabel(pass.Pass.Name, 0.20f, 0.55f, 0.85f, 1.0f);

        if (pass.Pass.Type == RenderGraphPassType::Raster) {
            RenderGraphRasterPassContext pass_context(m_frame_context.device,
                                                      m_frame_context.command_buffer,
                                                      &m_textures,
                                                      pass.FramebufferWidth,
                                                      pass.FramebufferHeight,
                                                      &pass.RenderingInfo);
            if (pass.ExecuteRaster) {
                pass.ExecuteRaster(pass_context);
            }
        } else {
            RenderGraphComputePassContext pass_context(m_frame_context.device,
                                                       m_frame_context.command_buffer,
                                                       &m_textures,
                                                       m_frame_context.framebuffer_width,
                                                       m_frame_context.framebuffer_height);
            if (pass.ExecuteCompute) {
                pass.ExecuteCompute(pass_context);
            }
        }

        command_buffer.EndDebugLabel();
        pass_profile.CpuTimeMs = elapsedMilliseconds(pass_begin, std::chrono::steady_clock::now());
        m_profile.Passes.push_back(std::move(pass_profile));
    }

    if (!m_final_texture_barriers.empty()) {
        LUNA_RENDERER_FRAME_TRACE("Applying {} final render graph texture barrier(s)", m_final_texture_barriers.size());
        command_buffer.PipelineBarrier(
            luna::RHI::SyncScope::AllCommands, luna::RHI::SyncScope::AllCommands, m_final_texture_barriers);
    }
    m_profile.TotalCpuTimeMs = elapsedMilliseconds(graph_begin, std::chrono::steady_clock::now());
    LUNA_RENDERER_FRAME_DEBUG("Render graph execution complete");
}

const RenderGraphProfileSnapshot& RenderGraph::profile() const
{
    return m_profile;
}

} // namespace luna




