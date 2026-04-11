#pragma once

#include "Editor/EditorPanel.h"

namespace luna::editor {

class RendererInfoPanel final : public EditorPanel {
public:
    void onImGuiRender() override;
};

} // namespace luna::editor
