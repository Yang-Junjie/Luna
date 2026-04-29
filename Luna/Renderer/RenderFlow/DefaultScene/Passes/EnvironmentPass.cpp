#include "Renderer/RenderFlow/DefaultScene/Environment.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/EnvironmentPass.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/PassCommon.h"
#include "Renderer/RenderGraphBuilder.h"

namespace luna::render_flow::default_scene {

EnvironmentPass::EnvironmentPass(PassSharedState& state)
    : m_state(&state)
{}

const char* EnvironmentPass::name() const noexcept
{
    return "SceneEnvironment";
}

void EnvironmentPass::setup(RenderPassContext& context)
{
    context.graph().AddComputePass(
        name(),
        [](RenderGraphComputePassBuilder&) {},
        [this, scene_context = context.sceneContext()](RenderGraphComputePassContext& pass_context) {
            execute(pass_context, scene_context);
        },
        true);
}

void EnvironmentPass::execute(RenderGraphComputePassContext& pass_context, const SceneRenderContext& context)
{
    EnvironmentResources& environment = m_state->environment();
    environment.ensure(context);
    environment.uploadIfNeeded(pass_context.commandBuffer());
    environment.precomputeIfNeeded(pass_context.commandBuffer());
    updateEnvironmentBindings(*m_state);
    updateSceneParameters(*m_state, context);
}

} // namespace luna::render_flow::default_scene
