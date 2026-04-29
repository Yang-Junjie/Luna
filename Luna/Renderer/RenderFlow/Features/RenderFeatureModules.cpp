#include "Renderer/RenderFlow/Features/RenderFeatureModules.h"

namespace luna::render_flow {

void linkScreenSpaceAmbientOcclusionFeature();
void linkTemporalAntiAliasingFeature();

void linkBuiltInRenderFeatureModules()
{
    linkScreenSpaceAmbientOcclusionFeature();
    linkTemporalAntiAliasingFeature();
}

} // namespace luna::render_flow
