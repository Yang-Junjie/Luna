#include "Core/Log.h"
#include "Editor/EditorShellLayer.h"
#include "imgui.h"

#include <utility>

namespace luna::editor {

EditorShellLayer::EditorShellLayer(EditorRegistry& registry)
    : Layer("EditorShellLayer"),
      m_registry(registry)
{}

void EditorShellLayer::onAttach()
{
    m_panels.clear();
    m_panels.reserve(m_registry.panels().size());

    for (const auto& registration : m_registry.panels()) {
        if (!registration.m_factory) {
            LUNA_EDITOR_WARN("Skipping panel '{}' because it has no factory", registration.m_id);
            continue;
        }

        auto panel = registration.m_factory();
        if (panel == nullptr) {
            LUNA_EDITOR_WARN("Skipping panel '{}' because its factory returned null", registration.m_id);
            continue;
        }

        panel->onAttach();
        m_panels.push_back(PanelInstance{
            .m_id = registration.m_id,
            .m_display_name = registration.m_display_name,
            .m_open = registration.m_open_by_default,
            .m_panel = std::move(panel),
        });
    }
}

void EditorShellLayer::onDetach()
{
    for (auto& panel : m_panels) {
        if (panel.m_panel != nullptr) {
            panel.m_panel->onDetach();
        }
    }

    m_panels.clear();
}

void EditorShellLayer::onImGuiRender()
{
    renderMainMenuBar();

    for (auto& panel : m_panels) {
        if (!panel.m_open || panel.m_panel == nullptr) {
            continue;
        }

        bool open = panel.m_open;
        const bool visible = ImGui::Begin(panel.m_display_name.c_str(), &open);
        if (visible) {
            panel.m_panel->onImGuiRender();
        }
        ImGui::End();
        panel.m_open = open;
    }
}

void EditorShellLayer::renderMainMenuBar()
{
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("Panels")) {
        for (auto& panel : m_panels) {
            ImGui::MenuItem(panel.m_display_name.c_str(), nullptr, &panel.m_open);
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Commands")) {
        for (const auto& command : m_registry.commands()) {
            if (ImGui::MenuItem(command.m_display_name.c_str())) {
                (void) m_registry.invokeCommand(command.m_id);
            }
        }
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

} // namespace luna::editor
