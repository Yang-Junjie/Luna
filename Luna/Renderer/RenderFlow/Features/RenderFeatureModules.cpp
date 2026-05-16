#include "Renderer/RenderFlow/Features/RenderFeatureModules.h"

namespace luna::render_flow {

void linkBloomFeature();
void linkScreenSpaceAmbientOcclusionFeature();
void linkEditorInfiniteGridFeature();
void linkTemporalAntiAliasingFeature();
void linkFXAAFeature();

void linkBuiltInRenderFeatureModules()
{
    linkBloomFeature();
    linkScreenSpaceAmbientOcclusionFeature();
    linkEditorInfiniteGridFeature();
    linkTemporalAntiAliasingFeature();
    linkFXAAFeature();
}

} // namespace luna::render_flow
