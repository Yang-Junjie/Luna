#pragma once

#include "Core/application.h"
#include "Core/layer.h"
#include "imgui.h"

namespace luna::editor {

class EditorLayer final : public Layer {
public:
    EditorLayer()
        : Layer("EditorLayer")
    {}

    void onImGuiRender() override
    {
        auto& engine = luna::Application::get().getEngine();
        auto& effects = engine.get_background_effects();

        if (ImGui::Begin("background")) {
            if (effects.empty()) {
                ImGui::TextUnformatted("No background effects available.");
                ImGui::End();
                return;
            }

            auto& currentEffectIndex = engine.get_current_background_effect();
            const int lastEffectIndex = static_cast<int>(effects.size()) - 1;

            if (currentEffectIndex < 0) {
                currentEffectIndex = 0;
            }
            if (currentEffectIndex > lastEffectIndex) {
                currentEffectIndex = lastEffectIndex;
            }

            ImGui::Text("Selected effect: %s", effects[static_cast<size_t>(currentEffectIndex)].name);
            ImGui::SliderInt("Effect Index", &currentEffectIndex, 0, lastEffectIndex);

            ImGui::Separator();

            auto& selected = effects[static_cast<size_t>(currentEffectIndex)];

            ImGui::SliderFloat4("data1", &selected.data.data1.x, 0.0f, 1.0f);
            ImGui::SliderFloat4("data2", &selected.data.data2.x, 0.0f, 1.0f);
            ImGui::SliderFloat4("data3", &selected.data.data3.x, 0.0f, 1.0f);
            ImGui::SliderFloat4("data4", &selected.data.data4.x, 0.0f, 1.0f);
        }

        ImGui::End();
    }
};

} // namespace luna::editor
