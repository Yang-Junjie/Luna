#pragma once

#include "Renderer/RenderFlow/RenderFlowTypes.h"

namespace luna {
class RenderWorld;
}

namespace luna::render_flow {

class RenderFlowBuilder;
class RenderPassBlackboard;

class IRenderFeature {
public:
    virtual ~IRenderFeature() = default;

    virtual bool registerPasses(RenderFlowBuilder& builder) = 0;
    virtual void prepareFrame(const RenderWorld& world,
                              const SceneRenderContext& scene_context,
                              RenderPassBlackboard& blackboard) = 0;
    virtual void shutdown() = 0;
};

} // namespace luna::render_flow
