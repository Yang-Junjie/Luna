#pragma once

#include "Renderer/RenderFlow/RenderFeature.h"

#include <memory>

namespace luna::render_flow {

struct ScreenSpaceAmbientOcclusionOptions {
    bool enabled{true};
    float radius{1.2f};
    float intensity{1.2f};
    float bias{0.08f};
    float power{1.0f};
};

class ScreenSpaceAmbientOcclusionFeature final : public IRenderFeature {
public:
    class Resources;
    using Options = ScreenSpaceAmbientOcclusionOptions;
    using OptionsHandle = std::shared_ptr<Options>;

    ScreenSpaceAmbientOcclusionFeature();
    explicit ScreenSpaceAmbientOcclusionFeature(Options options);
    explicit ScreenSpaceAmbientOcclusionFeature(OptionsHandle options);
    ~ScreenSpaceAmbientOcclusionFeature() override;

    [[nodiscard]] RenderFeatureInfo info() const noexcept override;
    [[nodiscard]] std::vector<RenderFeatureParameterInfo> parameters() const override;
    [[nodiscard]] RenderFeatureRequirements requirements() const noexcept override;
    bool setEnabled(bool enabled) noexcept override;
    bool setParameter(std::string_view name, const RenderFeatureParameterValue& value) noexcept override;
    [[nodiscard]] Options& options() noexcept;
    [[nodiscard]] const Options& options() const noexcept;

    bool registerPasses(RenderFlowBuilder& builder) override;
    void prepareFrame(const RenderWorld& world,
                      const SceneRenderContext& scene_context,
                      const RenderFeatureFrameContext& frame_context,
                      RenderPassBlackboard& blackboard) override;
    void releasePersistentResources() override;
    void shutdown() override;

private:
    OptionsHandle m_options;
    std::unique_ptr<Resources> m_resources;
};

} // namespace luna::render_flow
