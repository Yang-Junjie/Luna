#pragma once

#include "EditorApi/EditorTypes.h"

#include <string_view>

namespace luna::editor {

class Ui {
public:
    virtual ~Ui() = default;

    virtual bool beginWindow(std::string_view id,
                             std::string_view title,
                             bool* open = nullptr,
                             WindowFlags flags = static_cast<WindowFlags>(WindowFlag::None)) = 0;
    virtual void endWindow() = 0;

    virtual void text(std::string_view value) = 0;
    virtual void textDisabled(std::string_view value) = 0;
    virtual void separator() = 0;
    virtual void sameLine() = 0;
    virtual void spacing() = 0;

    virtual bool button(std::string_view label, Vec2 size = {}) = 0;
    virtual bool checkbox(std::string_view label, bool& value) = 0;
    virtual bool sliderInt(std::string_view label, int& value, int min_value, int max_value) = 0;

    [[nodiscard]] virtual float scale(float value) const noexcept = 0;
    [[nodiscard]] virtual Vec2 scaled(Vec2 value) const noexcept = 0;

    virtual bool beginTable(std::string_view id,
                            int column_count,
                            TableFlags flags = static_cast<TableFlags>(TableFlag::None),
                            Vec2 outer_size = {}) = 0;
    virtual void endTable() = 0;
    virtual void tableSetupColumn(
        std::string_view label,
        TableColumnFlags flags = static_cast<TableColumnFlags>(TableColumnFlag::None),
        float init_width_or_weight = 0.0f) = 0;
    virtual void tableHeadersRow() = 0;
    virtual void tableNextRow() = 0;
    virtual bool tableNextColumn() = 0;
};

} // namespace luna::editor
