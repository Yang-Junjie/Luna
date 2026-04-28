#pragma once

#include "Renderer/Renderer.h"

namespace luna {

class RenderDebugPanel {
public:
    void onImGuiRender(bool& open, Renderer& renderer, luna::RHI::BackendType backend_type);
};

} // namespace luna
