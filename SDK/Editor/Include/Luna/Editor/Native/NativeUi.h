#pragma once

#include "Luna/Editor/Native/NativeTypes.h"

#include <cstddef>
#include <cstdio>

namespace luna::editor::native {

class Ui final {
public:
    constexpr Ui() noexcept = default;

    explicit constexpr Ui(const LunaEditorUiApi* api) noexcept
        : api_(api)
    {}

    [[nodiscard]] bool available() const noexcept
    {
        return api_ != nullptr;
    }

    [[nodiscard]] bool canDrawText() const noexcept
    {
        return api_ != nullptr && api_->text != nullptr;
    }

    [[nodiscard]] bool hasField(size_t offset, size_t size) const noexcept
    {
        return api_ != nullptr && api_->struct_size >= offset + size;
    }

    [[nodiscard]] bool beginWindow(const char* id,
                                   const char* title,
                                   int* open,
                                   uint32_t flags = LunaEditorWindowFlag_None) const noexcept
    {
        return api_ != nullptr && api_->begin_window != nullptr &&
               api_->begin_window(api_->api_user_data, id, title, open, flags) != 0;
    }

    void endWindow() const noexcept
    {
        if (api_ != nullptr && api_->end_window != nullptr) {
            api_->end_window(api_->api_user_data);
        }
    }

    void text(const char* value) const noexcept
    {
        if (api_ != nullptr && api_->text != nullptr) {
            api_->text(api_->api_user_data, value != nullptr ? value : "");
        }
    }

    void textDisabled(const char* value) const noexcept
    {
        if (api_ != nullptr && api_->text_disabled != nullptr) {
            api_->text_disabled(api_->api_user_data, value != nullptr ? value : "");
        }
    }

    void textWrapped(const char* value) const noexcept
    {
        if (api_ != nullptr && api_->text_wrapped != nullptr) {
            api_->text_wrapped(api_->api_user_data, value != nullptr ? value : "");
        }
    }

    void bulletText(const char* value) const noexcept
    {
        if (api_ != nullptr && api_->bullet_text != nullptr) {
            api_->bullet_text(api_->api_user_data, value != nullptr ? value : "");
        }
    }

    void separator() const noexcept
    {
        if (api_ != nullptr && api_->separator != nullptr) {
            api_->separator(api_->api_user_data);
        }
    }

    void separatorText(const char* label) const noexcept
    {
        if (api_ != nullptr && api_->separator_text != nullptr) {
            api_->separator_text(api_->api_user_data, label != nullptr ? label : "");
        }
    }

    void sameLine() const noexcept
    {
        if (api_ != nullptr && api_->same_line != nullptr) {
            api_->same_line(api_->api_user_data);
        }
    }

    void spacing() const noexcept
    {
        if (api_ != nullptr && api_->spacing != nullptr) {
            api_->spacing(api_->api_user_data);
        }
    }

    void indent(float width = 0.0f) const noexcept
    {
        if (api_ != nullptr && api_->indent != nullptr) {
            api_->indent(api_->api_user_data, width);
        }
    }

    void unindent(float width = 0.0f) const noexcept
    {
        if (api_ != nullptr && api_->unindent != nullptr) {
            api_->unindent(api_->api_user_data, width);
        }
    }

    void beginDisabled() const noexcept
    {
        if (api_ != nullptr && api_->begin_disabled != nullptr) {
            api_->begin_disabled(api_->api_user_data);
        }
    }

    void endDisabled() const noexcept
    {
        if (api_ != nullptr && api_->end_disabled != nullptr) {
            api_->end_disabled(api_->api_user_data);
        }
    }

    void setNextItemWidth(float width) const noexcept
    {
        if (api_ != nullptr && api_->set_next_item_width != nullptr) {
            api_->set_next_item_width(api_->api_user_data, width);
        }
    }

    void heading(const char* title, const char* detail = nullptr) const noexcept
    {
        if (hasField(offsetof(LunaEditorUiApi, heading), sizeof(api_->heading)) && api_->heading != nullptr) {
            api_->heading(api_->api_user_data, title != nullptr ? title : "", detail != nullptr ? detail : "");
            return;
        }

        text(title != nullptr ? title : "");
        if (detail != nullptr && detail[0] != '\0') {
            textDisabled(detail);
        }
    }

    void keyValue(const char* label, const char* value) const noexcept
    {
        if (hasField(offsetof(LunaEditorUiApi, key_value), sizeof(api_->key_value)) && api_->key_value != nullptr) {
            api_->key_value(api_->api_user_data, label != nullptr ? label : "", value != nullptr ? value : "");
            return;
        }

        text(label != nullptr ? label : "");
        textWrapped(value != nullptr ? value : "");
    }

    void badge(const char* label, LunaEditorStatusVariant variant = LunaEditorStatusVariant_Neutral) const noexcept
    {
        if (hasField(offsetof(LunaEditorUiApi, badge), sizeof(api_->badge)) && api_->badge != nullptr) {
            api_->badge(api_->api_user_data, label != nullptr ? label : "", variant);
            return;
        }

        (void) variant;
        text(label != nullptr ? label : "");
    }

    void metric(const char* label,
                const char* value,
                const char* detail = nullptr,
                LunaEditorStatusVariant variant = LunaEditorStatusVariant_Neutral,
                Vec2 size = {}) const noexcept
    {
        if (hasField(offsetof(LunaEditorUiApi, metric), sizeof(api_->metric)) && api_->metric != nullptr) {
            api_->metric(api_->api_user_data,
                         label != nullptr ? label : "",
                         value != nullptr ? value : "",
                         detail != nullptr ? detail : "",
                         variant,
                         &size);
            return;
        }

        (void) variant;
        (void) size;
        text(label != nullptr ? label : "");
        text(value != nullptr ? value : "");
        if (detail != nullptr && detail[0] != '\0') {
            textDisabled(detail);
        }
    }

    void emptyState(const char* title, const char* detail = nullptr) const noexcept
    {
        if (hasField(offsetof(LunaEditorUiApi, empty_state), sizeof(api_->empty_state)) &&
            api_->empty_state != nullptr) {
            api_->empty_state(api_->api_user_data, title != nullptr ? title : "", detail != nullptr ? detail : "");
            return;
        }

        text(title != nullptr ? title : "");
        if (detail != nullptr && detail[0] != '\0') {
            textDisabled(detail);
        }
    }

    void beginPanel(const char* id, Vec2 size = {}) const noexcept
    {
        if (hasField(offsetof(LunaEditorUiApi, begin_panel), sizeof(api_->begin_panel)) &&
            api_->begin_panel != nullptr) {
            api_->begin_panel(api_->api_user_data, id != nullptr ? id : "", &size);
        }
    }

    void endPanel() const noexcept
    {
        if (hasField(offsetof(LunaEditorUiApi, end_panel), sizeof(api_->end_panel)) && api_->end_panel != nullptr) {
            api_->end_panel(api_->api_user_data);
        }
    }

    [[nodiscard]] bool assetField(const char* id,
                                  const char* label,
                                  const char* detail = nullptr,
                                  LunaEditorStatusVariant variant = LunaEditorStatusVariant_Neutral,
                                  Vec2 size = {}) const noexcept
    {
        if (hasField(offsetof(LunaEditorUiApi, asset_field), sizeof(api_->asset_field)) &&
            api_->asset_field != nullptr) {
            return api_->asset_field(api_->api_user_data,
                                     id != nullptr ? id : "",
                                     label != nullptr ? label : "",
                                     detail != nullptr ? detail : "",
                                     variant,
                                     &size) != 0;
        }

        char fallback_label[512]{};
        const char* safe_label = label != nullptr ? label : "";
        const char* safe_id = id != nullptr ? id : "";
        int written = 0;
        if (detail != nullptr && detail[0] != '\0') {
            written = std::snprintf(fallback_label, sizeof(fallback_label), "%s  %s##%s", safe_label, detail, safe_id);
        } else {
            written = std::snprintf(fallback_label, sizeof(fallback_label), "%s##%s", safe_label, safe_id);
        }
        if (written < 0 || static_cast<size_t>(written) >= sizeof(fallback_label)) {
            fallback_label[sizeof(fallback_label) - 1u] = '\0';
        }
        return button(
            fallback_label, size.x != 0.0f || size.y != 0.0f ? size : fillWidth(), LunaEditorButtonVariant_Subtle);
    }

    [[nodiscard]] bool button(const char* label,
                              Vec2 size = {},
                              LunaEditorButtonVariant variant = LunaEditorButtonVariant_Default) const noexcept
    {
        return api_ != nullptr && api_->button != nullptr &&
               api_->button(api_->api_user_data, label != nullptr ? label : "", &size, variant) != 0;
    }

    bool checkbox(const char* label, int* value) const noexcept
    {
        return api_ != nullptr && api_->checkbox != nullptr &&
               api_->checkbox(api_->api_user_data, label != nullptr ? label : "", value) != 0;
    }

    bool sliderInt(const char* label, int* value, int min_value, int max_value) const noexcept
    {
        return api_ != nullptr && api_->slider_int != nullptr &&
               api_->slider_int(api_->api_user_data, label != nullptr ? label : "", value, min_value, max_value) != 0;
    }

    bool sliderFloat(
        const char* label, float* value, float min_value, float max_value, const char* format = "%.3f") const noexcept
    {
        return api_ != nullptr && api_->slider_float != nullptr &&
               api_->slider_float(api_->api_user_data,
                                  label != nullptr ? label : "",
                                  value,
                                  min_value,
                                  max_value,
                                  format != nullptr ? format : "%.3f") != 0;
    }

    bool dragInt(const char* label, int* value, float speed, int min_value, int max_value) const noexcept
    {
        return api_ != nullptr && api_->drag_int != nullptr &&
               api_->drag_int(api_->api_user_data, label != nullptr ? label : "", value, speed, min_value, max_value) !=
                   0;
    }

    bool dragFloat(const char* label,
                   float* value,
                   float speed,
                   float min_value,
                   float max_value,
                   const char* format = "%.3f") const noexcept
    {
        return api_ != nullptr && api_->drag_float != nullptr &&
               api_->drag_float(api_->api_user_data,
                                label != nullptr ? label : "",
                                value,
                                speed,
                                min_value,
                                max_value,
                                format != nullptr ? format : "%.3f") != 0;
    }

    bool dragFloat3(const char* label,
                    Vec3* value,
                    float speed,
                    float min_value,
                    float max_value,
                    const char* format = "%.3f") const noexcept
    {
        return api_ != nullptr && api_->drag_float3 != nullptr &&
               api_->drag_float3(api_->api_user_data,
                                 label != nullptr ? label : "",
                                 value,
                                 speed,
                                 min_value,
                                 max_value,
                                 format != nullptr ? format : "%.3f") != 0;
    }

    bool colorEdit3(const char* label, Vec3* value) const noexcept
    {
        return api_ != nullptr && api_->color_edit3 != nullptr &&
               api_->color_edit3(api_->api_user_data, label != nullptr ? label : "", value) != 0;
    }

    bool colorEdit4(const char* label, Vec4* value) const noexcept
    {
        return api_ != nullptr && api_->color_edit4 != nullptr &&
               api_->color_edit4(api_->api_user_data, label != nullptr ? label : "", value) != 0;
    }

    bool inputText(const char* label, char* value, size_t buffer_size) const noexcept
    {
        return api_ != nullptr && api_->input_text != nullptr &&
               api_->input_text(api_->api_user_data, label != nullptr ? label : "", value, buffer_size) != 0;
    }

    bool inputTextWithHint(const char* label, const char* hint, char* value, size_t buffer_size) const noexcept
    {
        return api_ != nullptr && api_->input_text_with_hint != nullptr &&
               api_->input_text_with_hint(api_->api_user_data,
                                          label != nullptr ? label : "",
                                          hint != nullptr ? hint : "",
                                          value,
                                          buffer_size) != 0;
    }

    [[nodiscard]] bool treeNode(const char* label) const noexcept
    {
        return api_ != nullptr && api_->tree_node != nullptr &&
               api_->tree_node(api_->api_user_data, label != nullptr ? label : "") != 0;
    }

    [[nodiscard]] bool
        treeNodeEx(const char* id, const char* label, uint32_t flags = LunaEditorTreeNodeFlag_None) const noexcept
    {
        return api_ != nullptr && api_->tree_node_ex != nullptr &&
               api_->tree_node_ex(api_->api_user_data, id != nullptr ? id : "", label != nullptr ? label : "", flags) !=
                   0;
    }

    void treePop() const noexcept
    {
        if (api_ != nullptr && api_->tree_pop != nullptr) {
            api_->tree_pop(api_->api_user_data);
        }
    }

    [[nodiscard]] bool beginCombo(const char* label, const char* preview_value) const noexcept
    {
        return api_ != nullptr && api_->begin_combo != nullptr &&
               api_->begin_combo(api_->api_user_data,
                                 label != nullptr ? label : "",
                                 preview_value != nullptr ? preview_value : "") != 0;
    }

    void endCombo() const noexcept
    {
        if (api_ != nullptr && api_->end_combo != nullptr) {
            api_->end_combo(api_->api_user_data);
        }
    }

    [[nodiscard]] bool selectable(const char* label, bool selected = false) const noexcept
    {
        return api_ != nullptr && api_->selectable != nullptr &&
               api_->selectable(api_->api_user_data, label != nullptr ? label : "", selected ? 1 : 0) != 0;
    }

    void setItemDefaultFocus() const noexcept
    {
        if (api_ != nullptr && api_->set_item_default_focus != nullptr) {
            api_->set_item_default_focus(api_->api_user_data);
        }
    }

    [[nodiscard]] bool image(const TextureView& texture, Vec2 size) const noexcept
    {
        return api_ != nullptr && api_->image != nullptr && api_->image(api_->api_user_data, &texture, &size) != 0;
    }

    [[nodiscard]] bool isItemHovered() const noexcept
    {
        return api_ != nullptr && api_->is_item_hovered != nullptr && api_->is_item_hovered(api_->api_user_data) != 0;
    }

    [[nodiscard]] bool isItemClicked(LunaEditorMouseButton button = LunaEditorMouseButton_Left) const noexcept
    {
        return api_ != nullptr && api_->is_item_clicked != nullptr &&
               api_->is_item_clicked(api_->api_user_data, button) != 0;
    }

    [[nodiscard]] bool isItemDoubleClicked(LunaEditorMouseButton button = LunaEditorMouseButton_Left) const noexcept
    {
        return api_ != nullptr && api_->is_item_double_clicked != nullptr &&
               api_->is_item_double_clicked(api_->api_user_data, button) != 0;
    }

    [[nodiscard]] bool isItemDeactivatedAfterEdit() const noexcept
    {
        return api_ != nullptr && api_->is_item_deactivated_after_edit != nullptr &&
               api_->is_item_deactivated_after_edit(api_->api_user_data) != 0;
    }

    void setTooltip(const char* value) const noexcept
    {
        if (api_ != nullptr && api_->set_tooltip != nullptr) {
            api_->set_tooltip(api_->api_user_data, value != nullptr ? value : "");
        }
    }

    [[nodiscard]] bool invisibleButton(const char* id, Vec2 size) const noexcept
    {
        return api_ != nullptr && api_->invisible_button != nullptr &&
               api_->invisible_button(api_->api_user_data, id != nullptr ? id : "", &size) != 0;
    }

    [[nodiscard]] bool beginSection(const char* id, const char* label, bool default_open = true) const noexcept
    {
        return api_ != nullptr && api_->begin_section != nullptr &&
               api_->begin_section(
                   api_->api_user_data, id != nullptr ? id : "", label != nullptr ? label : "", default_open ? 1 : 0) !=
                   0;
    }

    void endSection() const noexcept
    {
        if (api_ != nullptr && api_->end_section != nullptr) {
            api_->end_section(api_->api_user_data);
        }
    }

    [[nodiscard]] bool beginMenu(const char* label, bool enabled = true) const noexcept
    {
        return api_ != nullptr && api_->begin_menu != nullptr &&
               api_->begin_menu(api_->api_user_data, label != nullptr ? label : "", enabled ? 1 : 0) != 0;
    }

    void endMenu() const noexcept
    {
        if (api_ != nullptr && api_->end_menu != nullptr) {
            api_->end_menu(api_->api_user_data);
        }
    }

    [[nodiscard]] bool menuItem(const char* label, bool selected = false, bool enabled = true) const noexcept
    {
        return api_ != nullptr && api_->menu_item != nullptr &&
               api_->menu_item(api_->api_user_data, label != nullptr ? label : "", selected ? 1 : 0, enabled ? 1 : 0) !=
                   0;
    }

    void openPopup(const char* id) const noexcept
    {
        if (api_ != nullptr && api_->open_popup != nullptr) {
            api_->open_popup(api_->api_user_data, id != nullptr ? id : "");
        }
    }

    [[nodiscard]] bool beginPopup(const char* id) const noexcept
    {
        return api_ != nullptr && api_->begin_popup != nullptr &&
               api_->begin_popup(api_->api_user_data, id != nullptr ? id : "") != 0;
    }

    [[nodiscard]] bool beginPopupContextItem(const char* id = nullptr,
                                             LunaEditorMouseButton button = LunaEditorMouseButton_Right) const noexcept
    {
        return api_ != nullptr && api_->begin_popup_context_item != nullptr &&
               api_->begin_popup_context_item(api_->api_user_data, id != nullptr ? id : "", button) != 0;
    }

    void closeCurrentPopup() const noexcept
    {
        if (api_ != nullptr && api_->close_current_popup != nullptr) {
            api_->close_current_popup(api_->api_user_data);
        }
    }

    void endPopup() const noexcept
    {
        if (api_ != nullptr && api_->end_popup != nullptr) {
            api_->end_popup(api_->api_user_data);
        }
    }

    [[nodiscard]] bool beginDragDropSource() const noexcept
    {
        return api_ != nullptr && api_->begin_drag_drop_source != nullptr &&
               api_->begin_drag_drop_source(api_->api_user_data) != 0;
    }

    [[nodiscard]] bool setDragDropPayload(const char* type, const void* data, size_t size) const noexcept
    {
        return api_ != nullptr && api_->set_drag_drop_payload != nullptr && type != nullptr &&
               api_->set_drag_drop_payload(api_->api_user_data, type, data, size) != 0;
    }

    void endDragDropSource() const noexcept
    {
        if (api_ != nullptr && api_->end_drag_drop_source != nullptr) {
            api_->end_drag_drop_source(api_->api_user_data);
        }
    }

    [[nodiscard]] bool beginDragDropTarget() const noexcept
    {
        return api_ != nullptr && api_->begin_drag_drop_target != nullptr &&
               api_->begin_drag_drop_target(api_->api_user_data) != 0;
    }

    [[nodiscard]] bool acceptDragDropPayload(const char* type, void* out_data, size_t size) const noexcept
    {
        return api_ != nullptr && api_->accept_drag_drop_payload != nullptr && type != nullptr &&
               api_->accept_drag_drop_payload(api_->api_user_data, type, out_data, size) != 0;
    }

    [[nodiscard]] bool acceptAssetDragDropPayload(AssetDropPayload* out_payload,
                                                  const uint32_t* accepted_types = nullptr,
                                                  size_t accepted_type_count = 0u) const noexcept
    {
        if (!hasField(offsetof(LunaEditorUiApi, accept_asset_drag_drop_payload),
                      sizeof(api_->accept_asset_drag_drop_payload)) ||
            api_->accept_asset_drag_drop_payload == nullptr || out_payload == nullptr) {
            return false;
        }

        return api_->accept_asset_drag_drop_payload(
                   api_->api_user_data, out_payload, accepted_types, accepted_type_count) != 0;
    }

    template <size_t Count>
    [[nodiscard]] bool acceptAssetDragDropPayload(AssetDropPayload* out_payload,
                                                  const uint32_t (&accepted_types)[Count]) const noexcept
    {
        return acceptAssetDragDropPayload(out_payload, accepted_types, Count);
    }

    void endDragDropTarget() const noexcept
    {
        if (api_ != nullptr && api_->end_drag_drop_target != nullptr) {
            api_->end_drag_drop_target(api_->api_user_data);
        }
    }

    [[nodiscard]] Vec2 contentRegionAvail() const noexcept
    {
        Vec2 value{};
        if (api_ != nullptr && api_->content_region_avail != nullptr) {
            api_->content_region_avail(api_->api_user_data, &value);
        }
        return value;
    }

    [[nodiscard]] Vec2 windowFramebufferScale() const noexcept
    {
        Vec2 value{};
        if (api_ != nullptr && api_->window_framebuffer_scale != nullptr) {
            api_->window_framebuffer_scale(api_->api_user_data, &value);
        }
        return value;
    }

    [[nodiscard]] float scale(float value) const noexcept
    {
        if (api_ != nullptr && api_->scale != nullptr) {
            return api_->scale(api_->api_user_data, value);
        }
        return value;
    }

    [[nodiscard]] Vec2 scaled(Vec2 value) const noexcept
    {
        Vec2 result = value;
        if (api_ != nullptr && api_->scaled != nullptr) {
            api_->scaled(api_->api_user_data, &value, &result);
        }
        return result;
    }

    [[nodiscard]] bool beginTable(const char* id,
                                  int column_count,
                                  uint32_t flags = LunaEditorTableFlag_None,
                                  Vec2 outer_size = {}) const noexcept
    {
        return api_ != nullptr && api_->begin_table != nullptr &&
               api_->begin_table(api_->api_user_data, id != nullptr ? id : "", column_count, flags, &outer_size) != 0;
    }

    void endTable() const noexcept
    {
        if (api_ != nullptr && api_->end_table != nullptr) {
            api_->end_table(api_->api_user_data);
        }
    }

    void tableSetupColumn(const char* label,
                          uint32_t flags = LunaEditorTableColumnFlag_None,
                          float init_width_or_weight = 0.0f) const noexcept
    {
        if (api_ != nullptr && api_->table_setup_column != nullptr) {
            api_->table_setup_column(api_->api_user_data, label != nullptr ? label : "", flags, init_width_or_weight);
        }
    }

    void tableHeadersRow() const noexcept
    {
        if (api_ != nullptr && api_->table_headers_row != nullptr) {
            api_->table_headers_row(api_->api_user_data);
        }
    }

    void tableNextRow() const noexcept
    {
        if (api_ != nullptr && api_->table_next_row != nullptr) {
            api_->table_next_row(api_->api_user_data);
        }
    }

    bool tableNextColumn() const noexcept
    {
        return api_ != nullptr && api_->table_next_column != nullptr &&
               api_->table_next_column(api_->api_user_data) != 0;
    }

    [[nodiscard]] bool canDrawTable() const noexcept
    {
        return api_ != nullptr && api_->begin_table != nullptr && api_->end_table != nullptr &&
               api_->table_next_row != nullptr && api_->table_next_column != nullptr;
    }

    [[nodiscard]] const LunaEditorUiApi* native() const noexcept
    {
        return api_;
    }

private:
    const LunaEditorUiApi* api_{};
};

} // namespace luna::editor::native
