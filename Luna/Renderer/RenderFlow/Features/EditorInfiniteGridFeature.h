#pragma once

#include "Renderer/RenderFlow/RenderFeature.h"

#include <memory>

namespace luna::render_flow {

struct EditorInfiniteGridOptions {
    bool enabled{false};
    float grid_scale{1.0f};
    float fade_distance{500.0f};
    float opacity{1.0f};
};

class EditorInfiniteGridFeature final : public IRenderFeature {
public:
    class Resources;
    using Options = EditorInfiniteGridOptions;
    using OptionsHandle = std::shared_ptr<Options>;

    EditorInfiniteGridFeature();
    explicit EditorInfiniteGridFeature(Options options);
    explicit EditorInfiniteGridFeature(OptionsHandle options);
    ~EditorInfiniteGridFeature() override;

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
