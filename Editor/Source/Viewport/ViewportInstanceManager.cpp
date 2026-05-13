#include "Viewport/ViewportInstanceManager.h"

namespace luna {

ViewportInstance& ViewportInstanceManager::defaultViewport()
{
    return m_default_viewport;
}

const ViewportInstance& ViewportInstanceManager::defaultViewport() const
{
    return m_default_viewport;
}

ViewportInstance& ViewportInstanceManager::runtimeViewport()
{
    return m_runtime_viewport;
}

const ViewportInstance& ViewportInstanceManager::runtimeViewport() const
{
    return m_runtime_viewport;
}

} // namespace luna
