#pragma once

#include "Renderer/RenderFlow/RenderFeature.h"

#include <memory>

namespace luna::render_flow {

class ScreenSpaceAmbientOcclusionFeature final : public IRenderFeature {
public:
    class Resources;

    struct Options {
        bool enabled{true};
        float radius{1.5f};
        float intensity{1.25f};
        float bias{0.03f};
        float power{1.15f};
    };

    ScreenSpaceAmbientOcclusionFeature();
    explicit ScreenSpaceAmbientOcclusionFeature(Options options);
    ~ScreenSpaceAmbientOcclusionFeature() override;

    bool registerPasses(RenderFlowBuilder& builder) override;
    void prepareFrame(const RenderWorld& world,
                      const SceneRenderContext& scene_context,
                      RenderPassBlackboard& blackboard) override;
    void shutdown() override;

private:
    Options m_options;
    std::unique_ptr<Resources> m_resources;
};

} // namespace luna::render_flow
