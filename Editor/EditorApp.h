#pragma once

#include "Core/Application.h"
#include "Editor/EditorRegistry.h"
#include "Plugin/ServiceRegistry.h"

namespace luna::editor {

class EditorApp final : public Application {
public:
    EditorApp();

protected:
    void onInit() override;

private:
    luna::ServiceRegistry m_service_registry;
    luna::editor::EditorRegistry m_editor_registry;
};

} // namespace luna::editor

