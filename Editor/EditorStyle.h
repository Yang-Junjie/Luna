#pragma once

#include <cstdint>

namespace luna::editor {

enum class EditorThemePreset : uint8_t {
    ModernLightweight,
};

void applyEditorTheme(EditorThemePreset preset = EditorThemePreset::ModernLightweight);

} // namespace luna::editor
