#pragma once

#include "Renderer/Renderer.h"

namespace luna {

class BackendCapabilitiesPanel {
public:
    void onImGuiRender(bool& open, const Renderer& renderer);
};

} // namespace luna
