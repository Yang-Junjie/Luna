#include "Core/Log.h"
#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/RendererUtilities.h"

#include <Builders.h>
#include <Device.h>

namespace luna {
namespace {

bool isValidTextureRef(const luna::RHI::Ref<luna::RHI::Texture>& texture)
{
    return texture != nullptr;
}

luna::RHI::ImageSubresourceRange fullSubresourceRangeForTexture(const luna::RHI::Texture& texture)
{
    luna::RHI::ImageAspectFlags aspect_mask = luna::RHI::ImageAspectFlags::Color;
    if (texture.HasDepth() && texture.HasStencil()) {
        aspect_mask = luna::RHI::ImageAspectFlags::Depth | luna::RHI::ImageAspectFlags::Stencil;
    } else if (texture.HasDepth()) {
        aspect_mask = luna::RHI::ImageAspectFlags::Depth;
    } else if (texture.HasStencil()) {
        aspect_mask = luna::RHI::ImageAspectFlags::Stencil;
    }

    return luna::RHI::ImageSubresourceRange{
        .BaseMipLevel = 0,
        .LevelCount = texture.GetMipLevels(),
        .BaseArrayLayer = 0,
        .LayerCount = texture.GetArrayLayers(),
        .AspectMask = aspect_mask,
    };
}

luna::RHI::Ref<luna::RHI::Texture> createTransientTexture(const luna::RHI::Ref<luna::RHI::Device>& device,
                                                          const RenderGraphTextureDesc& desc)
{
    if (!device || desc.Width == 0 || desc.Height == 0 || desc.Format == luna::RHI::Format::UNDEFINED) {
        LUNA_RENDERER_WARN(
            "Cannot create transient render graph texture '{}': device_available={} size={}x{} format={}",
            desc.Name,
            static_cast<bool>(device),
            desc.Width,
            desc.Height,
            renderer_detail::formatToString(desc.Format));
        return {};
    }

    auto texture = device->CreateTexture(luna::RHI::TextureBuilder()
                                             .SetSize(desc.Width, desc.Height)
                                             .SetFormat(desc.Format)
                                             .SetUsage(desc.Usage)
                                             .SetInitialState(desc.InitialState)
                                             .SetSampleCount(desc.SampleCount)
                                             .SetName(desc.Name)
                                             .Build());
    if (!texture) {
        LUNA_RENDERER_WARN("Failed to create transient render graph texture '{}' ({}x{}, format={} ({}) usage=0x{:x})",
                           desc.Name,
                           desc.Width,
                           desc.Height,
                           renderer_detail::formatToString(desc.Format),
                           static_cast<int>(desc.Format),
                           static_cast<uint32_t>(desc.Usage));
    } else {
        LUNA_RENDERER_FRAME_TRACE("Created transient render graph texture '{}' ({}x{}, format={} ({}) usage=0x{:x})",
                                  desc.Name,
                                  desc.Width,
                                  desc.Height,
                                  renderer_detail::formatToString(desc.Format),
                                  static_cast<int>(desc.Format),
                                  static_cast<uint32_t>(desc.Usage));
    }
    return texture;
}

void addBarrierIfNeeded(std::vector<luna::RHI::TextureBarrier>& barriers,
                        std::vector<luna::RHI::ResourceState>& current_states,
                        const std::vector<luna::RHI::Ref<luna::RHI::Texture>>& physical_textures,
                        RenderGraphTextureHandle handle,
                        luna::RHI::ResourceState desired_state)
{
    if (!handle.isValid() || handle.Index >= current_states.size() || handle.Index >= physical_textures.size()) {
        return;
    }

    const auto& texture = physical_textures[handle.Index];
    if (!texture) {
        return;
    }

    const luna::RHI::ResourceState current_state = current_states[handle.Index];
    if (current_state == desired_state) {
        return;
    }

    barriers.push_back(luna::RHI::TextureBarrier{
        .Texture = texture,
        .OldState = current_state,
        .NewState = desired_state,
        .SubresourceRange = fullSubresourceRangeForTexture(*texture),
    });
    current_states[handle.Index] = desired_state;
}

luna::RHI::Extent2D
    resolvePassFramebufferExtent(const detail::RenderGraphRasterPassNode& pass_node,
                                 const std::vector<luna::RHI::Ref<luna::RHI::Texture>>& physical_textures,
                                 uint32_t fallback_width,
                                 uint32_t fallback_height)
{
    for (const auto& attachment : pass_node.ColorAttachments) {
        if (attachment.Handle.isValid() && attachment.Handle.Index < physical_textures.size()) {
            const auto& texture = physical_textures[attachment.Handle.Index];
            if (texture) {
                return {texture->GetWidth(), texture->GetHeight()};
            }
        }
    }

    if (pass_node.DepthAttachment.has_value() && pass_node.DepthAttachment->Handle.isValid() &&
        pass_node.DepthAttachment->Handle.Index < physical_textures.size()) {
        const auto& texture = physical_textures[pass_node.DepthAttachment->Handle.Index];
        if (texture) {
            return {texture->GetWidth(), texture->GetHeight()};
        }
    }

    return {fallback_width, fallback_height};
}

} // namespace

void RenderGraphTransientTextureCache::BeginFrame()
{
    LUNA_RENDERER_FRAME_TRACE("Resetting render graph transient texture cache with {} entrie(s)", m_entries.size());
    for (auto& entry : m_entries) {
        entry.InUse = false;
    }
}

luna::RHI::Ref<luna::RHI::Texture>
    RenderGraphTransientTextureCache::AcquireTexture(const luna::RHI::Ref<luna::RHI::Device>& device,
                                                     const RenderGraphTextureDesc& desc)
{
    for (auto& entry : m_entries) {
        if (!entry.InUse && IsCompatible(entry, desc) && entry.Texture) {
            entry.InUse = true;
            LUNA_RENDERER_FRAME_TRACE(
                "Reusing transient render graph texture '{}' ({}x{})", desc.Name, desc.Width, desc.Height);
            return entry.Texture;
        }
    }

    const size_t removed_count = std::erase_if(m_entries, [&desc](const TextureEntry& entry) {
        return !entry.InUse && !IsCompatible(entry, desc);
    });
    if (removed_count > 0) {
        LUNA_RENDERER_FRAME_DEBUG("Pruned {} incompatible transient render graph texture cache entrie(s)",
                                  removed_count);
    }

    auto texture = createTransientTexture(device, desc);
    if (!texture) {
        return {};
    }

    m_entries.push_back(TextureEntry{
        .Desc = desc,
        .Texture = texture,
        .InUse = true,
    });
    LUNA_RENDERER_FRAME_DEBUG("Cached new transient render graph texture '{}' ({}x{}, format={} ({}))",
                              desc.Name,
                              desc.Width,
                              desc.Height,
                              renderer_detail::formatToString(desc.Format),
                              static_cast<int>(desc.Format));
    return texture;
}

bool RenderGraphTransientTextureCache::IsCompatible(const TextureEntry& entry, const RenderGraphTextureDesc& desc)
{
    return entry.Desc.Width == desc.Width && entry.Desc.Height == desc.Height && entry.Desc.Format == desc.Format &&
           entry.Desc.Usage == desc.Usage && entry.Desc.SampleCount == desc.SampleCount;
}

RenderGraphRasterPassBuilder::RenderGraphRasterPassBuilder(detail::RenderGraphRasterPassNode* pass_node)
    : m_pass_node(pass_node)
{}

RenderGraphRasterPassBuilder& RenderGraphRasterPassBuilder::ReadTexture(RenderGraphTextureHandle handle)
{
    if (m_pass_node != nullptr && handle.isValid()) {
        m_pass_node->Reads.push_back(detail::RenderGraphTextureReadDesc{.Handle = handle});
    }
    return *this;
}

RenderGraphRasterPassBuilder& RenderGraphRasterPassBuilder::WriteColor(RenderGraphTextureHandle handle,
                                                                       luna::RHI::AttachmentLoadOp load_op,
                                                                       luna::RHI::AttachmentStoreOp store_op,
                                                                       const luna::RHI::ClearValue& clear_value)
{
    if (m_pass_node != nullptr && handle.isValid()) {
        m_pass_node->ColorAttachments.push_back(detail::RenderGraphColorAttachmentDesc{
            .Handle = handle,
            .LoadOp = load_op,
            .StoreOp = store_op,
            .ClearValue = clear_value,
        });
    }
    return *this;
}

RenderGraphRasterPassBuilder&
    RenderGraphRasterPassBuilder::WriteDepth(RenderGraphTextureHandle handle,
                                             luna::RHI::AttachmentLoadOp load_op,
                                             luna::RHI::AttachmentStoreOp store_op,
                                             const luna::RHI::ClearDepthStencilValue& clear_value)
{
    if (m_pass_node != nullptr && handle.isValid()) {
        m_pass_node->DepthAttachment = detail::RenderGraphDepthAttachmentDesc{
            .Handle = handle,
            .LoadOp = load_op,
            .StoreOp = store_op,
            .ClearValue = clear_value,
        };
    }
    return *this;
}

RenderGraphBuilder::RenderGraphBuilder(FrameContext frame_context,
                                       RenderGraphTransientTextureCache* transient_texture_cache)
    : m_frame_context(std::move(frame_context)),
      m_transient_texture_cache(transient_texture_cache)
{}

RenderGraphTextureHandle RenderGraphBuilder::ImportTexture(std::string name,
                                                           luna::RHI::Ref<luna::RHI::Texture> texture,
                                                           luna::RHI::ResourceState initial_state,
                                                           luna::RHI::ResourceState final_state)
{
    if (!isValidTextureRef(texture)) {
        LUNA_RENDERER_WARN("Ignoring render graph import for '{}' because texture is null", name);
        return {};
    }

    const uint32_t index = static_cast<uint32_t>(m_texture_nodes.size());
    m_texture_nodes.push_back(detail::RenderGraphTextureNode{
        .Desc =
            RenderGraphTextureDesc{
                .Name = std::move(name),
                .Width = texture->GetWidth(),
                .Height = texture->GetHeight(),
                .Format = texture->GetFormat(),
                .Usage = texture->GetUsage(),
                .InitialState = initial_state,
                .SampleCount = texture->GetSampleCount(),
            },
        .ImportedTexture = std::move(texture),
        .InitialState = initial_state,
        .FinalState = final_state,
        .Imported = true,
        .Exported = false,
    });
    LUNA_RENDERER_FRAME_DEBUG("Imported render graph texture '{}' as handle {} ({}x{}, format={} ({}))",
                              m_texture_nodes.back().Desc.Name,
                              index,
                              m_texture_nodes.back().Desc.Width,
                              m_texture_nodes.back().Desc.Height,
                              renderer_detail::formatToString(m_texture_nodes.back().Desc.Format),
                              static_cast<int>(m_texture_nodes.back().Desc.Format));
    return RenderGraphTextureHandle{index};
}

RenderGraphTextureHandle RenderGraphBuilder::CreateTexture(RenderGraphTextureDesc desc)
{
    if (desc.Width == 0 || desc.Height == 0) {
        LUNA_RENDERER_WARN("Ignoring render graph texture creation for '{}' because size is {}x{}",
                           desc.Name,
                           desc.Width,
                           desc.Height);
        return {};
    }

    if (desc.Name.empty()) {
        desc.Name = "RenderGraphTexture";
    }

    const uint32_t index = static_cast<uint32_t>(m_texture_nodes.size());
    m_texture_nodes.push_back(detail::RenderGraphTextureNode{
        .Desc = std::move(desc),
        .ImportedTexture = {},
        .InitialState = luna::RHI::ResourceState::Undefined,
        .FinalState = luna::RHI::ResourceState::Common,
        .Imported = false,
        .Exported = false,
    });
    m_texture_nodes.back().InitialState = m_texture_nodes.back().Desc.InitialState;
    LUNA_RENDERER_FRAME_DEBUG(
        "Declared transient render graph texture '{}' as handle {} ({}x{}, format={} ({}) usage=0x{:x})",
        m_texture_nodes.back().Desc.Name,
        index,
        m_texture_nodes.back().Desc.Width,
        m_texture_nodes.back().Desc.Height,
        renderer_detail::formatToString(m_texture_nodes.back().Desc.Format),
        static_cast<int>(m_texture_nodes.back().Desc.Format),
        static_cast<uint32_t>(m_texture_nodes.back().Desc.Usage));
    return RenderGraphTextureHandle{index};
}

RenderGraphBuilder& RenderGraphBuilder::ExportTexture(RenderGraphTextureHandle handle,
                                                      luna::RHI::ResourceState final_state)
{
    if (isHandleValid(handle, m_texture_nodes.size())) {
        auto& texture_node = m_texture_nodes[handle.Index];
        texture_node.Exported = true;
        texture_node.FinalState = final_state;
        LUNA_RENDERER_FRAME_DEBUG("Exported render graph texture '{}' (handle {}) to final state 0x{:x}",
                                  texture_node.Desc.Name,
                                  handle.Index,
                                  static_cast<uint32_t>(final_state));
    } else {
        LUNA_RENDERER_WARN("Ignoring render graph texture export for invalid handle {}", handle.Index);
    }
    return *this;
}

RenderGraphBuilder& RenderGraphBuilder::AddRasterPass(const std::string& name,
                                                      RasterPassSetupCallback setup,
                                                      RasterPassExecuteCallback execute,
                                                      bool side_effect)
{
    m_raster_pass_nodes.push_back(detail::RenderGraphRasterPassNode{
        .Name = name,
        .Reads = {},
        .ColorAttachments = {},
        .DepthAttachment = std::nullopt,
        .Execute = std::move(execute),
        .SideEffect = side_effect,
    });

    RenderGraphRasterPassBuilder builder(&m_raster_pass_nodes.back());
    if (setup) {
        setup(builder);
    }

    LUNA_RENDERER_FRAME_DEBUG("Added raster render graph pass '{}' (reads={}, colors={}, has_depth={}, side_effect={})",
                              name,
                              m_raster_pass_nodes.back().Reads.size(),
                              m_raster_pass_nodes.back().ColorAttachments.size(),
                              m_raster_pass_nodes.back().DepthAttachment.has_value(),
                              side_effect);
    return *this;
}

std::unique_ptr<RenderGraph> RenderGraphBuilder::Build()
{
    if (!m_frame_context.device || !m_frame_context.command_buffer) {
        LUNA_RENDERER_WARN(
            "Building empty render graph because frame context is incomplete: device={} command_buffer={}",
            static_cast<bool>(m_frame_context.device),
            static_cast<bool>(m_frame_context.command_buffer));
        return std::make_unique<RenderGraph>();
    }

    const size_t texture_count = m_texture_nodes.size();
    std::vector<bool> live_resources(texture_count, false);
    for (size_t i = 0; i < texture_count; ++i) {
        if (m_texture_nodes[i].Exported) {
            live_resources[i] = true;
        }
    }

    std::vector<bool> live_passes(m_raster_pass_nodes.size(), false);
    for (size_t i = m_raster_pass_nodes.size(); i-- > 0;) {
        const auto& pass_node = m_raster_pass_nodes[i];

        bool is_live = pass_node.SideEffect;
        for (const auto& attachment : pass_node.ColorAttachments) {
            if (isHandleValid(attachment.Handle, texture_count) && live_resources[attachment.Handle.Index]) {
                is_live = true;
                break;
            }
        }

        if (!is_live && pass_node.DepthAttachment.has_value() &&
            isHandleValid(pass_node.DepthAttachment->Handle, texture_count) &&
            live_resources[pass_node.DepthAttachment->Handle.Index]) {
            is_live = true;
        }

        if (!is_live) {
            continue;
        }

        live_passes[i] = true;

        for (const auto& read : pass_node.Reads) {
            if (isHandleValid(read.Handle, texture_count)) {
                live_resources[read.Handle.Index] = true;
            }
        }

        for (const auto& attachment : pass_node.ColorAttachments) {
            if (isHandleValid(attachment.Handle, texture_count)) {
                live_resources[attachment.Handle.Index] = true;
            }
        }

        if (pass_node.DepthAttachment.has_value() && isHandleValid(pass_node.DepthAttachment->Handle, texture_count)) {
            live_resources[pass_node.DepthAttachment->Handle.Index] = true;
        }
    }

    std::vector<luna::RHI::Ref<luna::RHI::Texture>> physical_textures(texture_count);
    std::vector<luna::RHI::ResourceState> current_states(texture_count, luna::RHI::ResourceState::Undefined);
    for (size_t i = 0; i < texture_count; ++i) {
        const auto& texture_node = m_texture_nodes[i];

        if (!live_resources[i] && !texture_node.Exported) {
            continue;
        }

        if (texture_node.Imported) {
            physical_textures[i] = texture_node.ImportedTexture;
            current_states[i] = texture_node.InitialState;
        } else if (m_transient_texture_cache != nullptr) {
            physical_textures[i] = m_transient_texture_cache->AcquireTexture(m_frame_context.device, texture_node.Desc);
            current_states[i] =
                physical_textures[i] ? physical_textures[i]->GetCurrentState() : texture_node.InitialState;
        } else {
            physical_textures[i] = createTransientTexture(m_frame_context.device, texture_node.Desc);
            current_states[i] =
                physical_textures[i] ? physical_textures[i]->GetCurrentState() : texture_node.InitialState;
        }

        if (!physical_textures[i]) {
            LUNA_RENDERER_WARN(
                "Render graph texture '{}' (handle {}) is live but has no physical texture", texture_node.Desc.Name, i);
        }
    }

    RenderGraph::PassList pass_list;
    RenderGraph::CompiledRasterPassList compiled_passes;
    compiled_passes.reserve(m_raster_pass_nodes.size());

    for (size_t i = 0; i < m_raster_pass_nodes.size(); ++i) {
        if (!live_passes[i]) {
            LUNA_RENDERER_FRAME_TRACE("Culled render graph pass '{}'", m_raster_pass_nodes[i].Name);
            continue;
        }

        auto& pass_node = m_raster_pass_nodes[i];

        RenderGraphCompiledRasterPass compiled_pass;
        compiled_pass.Pass = RenderGraphPass{
            .Name = pass_node.Name,
            .Type = RenderGraphPassType::Raster,
        };
        compiled_pass.Execute = std::move(pass_node.Execute);
        const auto pass_extent = resolvePassFramebufferExtent(
            pass_node, physical_textures, m_frame_context.framebuffer_width, m_frame_context.framebuffer_height);
        compiled_pass.FramebufferWidth = pass_extent.width;
        compiled_pass.FramebufferHeight = pass_extent.height;
        compiled_pass.RenderingInfo.RenderArea = {0, 0, pass_extent.width, pass_extent.height};
        compiled_pass.RenderingInfo.LayerCount = 1;

        for (const auto& read : pass_node.Reads) {
            addBarrierIfNeeded(compiled_pass.PreTextureBarriers,
                               current_states,
                               physical_textures,
                               read.Handle,
                               luna::RHI::ResourceState::ShaderRead);
        }

        for (const auto& attachment : pass_node.ColorAttachments) {
            addBarrierIfNeeded(compiled_pass.PreTextureBarriers,
                               current_states,
                               physical_textures,
                               attachment.Handle,
                               luna::RHI::ResourceState::RenderTarget);

            if (!isHandleValid(attachment.Handle, physical_textures.size())) {
                continue;
            }

            compiled_pass.RenderingInfo.ColorAttachments.push_back(luna::RHI::RenderingAttachmentInfo{
                .Texture = physical_textures[attachment.Handle.Index],
                .LoadOp = attachment.LoadOp,
                .StoreOp = attachment.StoreOp,
                .ClearValue = attachment.ClearValue,
            });
        }

        if (pass_node.DepthAttachment.has_value()) {
            addBarrierIfNeeded(compiled_pass.PreTextureBarriers,
                               current_states,
                               physical_textures,
                               pass_node.DepthAttachment->Handle,
                               luna::RHI::ResourceState::DepthWrite);

            if (isHandleValid(pass_node.DepthAttachment->Handle, physical_textures.size())) {
                auto depth_attachment = luna::RHI::CreateRef<luna::RHI::RenderingAttachmentInfo>();
                depth_attachment->Texture = physical_textures[pass_node.DepthAttachment->Handle.Index];
                depth_attachment->LoadOp = pass_node.DepthAttachment->LoadOp;
                depth_attachment->StoreOp = pass_node.DepthAttachment->StoreOp;
                depth_attachment->ClearDepthStencil = pass_node.DepthAttachment->ClearValue;
                compiled_pass.RenderingInfo.DepthAttachment = depth_attachment;
            }
        }

        pass_list.push_back(compiled_pass.Pass);
        compiled_passes.push_back(std::move(compiled_pass));
    }

    std::vector<luna::RHI::TextureBarrier> final_texture_barriers;
    for (size_t i = 0; i < texture_count; ++i) {
        if (!m_texture_nodes[i].Exported || !physical_textures[i]) {
            continue;
        }

        const luna::RHI::ResourceState final_state = m_texture_nodes[i].FinalState;
        if (current_states[i] == final_state) {
            continue;
        }

        final_texture_barriers.push_back(luna::RHI::TextureBarrier{
            .Texture = physical_textures[i],
            .OldState = current_states[i],
            .NewState = final_state,
            .SubresourceRange = fullSubresourceRangeForTexture(*physical_textures[i]),
        });
        current_states[i] = final_state;
    }

    const size_t live_resource_count =
        static_cast<size_t>(std::count(live_resources.begin(), live_resources.end(), true));
    const size_t live_pass_count = static_cast<size_t>(std::count(live_passes.begin(), live_passes.end(), true));
    LUNA_RENDERER_FRAME_DEBUG("Built render graph: declared_passes={} compiled_passes={} live_passes={} textures={} "
                              "live_textures={} final_barriers={}",
                              m_raster_pass_nodes.size(),
                              compiled_passes.size(),
                              live_pass_count,
                              texture_count,
                              live_resource_count,
                              final_texture_barriers.size());

    return std::make_unique<RenderGraph>(m_frame_context,
                                         std::move(pass_list),
                                         std::move(physical_textures),
                                         std::move(compiled_passes),
                                         std::move(final_texture_barriers));
}

bool RenderGraphBuilder::isHandleValid(RenderGraphTextureHandle handle, size_t resource_count)
{
    return handle.isValid() && handle.Index < resource_count;
}

} // namespace luna
