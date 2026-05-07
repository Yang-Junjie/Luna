#include "Asset/AssetDatabase.h"
#include "EditorAssetDragDrop.h"
#include "EditorUI.h"

#include <cstdint>

#include <algorithm>
#include <vector>

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
        info.detail = "Referenced asset is not in the database";
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
    info.detail = AssetUtils::AssetTypeToString(metadata.Type);
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

bool drawAssetClearContextMenu(const char* id, AssetHandle& handle)
{
    bool changed = false;
    if (ImGui::BeginPopupContextItem(id)) {
        if (ImGui::MenuItem("Clear", nullptr, false, handle.isValid())) {
            handle = AssetHandle(0);
            changed = true;
        }
        ImGui::EndPopup();
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
    const float width = (std::max)(ImGui::GetContentRegionAvail().x, scale(64.0f));
    const float height = ImGui::GetTextLineHeight() * 2.0f + style.FramePadding.y * 2.0f + scale(6.0f);
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const ImVec2 size{width, height};

    ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    bool changed = acceptDroppedAsset(handle, accepted_types, accepts_handle);
    changed |= drawAssetClearContextMenu("AssetPreviewContext", handle);

    const ImVec2 min = position;
    const ImVec2 max{position.x + size.x, position.y + size.y};
    const ImVec4 frame_bg = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
    const ImVec4 accent = info.missing ? ImVec4{0.86f, 0.32f, 0.32f, 1.0f} : assetAccentColor(info.type);
    const ImVec4 fill = hovered || active ? mixColor(frame_bg, accent, 0.12f) : mixColor(frame_bg, accent, 0.06f);
    const ImVec4 border = hovered || active ? withAlpha(accent, 0.70f) : ImGui::GetStyleColorVec4(ImGuiCol_Border);
    const ImGuiCol label_color_index = handle.isValid() ? ImGuiCol_Text : ImGuiCol_TextDisabled;
    const ImU32 label_color =
        info.missing ? ImGui::GetColorU32(ImVec4{1.0f, 0.48f, 0.48f, 1.0f}) : ImGui::GetColorU32(label_color_index);
    const ImU32 detail_color = ImGui::GetColorU32(ImGuiCol_TextDisabled);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(min, max, ImGui::GetColorU32(fill), style.FrameRounding);
    draw_list->AddRect(min, max, ImGui::GetColorU32(border), style.FrameRounding);
    draw_list->AddRectFilled(ImVec2{min.x + scale(6.0f), min.y + scale(7.0f)},
                             ImVec2{min.x + scale(9.0f), max.y - scale(7.0f)},
                             ImGui::GetColorU32(accent),
                             scale(2.0f));

    const ImVec2 text_min{min.x + scale(16.0f), min.y + style.FramePadding.y + scale(3.0f)};
    const ImVec2 text_max{max.x - scale(8.0f), max.y - style.FramePadding.y};
    draw_list->PushClipRect(text_min, text_max, true);
    draw_list->AddText(text_min, label_color, info.label.c_str());
    draw_list->AddText(
        ImVec2{text_min.x, text_min.y + ImGui::GetTextLineHeight() + scale(3.0f)}, detail_color, info.detail.c_str());
    draw_list->PopClipRect();

    return changed;
}

bool drawAxisControl(
    const char* label, char axis, float& value, float reset_value, float drag_speed, float width, bool last)
{
    bool changed = false;
    const float line_height = ImGui::GetFrameHeight();
    const ImVec2 button_size{line_height, line_height};
    const ImVec4 color = axisColor(axis);

    ImGui::PushID(label);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, scaled(3.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, withAlpha(color, 0.72f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, withAlpha(color, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, withAlpha(color, 1.0f));
    if (ImGui::Button(label, button_size)) {
        value = reset_value;
        changed = true;
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    ImGui::SetNextItemWidth((std::max)(width - button_size.x - scale(3.0f), scale(24.0f)));
    changed |= ImGui::DragFloat("##value", &value, drag_speed, 0.0f, 0.0f, "%.2f");
    ImGui::PopStyleVar();
    ImGui::PopID();

    if (!last) {
        ImGui::SameLine();
    }
    return changed;
}

bool pushButtonVariant(ButtonVariant variant)
{
    switch (variant) {
        case ButtonVariant::Primary:
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.15f, 0.31f, 0.35f, 1.0f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.18f, 0.40f, 0.45f, 1.0f});
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.11f, 0.26f, 0.30f, 1.0f});
            return true;
        case ButtonVariant::Danger:
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.32f, 0.16f, 0.17f, 1.0f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.46f, 0.20f, 0.22f, 1.0f});
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.26f, 0.12f, 0.14f, 1.0f});
            return true;
        case ButtonVariant::Subtle:
            ImGui::PushStyleColor(ImGuiCol_Button, withAlpha(ImGui::GetStyleColorVec4(ImGuiCol_Button), 0.55f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            return true;
        case ButtonVariant::Default:
            break;
    }

    return false;
}

std::vector<char> makeTextEditBuffer(const std::string& value, std::size_t buffer_size)
{
    std::vector<char> buffer((std::max)(buffer_size, value.size() + 1), '\0');
    const std::size_t copy_size = (std::min)(value.size(), buffer.size() - 1);
    std::copy_n(value.data(), copy_size, buffer.data());
    return buffer;
}

PropertyLayout compactVectorLayout(PropertyLayout layout)
{
    layout.row_padding_y = 1.0f;
    return layout;
}

} // namespace

std::string assetDisplayLabel(AssetHandle handle)
{
    return describeAsset(handle).label;
}

bool beginPropertyRow(const char* label, const PropertyLayout& layout)
{
    ImGui::PushID(label);
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2{0.0f, layout.scaledRowPaddingY()});
    if (!ImGui::BeginTable("##PropertyRow", 2, ImGuiTableFlags_NoSavedSettings)) {
        ImGui::PopStyleVar();
        ImGui::PopID();
        return false;
    }

    ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, layout.scaledLabelWidth());
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

bool drawBool(const char* label, bool& value, const PropertyLayout& layout)
{
    if (!beginPropertyRow(label, layout)) {
        return false;
    }

    const bool changed = ImGui::Checkbox("##value", &value);
    endPropertyRow();
    return changed;
}

bool drawInt(const char* label, int& value, int step, int step_fast, const PropertyLayout& layout)
{
    if (!beginPropertyRow(label, layout)) {
        return false;
    }

    const bool changed = ImGui::InputInt("##value", &value, step, step_fast);
    endPropertyRow();
    return changed;
}

bool drawFloat(const char* label,
               float& value,
               float drag_speed,
               float min_value,
               float max_value,
               const char* format,
               const PropertyLayout& layout)
{
    if (!beginPropertyRow(label, layout)) {
        return false;
    }

    const bool changed = ImGui::DragFloat("##value", &value, drag_speed, min_value, max_value, format);
    endPropertyRow();
    return changed;
}

bool drawColor3(const char* label, glm::vec3& value, const PropertyLayout& layout)
{
    if (!beginPropertyRow(label, layout)) {
        return false;
    }

    const bool changed = ImGui::ColorEdit3("##value", &value.x);
    endPropertyRow();
    return changed;
}

bool drawTextValue(const char* label, const std::string& value, const PropertyLayout& layout)
{
    if (!beginPropertyRow(label, layout)) {
        return false;
    }

    ImGui::TextDisabled("%s", value.c_str());
    endPropertyRow();
    return false;
}

bool drawTextInput(const char* label,
                   std::string& value,
                   std::size_t buffer_size,
                   const PropertyLayout& layout,
                   bool* deactivated_after_edit)
{
    if (!beginPropertyRow(label, layout)) {
        return false;
    }

    std::vector<char> buffer = makeTextEditBuffer(value, buffer_size);
    const bool changed = ImGui::InputText("##value", buffer.data(), buffer.size());
    if (changed) {
        value = buffer.data();
    }
    if (deactivated_after_edit != nullptr) {
        *deactivated_after_edit = ImGui::IsItemDeactivatedAfterEdit();
    }

    endPropertyRow();
    return changed;
}

bool drawTextMultiline(
    const char* label, std::string& value, std::size_t buffer_size, float visible_lines, const PropertyLayout& layout)
{
    if (!beginPropertyRow(label, layout)) {
        return false;
    }

    std::vector<char> buffer = makeTextEditBuffer(value, buffer_size);
    const ImVec2 size{-1.0f, ImGui::GetTextLineHeight() * visible_lines};
    const bool changed = ImGui::InputTextMultiline("##value", buffer.data(), buffer.size(), size);
    if (changed) {
        value = buffer.data();
    }

    endPropertyRow();
    return changed;
}

bool drawCombo(const char* label,
               const char* preview_value,
               const std::function<bool()>& draw_options,
               const PropertyLayout& layout)
{
    if (!beginPropertyRow(label, layout)) {
        return false;
    }

    bool changed = false;
    if (ImGui::BeginCombo("##value", preview_value)) {
        if (draw_options) {
            changed |= draw_options();
        }
        ImGui::EndCombo();
    }

    endPropertyRow();
    return changed;
}

bool drawButton(const char* label, ButtonVariant variant, const ImVec2& size)
{
    const bool pushed_colors = pushButtonVariant(variant);
    const bool pressed = ImGui::Button(label, size);
    if (pushed_colors) {
        ImGui::PopStyleColor(3);
    }
    return pressed;
}

void pushCompactInspectorStyle()
{
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, scaled(6.0f, 2.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, scaled(4.0f, 2.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, scaled(6.0f, 2.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, scaled(4.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, scale(10.0f));
}

void popCompactInspectorStyle()
{
    ImGui::PopStyleVar(5);
}

bool beginSection(const char* label, const char* id, ImGuiTreeNodeFlags extra_flags)
{
    const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
                                     ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap | extra_flags;

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, scaled(2.0f, 2.0f));
    ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
    const bool open = ImGui::TreeNodeEx(id, flags, "%s", label);
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    const ImVec2 item_min = ImGui::GetItemRectMin();
    const ImVec2 item_max = ImGui::GetItemRectMax();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const float accent_width = scale(3.0f);
    draw_list->AddRectFilled(item_min,
                             ImVec2{item_min.x + accent_width, item_max.y},
                             ImGui::GetColorU32(withAlpha(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), 0.72f)));
    draw_list->AddLine(ImVec2{item_min.x, item_max.y - 1.0f},
                       ImVec2{item_max.x, item_max.y - 1.0f},
                       ImGui::GetColorU32(ImGuiCol_Border));
    return open;
}

void endSection()
{
    ImGui::TreePop();
}

bool drawVec2Control(const char* label,
                     glm::vec2& values,
                     float drag_speed,
                     float min_value,
                     float max_value,
                     const char* format,
                     const PropertyLayout& layout)
{
    if (!beginPropertyRow(label, compactVectorLayout(layout))) {
        return false;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, scaled(6.0f, 2.0f));
    const bool changed = ImGui::DragFloat2("##value", &values.x, drag_speed, min_value, max_value, format);
    ImGui::PopStyleVar();
    endPropertyRow();
    return changed;
}

bool drawVec3Control(
    const char* label, glm::vec3& values, float reset_value, float drag_speed, const PropertyLayout& layout)
{
    if (!beginPropertyRow(label, compactVectorLayout(layout))) {
        return false;
    }

    bool changed = false;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, scaled(5.0f, 2.0f));
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float axis_width = (std::max)((ImGui::GetContentRegionAvail().x - spacing * 2.0f) / 3.0f, scale(40.0f));
    changed |= drawAxisControl("X", 'X', values.x, reset_value, drag_speed, axis_width, false);
    changed |= drawAxisControl("Y", 'Y', values.y, reset_value, drag_speed, axis_width, false);
    changed |= drawAxisControl("Z", 'Z', values.z, reset_value, drag_speed, axis_width, true);
    ImGui::PopStyleVar();

    endPropertyRow();
    return changed;
}

bool drawAssetHandleSelector(const char* label,
                             AssetHandle& handle,
                             std::initializer_list<AssetType> accepted_types,
                             const std::function<bool(AssetHandle)>& accepts_handle,
                             const PropertyLayout& layout)
{
    if (!beginPropertyRow(label, layout)) {
        return false;
    }

    bool changed = false;
    changed |= drawAssetPreview("##assetPreview", handle, accepted_types, accepts_handle);
    endPropertyRow();
    return changed;
}

} // namespace luna::editor::ui
