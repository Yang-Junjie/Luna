#pragma once

#include "Viewport/ViewportInstance.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace luna {

class ViewportInstanceManager final {
public:
    ViewportInstance& defaultViewport();
    const ViewportInstance& defaultViewport() const;
    ViewportInstance& runtimeViewport();
    const ViewportInstance& runtimeViewport() const;

private:
    ViewportInstance m_default_viewport;
    ViewportInstance m_runtime_viewport;
};

} // namespace luna
