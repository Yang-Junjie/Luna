#pragma once

#include "Renderer/RenderGraph.h"

#include <optional>

namespace luna::rhi::detail {

struct RenderGraphTextureNode {
    RenderGraphTextureDesc Desc;
    luna::RHI::Ref<luna::RHI::Texture> ImportedTexture;
    luna::RHI::ResourceState InitialState{luna::RHI::ResourceState::Undefined};
    luna::RHI::ResourceState FinalState{luna::RHI::ResourceState::Common};
    bool Imported{false};
    bool Exported{false};
};

struct RenderGraphTextureReadDesc {
    RenderGraphTextureHandle Handle{};
};

struct RenderGraphColorAttachmentDesc {
    RenderGraphTextureHandle Handle{};
    luna::RHI::AttachmentLoadOp LoadOp{luna::RHI::AttachmentLoadOp::Clear};
    luna::RHI::AttachmentStoreOp StoreOp{luna::RHI::AttachmentStoreOp::Store};
    luna::RHI::ClearValue ClearValue = luna::RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 1.0f);
};

struct RenderGraphDepthAttachmentDesc {
    RenderGraphTextureHandle Handle{};
    luna::RHI::AttachmentLoadOp LoadOp{luna::RHI::AttachmentLoadOp::Clear};
    luna::RHI::AttachmentStoreOp StoreOp{luna::RHI::AttachmentStoreOp::Store};
    luna::RHI::ClearDepthStencilValue ClearValue{1.0f, 0};
};

struct RenderGraphRasterPassNode {
    std::string Name;
    std::vector<RenderGraphTextureReadDesc> Reads;
    std::vector<RenderGraphColorAttachmentDesc> ColorAttachments;
    std::optional<RenderGraphDepthAttachmentDesc> DepthAttachment;
    std::function<void(RenderGraphRasterPassContext&)> Execute;
    bool SideEffect{false};
};

} // namespace luna::rhi::detail

namespace luna::rhi {

class RenderGraphRasterPassBuilder {
public:
    RenderGraphRasterPassBuilder& ReadTexture(RenderGraphTextureHandle handle);
    RenderGraphRasterPassBuilder& WriteColor(
        RenderGraphTextureHandle handle,
        luna::RHI::AttachmentLoadOp load_op,
        luna::RHI::AttachmentStoreOp store_op,
        const luna::RHI::ClearValue& clear_value = luna::RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 1.0f));
    RenderGraphRasterPassBuilder&
        WriteDepth(RenderGraphTextureHandle handle,
                   luna::RHI::AttachmentLoadOp load_op,
                   luna::RHI::AttachmentStoreOp store_op,
                   const luna::RHI::ClearDepthStencilValue& clear_value = {1.0f, 0});

private:
    friend class RenderGraphBuilder;

    explicit RenderGraphRasterPassBuilder(detail::RenderGraphRasterPassNode* pass_node);

    detail::RenderGraphRasterPassNode* m_pass_node{nullptr};
};

class RenderGraphBuilder {
public:
    using FrameContext = RenderGraph::FrameContext;
    using RasterPassSetupCallback = std::function<void(RenderGraphRasterPassBuilder&)>;
    using RasterPassExecuteCallback = std::function<void(RenderGraphRasterPassContext&)>;

    explicit RenderGraphBuilder(FrameContext frame_context);

    RenderGraphTextureHandle ImportTexture(std::string name,
                                           luna::RHI::Ref<luna::RHI::Texture> texture,
                                           luna::RHI::ResourceState initial_state,
                                           luna::RHI::ResourceState final_state = luna::RHI::ResourceState::Common);
    RenderGraphTextureHandle CreateTexture(RenderGraphTextureDesc desc);
    RenderGraphBuilder& ExportTexture(RenderGraphTextureHandle handle, luna::RHI::ResourceState final_state);
    RenderGraphBuilder&
        AddRasterPass(const std::string& name,
                      RasterPassSetupCallback setup,
                      RasterPassExecuteCallback execute,
                      bool side_effect = false);

    std::unique_ptr<RenderGraph> Build();

private:
    static bool isHandleValid(RenderGraphTextureHandle handle, size_t resource_count);

private:
    FrameContext m_frame_context;
    std::vector<detail::RenderGraphTextureNode> m_texture_nodes;
    std::vector<detail::RenderGraphRasterPassNode> m_raster_pass_nodes;
};

} // namespace luna::rhi
