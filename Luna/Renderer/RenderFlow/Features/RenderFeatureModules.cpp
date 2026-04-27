#include "Renderer/RenderFlow/Features/RenderFeatureModules.h"

namespace luna::render_flow {

void linkScreenSpaceAmbientOcclusionFeature();

void linkBuiltInRenderFeatureModules()
{
    linkScreenSpaceAmbientOcclusionFeature();
}

} // namespace luna::render_flow
