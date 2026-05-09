#pragma once

#include "Renderer/RenderFlow/RenderFeature.h"

#include <cstdint>

#include <memory>

namespace luna::render_flow {

struct BloomOptions {
    bool enabled{true};
    float threshold{1.0f};
    float soft_knee{0.5f};
    float intensity{0.10f};
    float radius{0.65f};
    int32_t mip_count{5};
};

class BloomFeature final : public IRenderFeature {
public:
    class Resources;
    using Options = BloomOptions;
    using OptionsHandle = std::shared_ptr<Options>;

    BloomFeature();
    explicit BloomFeature(Options options);
    explicit BloomFeature(OptionsHandle options);
    ~BloomFeature() override;

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
