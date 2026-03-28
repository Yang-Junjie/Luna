#pragma once

#include "Core/layer.h"
#include "imgui.h"

namespace luna::editor {

class EditorLayer final : public Layer {
public:
    EditorLayer()
        : Layer("EditorLayer")
    {}

    virtual void onImGuiRender()
    {
        ImGui::ShowDemoWindow();
    }
};

} // namespace luna::editor
