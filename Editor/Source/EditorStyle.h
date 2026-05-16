#pragma once

#include "EditorApi/EditorSettingsService.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

struct ImVec2;
struct ImVec4;

namespace luna::editor {

enum class EditorThemeColor : uint8_t {
    Text,
    TextMuted,
    WindowBg,
    PopupBg,
    PanelBg,
    PanelBorder,
    FrameBg,
    FrameBgHovered,
    FrameBgActive,
    Header,
    HeaderHovered,
    HeaderActive,
    Accent,
    AccentHovered,
    AccentActive,
    Info,
    Success,
    Warning,
    Danger,
    Button,
    ButtonHovered,
    ButtonActive,
    ButtonPrimary,
    ButtonPrimaryHovered,
    ButtonPrimaryActive,
    ButtonDanger,
    ButtonDangerHovered,
    ButtonDangerActive,
    AxisX,
    AxisY,
    AxisZ,
    AssetTexture,
    AssetMesh,
    AssetMaterial,
    AssetModel,
    AssetScene,
    AssetScript,
};

enum class EditorThemeMetric : uint8_t {
    WindowPaddingX,
    WindowPaddingY,
    FramePaddingX,
    FramePaddingY,
    CellPaddingX,
    CellPaddingY,
    ItemSpacingX,
    ItemSpacingY,
    ItemInnerSpacingX,
    ItemInnerSpacingY,
    IndentSpacing,
    ScrollbarSize,
    GrabMinSize,
    WindowRounding,
    ChildRounding,
    FrameRounding,
    PopupRounding,
    ScrollbarRounding,
    GrabRounding,
    TabRounding,
    PropertyLabelWidth,
    PropertyRowPaddingY,
    BadgePaddingX,
    BadgePaddingY,
    BadgeRounding,
    MetricMinWidth,
    MetricPaddingLeft,
    MetricPaddingRight,
    MetricPaddingTop,
    MetricPaddingBottom,
    MetricLineGap,
    MetricAccentWidth,
    MetricDefaultHeight,
    MetricDetailedHeight,
    EmptyStateMinWidth,
    EmptyStateHeight,
    EmptyStateDetailedHeight,
    EmptyStateDetailTop,
    EmptyStateLineGap,
    PanelPaddingX,
    PanelPaddingY,
    PanelRounding,
    HeadingAccentOffsetY,
    HeadingAccentWidth,
    HeadingAccentRounding,
    HeadingIndent,
    SectionFramePaddingX,
    SectionFramePaddingY,
    SectionAccentWidth,
    CompactInspectorItemSpacingX,
    CompactInspectorItemSpacingY,
    CompactInspectorItemInnerSpacingX,
    CompactInspectorItemInnerSpacingY,
    CompactInspectorFramePaddingX,
    CompactInspectorFramePaddingY,
    CompactInspectorCellPaddingX,
    CompactInspectorCellPaddingY,
    CompactInspectorIndentSpacing,
    AssetPreviewMinWidth,
    AssetPreviewExtraHeight,
    AssetPreviewAccentOffsetX,
    AssetPreviewAccentOffsetY,
    AssetPreviewAccentWidth,
    AssetPreviewAccentRounding,
    AssetPreviewTextOffsetX,
    AssetPreviewTextOffsetY,
    AssetPreviewTextRightPadding,
    AssetPreviewLineGap,
    AxisControlSpacing,
    AxisControlMinDragWidth,
    Vector2FramePaddingX,
    Vector2FramePaddingY,
    Vector3FramePaddingX,
    Vector3FramePaddingY,
    Vector3MinAxisWidth,
    ComponentIndentSpacing,
};

[[nodiscard]] float getEditorUiScale() noexcept;
[[nodiscard]] float scaleEditorUi(float value) noexcept;
[[nodiscard]] ImVec2 scaleEditorUi(float x, float y) noexcept;
[[nodiscard]] ImVec4 editorThemeColor(EditorThemeColor color) noexcept;
[[nodiscard]] ImVec4 editorThemeColor(EditorThemeColor color, float alpha) noexcept;
[[nodiscard]] float editorThemeMetric(EditorThemeMetric metric) noexcept;
[[nodiscard]] ImVec2 editorThemeMetric(EditorThemeMetric x, EditorThemeMetric y) noexcept;
[[nodiscard]] EditorThemePreset activeEditorThemePreset() noexcept;
[[nodiscard]] const EditorThemePreset* editorThemePresets() noexcept;
[[nodiscard]] std::size_t editorThemePresetCount() noexcept;
[[nodiscard]] std::string_view editorThemePresetName(EditorThemePreset preset) noexcept;
[[nodiscard]] EditorThemePreset editorThemePresetFromName(std::string_view name) noexcept;

void applyEditorTheme(EditorThemePreset preset = EditorThemePreset::ModernLightweight, float ui_scale = 1.0f);

} // namespace luna::editor
