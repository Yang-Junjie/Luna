#pragma once

#include "Core/Layer.h"
#include "imgui.h"

namespace luna::samples::texture {

class TextureLayer final : public Layer {
public:
    TextureLayer()
        : Layer("TextureLayer")
    {}

    void onImGuiRender() override
    {
        if (ImGui::Begin("Texture")) {
            ImGui::TextUnformatted("Fixed textured quad sample.");
            ImGui::Separator();
            ImGui::TextUnformatted("Asset: assets/head.jpg");
            ImGui::TextUnformatted("No camera controls.");
        }
        ImGui::End();
    }
};

} // namespace luna::samples::texture
