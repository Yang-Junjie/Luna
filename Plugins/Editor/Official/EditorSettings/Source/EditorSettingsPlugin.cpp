#include "EditorSettingsPlugin.h"

#include "EditorApi/EditorApi.h"
#include "Luna/Editor/EditorBuiltinPluginRegistration.h"

#include <filesystem>
#include <string>
#include <vector>

namespace {

constexpr const char* kPluginId = "luna.editor.editor-settings";
constexpr const char* kWindowId = "luna.editor.editor-settings.window";

std::string pathLabel(const std::filesystem::path& path)
{
    return path.empty() ? std::string("<none>") : path.generic_string();
}

void saveStatus(luna::editor::SettingsService& settings, std::string& status)
{
    if (settings.save()) {
        status = "Saved. Restart Luna Editor to apply font changes.";
        return;
    }

    status = settings.lastError().empty() ? "Failed to save editor settings." : settings.lastError();
}

class EditorSettingsPlugin final : public luna::editor::Plugin {
public:
    [[nodiscard]] luna::editor::PluginDescriptor descriptor() const override
    {
        return luna::editor::PluginDescriptor{
            .id = kPluginId,
            .display_name = "Editor Settings",
            .version = "0.1.0",
        };
    }

    bool onLoad(luna::editor::Host& host) override
    {
        return host.windows().registerWindow(luna::editor::WindowDescriptor{
            .id = kWindowId,
            .title = "Editor Settings",
            .default_open = false,
            .default_size = luna::editor::Vec2{.x = 560.0f, .y = 420.0f},
            .draw =
                [this](luna::editor::WindowDrawContext& context) {
                    luna::editor::Host& host = context.host();
                    luna::editor::Ui& ui = context.ui();
                    luna::editor::SettingsService& settings = host.settings();

                    luna::editor::EditorFontSettings font = settings.editorFont();
                    ui.text(std::string("Settings File: ") + pathLabel(settings.settingsPath()));
                    ui.separatorText("Font");

                    ui.text(std::string("Active: ") + pathLabel(font.font_path));
                    ui.text(std::string("Mode: ") + (font.using_default ? "Default" : "Custom"));

                    float font_size = font.size_pixels;
                    if (ui.dragFloat("Size Pixels", font_size, 0.25f, 8.0f, 48.0f, "%.1f")) {
                        const std::filesystem::path selected_font_path =
                            font.using_default ? std::filesystem::path{} : font.font_path;
                        if (settings.setEditorFont(selected_font_path, font_size)) {
                            saveStatus(settings, m_status);
                        } else {
                            m_status = settings.lastError();
                        }
                    }

                    if (ui.button("Reset Font", luna::editor::ButtonVariant::Subtle)) {
                        if (settings.resetEditorFont()) {
                            saveStatus(settings, m_status);
                        } else {
                            m_status = settings.lastError();
                        }
                    }

                    const std::vector<luna::editor::EditorFontInfo> fonts = settings.listEditorFonts();
                    if (fonts.empty()) {
                        ui.textDisabled("No editor fonts were found.");
                    } else {
                        const luna::editor::TableFlags table_flags =
                            luna::editor::TableFlag::RowBg | luna::editor::TableFlag::BordersInnerH |
                            luna::editor::TableFlag::SizingStretchProp | luna::editor::TableFlag::ScrollY;
                        if (ui.beginTable("##EditorSettingsFonts", 2, table_flags,
                                          luna::editor::Vec2{.x = 0.0f, .y = 210.0f})) {
                            ui.tableSetupColumn("Font",
                                                static_cast<luna::editor::TableColumnFlags>(
                                                    luna::editor::TableColumnFlag::WidthFixed),
                                                180.0f);
                            ui.tableSetupColumn("Path",
                                                static_cast<luna::editor::TableColumnFlags>(
                                                    luna::editor::TableColumnFlag::WidthStretch),
                                                1.0f);
                            ui.tableHeadersRow();

                            for (const luna::editor::EditorFontInfo& candidate : fonts) {
                                ui.tableNextRow();
                                ui.tableNextColumn();
                                if (ui.selectable(candidate.display_name + "##" + candidate.path.generic_string(),
                                                  candidate.active)) {
                                    if (settings.setEditorFont(candidate.path, font_size)) {
                                        saveStatus(settings, m_status);
                                    } else {
                                        m_status = settings.lastError();
                                    }
                                }
                                if (candidate.active) {
                                    ui.setItemDefaultFocus();
                                }

                                ui.tableNextColumn();
                                ui.textDisabled(candidate.path.generic_string());
                            }

                            ui.endTable();
                        }
                    }

                    if (settings.restartRequired()) {
                        ui.textDisabled("Restart Luna Editor to apply font changes.");
                    }
                    if (!m_status.empty()) {
                        ui.textWrapped(m_status);
                    }
                },
        });
    }

    void onUnload(luna::editor::Host& host) override
    {
        host.windows().unregisterWindow(kWindowId);
    }

private:
    std::string m_status;
};

} // namespace

namespace luna::editor {

std::unique_ptr<Plugin> createEditorSettingsPlugin()
{
    return std::make_unique<EditorSettingsPlugin>();
}

namespace {

const EditorBuiltinPluginFactoryRegistration kEditorSettingsPluginRegistration{
    kPluginId,
    createEditorSettingsPlugin,
};

} // namespace

} // namespace luna::editor
