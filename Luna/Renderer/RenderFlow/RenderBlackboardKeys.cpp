#include "Renderer/RenderFlow/RenderBlackboardKeys.h"

#include "Renderer/RenderFlow/RenderPass.h"

namespace luna::render_flow::blackboard {

RenderResourceKey<RenderGraphTextureHandle> sceneColorStageKey(SceneColorStage stage) noexcept
{
    switch (stage) {
        case SceneColorStage::Lit:
            return SceneLitColor;
        case SceneColorStage::SkyComposited:
            return SceneSkyCompositedColor;
        case SceneColorStage::TemporalResolved:
            return SceneTemporalResolvedColor;
        case SceneColorStage::TransparentComposited:
            return SceneTransparentCompositedColor;
        case SceneColorStage::Final:
            return SceneFinalColor;
    }
    return SceneColor;
}

void initializeSceneColorStageAliases(RenderPassBlackboard& blackboard, RenderGraphTextureHandle external_color_target)
{
    blackboard.set(SceneColor, external_color_target);
    blackboard.set(SceneLitColor, external_color_target);
    blackboard.set(SceneSkyCompositedColor, external_color_target);
    blackboard.set(SceneTemporalResolvedColor, external_color_target);
    blackboard.set(SceneTransparentCompositedColor, external_color_target);
    blackboard.set(SceneFinalColor, external_color_target);
}

void publishSceneColorStage(RenderPassBlackboard& blackboard,
                            SceneColorStage stage,
                            RenderGraphTextureHandle color_handle)
{
    blackboard.set(sceneColorStageKey(stage), color_handle);
    blackboard.set(SceneColor, color_handle);

    if (stage == SceneColorStage::Final) {
        return;
    }

    if (stage == SceneColorStage::TransparentComposited) {
        blackboard.set(SceneFinalColor, color_handle);
    }
}

} // namespace luna::render_flow::blackboard
