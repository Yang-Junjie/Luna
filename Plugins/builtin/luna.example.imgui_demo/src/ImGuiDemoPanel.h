#pragma once

#include "Editor/EditorPanel.h"

namespace luna::example {

class ImGuiDemoPanel final : public luna::editor::EditorPanel {
public:
    void onImGuiRender() override;

private:
    bool m_show_demo_window = true;
};

} // namespace luna::example
