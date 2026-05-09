#include "Renderer/RenderFlow/Features/RenderFeatureModules.h"

namespace luna::render_flow {

void linkBloomFeature();
void linkScreenSpaceAmbientOcclusionFeature();
void linkEditorInfiniteGridFeature();
void linkTemporalAntiAliasingFeature();

void linkBuiltInRenderFeatureModules()
{
    linkBloomFeature();
    linkScreenSpaceAmbientOcclusionFeature();
    linkEditorInfiniteGridFeature();
    linkTemporalAntiAliasingFeature();
}

} // namespace luna::render_flow
