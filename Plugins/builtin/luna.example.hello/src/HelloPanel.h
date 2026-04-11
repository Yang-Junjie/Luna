#pragma once

#include "Editor/EditorPanel.h"

namespace luna::example {

class HelloPanel final : public luna::editor::EditorPanel {
public:
    void onImGuiRender() override;
};

} // namespace luna::example
