#pragma once

#include "Renderer/RenderFlow/RenderFeature.h"

#include <memory>

namespace luna::render_flow {

struct TemporalAntiAliasingOptions {
    bool enabled{false};
};

class TemporalAntiAliasingFeature final : public IRenderFeature {
public:
    class Resources;
    using Options = TemporalAntiAliasingOptions;
    using OptionsHandle = std::shared_ptr<Options>;

    TemporalAntiAliasingFeature();
    explicit TemporalAntiAliasingFeature(Options options);
    explicit TemporalAntiAliasingFeature(OptionsHandle options);
    ~TemporalAntiAliasingFeature() override;

    [[nodiscard]] RenderFeatureContract contract() const noexcept override;
    [[nodiscard]] bool enabled() const noexcept override;
    [[nodiscard]] RenderFeatureDiagnostics diagnostics() const override;
    bool setEnabled(bool enabled) noexcept override;

    bool registerPasses(RenderFlowBuilder& builder) override;
    void prepareFrame(const RenderWorld& world,
                      const SceneRenderContext& scene_context,
                      const RenderFeatureFrameContext& frame_context,
                      RenderPassBlackboard& blackboard) override;
    void commitFrame() override;
    void releasePersistentResources() override;
    void shutdown() override;

private:
    OptionsHandle m_options;
    std::unique_ptr<Resources> m_resources;
};

} // namespace luna::render_flow
