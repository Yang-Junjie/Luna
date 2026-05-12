#pragma once

#include "EditorApi/EditorTypes.h"

#include <cstddef>
#include <initializer_list>
#include <string_view>

namespace luna::editor {

enum class ButtonVariant {
    Default,
    Primary,
    Danger,
    Subtle,
};

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
    virtual void textWrapped(std::string_view value) = 0;
    virtual void bulletText(std::string_view value) = 0;
    virtual void separator() = 0;
    virtual void separatorText(std::string_view label) = 0;
    virtual void sameLine() = 0;
    virtual void spacing() = 0;
    virtual void indent(float width = 0.0f) = 0;
    virtual void unindent(float width = 0.0f) = 0;
    virtual void beginDisabled() = 0;
    virtual void endDisabled() = 0;
    virtual void setNextItemWidth(float width) = 0;
    [[nodiscard]] virtual Vec2 contentRegionAvail() const noexcept = 0;
    [[nodiscard]] virtual Vec2 windowFramebufferScale() const noexcept = 0;

    virtual bool button(std::string_view label,
                        Vec2 size = {},
                        ButtonVariant variant = ButtonVariant::Default) = 0;
    bool button(std::string_view label, ButtonVariant variant)
    {
        return button(label, Vec2{}, variant);
    }
    virtual bool checkbox(std::string_view label, bool& value) = 0;
    virtual bool colorEdit3(std::string_view label, Vec3& value) = 0;
    virtual bool sliderInt(std::string_view label, int& value, int min_value, int max_value) = 0;
    virtual bool sliderFloat(
        std::string_view label, float& value, float min_value, float max_value, std::string_view format = "%.3f") = 0;
    virtual bool dragFloat3(std::string_view label,
                            Vec3& value,
                            float speed = 1.0f,
                            float min_value = 0.0f,
                            float max_value = 0.0f,
                            std::string_view format = "%.3f") = 0;
    virtual bool
        dragInt(std::string_view label, int& value, float speed = 1.0f, int min_value = 0, int max_value = 0) = 0;
    virtual bool dragFloat(std::string_view label,
                           float& value,
                           float speed = 1.0f,
                           float min_value = 0.0f,
                           float max_value = 0.0f,
                           std::string_view format = "%.3f") = 0;
    virtual bool inputText(std::string_view label,
                           std::string& value,
                           std::size_t buffer_size = 256) = 0;
    virtual bool inputTextWithHint(std::string_view label,
                                   std::string_view hint,
                                   std::string& value,
                                   std::size_t buffer_size = 256) = 0;
    virtual bool colorEdit4(std::string_view label, Vec4& value) = 0;
    virtual bool treeNode(std::string_view label) = 0;
    virtual void treePop() = 0;
    virtual bool beginCombo(std::string_view label, std::string_view preview_value) = 0;
    virtual void endCombo() = 0;
    virtual bool selectable(std::string_view label, bool selected = false) = 0;
    virtual void setItemDefaultFocus() = 0;
    virtual bool image(const TextureView& texture, Vec2 size) = 0;
    [[nodiscard]] virtual bool isItemHovered() const noexcept = 0;
    [[nodiscard]] virtual bool isItemClicked(MouseButton button = MouseButton::Left) const noexcept = 0;
    [[nodiscard]] virtual bool isItemDoubleClicked(MouseButton button = MouseButton::Left) const noexcept = 0;
    [[nodiscard]] virtual bool isItemDeactivatedAfterEdit() const noexcept = 0;
    virtual void setTooltip(std::string_view value) = 0;
    virtual bool invisibleButton(std::string_view id, Vec2 size) = 0;

    virtual bool treeNodeEx(std::string_view id, std::string_view label, TreeNodeFlags flags) = 0;
    virtual bool beginSection(std::string_view id, std::string_view label, bool default_open = true) = 0;
    virtual void endSection() = 0;
    virtual bool beginMenu(std::string_view label, bool enabled = true) = 0;
    virtual void endMenu() = 0;
    virtual bool menuItem(std::string_view label, bool selected = false, bool enabled = true) = 0;
    virtual void openPopup(std::string_view id) = 0;
    virtual bool beginPopup(std::string_view id) = 0;
    virtual bool beginPopupContextItem(std::string_view id = {}, MouseButton button = MouseButton::Right) = 0;
    virtual void closeCurrentPopup() = 0;
    virtual void endPopup() = 0;

    virtual bool beginDragDropSource() = 0;
    virtual bool setDragDropPayload(std::string_view type, const void* data, std::size_t size) = 0;
    virtual void endDragDropSource() = 0;
    virtual bool beginDragDropTarget() = 0;
    virtual bool acceptDragDropPayload(std::string_view type, void* out_data, std::size_t size) = 0;
    virtual bool acceptAssetDragDropPayload(AssetDropPayload& out_payload,
                                            const AssetType* accepted_types,
                                            std::size_t accepted_type_count) = 0;
    virtual void endDragDropTarget() = 0;

    bool acceptAssetDragDropPayload(AssetDropPayload& out_payload, std::initializer_list<AssetType> accepted_types)
    {
        return acceptAssetDragDropPayload(out_payload, accepted_types.begin(), accepted_types.size());
    }

    [[nodiscard]] virtual float scale(float value) const noexcept = 0;
    [[nodiscard]] virtual Vec2 scaled(Vec2 value) const noexcept = 0;

    virtual bool beginTable(std::string_view id,
                            int column_count,
                            TableFlags flags = static_cast<TableFlags>(TableFlag::None),
                            Vec2 outer_size = {}) = 0;
    virtual void endTable() = 0;
    virtual void tableSetupColumn(std::string_view label,
                                  TableColumnFlags flags = static_cast<TableColumnFlags>(TableColumnFlag::None),
                                  float init_width_or_weight = 0.0f) = 0;
    virtual void tableHeadersRow() = 0;
    virtual void tableNextRow() = 0;
    virtual bool tableNextColumn() = 0;
};

} // namespace luna::editor
