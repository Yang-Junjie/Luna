#pragma once

#include "Core/Application.h"
#include "Editor/EditorRegistry.h"
#include "Plugin/ServiceRegistry.h"

#include <memory>

namespace luna::app {

class LunaApp final : public Application {
public:
    LunaApp();

protected:
    void onInit() override;

private:
    luna::ServiceRegistry m_service_registry;
    std::shared_ptr<luna::editor::EditorRegistry> m_editor_registry;
};

} // namespace luna::app
