#pragma once

// Builds a frame-local render graph from imported textures and declared passes.
// Tracks temporary resources, access patterns, and exported states before
// producing the compiled RenderGraph executed later in the frame.

#include "Renderer/RenderGraph.h"

#include <optional>

namespace luna::detail {

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

struct RenderGraphTextureWriteDesc {
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

struct RenderGraphComputePassNode {
    std::string Name;
    std::vector<RenderGraphTextureReadDesc> Reads;
    std::vector<RenderGraphTextureWriteDesc> Writes;
    std::function<void(RenderGraphComputePassContext&)> Execute;
    bool SideEffect{false};
};

struct RenderGraphPassOrderEntry {
    RenderGraphPassType Type{RenderGraphPassType::Raster};
    size_t Index{0};
};

} // namespace luna::detail

namespace luna {

class RenderGraphTransientTextureCache {
public:
    void BeginFrame();
    luna::RHI::Ref<luna::RHI::Texture> AcquireTexture(const luna::RHI::Ref<luna::RHI::Device>& device,
                                                      const RenderGraphTextureDesc& desc);

private:
    struct TextureEntry {
        RenderGraphTextureDesc Desc;
        luna::RHI::Ref<luna::RHI::Texture> Texture;
        bool InUse{false};
    };

    static bool IsCompatible(const TextureEntry& entry, const RenderGraphTextureDesc& desc);

private:
    std::vector<TextureEntry> m_entries;
};

class RenderGraphRasterPassBuilder {
public:
    RenderGraphRasterPassBuilder& ReadTexture(RenderGraphTextureHandle handle);
    RenderGraphRasterPassBuilder& WriteColor(
        RenderGraphTextureHandle handle,
        luna::RHI::AttachmentLoadOp load_op,
        luna::RHI::AttachmentStoreOp store_op,
        const luna::RHI::ClearValue& clear_value = luna::RHI::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 1.0f));
    RenderGraphRasterPassBuilder& WriteDepth(RenderGraphTextureHandle handle,
                                             luna::RHI::AttachmentLoadOp load_op,
                                             luna::RHI::AttachmentStoreOp store_op,
                                             const luna::RHI::ClearDepthStencilValue& clear_value = {1.0f, 0});

private:
    friend class RenderGraphBuilder;

    explicit RenderGraphRasterPassBuilder(detail::RenderGraphRasterPassNode* pass_node);

    detail::RenderGraphRasterPassNode* m_pass_node{nullptr};
};

class RenderGraphComputePassBuilder {
public:
    RenderGraphComputePassBuilder& ReadTexture(RenderGraphTextureHandle handle);
    RenderGraphComputePassBuilder& WriteTexture(RenderGraphTextureHandle handle);

private:
    friend class RenderGraphBuilder;

    explicit RenderGraphComputePassBuilder(detail::RenderGraphComputePassNode* pass_node);

    detail::RenderGraphComputePassNode* m_pass_node{nullptr};
};

class RenderGraphBuilder {
public:
    using FrameContext = RenderGraph::FrameContext;
    using RasterPassSetupCallback = std::function<void(RenderGraphRasterPassBuilder&)>;
    using RasterPassExecuteCallback = std::function<void(RenderGraphRasterPassContext&)>;
    using ComputePassSetupCallback = std::function<void(RenderGraphComputePassBuilder&)>;
    using ComputePassExecuteCallback = std::function<void(RenderGraphComputePassContext&)>;

    explicit RenderGraphBuilder(FrameContext frame_context,
                                RenderGraphTransientTextureCache* transient_texture_cache = nullptr);

    RenderGraphTextureHandle ImportTexture(std::string name,
                                           luna::RHI::Ref<luna::RHI::Texture> texture,
                                           luna::RHI::ResourceState initial_state,
                                           luna::RHI::ResourceState final_state = luna::RHI::ResourceState::Common);
    RenderGraphTextureHandle CreateTexture(RenderGraphTextureDesc desc);
    RenderGraphBuilder& ExportTexture(RenderGraphTextureHandle handle, luna::RHI::ResourceState final_state);
    RenderGraphBuilder& AddRasterPass(const std::string& name,
                                      RasterPassSetupCallback setup,
                                      RasterPassExecuteCallback execute,
                                      bool side_effect = false);
    RenderGraphBuilder& AddComputePass(const std::string& name,
                                       ComputePassSetupCallback setup,
                                       ComputePassExecuteCallback execute,
                                       bool side_effect = false);

    std::unique_ptr<RenderGraph> Build();

private:
    static bool isHandleValid(RenderGraphTextureHandle handle, size_t resource_count);

private:
    FrameContext m_frame_context;
    RenderGraphTransientTextureCache* m_transient_texture_cache{nullptr};
    std::vector<detail::RenderGraphTextureNode> m_texture_nodes;
    std::vector<detail::RenderGraphRasterPassNode> m_raster_pass_nodes;
    std::vector<detail::RenderGraphComputePassNode> m_compute_pass_nodes;
    std::vector<detail::RenderGraphPassOrderEntry> m_pass_order;
};

} // namespace luna
