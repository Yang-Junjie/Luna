#include "EditorStyle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

#include <imgui.h>

namespace luna::editor {
namespace {

constexpr float kMinUiScale = 1.0f;
constexpr float kMaxUiScale = 2.5f;
constexpr float kEditorDensityScale = 0.88f;
constexpr std::array<EditorThemePreset, 2> kEditorThemePresets{
    EditorThemePreset::ModernLightweight,
    EditorThemePreset::HighContrastDark,
};

float g_editor_ui_scale = 1.0f;
EditorThemePreset g_editor_theme_preset = EditorThemePreset::ModernLightweight;

struct EditorThemePalette {
    ImVec4 text;
    ImVec4 text_muted;
    ImVec4 window_bg;
    ImVec4 popup_bg;
    ImVec4 panel_bg;
    ImVec4 panel_border;
    ImVec4 frame_bg;
    ImVec4 frame_bg_hovered;
    ImVec4 frame_bg_active;
    ImVec4 header;
    ImVec4 header_hovered;
    ImVec4 header_active;
    ImVec4 accent;
    ImVec4 accent_hovered;
    ImVec4 accent_active;
    ImVec4 info;
    ImVec4 success;
    ImVec4 warning;
    ImVec4 danger;
    ImVec4 button;
    ImVec4 button_hovered;
    ImVec4 button_active;
    ImVec4 button_primary;
    ImVec4 button_primary_hovered;
    ImVec4 button_primary_active;
    ImVec4 button_danger;
    ImVec4 button_danger_hovered;
    ImVec4 button_danger_active;
    ImVec4 axis_x;
    ImVec4 axis_y;
    ImVec4 axis_z;
    ImVec4 asset_texture;
    ImVec4 asset_mesh;
    ImVec4 asset_material;
    ImVec4 asset_model;
    ImVec4 asset_scene;
    ImVec4 asset_script;
};

struct EditorThemeMetrics {
    float window_padding_x;
    float window_padding_y;
    float frame_padding_x;
    float frame_padding_y;
    float cell_padding_x;
    float cell_padding_y;
    float item_spacing_x;
    float item_spacing_y;
    float item_inner_spacing_x;
    float item_inner_spacing_y;
    float indent_spacing;
    float scrollbar_size;
    float grab_min_size;
    float window_rounding;
    float child_rounding;
    float frame_rounding;
    float popup_rounding;
    float scrollbar_rounding;
    float grab_rounding;
    float tab_rounding;
    float property_label_width;
    float property_row_padding_y;
    float badge_padding_x;
    float badge_padding_y;
    float badge_rounding;
    float metric_min_width;
    float metric_padding_left;
    float metric_padding_right;
    float metric_padding_top;
    float metric_padding_bottom;
    float metric_line_gap;
    float metric_accent_width;
    float metric_default_height;
    float metric_detailed_height;
    float empty_state_min_width;
    float empty_state_height;
    float empty_state_detailed_height;
    float empty_state_detail_top;
    float empty_state_line_gap;
    float panel_padding_x;
    float panel_padding_y;
    float panel_rounding;
    float heading_accent_offset_y;
    float heading_accent_width;
    float heading_accent_rounding;
    float heading_indent;
    float section_frame_padding_x;
    float section_frame_padding_y;
    float section_accent_width;
    float compact_inspector_item_spacing_x;
    float compact_inspector_item_spacing_y;
    float compact_inspector_item_inner_spacing_x;
    float compact_inspector_item_inner_spacing_y;
    float compact_inspector_frame_padding_x;
    float compact_inspector_frame_padding_y;
    float compact_inspector_cell_padding_x;
    float compact_inspector_cell_padding_y;
    float compact_inspector_indent_spacing;
    float asset_preview_min_width;
    float asset_preview_extra_height;
    float asset_preview_accent_offset_x;
    float asset_preview_accent_offset_y;
    float asset_preview_accent_width;
    float asset_preview_accent_rounding;
    float asset_preview_text_offset_x;
    float asset_preview_text_offset_y;
    float asset_preview_text_right_padding;
    float asset_preview_line_gap;
    float axis_control_spacing;
    float axis_control_min_drag_width;
    float vector2_frame_padding_x;
    float vector2_frame_padding_y;
    float vector3_frame_padding_x;
    float vector3_frame_padding_y;
    float vector3_min_axis_width;
    float component_indent_spacing;
};

EditorThemePalette g_editor_theme_palette = {};
EditorThemeMetrics g_editor_theme_metrics = {};

float sanitizeUiScale(float ui_scale) noexcept
{
    if (!std::isfinite(ui_scale) || ui_scale <= 0.0f) {
        return 1.0f;
    }

    return std::clamp(ui_scale * kEditorDensityScale, kMinUiScale, kMaxUiScale);
}

ImVec4 color(float r, float g, float b, float a = 1.0f)
{
    return ImVec4{r, g, b, a};
}

EditorThemePalette modernLightweightPalette()
{
    return EditorThemePalette{
        .text = color(0.800f, 0.824f, 0.855f),
        .text_muted = color(0.525f, 0.557f, 0.600f),
        .window_bg = color(0.118f, 0.122f, 0.133f),
        .popup_bg = color(0.145f, 0.153f, 0.169f, 0.98f),
        .panel_bg = color(0.145f, 0.153f, 0.169f),
        .panel_border = color(0.255f, 0.278f, 0.310f),
        .frame_bg = color(0.180f, 0.192f, 0.212f),
        .frame_bg_hovered = color(0.220f, 0.240f, 0.267f),
        .frame_bg_active = color(0.106f, 0.278f, 0.427f),
        .header = color(0.176f, 0.259f, 0.349f, 0.78f),
        .header_hovered = color(0.212f, 0.329f, 0.443f, 0.88f),
        .header_active = color(0.117f, 0.388f, 0.635f, 0.92f),
        .accent = color(0.000f, 0.478f, 0.800f),
        .accent_hovered = color(0.231f, 0.620f, 0.996f),
        .accent_active = color(0.047f, 0.333f, 0.541f),
        .info = color(0.231f, 0.620f, 0.996f),
        .success = color(0.365f, 0.722f, 0.455f),
        .warning = color(0.890f, 0.650f, 0.310f),
        .danger = color(0.900f, 0.330f, 0.330f),
        .button = color(0.188f, 0.200f, 0.220f),
        .button_hovered = color(0.240f, 0.263f, 0.294f),
        .button_active = color(0.106f, 0.278f, 0.427f),
        .button_primary = color(0.000f, 0.478f, 0.800f),
        .button_primary_hovered = color(0.231f, 0.620f, 0.996f),
        .button_primary_active = color(0.047f, 0.333f, 0.541f),
        .button_danger = color(0.459f, 0.155f, 0.165f),
        .button_danger_hovered = color(0.610f, 0.210f, 0.220f),
        .button_danger_active = color(0.349f, 0.110f, 0.120f),
        .axis_x = color(0.890f, 0.365f, 0.365f),
        .axis_y = color(0.365f, 0.722f, 0.455f),
        .axis_z = color(0.231f, 0.620f, 0.996f),
        .asset_texture = color(0.840f, 0.610f, 0.330f),
        .asset_mesh = color(0.231f, 0.620f, 0.996f),
        .asset_material = color(0.365f, 0.722f, 0.455f),
        .asset_model = color(0.650f, 0.560f, 0.880f),
        .asset_scene = color(0.760f, 0.620f, 0.380f),
        .asset_script = color(0.320f, 0.680f, 0.780f),
    };
}

EditorThemeMetrics modernLightweightMetrics()
{
    return EditorThemeMetrics{
        .window_padding_x = 9.0f,
        .window_padding_y = 8.0f,
        .frame_padding_x = 8.0f,
        .frame_padding_y = 4.0f,
        .cell_padding_x = 6.0f,
        .cell_padding_y = 4.0f,
        .item_spacing_x = 7.0f,
        .item_spacing_y = 5.0f,
        .item_inner_spacing_x = 6.0f,
        .item_inner_spacing_y = 3.0f,
        .indent_spacing = 14.0f,
        .scrollbar_size = 11.0f,
        .grab_min_size = 10.0f,
        .window_rounding = 5.0f,
        .child_rounding = 4.0f,
        .frame_rounding = 3.0f,
        .popup_rounding = 4.0f,
        .scrollbar_rounding = 4.0f,
        .grab_rounding = 3.0f,
        .tab_rounding = 3.0f,
        .property_label_width = 112.0f,
        .property_row_padding_y = 4.0f,
        .badge_padding_x = 9.0f,
        .badge_padding_y = 3.0f,
        .badge_rounding = 4.0f,
        .metric_min_width = 140.0f,
        .metric_padding_left = 12.0f,
        .metric_padding_right = 10.0f,
        .metric_padding_top = 8.0f,
        .metric_padding_bottom = 7.0f,
        .metric_line_gap = 4.0f,
        .metric_accent_width = 3.0f,
        .metric_default_height = 56.0f,
        .metric_detailed_height = 70.0f,
        .empty_state_min_width = 180.0f,
        .empty_state_height = 60.0f,
        .empty_state_detailed_height = 82.0f,
        .empty_state_detail_top = 18.0f,
        .empty_state_line_gap = 6.0f,
        .panel_padding_x = 11.0f,
        .panel_padding_y = 8.0f,
        .panel_rounding = 4.0f,
        .heading_accent_offset_y = 2.0f,
        .heading_accent_width = 3.0f,
        .heading_accent_rounding = 2.0f,
        .heading_indent = 10.0f,
        .section_frame_padding_x = 7.0f,
        .section_frame_padding_y = 3.0f,
        .section_accent_width = 3.0f,
        .compact_inspector_item_spacing_x = 6.0f,
        .compact_inspector_item_spacing_y = 2.0f,
        .compact_inspector_item_inner_spacing_x = 4.0f,
        .compact_inspector_item_inner_spacing_y = 1.0f,
        .compact_inspector_frame_padding_x = 6.0f,
        .compact_inspector_frame_padding_y = 2.0f,
        .compact_inspector_cell_padding_x = 4.0f,
        .compact_inspector_cell_padding_y = 1.0f,
        .compact_inspector_indent_spacing = 8.0f,
        .asset_preview_min_width = 64.0f,
        .asset_preview_extra_height = 6.0f,
        .asset_preview_accent_offset_x = 6.0f,
        .asset_preview_accent_offset_y = 7.0f,
        .asset_preview_accent_width = 3.0f,
        .asset_preview_accent_rounding = 2.0f,
        .asset_preview_text_offset_x = 16.0f,
        .asset_preview_text_offset_y = 3.0f,
        .asset_preview_text_right_padding = 8.0f,
        .asset_preview_line_gap = 3.0f,
        .axis_control_spacing = 3.0f,
        .axis_control_min_drag_width = 24.0f,
        .vector2_frame_padding_x = 6.0f,
        .vector2_frame_padding_y = 2.0f,
        .vector3_frame_padding_x = 5.0f,
        .vector3_frame_padding_y = 2.0f,
        .vector3_min_axis_width = 40.0f,
        .component_indent_spacing = 10.0f,
    };
}

EditorThemePalette highContrastDarkPalette()
{
    return EditorThemePalette{
        .text = color(0.96f, 0.97f, 0.98f),
        .text_muted = color(0.70f, 0.75f, 0.80f),
        .window_bg = color(0.035f, 0.040f, 0.048f),
        .popup_bg = color(0.025f, 0.030f, 0.038f, 0.98f),
        .panel_bg = color(0.075f, 0.085f, 0.102f),
        .panel_border = color(0.36f, 0.40f, 0.46f),
        .frame_bg = color(0.115f, 0.130f, 0.155f),
        .frame_bg_hovered = color(0.170f, 0.200f, 0.235f),
        .frame_bg_active = color(0.145f, 0.245f, 0.285f),
        .header = color(0.110f, 0.330f, 0.380f, 0.72f),
        .header_hovered = color(0.150f, 0.430f, 0.490f, 0.82f),
        .header_active = color(0.100f, 0.560f, 0.640f, 0.90f),
        .accent = color(0.23f, 0.86f, 0.94f),
        .accent_hovered = color(0.42f, 0.94f, 1.00f),
        .accent_active = color(0.12f, 0.68f, 0.78f),
        .info = color(0.42f, 0.68f, 1.00f),
        .success = color(0.39f, 0.86f, 0.52f),
        .warning = color(1.00f, 0.78f, 0.32f),
        .danger = color(1.00f, 0.42f, 0.44f),
        .button = color(0.165f, 0.190f, 0.225f),
        .button_hovered = color(0.225f, 0.265f, 0.310f),
        .button_active = color(0.120f, 0.310f, 0.360f),
        .button_primary = color(0.070f, 0.360f, 0.420f),
        .button_primary_hovered = color(0.090f, 0.470f, 0.540f),
        .button_primary_active = color(0.040f, 0.290f, 0.350f),
        .button_danger = color(0.520f, 0.140f, 0.160f),
        .button_danger_hovered = color(0.720f, 0.190f, 0.220f),
        .button_danger_active = color(0.400f, 0.100f, 0.120f),
        .axis_x = color(1.00f, 0.36f, 0.40f),
        .axis_y = color(0.42f, 0.90f, 0.48f),
        .axis_z = color(0.42f, 0.62f, 1.00f),
        .asset_texture = color(1.00f, 0.68f, 0.28f),
        .asset_mesh = color(0.42f, 0.68f, 1.00f),
        .asset_material = color(0.42f, 0.86f, 0.54f),
        .asset_model = color(0.74f, 0.62f, 1.00f),
        .asset_scene = color(0.92f, 0.74f, 0.42f),
        .asset_script = color(0.30f, 0.86f, 0.92f),
    };
}

EditorThemeMetrics highContrastDarkMetrics()
{
    EditorThemeMetrics metrics = modernLightweightMetrics();
    metrics.window_rounding = 3.0f;
    metrics.child_rounding = 3.0f;
    metrics.frame_rounding = 3.0f;
    metrics.panel_rounding = 4.0f;
    metrics.badge_rounding = 4.0f;
    metrics.section_accent_width = 4.0f;
    metrics.metric_accent_width = 4.0f;
    return metrics;
}

const EditorThemePalette& activeEditorThemePalette() noexcept
{
    return g_editor_theme_palette;
}

const EditorThemeMetrics& activeEditorThemeMetrics() noexcept
{
    return g_editor_theme_metrics;
}

void selectEditorTheme(EditorThemePreset preset) noexcept
{
    g_editor_theme_preset = preset;
    switch (preset) {
        case EditorThemePreset::HighContrastDark:
            g_editor_theme_palette = highContrastDarkPalette();
            g_editor_theme_metrics = highContrastDarkMetrics();
            break;
        case EditorThemePreset::ModernLightweight:
            g_editor_theme_palette = modernLightweightPalette();
            g_editor_theme_metrics = modernLightweightMetrics();
            break;
    }
}

void applyActiveEditorTheme()
{
    ImGui::GetStyle() = ImGuiStyle{};
    const EditorThemePalette& palette = activeEditorThemePalette();
    const EditorThemeMetrics& metrics = activeEditorThemeMetrics();

    ImGuiStyle& style = ImGui::GetStyle();
    style.FontScaleMain = 1.0f;
    style.FontScaleDpi = 1.0f;
    style.WindowPadding = ImVec2(metrics.window_padding_x, metrics.window_padding_y);
    style.FramePadding = ImVec2(metrics.frame_padding_x, metrics.frame_padding_y);
    style.CellPadding = ImVec2(metrics.cell_padding_x, metrics.cell_padding_y);
    style.ItemSpacing = ImVec2(metrics.item_spacing_x, metrics.item_spacing_y);
    style.ItemInnerSpacing = ImVec2(metrics.item_inner_spacing_x, metrics.item_inner_spacing_y);
    style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
    style.IndentSpacing = metrics.indent_spacing;
    style.ScrollbarSize = metrics.scrollbar_size;
    style.GrabMinSize = metrics.grab_min_size;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.TabBorderSize = 1.0f;

    style.WindowRounding = metrics.window_rounding;
    style.ChildRounding = metrics.child_rounding;
    style.FrameRounding = metrics.frame_rounding;
    style.PopupRounding = metrics.popup_rounding;
    style.ScrollbarRounding = metrics.scrollbar_rounding;
    style.GrabRounding = metrics.grab_rounding;
    style.TabRounding = metrics.tab_rounding;

    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.5f);
    style.SeparatorTextBorderSize = 1.0f;
    style.SeparatorTextAlign = ImVec2(0.0f, 0.5f);
    style.SeparatorTextPadding = ImVec2(20.0f, 4.0f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = palette.text;
    colors[ImGuiCol_TextDisabled] = palette.text_muted;

    colors[ImGuiCol_WindowBg] = palette.window_bg;
    colors[ImGuiCol_ChildBg] = palette.window_bg;
    colors[ImGuiCol_PopupBg] = palette.popup_bg;
    colors[ImGuiCol_Border] = palette.panel_border;
    colors[ImGuiCol_BorderShadow] = color(0.0f, 0.0f, 0.0f, 0.0f);

    colors[ImGuiCol_FrameBg] = palette.frame_bg;
    colors[ImGuiCol_FrameBgHovered] = palette.frame_bg_hovered;
    colors[ImGuiCol_FrameBgActive] = palette.frame_bg_active;

    colors[ImGuiCol_TitleBg] = palette.window_bg;
    colors[ImGuiCol_TitleBgActive] = palette.panel_bg;
    colors[ImGuiCol_TitleBgCollapsed] = palette.panel_bg;
    colors[ImGuiCol_MenuBarBg] = palette.window_bg;

    colors[ImGuiCol_Button] = palette.button;
    colors[ImGuiCol_ButtonHovered] = palette.button_hovered;
    colors[ImGuiCol_ButtonActive] = palette.button_active;

    colors[ImGuiCol_Header] = palette.header;
    colors[ImGuiCol_HeaderHovered] = palette.header_hovered;
    colors[ImGuiCol_HeaderActive] = palette.header_active;

    colors[ImGuiCol_CheckMark] = palette.accent_hovered;
    colors[ImGuiCol_SliderGrab] = palette.accent;
    colors[ImGuiCol_SliderGrabActive] = palette.accent_hovered;

    colors[ImGuiCol_ScrollbarBg] = color(0.0f, 0.0f, 0.0f, 0.16f);
    colors[ImGuiCol_ScrollbarGrab] = color(0.36f, 0.39f, 0.43f, 0.72f);
    colors[ImGuiCol_ScrollbarGrabHovered] = color(0.44f, 0.48f, 0.54f, 0.86f);
    colors[ImGuiCol_ScrollbarGrabActive] = palette.accent;

    colors[ImGuiCol_Tab] = palette.window_bg;
    colors[ImGuiCol_TabHovered] = palette.header_hovered;
    colors[ImGuiCol_TabSelected] = palette.panel_bg;
    colors[ImGuiCol_TabSelectedOverline] = palette.accent;
    colors[ImGuiCol_TabUnfocused] = palette.window_bg;
    colors[ImGuiCol_TabUnfocusedActive] = palette.frame_bg;

    colors[ImGuiCol_Separator] = palette.panel_border;
    colors[ImGuiCol_SeparatorHovered] = palette.accent_hovered;
    colors[ImGuiCol_SeparatorActive] = palette.accent_active;

    colors[ImGuiCol_ResizeGrip] = ImVec4{palette.accent.x, palette.accent.y, palette.accent.z, 0.18f};
    colors[ImGuiCol_ResizeGripHovered] = ImVec4{palette.accent.x, palette.accent.y, palette.accent.z, 0.45f};
    colors[ImGuiCol_ResizeGripActive] = ImVec4{palette.accent.x, palette.accent.y, palette.accent.z, 0.70f};
    colors[ImGuiCol_TextSelectedBg] = ImVec4{palette.accent.x, palette.accent.y, palette.accent.z, 0.32f};

    colors[ImGuiCol_TableHeaderBg] = color(0.165f, 0.180f, 0.204f);
    colors[ImGuiCol_TableBorderStrong] = palette.panel_border;
    colors[ImGuiCol_TableBorderLight] = ImVec4{palette.panel_border.x, palette.panel_border.y, palette.panel_border.z, 0.55f};
    colors[ImGuiCol_TableRowBg] = color(1.0f, 1.0f, 1.0f, 0.010f);
    colors[ImGuiCol_TableRowBgAlt] = color(1.0f, 1.0f, 1.0f, 0.035f);

    colors[ImGuiCol_DockingPreview] = ImVec4{palette.accent.x, palette.accent.y, palette.accent.z, 0.35f};
    colors[ImGuiCol_DockingEmptyBg] = palette.window_bg;
    colors[ImGuiCol_NavHighlight] = palette.accent;
}

void clampScaledStyleMetrics(ImGuiStyle& style)
{
    style.WindowBorderSize = (std::max)(style.WindowBorderSize, 1.0f);
    style.ChildBorderSize = (std::max)(style.ChildBorderSize, 1.0f);
    style.PopupBorderSize = (std::max)(style.PopupBorderSize, 1.0f);
    style.SeparatorSize = (std::max)(style.SeparatorSize, 1.0f);
    style.SeparatorTextBorderSize = (std::max)(style.SeparatorTextBorderSize, 1.0f);
    style.TreeLinesSize = (std::max)(style.TreeLinesSize, 1.0f);
    style.DragDropTargetBorderSize = (std::max)(style.DragDropTargetBorderSize, 1.0f);
    style.DockingSeparatorSize = (std::max)(style.DockingSeparatorSize, 1.0f);
}

} // namespace

float getEditorUiScale() noexcept
{
    return g_editor_ui_scale;
}

float scaleEditorUi(float value) noexcept
{
    return value * g_editor_ui_scale;
}

ImVec2 scaleEditorUi(float x, float y) noexcept
{
    return ImVec2{x * g_editor_ui_scale, y * g_editor_ui_scale};
}

ImVec4 editorThemeColor(EditorThemeColor color) noexcept
{
    const EditorThemePalette& palette = activeEditorThemePalette();
    switch (color) {
        case EditorThemeColor::Text:
            return palette.text;
        case EditorThemeColor::TextMuted:
            return palette.text_muted;
        case EditorThemeColor::WindowBg:
            return palette.window_bg;
        case EditorThemeColor::PopupBg:
            return palette.popup_bg;
        case EditorThemeColor::PanelBg:
            return palette.panel_bg;
        case EditorThemeColor::PanelBorder:
            return palette.panel_border;
        case EditorThemeColor::FrameBg:
            return palette.frame_bg;
        case EditorThemeColor::FrameBgHovered:
            return palette.frame_bg_hovered;
        case EditorThemeColor::FrameBgActive:
            return palette.frame_bg_active;
        case EditorThemeColor::Header:
            return palette.header;
        case EditorThemeColor::HeaderHovered:
            return palette.header_hovered;
        case EditorThemeColor::HeaderActive:
            return palette.header_active;
        case EditorThemeColor::Accent:
            return palette.accent;
        case EditorThemeColor::AccentHovered:
            return palette.accent_hovered;
        case EditorThemeColor::AccentActive:
            return palette.accent_active;
        case EditorThemeColor::Info:
            return palette.info;
        case EditorThemeColor::Success:
            return palette.success;
        case EditorThemeColor::Warning:
            return palette.warning;
        case EditorThemeColor::Danger:
            return palette.danger;
        case EditorThemeColor::Button:
            return palette.button;
        case EditorThemeColor::ButtonHovered:
            return palette.button_hovered;
        case EditorThemeColor::ButtonActive:
            return palette.button_active;
        case EditorThemeColor::ButtonPrimary:
            return palette.button_primary;
        case EditorThemeColor::ButtonPrimaryHovered:
            return palette.button_primary_hovered;
        case EditorThemeColor::ButtonPrimaryActive:
            return palette.button_primary_active;
        case EditorThemeColor::ButtonDanger:
            return palette.button_danger;
        case EditorThemeColor::ButtonDangerHovered:
            return palette.button_danger_hovered;
        case EditorThemeColor::ButtonDangerActive:
            return palette.button_danger_active;
        case EditorThemeColor::AxisX:
            return palette.axis_x;
        case EditorThemeColor::AxisY:
            return palette.axis_y;
        case EditorThemeColor::AxisZ:
            return palette.axis_z;
        case EditorThemeColor::AssetTexture:
            return palette.asset_texture;
        case EditorThemeColor::AssetMesh:
            return palette.asset_mesh;
        case EditorThemeColor::AssetMaterial:
            return palette.asset_material;
        case EditorThemeColor::AssetModel:
            return palette.asset_model;
        case EditorThemeColor::AssetScene:
            return palette.asset_scene;
        case EditorThemeColor::AssetScript:
            return palette.asset_script;
    }

    return palette.text;
}

ImVec4 editorThemeColor(EditorThemeColor color, float alpha) noexcept
{
    ImVec4 result = editorThemeColor(color);
    result.w = alpha;
    return result;
}

float editorThemeMetric(EditorThemeMetric metric) noexcept
{
    const EditorThemeMetrics& metrics = activeEditorThemeMetrics();
    float value = 0.0f;

    switch (metric) {
        case EditorThemeMetric::WindowPaddingX:
            value = metrics.window_padding_x;
            break;
        case EditorThemeMetric::WindowPaddingY:
            value = metrics.window_padding_y;
            break;
        case EditorThemeMetric::FramePaddingX:
            value = metrics.frame_padding_x;
            break;
        case EditorThemeMetric::FramePaddingY:
            value = metrics.frame_padding_y;
            break;
        case EditorThemeMetric::CellPaddingX:
            value = metrics.cell_padding_x;
            break;
        case EditorThemeMetric::CellPaddingY:
            value = metrics.cell_padding_y;
            break;
        case EditorThemeMetric::ItemSpacingX:
            value = metrics.item_spacing_x;
            break;
        case EditorThemeMetric::ItemSpacingY:
            value = metrics.item_spacing_y;
            break;
        case EditorThemeMetric::ItemInnerSpacingX:
            value = metrics.item_inner_spacing_x;
            break;
        case EditorThemeMetric::ItemInnerSpacingY:
            value = metrics.item_inner_spacing_y;
            break;
        case EditorThemeMetric::IndentSpacing:
            value = metrics.indent_spacing;
            break;
        case EditorThemeMetric::ScrollbarSize:
            value = metrics.scrollbar_size;
            break;
        case EditorThemeMetric::GrabMinSize:
            value = metrics.grab_min_size;
            break;
        case EditorThemeMetric::WindowRounding:
            value = metrics.window_rounding;
            break;
        case EditorThemeMetric::ChildRounding:
            value = metrics.child_rounding;
            break;
        case EditorThemeMetric::FrameRounding:
            value = metrics.frame_rounding;
            break;
        case EditorThemeMetric::PopupRounding:
            value = metrics.popup_rounding;
            break;
        case EditorThemeMetric::ScrollbarRounding:
            value = metrics.scrollbar_rounding;
            break;
        case EditorThemeMetric::GrabRounding:
            value = metrics.grab_rounding;
            break;
        case EditorThemeMetric::TabRounding:
            value = metrics.tab_rounding;
            break;
        case EditorThemeMetric::PropertyLabelWidth:
            value = metrics.property_label_width;
            break;
        case EditorThemeMetric::PropertyRowPaddingY:
            value = metrics.property_row_padding_y;
            break;
        case EditorThemeMetric::BadgePaddingX:
            value = metrics.badge_padding_x;
            break;
        case EditorThemeMetric::BadgePaddingY:
            value = metrics.badge_padding_y;
            break;
        case EditorThemeMetric::BadgeRounding:
            value = metrics.badge_rounding;
            break;
        case EditorThemeMetric::MetricMinWidth:
            value = metrics.metric_min_width;
            break;
        case EditorThemeMetric::MetricPaddingLeft:
            value = metrics.metric_padding_left;
            break;
        case EditorThemeMetric::MetricPaddingRight:
            value = metrics.metric_padding_right;
            break;
        case EditorThemeMetric::MetricPaddingTop:
            value = metrics.metric_padding_top;
            break;
        case EditorThemeMetric::MetricPaddingBottom:
            value = metrics.metric_padding_bottom;
            break;
        case EditorThemeMetric::MetricLineGap:
            value = metrics.metric_line_gap;
            break;
        case EditorThemeMetric::MetricAccentWidth:
            value = metrics.metric_accent_width;
            break;
        case EditorThemeMetric::MetricDefaultHeight:
            value = metrics.metric_default_height;
            break;
        case EditorThemeMetric::MetricDetailedHeight:
            value = metrics.metric_detailed_height;
            break;
        case EditorThemeMetric::EmptyStateMinWidth:
            value = metrics.empty_state_min_width;
            break;
        case EditorThemeMetric::EmptyStateHeight:
            value = metrics.empty_state_height;
            break;
        case EditorThemeMetric::EmptyStateDetailedHeight:
            value = metrics.empty_state_detailed_height;
            break;
        case EditorThemeMetric::EmptyStateDetailTop:
            value = metrics.empty_state_detail_top;
            break;
        case EditorThemeMetric::EmptyStateLineGap:
            value = metrics.empty_state_line_gap;
            break;
        case EditorThemeMetric::PanelPaddingX:
            value = metrics.panel_padding_x;
            break;
        case EditorThemeMetric::PanelPaddingY:
            value = metrics.panel_padding_y;
            break;
        case EditorThemeMetric::PanelRounding:
            value = metrics.panel_rounding;
            break;
        case EditorThemeMetric::HeadingAccentOffsetY:
            value = metrics.heading_accent_offset_y;
            break;
        case EditorThemeMetric::HeadingAccentWidth:
            value = metrics.heading_accent_width;
            break;
        case EditorThemeMetric::HeadingAccentRounding:
            value = metrics.heading_accent_rounding;
            break;
        case EditorThemeMetric::HeadingIndent:
            value = metrics.heading_indent;
            break;
        case EditorThemeMetric::SectionFramePaddingX:
            value = metrics.section_frame_padding_x;
            break;
        case EditorThemeMetric::SectionFramePaddingY:
            value = metrics.section_frame_padding_y;
            break;
        case EditorThemeMetric::SectionAccentWidth:
            value = metrics.section_accent_width;
            break;
        case EditorThemeMetric::CompactInspectorItemSpacingX:
            value = metrics.compact_inspector_item_spacing_x;
            break;
        case EditorThemeMetric::CompactInspectorItemSpacingY:
            value = metrics.compact_inspector_item_spacing_y;
            break;
        case EditorThemeMetric::CompactInspectorItemInnerSpacingX:
            value = metrics.compact_inspector_item_inner_spacing_x;
            break;
        case EditorThemeMetric::CompactInspectorItemInnerSpacingY:
            value = metrics.compact_inspector_item_inner_spacing_y;
            break;
        case EditorThemeMetric::CompactInspectorFramePaddingX:
            value = metrics.compact_inspector_frame_padding_x;
            break;
        case EditorThemeMetric::CompactInspectorFramePaddingY:
            value = metrics.compact_inspector_frame_padding_y;
            break;
        case EditorThemeMetric::CompactInspectorCellPaddingX:
            value = metrics.compact_inspector_cell_padding_x;
            break;
        case EditorThemeMetric::CompactInspectorCellPaddingY:
            value = metrics.compact_inspector_cell_padding_y;
            break;
        case EditorThemeMetric::CompactInspectorIndentSpacing:
            value = metrics.compact_inspector_indent_spacing;
            break;
        case EditorThemeMetric::AssetPreviewMinWidth:
            value = metrics.asset_preview_min_width;
            break;
        case EditorThemeMetric::AssetPreviewExtraHeight:
            value = metrics.asset_preview_extra_height;
            break;
        case EditorThemeMetric::AssetPreviewAccentOffsetX:
            value = metrics.asset_preview_accent_offset_x;
            break;
        case EditorThemeMetric::AssetPreviewAccentOffsetY:
            value = metrics.asset_preview_accent_offset_y;
            break;
        case EditorThemeMetric::AssetPreviewAccentWidth:
            value = metrics.asset_preview_accent_width;
            break;
        case EditorThemeMetric::AssetPreviewAccentRounding:
            value = metrics.asset_preview_accent_rounding;
            break;
        case EditorThemeMetric::AssetPreviewTextOffsetX:
            value = metrics.asset_preview_text_offset_x;
            break;
        case EditorThemeMetric::AssetPreviewTextOffsetY:
            value = metrics.asset_preview_text_offset_y;
            break;
        case EditorThemeMetric::AssetPreviewTextRightPadding:
            value = metrics.asset_preview_text_right_padding;
            break;
        case EditorThemeMetric::AssetPreviewLineGap:
            value = metrics.asset_preview_line_gap;
            break;
        case EditorThemeMetric::AxisControlSpacing:
            value = metrics.axis_control_spacing;
            break;
        case EditorThemeMetric::AxisControlMinDragWidth:
            value = metrics.axis_control_min_drag_width;
            break;
        case EditorThemeMetric::Vector2FramePaddingX:
            value = metrics.vector2_frame_padding_x;
            break;
        case EditorThemeMetric::Vector2FramePaddingY:
            value = metrics.vector2_frame_padding_y;
            break;
        case EditorThemeMetric::Vector3FramePaddingX:
            value = metrics.vector3_frame_padding_x;
            break;
        case EditorThemeMetric::Vector3FramePaddingY:
            value = metrics.vector3_frame_padding_y;
            break;
        case EditorThemeMetric::Vector3MinAxisWidth:
            value = metrics.vector3_min_axis_width;
            break;
        case EditorThemeMetric::ComponentIndentSpacing:
            value = metrics.component_indent_spacing;
            break;
    }

    return scaleEditorUi(value);
}

ImVec2 editorThemeMetric(EditorThemeMetric x, EditorThemeMetric y) noexcept
{
    return ImVec2{editorThemeMetric(x), editorThemeMetric(y)};
}

EditorThemePreset activeEditorThemePreset() noexcept
{
    return g_editor_theme_preset;
}

const EditorThemePreset* editorThemePresets() noexcept
{
    return kEditorThemePresets.data();
}

std::size_t editorThemePresetCount() noexcept
{
    return kEditorThemePresets.size();
}

std::string_view editorThemePresetName(EditorThemePreset preset) noexcept
{
    switch (preset) {
        case EditorThemePreset::ModernLightweight:
            return "Modern Lightweight";
        case EditorThemePreset::HighContrastDark:
            return "High Contrast Dark";
    }

    return "Modern Lightweight";
}

EditorThemePreset editorThemePresetFromName(std::string_view name) noexcept
{
    if (name == "HighContrastDark" || name == "High Contrast Dark") {
        return EditorThemePreset::HighContrastDark;
    }
    return EditorThemePreset::ModernLightweight;
}

void applyEditorTheme(EditorThemePreset preset, float ui_scale)
{
    g_editor_ui_scale = sanitizeUiScale(ui_scale);
    selectEditorTheme(preset);
    applyActiveEditorTheme();

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(g_editor_ui_scale);
    clampScaledStyleMetrics(style);
    style.FontScaleMain = g_editor_ui_scale;
    style.FontScaleDpi = 1.0f;
}

} // namespace luna::editor
