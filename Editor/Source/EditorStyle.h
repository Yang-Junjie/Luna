#pragma once

#include <cstdint>

struct ImVec2;

namespace luna::editor {

enum class EditorThemePreset : uint8_t {
    ModernLightweight,
};

[[nodiscard]] float getEditorUiScale() noexcept;
[[nodiscard]] float scaleEditorUi(float value) noexcept;
[[nodiscard]] ImVec2 scaleEditorUi(float x, float y) noexcept;

void applyEditorTheme(EditorThemePreset preset = EditorThemePreset::ModernLightweight, float ui_scale = 1.0f);

} // namespace luna::editor
