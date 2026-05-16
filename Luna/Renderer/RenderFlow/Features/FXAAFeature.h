#pragma once

#include "Renderer/RenderFlow/RenderFeature.h"

#include <memory>

namespace luna::render_flow {

struct FXAAOptions {
    bool enabled{true};
    float subpixel_quality{0.75f};
    float edge_threshold{0.166f};
    float edge_threshold_min{0.0833f};
};

class FXAAFeature final : public IRenderFeature {
public:
    class Resources;
    using Options = FXAAOptions;
    using OptionsHandle = std::shared_ptr<Options>;

    FXAAFeature();
    explicit FXAAFeature(Options options);
    explicit FXAAFeature(OptionsHandle options);
    ~FXAAFeature() override;

    [[nodiscard]] RenderFeatureContract contract() const noexcept override;
    [[nodiscard]] bool enabled() const noexcept override;
    [[nodiscard]] std::vector<RenderFeatureParameterInfo> parameters() const override;
    [[nodiscard]] RenderFeatureDiagnostics diagnostics() const override;
    bool setEnabled(bool enabled) noexcept override;
    bool setParameter(std::string_view name, const RenderFeatureParameterValue& value) noexcept override;
    [[nodiscard]] Options& options() noexcept;
    [[nodiscard]] const Options& options() const noexcept;

    bool registerPasses(RenderFlowBuilder& builder) override;
    void prepareFrame(const RenderWorld& world,
                      const SceneRenderContext& scene_context,
                      const RenderFeatureFrameContext& frame_context,
                      RenderPassBlackboard& blackboard) override;
    void shutdown() override;

private:
    OptionsHandle m_options;
    std::unique_ptr<Resources> m_resources;
};

} // namespace luna::render_flow
