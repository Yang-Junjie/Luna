#include "EditorUI.h"

#include "Asset/AssetDatabase.h"
#include "EditorAssetDragDrop.h"

#include <algorithm>
#include <cstdint>

namespace luna::editor::ui {
namespace {

ImVec4 withAlpha(ImVec4 color, float alpha)
{
    color.w = alpha;
    return color;
}

ImVec4 mixColor(ImVec4 lhs, ImVec4 rhs, float amount)
{
    const float inverse_amount = 1.0f - amount;
    return ImVec4{
        lhs.x * inverse_amount + rhs.x * amount,
        lhs.y * inverse_amount + rhs.y * amount,
        lhs.z * inverse_amount + rhs.z * amount,
        lhs.w * inverse_amount + rhs.w * amount,
    };
}

ImVec4 axisColor(char axis)
{
    switch (axis) {
        case 'X':
            return ImVec4{0.78f, 0.22f, 0.26f, 1.0f};
        case 'Y':
            return ImVec4{0.28f, 0.66f, 0.32f, 1.0f};
        case 'Z':
            return ImVec4{0.25f, 0.46f, 0.84f, 1.0f};
        default:
            return ImGui::GetStyleColorVec4(ImGuiCol_Button);
    }
}

ImVec4 assetAccentColor(AssetType type)
{
    switch (type) {
        case AssetType::Texture:
            return ImVec4{0.86f, 0.57f, 0.22f, 1.0f};
        case AssetType::Mesh:
            return ImVec4{0.28f, 0.53f, 0.86f, 1.0f};
        case AssetType::Material:
            return ImVec4{0.34f, 0.70f, 0.43f, 1.0f};
        case AssetType::Model:
            return ImVec4{0.58f, 0.48f, 0.86f, 1.0f};
        case AssetType::Scene:
            return ImVec4{0.74f, 0.59f, 0.34f, 1.0f};
        case AssetType::Script:
            return ImVec4{0.25f, 0.68f, 0.74f, 1.0f};
        case AssetType::None:
            break;
    }

    return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
}

struct AssetDisplayInfo {
    std::string label;
    std::string detail;
    AssetType type = AssetType::None;
    bool missing = false;
};

std::string assetHandleText(AssetHandle handle)
{
    return std::to_string(static_cast<uint64_t>(handle));
}

AssetDisplayInfo describeAsset(AssetHandle handle)
{
    AssetDisplayInfo info{};
    if (!handle.isValid()) {
        info.label = "None";
        info.detail = "No asset";
        return info;
    }

    if (!AssetDatabase::exists(handle)) {
        info.label = "Missing asset";
        info.detail = "#" + assetHandleText(handle);
        info.missing = true;
        return info;
    }

    const auto& metadata = AssetDatabase::getAssetMetadata(handle);
    info.type = metadata.Type;
    if (!metadata.Name.empty()) {
        info.label = metadata.Name;
    } else if (!metadata.FilePath.empty()) {
        info.label = metadata.FilePath.generic_string();
    } else {
        info.label = "Unnamed Asset";
    }
    info.detail = std::string(AssetUtils::AssetTypeToString(metadata.Type)) + " #" + assetHandleText(handle);
    return info;
}

bool acceptDroppedAsset(AssetHandle& handle,
                        std::initializer_list<AssetType> accepted_types,
                        const std::function<bool(AssetHandle)>& accepts_handle)
{
    bool changed = false;
    if (ImGui::BeginDragDropTarget()) {
        AssetDragDropData payload{};
        if (acceptAssetDragDropPayload(payload, accepted_types)) {
            const AssetHandle candidate_handle = getAssetHandle(payload);
            if (!accepts_handle || accepts_handle(candidate_handle)) {
                handle = candidate_handle;
                changed = true;
            }
        }
        ImGui::EndDragDropTarget();
    }
    return changed;
}

bool drawAssetPreview(const char* id,
                      AssetHandle& handle,
                      std::initializer_list<AssetType> accepted_types,
                      const std::function<bool(AssetHandle)>& accepts_handle)
{
    const AssetDisplayInfo info = describeAsset(handle);
    const ImGuiStyle& style = ImGui::GetStyle();
    const float width = (std::max)(ImGui::GetContentRegionAvail().x, 64.0f);
    const float height = ImGui::GetTextLineHeight() * 2.0f + style.FramePadding.y * 2.0f + 6.0f;
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const ImVec2 size{width, height};

    ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const bool changed = acceptDroppedAsset(handle, accepted_types, accepts_handle);

    const ImVec2 min = position;
    const ImVec2 max{position.x + size.x, position.y + size.y};
    const ImVec4 frame_bg = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
    const ImVec4 accent = info.missing ? ImVec4{0.86f, 0.32f, 0.32f, 1.0f} : assetAccentColor(info.type);
    const ImVec4 fill = hovered || active ? mixColor(frame_bg, accent, 0.12f) : mixColor(frame_bg, accent, 0.06f);
    const ImVec4 border = hovered || active ? withAlpha(accent, 0.70f) : ImGui::GetStyleColorVec4(ImGuiCol_Border);
    const ImGuiCol label_color_index = handle.isValid() ? ImGuiCol_Text : ImGuiCol_TextDisabled;
    const ImU32 label_color = info.missing ? ImGui::GetColorU32(ImVec4{1.0f, 0.48f, 0.48f, 1.0f})
                                           : ImGui::GetColorU32(label_color_index);
    const ImU32 detail_color = ImGui::GetColorU32(ImGuiCol_TextDisabled);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(min, max, ImGui::GetColorU32(fill), style.FrameRounding);
    draw_list->AddRect(min, max, ImGui::GetColorU32(border), style.FrameRounding);
    draw_list->AddRectFilled(ImVec2{min.x + 6.0f, min.y + 7.0f},
                             ImVec2{min.x + 9.0f, max.y - 7.0f},
                             ImGui::GetColorU32(accent),
                             2.0f);

    const ImVec2 text_min{min.x + 16.0f, min.y + style.FramePadding.y + 3.0f};
    const ImVec2 text_max{max.x - 8.0f, max.y - style.FramePadding.y};
    draw_list->PushClipRect(text_min, text_max, true);
    draw_list->AddText(text_min, label_color, info.label.c_str());
    draw_list->AddText(ImVec2{text_min.x, text_min.y + ImGui::GetTextLineHeight() + 3.0f},
                       detail_color,
                       info.detail.c_str());
    draw_list->PopClipRect();

    return changed;
}

bool drawAxisControl(const char* label,
                     char axis,
                     float& value,
                     float reset_value,
                     float drag_speed,
                     float width,
                     bool last)
{
    bool changed = false;
    const float line_height = ImGui::GetFrameHeight();
    const ImVec2 button_size{line_height, line_height};
    const ImVec4 color = axisColor(axis);

    ImGui::PushID(label);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{3.0f, 0.0f});
    ImGui::PushStyleColor(ImGuiCol_Button, withAlpha(color, 0.72f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, withAlpha(color, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, withAlpha(color, 1.0f));
    if (ImGui::Button(label, button_size)) {
        value = reset_value;
        changed = true;
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    ImGui::SetNextItemWidth((std::max)(width - button_size.x - 3.0f, 24.0f));
    changed |= ImGui::DragFloat("##value", &value, drag_speed, 0.0f, 0.0f, "%.2f");
    ImGui::PopStyleVar();
    ImGui::PopID();

    if (!last) {
        ImGui::SameLine();
    }
    return changed;
}

} // namespace

std::string assetDisplayLabel(AssetHandle handle)
{
    return describeAsset(handle).label;
}

bool beginPropertyRow(const char* label, const PropertyLayout& layout)
{
    ImGui::PushID(label);
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2{0.0f, 3.0f});
    if (!ImGui::BeginTable("##PropertyRow", 2, ImGuiTableFlags_NoSavedSettings)) {
        ImGui::PopStyleVar();
        ImGui::PopID();
        return false;
    }

    ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, layout.label_width);
    ImGui::TableSetupColumn("##control", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", label);
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-1.0f);
    return true;
}

void endPropertyRow()
{
    ImGui::EndTable();
    ImGui::PopStyleVar();
    ImGui::PopID();
}

bool drawVec3Control(const char* label,
                     glm::vec3& values,
                     float reset_value,
                     float drag_speed,
                     const PropertyLayout& layout)
{
    if (!beginPropertyRow(label, layout)) {
        return false;
    }

    bool changed = false;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{5.0f, 3.0f});
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float axis_width = (std::max)((ImGui::GetContentRegionAvail().x - spacing * 2.0f) / 3.0f, 40.0f);
    changed |= drawAxisControl("X", 'X', values.x, reset_value, drag_speed, axis_width, false);
    changed |= drawAxisControl("Y", 'Y', values.y, reset_value, drag_speed, axis_width, false);
    changed |= drawAxisControl("Z", 'Z', values.z, reset_value, drag_speed, axis_width, true);
    ImGui::PopStyleVar();

    endPropertyRow();
    return changed;
}

bool drawAssetHandleEditor(const char* label,
                           AssetHandle& handle,
                           std::initializer_list<AssetType> accepted_types,
                           const std::function<bool(AssetHandle)>& accepts_handle,
                           const PropertyLayout& layout)
{
    if (!beginPropertyRow(label, layout)) {
        return false;
    }

    bool changed = false;
    unsigned long long raw_handle = static_cast<unsigned long long>(static_cast<uint64_t>(handle));

    changed |= drawAssetPreview("##assetPreview", handle, accepted_types, accepts_handle);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{6.0f, 3.0f});
    const float clear_button_width = 70.0f;
    ImGui::SetNextItemWidth((std::max)(ImGui::GetContentRegionAvail().x - clear_button_width - 6.0f, 64.0f));
    if (ImGui::InputScalar("##handle", ImGuiDataType_U64, &raw_handle)) {
        const AssetHandle candidate_handle(static_cast<uint64_t>(raw_handle));
        if (!accepts_handle || accepts_handle(candidate_handle)) {
            handle = candidate_handle;
            changed = true;
        }
    }

    changed |= acceptDroppedAsset(handle, accepted_types, accepts_handle);

    ImGui::SameLine();
    if (!handle.isValid()) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Clear", ImVec2{clear_button_width, 0.0f})) {
        if (handle.isValid()) {
            handle = AssetHandle(0);
            changed = true;
        }
    }
    if (!handle.isValid()) {
        ImGui::EndDisabled();
    }
    ImGui::PopStyleVar();

    endPropertyRow();
    return changed;
}

} // namespace luna::editor::ui
