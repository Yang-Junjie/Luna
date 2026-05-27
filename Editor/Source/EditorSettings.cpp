#include "Core/Log.h"
#include "EditorSettings.h"

#include <cctype>
#include <cmath>

#include <algorithm>
#include <fstream>
#include <system_error>
#include <yaml-cpp/yaml.h>

namespace {

constexpr const char* kRootKey = "EditorSettings";
constexpr float kDefaultFontSizePixels = 16.0f;

std::string_view themePresetName(luna::editor::EditorThemePreset preset) noexcept
{
    switch (preset) {
        case luna::editor::EditorThemePreset::HighContrastDark:
            return "HighContrastDark";
        case luna::editor::EditorThemePreset::ModernLightweight:
            return "ModernLightweight";
    }
    return "ModernLightweight";
}

bool tryParseThemePreset(std::string_view value, luna::editor::EditorThemePreset& out_preset) noexcept
{
    if (value == "ModernLightweight" || value == "Modern Lightweight") {
        out_preset = luna::editor::EditorThemePreset::ModernLightweight;
        return true;
    }
    if (value == "HighContrastDark" || value == "High Contrast Dark") {
        out_preset = luna::editor::EditorThemePreset::HighContrastDark;
        return true;
    }
    return false;
}

std::filesystem::path normalizePath(std::filesystem::path path)
{
    return path.lexically_normal();
}

bool isFontFile(const std::filesystem::path& path)
{
    const std::string extension = path.extension().string();
    std::string lowered;
    lowered.reserve(extension.size());
    std::transform(extension.begin(), extension.end(), std::back_inserter(lowered), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lowered == ".ttf" || lowered == ".otf";
}

bool samePath(const std::filesystem::path& lhs, const std::filesystem::path& rhs)
{
    return normalizePath(lhs).generic_string() == normalizePath(rhs).generic_string();
}

} // namespace

namespace luna::editor {

EditorSettingsStore::EditorSettingsStore(std::filesystem::path settings_path,
                                         std::filesystem::path default_font_path,
                                         std::vector<std::filesystem::path> font_roots)
    : m_settings_path(normalizePath(std::move(settings_path))),
      m_default_font_path(normalizePath(std::move(default_font_path))),
      m_font_roots(std::move(font_roots))
{
    for (std::filesystem::path& root : m_font_roots) {
        root = normalizePath(std::move(root));
    }
    m_data = EditorSettingsData{.theme_preset = EditorThemePreset::ModernLightweight,
                                .font_size_pixels = kDefaultFontSizePixels};
    m_loaded_data = m_data;
}

const EditorSettingsData& EditorSettingsStore::data() const noexcept
{
    return m_data;
}

const std::filesystem::path& EditorSettingsStore::settingsPath() const noexcept
{
    return m_settings_path;
}

const std::filesystem::path& EditorSettingsStore::defaultFontPath() const noexcept
{
    return m_default_font_path;
}

const std::string& EditorSettingsStore::lastError() const noexcept
{
    return m_last_error;
}

bool EditorSettingsStore::restartRequired() const noexcept
{
    return m_restart_required;
}

bool EditorSettingsStore::usingDefaultFont() const
{
    return m_data.font_path.empty();
}

ImGuiFontConfig EditorSettingsStore::imguiFontConfig() const
{
    return ImGuiFontConfig{
        .font_path = activeFontPath(),
        .size_pixels = sanitizedFontSize(m_data.font_size_pixels),
    };
}

std::vector<EditorSettingsFontInfo> EditorSettingsStore::listFonts() const
{
    std::vector<EditorSettingsFontInfo> fonts;
    const std::filesystem::path active_path = activeFontPath();

    std::error_code ec;
    for (const std::filesystem::path& root : m_font_roots) {
        if (root.empty() || !std::filesystem::exists(root, ec) || ec || !std::filesystem::is_directory(root, ec) ||
            ec) {
            ec.clear();
            continue;
        }

        for (std::filesystem::directory_iterator
                 it(root, std::filesystem::directory_options::skip_permission_denied, ec),
             end;
             !ec && it != end;
             it.increment(ec)) {
            const std::filesystem::path path = it->path().lexically_normal();
            if (!it->is_regular_file(ec) || ec || !isFontFile(path)) {
                ec.clear();
                continue;
            }

            const auto duplicate = std::find_if(fonts.begin(), fonts.end(), [&](const EditorSettingsFontInfo& font) {
                return samePath(font.path, path);
            });
            if (duplicate != fonts.end()) {
                continue;
            }

            fonts.push_back(EditorSettingsFontInfo{
                .display_name = path.stem().string(),
                .path = path,
                .active = samePath(path, active_path),
            });
        }
        if (ec) {
            LUNA_EDITOR_WARN("Failed to scan editor fonts in '{}': {}", root.string(), ec.message());
            ec.clear();
        }
    }

    std::sort(fonts.begin(), fonts.end(), [](const EditorSettingsFontInfo& lhs, const EditorSettingsFontInfo& rhs) {
        return lhs.display_name < rhs.display_name;
    });
    return fonts;
}

bool EditorSettingsStore::load()
{
    m_last_error.clear();
    m_data = EditorSettingsData{.theme_preset = EditorThemePreset::ModernLightweight,
                                .font_size_pixels = kDefaultFontSizePixels};

    std::error_code exists_ec;
    if (!std::filesystem::exists(m_settings_path, exists_ec)) {
        m_loaded_data = m_data;
        m_restart_required = false;
        return true;
    }
    if (exists_ec) {
        m_last_error = "Failed to check editor settings file: " + exists_ec.message();
        LUNA_EDITOR_WARN("{}", m_last_error);
        m_loaded_data = m_data;
        m_restart_required = false;
        return false;
    }

    try {
        const YAML::Node root = YAML::LoadFile(m_settings_path.string());
        const YAML::Node settings = root[kRootKey] ? root[kRootKey] : root;
        if (const YAML::Node theme = settings["Theme"]) {
            if (const YAML::Node preset = theme["Preset"]) {
                EditorThemePreset parsed_preset = EditorThemePreset::ModernLightweight;
                if (tryParseThemePreset(preset.as<std::string>(), parsed_preset)) {
                    m_data.theme_preset = parsed_preset;
                }
            }
        }
        if (const YAML::Node font = settings["Font"]) {
            if (const YAML::Node path = font["Path"]) {
                m_data.font_path = normalizePath(std::filesystem::path(path.as<std::string>()));
            }
            if (const YAML::Node size = font["SizePixels"]) {
                m_data.font_size_pixels = sanitizedFontSize(size.as<float>());
            }
        }
    } catch (const YAML::Exception& error) {
        m_last_error = std::string("Failed to load editor settings: ") + error.what();
        LUNA_EDITOR_WARN("{}", m_last_error);
        m_data = EditorSettingsData{.theme_preset = EditorThemePreset::ModernLightweight,
                                    .font_size_pixels = kDefaultFontSizePixels};
        m_loaded_data = m_data;
        m_restart_required = false;
        return false;
    }

    m_loaded_data = m_data;
    m_restart_required = false;
    return true;
}

bool EditorSettingsStore::save()
{
    m_last_error.clear();

    std::error_code create_ec;
    std::filesystem::create_directories(m_settings_path.parent_path(), create_ec);
    if (create_ec) {
        m_last_error = "Failed to create editor settings directory: " + create_ec.message();
        LUNA_EDITOR_WARN("{}", m_last_error);
        return false;
    }

    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << kRootKey << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "Theme" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "Preset" << YAML::Value << std::string(themePresetName(m_data.theme_preset));
    out << YAML::EndMap;
    out << YAML::Key << "Font" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "Path" << YAML::Value << m_data.font_path.generic_string();
    out << YAML::Key << "SizePixels" << YAML::Value << sanitizedFontSize(m_data.font_size_pixels);
    out << YAML::EndMap;
    out << YAML::EndMap;
    out << YAML::EndMap;

    std::ofstream file(m_settings_path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        m_last_error = "Failed to open editor settings file for writing.";
        LUNA_EDITOR_WARN("{}", m_last_error);
        return false;
    }

    file << out.c_str();
    if (!file.good()) {
        m_last_error = "Failed to write editor settings file.";
        LUNA_EDITOR_WARN("{}", m_last_error);
        return false;
    }

    return true;
}

bool EditorSettingsStore::setEditorTheme(EditorThemePreset preset)
{
    m_last_error.clear();
    m_data.theme_preset = preset;
    return true;
}

bool EditorSettingsStore::setEditorFont(std::filesystem::path font_path, float size_pixels)
{
    m_last_error.clear();
    font_path = normalizePath(std::move(font_path));
    if (font_path.empty()) {
        m_data.font_path.clear();
        m_data.font_size_pixels = sanitizedFontSize(size_pixels);
        m_restart_required =
            !samePath(activeFontPath(),
                      m_loaded_data.font_path.empty() ? m_default_font_path : m_loaded_data.font_path) ||
            sanitizedFontSize(m_data.font_size_pixels) != sanitizedFontSize(m_loaded_data.font_size_pixels);
        return true;
    }
    if (!isKnownFontPath(font_path)) {
        m_last_error = "Font is outside known editor font directories: " + font_path.generic_string();
        return false;
    }

    m_data.font_path = std::move(font_path);
    m_data.font_size_pixels = sanitizedFontSize(size_pixels);
    m_restart_required =
        !samePath(activeFontPath(), m_loaded_data.font_path.empty() ? m_default_font_path : m_loaded_data.font_path) ||
        sanitizedFontSize(m_data.font_size_pixels) != sanitizedFontSize(m_loaded_data.font_size_pixels);
    return true;
}

bool EditorSettingsStore::resetEditorFont()
{
    m_last_error.clear();
    m_data.font_path.clear();
    m_data.font_size_pixels = kDefaultFontSizePixels;
    m_restart_required =
        !m_loaded_data.font_path.empty() || sanitizedFontSize(m_loaded_data.font_size_pixels) != kDefaultFontSizePixels;
    return true;
}

std::filesystem::path EditorSettingsStore::activeFontPath() const
{
    return m_data.font_path.empty() ? m_default_font_path : m_data.font_path;
}

float EditorSettingsStore::sanitizedFontSize(float value) const noexcept
{
    if (!std::isfinite(value) || value <= 0.0f) {
        return kDefaultFontSizePixels;
    }
    return std::clamp(value, 8.0f, 48.0f);
}

bool EditorSettingsStore::isKnownFontPath(const std::filesystem::path& path) const
{
    if (!isFontFile(path)) {
        return false;
    }

    const std::filesystem::path normalized_path = normalizePath(path);
    for (const std::filesystem::path& root : m_font_roots) {
        if (samePath(normalized_path.parent_path(), root)) {
            return true;
        }
    }
    return false;
}

} // namespace luna::editor
