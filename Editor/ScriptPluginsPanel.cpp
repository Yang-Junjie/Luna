#include "ScriptPluginsPanel.h"

#include "LunaEditorLayer.h"

#include <imgui.h>

namespace {

const char* scriptPluginScopeToString(luna::ScriptPluginScope scope)
{
    switch (scope) {
        case luna::ScriptPluginScope::Engine:
            return "Engine";
        case luna::ScriptPluginScope::Project:
            return "Project";
        default:
            return "Unknown";
    }
}

} // namespace

namespace luna {

ScriptPluginsPanel::ScriptPluginsPanel(LunaEditorLayer& editor_layer)
    : m_editor_layer(&editor_layer)
{}

void ScriptPluginsPanel::onImGuiRender(bool& open)
{
    if (!open || m_editor_layer == nullptr) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Script Plugins", &open)) {
        ImGui::End();
        return;
    }

    if (!m_editor_layer->hasProjectLoaded()) {
        ImGui::TextUnformatted("Open a project to configure script plugins.");
        ImGui::End();
        return;
    }

    if (ImGui::Button("Refresh")) {
        m_editor_layer->refreshProjectScriptPlugins();
    }

    const ScriptPluginCandidate* selected_candidate = m_editor_layer->getSelectedScriptPluginCandidate();

    ImGui::SameLine();
    if (selected_candidate != nullptr) {
        ImGui::Text("Selected: %s", selected_candidate->Manifest.DisplayName.c_str());
    } else {
        ImGui::TextUnformatted("Selected: <none>");
    }

    const std::string& status = m_editor_layer->getScriptPluginStatus();
    if (!status.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", status.c_str());
    }

    ImGui::Separator();

    const auto& candidates = m_editor_layer->getDiscoveredScriptPlugins();
    if (candidates.empty()) {
        ImGui::TextUnformatted("No script plugin candidates were discovered.");
        ImGui::End();
        return;
    }

    for (const auto& candidate : candidates) {
        const bool is_selected = selected_candidate != nullptr &&
                                 selected_candidate->Manifest.PluginId == candidate.Manifest.PluginId;

        ImGui::PushID(candidate.Manifest.PluginId.c_str());
        if (ImGui::RadioButton("##selected", is_selected)) {
            m_editor_layer->selectScriptPlugin(&candidate);
        }
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::Text("%s", candidate.Manifest.DisplayName.c_str());
        ImGui::TextDisabled("%s | %s | %s",
                            candidate.Manifest.Language.c_str(),
                            candidate.Manifest.BackendName.c_str(),
                            scriptPluginScopeToString(candidate.Scope));
        ImGui::TextWrapped("Id: %s", candidate.Manifest.PluginId.c_str());
        if (!candidate.Manifest.Version.empty()) {
            ImGui::TextWrapped("Version: %s", candidate.Manifest.Version.c_str());
        }
        if (!candidate.ResolvedEntryPath.empty()) {
            ImGui::TextWrapped("Entry: %s", candidate.ResolvedEntryPath.string().c_str());
            if (!candidate.EntryExists) {
                ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "Entry DLL not found yet.");
            }
        }
        ImGui::EndGroup();
        ImGui::Spacing();
        ImGui::PopID();
    }

    ImGui::End();
}

} // namespace luna
