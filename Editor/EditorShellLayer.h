#pragma once

#include "Core/Layer.h"
#include "Editor/EditorPanel.h"
#include "Editor/EditorRegistry.h"

#include <memory>
#include <string>
#include <vector>

namespace luna::editor {

class EditorShellLayer final : public luna::Layer {
public:
    explicit EditorShellLayer(EditorRegistry& registry);

    void onAttach() override;
    void onDetach() override;
    void onImGuiRender() override;

private:
    struct PanelInstance {
        std::string m_id;
        std::string m_display_name;
        bool m_open = false;
        std::unique_ptr<EditorPanel> m_panel;
    };

    void renderMainMenuBar();

private:
    EditorRegistry& m_registry;
    std::vector<PanelInstance> m_panels;
};

} // namespace luna::editor
