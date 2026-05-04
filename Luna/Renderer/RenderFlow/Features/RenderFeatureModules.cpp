#include "Renderer/RenderFlow/Features/RenderFeatureModules.h"

namespace luna::render_flow {

void linkScreenSpaceAmbientOcclusionFeature();
void linkEditorInfiniteGridFeature();
void linkTemporalAntiAliasingFeature();

void linkBuiltInRenderFeatureModules()
{
    linkScreenSpaceAmbientOcclusionFeature();
    linkEditorInfiniteGridFeature();
    linkTemporalAntiAliasingFeature();
}

} // namespace luna::render_flow
