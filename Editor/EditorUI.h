#pragma once

#include "Asset/Asset.h"
#include "Asset/AssetTypes.h"
#include "Scene/Entity.h"

#include <functional>
#include <initializer_list>
#include <string>

#include <glm/vec3.hpp>
#include <imgui.h>

namespace luna::editor::ui {

struct PropertyLayout {
    float label_width = 112.0f;
};

[[nodiscard]] std::string assetDisplayLabel(AssetHandle handle);

bool beginPropertyRow(const char* label, const PropertyLayout& layout = {});
void endPropertyRow();

bool drawVec3Control(const char* label,
                     glm::vec3& values,
                     float reset_value = 0.0f,
                     float drag_speed = 0.1f,
                     const PropertyLayout& layout = {});

bool drawAssetHandleEditor(const char* label,
                           AssetHandle& handle,
                           std::initializer_list<AssetType> accepted_types = {},
                           const std::function<bool(AssetHandle)>& accepts_handle = {},
                           const PropertyLayout& layout = {});

template <typename T, typename UIFunction>
bool drawComponentSection(const char* label, Entity entity, UIFunction&& ui_function, bool allow_remove)
{
    if (!entity.hasComponent<T>()) {
        return false;
    }

    auto& component = entity.getComponent<T>();
    bool changed = false;
    ImGui::PushID(label);

    const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
                                     ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding |
                                     ImGuiTreeNodeFlags_AllowOverlap;

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{8.0f, 6.0f});
    ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
    const bool open = ImGui::TreeNodeEx("##Section", flags, "%s", label);
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    bool remove_component = false;
    if (allow_remove) {
        if (ImGui::BeginPopupContextItem("ComponentSettings")) {
            if (ImGui::MenuItem("Remove Component")) {
                remove_component = true;
            }
            ImGui::EndPopup();
        }
    }

    if (open) {
        ImGui::Spacing();
        changed |= ui_function(component);
        ImGui::TreePop();
    }

    if (remove_component) {
        entity.removeComponent<T>();
        changed = true;
    }

    ImGui::PopID();
    return changed;
}

} // namespace luna::editor::ui
