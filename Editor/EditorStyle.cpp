#include "EditorStyle.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>

namespace luna::editor {
namespace {

constexpr float kMinUiScale = 1.0f;
constexpr float kMaxUiScale = 2.5f;
constexpr float kEditorDensityScale = 0.88f;
float g_editor_ui_scale = 1.0f;

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

void applyModernLightweightTheme()
{
    ImGui::GetStyle() = ImGuiStyle{};

    ImGuiStyle& style = ImGui::GetStyle();
    style.FontScaleMain = 1.0f;
    style.FontScaleDpi = 1.0f;
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.FramePadding = ImVec2(7.0f, 3.0f);
    style.CellPadding = ImVec2(5.0f, 3.0f);
    style.ItemSpacing = ImVec2(7.0f, 4.0f);
    style.ItemInnerSpacing = ImVec2(5.0f, 3.0f);
    style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
    style.IndentSpacing = 14.0f;
    style.ScrollbarSize = 10.0f;
    style.GrabMinSize = 9.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;

    style.WindowRounding = 4.0f;
    style.ChildRounding = 3.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 3.0f;

    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = color(0.88f, 0.91f, 0.93f);
    colors[ImGuiCol_TextDisabled] = color(0.48f, 0.53f, 0.58f);

    colors[ImGuiCol_WindowBg] = color(0.075f, 0.083f, 0.095f);
    colors[ImGuiCol_ChildBg] = color(0.075f, 0.083f, 0.095f);
    colors[ImGuiCol_PopupBg] = color(0.065f, 0.073f, 0.085f, 0.98f);
    colors[ImGuiCol_Border] = color(0.18f, 0.20f, 0.23f);
    colors[ImGuiCol_BorderShadow] = color(0.0f, 0.0f, 0.0f, 0.0f);

    colors[ImGuiCol_FrameBg] = color(0.125f, 0.140f, 0.160f);
    colors[ImGuiCol_FrameBgHovered] = color(0.165f, 0.190f, 0.215f);
    colors[ImGuiCol_FrameBgActive] = color(0.105f, 0.180f, 0.205f);

    colors[ImGuiCol_TitleBg] = color(0.065f, 0.073f, 0.085f);
    colors[ImGuiCol_TitleBgActive] = color(0.095f, 0.110f, 0.128f);
    colors[ImGuiCol_TitleBgCollapsed] = color(0.065f, 0.073f, 0.085f);
    colors[ImGuiCol_MenuBarBg] = color(0.060f, 0.068f, 0.080f);

    const ImVec4 accent = color(0.20f, 0.72f, 0.78f);
    const ImVec4 accent_hovered = color(0.27f, 0.82f, 0.88f);
    const ImVec4 accent_active = color(0.13f, 0.58f, 0.65f);

    colors[ImGuiCol_Button] = color(0.145f, 0.165f, 0.190f);
    colors[ImGuiCol_ButtonHovered] = color(0.185f, 0.215f, 0.245f);
    colors[ImGuiCol_ButtonActive] = color(0.105f, 0.180f, 0.205f);

    colors[ImGuiCol_Header] = color(0.125f, 0.245f, 0.280f, 0.55f);
    colors[ImGuiCol_HeaderHovered] = color(0.175f, 0.335f, 0.375f, 0.65f);
    colors[ImGuiCol_HeaderActive] = color(0.135f, 0.435f, 0.485f, 0.75f);

    colors[ImGuiCol_CheckMark] = accent;
    colors[ImGuiCol_SliderGrab] = accent;
    colors[ImGuiCol_SliderGrabActive] = accent_hovered;

    colors[ImGuiCol_Tab] = color(0.095f, 0.108f, 0.125f);
    colors[ImGuiCol_TabHovered] = color(0.145f, 0.300f, 0.340f);
    colors[ImGuiCol_TabActive] = color(0.125f, 0.175f, 0.205f);
    colors[ImGuiCol_TabUnfocused] = color(0.075f, 0.083f, 0.095f);
    colors[ImGuiCol_TabUnfocusedActive] = color(0.105f, 0.120f, 0.140f);

    colors[ImGuiCol_Separator] = color(0.18f, 0.20f, 0.23f);
    colors[ImGuiCol_SeparatorHovered] = accent_hovered;
    colors[ImGuiCol_SeparatorActive] = accent_active;

    colors[ImGuiCol_ResizeGrip] = color(0.20f, 0.72f, 0.78f, 0.18f);
    colors[ImGuiCol_ResizeGripHovered] = color(0.20f, 0.72f, 0.78f, 0.45f);
    colors[ImGuiCol_ResizeGripActive] = color(0.20f, 0.72f, 0.78f, 0.70f);
    colors[ImGuiCol_TextSelectedBg] = color(0.20f, 0.72f, 0.78f, 0.30f);

    colors[ImGuiCol_TableHeaderBg] = color(0.105f, 0.120f, 0.140f);
    colors[ImGuiCol_TableBorderStrong] = color(0.20f, 0.23f, 0.26f);
    colors[ImGuiCol_TableBorderLight] = color(0.15f, 0.17f, 0.20f);
    colors[ImGuiCol_TableRowBg] = color(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_TableRowBgAlt] = color(1.0f, 1.0f, 1.0f, 0.025f);

    colors[ImGuiCol_DockingPreview] = color(0.20f, 0.72f, 0.78f, 0.35f);
    colors[ImGuiCol_DockingEmptyBg] = color(0.055f, 0.062f, 0.073f);
    colors[ImGuiCol_NavHighlight] = accent;
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

void applyEditorTheme(EditorThemePreset preset, float ui_scale)
{
    g_editor_ui_scale = sanitizeUiScale(ui_scale);

    switch (preset) {
        case EditorThemePreset::ModernLightweight:
            applyModernLightweightTheme();
            break;
    }

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(g_editor_ui_scale);
    clampScaledStyleMetrics(style);
    style.FontScaleMain = g_editor_ui_scale;
    style.FontScaleDpi = 1.0f;
}

} // namespace luna::editor
