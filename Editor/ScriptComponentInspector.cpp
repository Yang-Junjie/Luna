#include "ScriptComponentInspector.h"

#include "Asset/AssetDatabase.h"
#include "EditorAssetDragDrop.h"
#include "Scene/Components/ScriptComponent.h"

#include <imgui.h>

#include <cstddef>
#include <cstring>
#include <string>
#include <utility>

namespace {

std::string getAssetDisplayLabel(luna::AssetHandle handle)
{
    if (!handle.isValid()) {
        return "None";
    }

    if (!luna::AssetDatabase::exists(handle)) {
        return "Unknown Asset";
    }

    const auto& metadata = luna::AssetDatabase::getAssetMetadata(handle);
    if (!metadata.Name.empty()) {
        return metadata.Name;
    }

    if (!metadata.FilePath.empty()) {
        return metadata.FilePath.generic_string();
    }

    return "Unnamed Asset";
}

bool drawScriptAssetHandleEditor(luna::AssetHandle& handle)
{
    bool changed = false;
    unsigned long long raw_handle = static_cast<unsigned long long>(static_cast<uint64_t>(handle));
    if (ImGui::InputScalar("Script Asset", ImGuiDataType_U64, &raw_handle)) {
        handle = luna::AssetHandle(static_cast<uint64_t>(raw_handle));
        changed = true;
    }

    if (ImGui::BeginDragDropTarget()) {
        luna::editor::AssetDragDropData payload{};
        if (luna::editor::acceptAssetDragDropPayload(payload, {luna::AssetType::Script})) {
            handle = luna::editor::getAssetHandle(payload);
            changed = true;
        }
        ImGui::EndDragDropTarget();
    }

    return changed;
}

} // namespace

namespace luna {

void drawScriptComponentInspector(ScriptComponent& script_component)
{
    ImGui::Checkbox("Enabled", &script_component.enabled);
    ImGui::Text("Scripts: %zu", script_component.scripts.size());

    if (ImGui::Button("Add Script", ImVec2(-1.0f, 0.0f))) {
        ScriptEntry entry{};
        entry.id = UUID{};
        script_component.scripts.push_back(std::move(entry));
    }

    for (size_t script_index = 0; script_index < script_component.scripts.size(); ++script_index) {
        auto& script = script_component.scripts[script_index];
        ImGui::PushID(static_cast<int>(script_index));
        ImGui::Separator();
        ImGui::Checkbox("Script Enabled", &script.enabled);

        char type_buffer[256] = {};
        strncpy_s(type_buffer, script.typeName.c_str(), _TRUNCATE);
        if (ImGui::InputText("Type Name", type_buffer, sizeof(type_buffer))) {
            script.typeName = type_buffer;
        }

        drawScriptAssetHandleEditor(script.scriptAsset);
        ImGui::TextDisabled("Script Asset: %s", getAssetDisplayLabel(script.scriptAsset).c_str());
        ImGui::InputInt("Execution Order", &script.executionOrder);
        ImGui::TextDisabled("Properties: %zu", script.properties.size());

        if (ImGui::Button("Remove Script", ImVec2(-1.0f, 0.0f))) {
            script_component.scripts.erase(script_component.scripts.begin() + static_cast<std::ptrdiff_t>(script_index));
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
}

} // namespace luna
