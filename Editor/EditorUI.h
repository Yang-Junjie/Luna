#pragma once

#include "Asset/Asset.h"
#include "Asset/AssetTypes.h"
#include "EditorStyle.h"
#include "Scene/Entity.h"

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <string>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <imgui.h>

namespace luna::editor::ui {

[[nodiscard]] inline float scale(float value) noexcept
{
    return editor::scaleEditorUi(value);
}

[[nodiscard]] inline ImVec2 scaled(float x, float y) noexcept
{
    return editor::scaleEditorUi(x, y);
}

enum class ButtonVariant {
    Default,
    Primary,
    Danger,
    Subtle,
};

struct PropertyLayout {
    float label_width = 112.0f;

    [[nodiscard]] float scaledLabelWidth() const noexcept
    {
        return scale(label_width);
    }
};

[[nodiscard]] std::string assetDisplayLabel(AssetHandle handle);

bool beginPropertyRow(const char* label, const PropertyLayout& layout = {});
void endPropertyRow();

bool drawBool(const char* label, bool& value, const PropertyLayout& layout = {});
bool drawInt(const char* label, int& value, int step = 1, int step_fast = 100, const PropertyLayout& layout = {});
bool drawFloat(const char* label,
               float& value,
               float drag_speed = 0.1f,
               float min_value = 0.0f,
               float max_value = 0.0f,
               const char* format = "%.3f",
               const PropertyLayout& layout = {});
bool drawColor3(const char* label, glm::vec3& value, const PropertyLayout& layout = {});
bool drawTextValue(const char* label, const std::string& value, const PropertyLayout& layout = {});
bool drawTextInput(const char* label,
                   std::string& value,
                   std::size_t buffer_size = 256,
                   const PropertyLayout& layout = {},
                   bool* deactivated_after_edit = nullptr);
bool drawTextMultiline(const char* label,
                       std::string& value,
                       std::size_t buffer_size = 512,
                       float visible_lines = 4.0f,
                       const PropertyLayout& layout = {});
bool drawCombo(const char* label,
               const char* preview_value,
               const std::function<bool()>& draw_options,
               const PropertyLayout& layout = {});
bool drawButton(const char* label, ButtonVariant variant = ButtonVariant::Default, const ImVec2& size = ImVec2{0.0f, 0.0f});

bool drawVec2Control(const char* label,
                     glm::vec2& values,
                     float drag_speed = 0.1f,
                     float min_value = 0.0f,
                     float max_value = 0.0f,
                     const char* format = "%.2f",
                     const PropertyLayout& layout = {});

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

    const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
                                     ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap;

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, scaled(2.0f, 5.0f));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4{0.0f, 0.0f, 0.0f, 0.0f});
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
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, scale(10.0f));
        changed |= ui_function(component);
        ImGui::PopStyleVar();
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
