#pragma once

#include "Renderer/RenderPipeline.h"
#include "SyncLabState.h"

#include <memory>
#include <string>

namespace sync_lab {

class RhiSyncLabRenderPipeline final : public luna::IRenderPipeline {
public:
    explicit RhiSyncLabRenderPipeline(std::shared_ptr<State> state);
    ~RhiSyncLabRenderPipeline() override;

    bool init(luna::IRHIDevice& device) override;
    void shutdown(luna::IRHIDevice& device) override;
    bool render(luna::IRHIDevice& device, const luna::FrameContext& frameContext) override;

private:
    bool ensure_shared_resources(luna::IRHIDevice& device, luna::PixelFormat backbufferFormat);
    bool ensure_history_resources(luna::IRHIDevice& device);
    bool ensure_readback_resources(luna::IRHIDevice& device);
    bool ensure_indirect_resources(luna::IRHIDevice& device);
    bool ensure_subresource_resources(luna::IRHIDevice& device);

    bool render_history_copy(luna::IRHIDevice& device, const luna::FrameContext& frameContext);
    bool render_readback(luna::IRHIDevice& device, const luna::FrameContext& frameContext);
    bool render_indirect(luna::IRHIDevice& device, const luna::FrameContext& frameContext);
    bool render_subresource(luna::IRHIDevice& device, const luna::FrameContext& frameContext);

    void destroy_shared_resources(luna::IRHIDevice& device);
    void destroy_history_resources(luna::IRHIDevice& device);
    void destroy_readback_resources(luna::IRHIDevice& device);
    void destroy_indirect_resources(luna::IRHIDevice& device);
    void destroy_subresource_resources(luna::IRHIDevice& device);

private:
    struct Impl;
    std::shared_ptr<State> m_state;
    std::string m_shaderRoot;
    std::unique_ptr<Impl> m_impl;
};

} // namespace sync_lab
