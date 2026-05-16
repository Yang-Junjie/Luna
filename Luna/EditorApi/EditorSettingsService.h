#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace luna::editor {

struct EditorFontSettings {
    std::filesystem::path font_path;
    float size_pixels{16.0f};
    bool using_default{true};
};

struct EditorFontInfo {
    std::string display_name;
    std::filesystem::path path;
    bool active{false};
};

class SettingsService {
public:
    virtual ~SettingsService() = default;

    [[nodiscard]] virtual EditorFontSettings editorFont() const = 0;
    [[nodiscard]] virtual std::vector<EditorFontInfo> listEditorFonts() const = 0;
    [[nodiscard]] virtual std::filesystem::path settingsPath() const = 0;
    [[nodiscard]] virtual std::string lastError() const = 0;
    [[nodiscard]] virtual bool restartRequired() const noexcept = 0;

    virtual bool setEditorFont(const std::filesystem::path& font_path, float size_pixels) = 0;
    virtual bool resetEditorFont() = 0;
    virtual bool save() = 0;
};

} // namespace luna::editor
