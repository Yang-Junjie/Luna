#pragma once

#include "Core/Layer.h"
#include "imgui.h"

namespace luna::samples::triangle {

class TriangleLayer final : public Layer {
public:
    TriangleLayer()
        : Layer("TriangleLayer")
    {}

    void onImGuiRender() override
    {
        if (ImGui::Begin("Triangle")) {
            ImGui::TextUnformatted("Fixed triangle sample.");
            ImGui::Separator();
            ImGui::TextUnformatted("No camera controls.");
        }
        ImGui::End();
    }
};

} // namespace luna::samples::triangle
