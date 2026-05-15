#pragma once

#include "Luna/Editor/Native/NativeTypes.h"

namespace luna::editor::native {

class Ui final {
public:
    constexpr Ui() noexcept = default;
    explicit constexpr Ui(const LunaEditorUiApi* api) noexcept
        : api_(api)
    {
    }

    [[nodiscard]] bool available() const noexcept
    {
        return api_ != nullptr;
    }

    [[nodiscard]] bool canDrawText() const noexcept
    {
        return api_ != nullptr && api_->text != nullptr;
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

    bool sliderFloat(const char* label,
                     float* value,
                     float min_value,
                     float max_value,
                     const char* format = "%.3f") const noexcept
    {
        return api_ != nullptr && api_->slider_float != nullptr &&
               api_->slider_float(api_->api_user_data,
                                  label != nullptr ? label : "",
                                  value,
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

    bool inputTextWithHint(const char* label, const char* hint, char* value, size_t buffer_size) const noexcept
    {
        return api_ != nullptr && api_->input_text_with_hint != nullptr &&
               api_->input_text_with_hint(api_->api_user_data,
                                          label != nullptr ? label : "",
                                          hint != nullptr ? hint : "",
                                          value,
                                          buffer_size) != 0;
    }

    [[nodiscard]] bool image(const TextureView& texture, Vec2 size) const noexcept
    {
        return api_ != nullptr && api_->image != nullptr && api_->image(api_->api_user_data, &texture, &size) != 0;
    }

    void setTooltip(const char* value) const noexcept
    {
        if (api_ != nullptr && api_->set_tooltip != nullptr) {
            api_->set_tooltip(api_->api_user_data, value != nullptr ? value : "");
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

    [[nodiscard]] float scale(float value) const noexcept
    {
        if (api_ != nullptr && api_->scale != nullptr) {
            return api_->scale(api_->api_user_data, value);
        }
        return value;
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
            api_->table_setup_column(api_->api_user_data,
                                     label != nullptr ? label : "",
                                     flags,
                                     init_width_or_weight);
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
