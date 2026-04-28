#pragma once

#include "Renderer/Renderer.h"

namespace luna {

class RenderDebugPanel {
public:
    void onImGuiRender(bool& open, Renderer& renderer);
};

} // namespace luna
