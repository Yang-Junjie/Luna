#pragma once

#include "Imgui/ImGuiLayer.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace luna::editor {

struct EditorSettingsData {
    std::filesystem::path font_path;
    float font_size_pixels{16.0f};
};

struct EditorSettingsFontInfo {
    std::string display_name;
    std::filesystem::path path;
    bool active{false};
};

class EditorSettingsStore {
public:
    EditorSettingsStore(std::filesystem::path settings_path,
                        std::filesystem::path default_font_path,
                        std::vector<std::filesystem::path> font_roots);

    [[nodiscard]] const EditorSettingsData& data() const noexcept;
    [[nodiscard]] const std::filesystem::path& settingsPath() const noexcept;
    [[nodiscard]] const std::filesystem::path& defaultFontPath() const noexcept;
    [[nodiscard]] const std::string& lastError() const noexcept;
    [[nodiscard]] bool restartRequired() const noexcept;
    [[nodiscard]] bool usingDefaultFont() const;
    [[nodiscard]] ImGuiFontConfig imguiFontConfig() const;
    [[nodiscard]] std::vector<EditorSettingsFontInfo> listFonts() const;

    bool load();
    bool save();
    bool setEditorFont(std::filesystem::path font_path, float size_pixels);
    bool resetEditorFont();

private:
    [[nodiscard]] std::filesystem::path activeFontPath() const;
    [[nodiscard]] float sanitizedFontSize(float value) const noexcept;
    [[nodiscard]] bool isKnownFontPath(const std::filesystem::path& path) const;

private:
    std::filesystem::path m_settings_path;
    std::filesystem::path m_default_font_path;
    std::vector<std::filesystem::path> m_font_roots;
    EditorSettingsData m_data;
    EditorSettingsData m_loaded_data;
    std::string m_last_error;
    bool m_restart_required{false};
};

} // namespace luna::editor
