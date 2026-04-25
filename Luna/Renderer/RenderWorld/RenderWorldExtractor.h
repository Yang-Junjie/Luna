#pragma once

#include "Renderer/Camera.h"
#include "Scene/Scene.h"

namespace luna {

class RenderWorld;

class RenderWorldExtractor {
public:
    void extract(Scene& scene, const Camera& camera, RenderWorld& render_world) const;
};

} // namespace luna




