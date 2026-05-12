#include "Plugins/ScriptPlugins/ScriptPluginsPlugin.h"

#include "EditorApi/EditorApi.h"

#include <string>
#include <vector>

namespace {

constexpr const char* kPluginId = "luna.editor.script-plugins";
constexpr const char* kWindowId = "luna.editor.script-plugins.window";

const char* scriptPluginScopeToString(luna::ScriptPluginScope scope)
{
    switch (scope) {
        case luna::ScriptPluginScope::Engine:
            return "Engine";
        case luna::ScriptPluginScope::Project:
            return "Project";
        default:
            return "Unknown";
    }
}

std::string joinScriptExtensions(const std::vector<std::string>& extensions)
{
    std::string joined_extensions;
    for (const std::string& extension : extensions) {
        if (!joined_extensions.empty()) {
            joined_extensions += ", ";
        }
        joined_extensions += extension;
    }
    return joined_extensions;
}

} // namespace

namespace luna::editor {

class ScriptPluginsPlugin final : public Plugin {
public:
    [[nodiscard]] PluginDescriptor descriptor() const override
    {
        return PluginDescriptor{
            .id = kPluginId,
            .display_name = "Script Plugins",
            .version = "0.1.0",
        };
    }

    bool onLoad(Host& host) override
    {
        return host.windows().registerWindow(WindowDescriptor{
            .id = kWindowId,
            .title = "Script Plugins",
            .default_open = true,
            .default_size = Vec2{.x = 700.0f, .y = 520.0f},
            .draw =
                [](WindowDrawContext& context) {
                    Host& host = context.host();
                    Ui& ui = context.ui();
                    ScriptPluginService& script_plugins = host.scriptPlugins();

                    if (!host.project().hasProjectLoaded()) {
                        ui.text("Open a project to configure script plugins.");
                        return;
                    }

                    if (ui.button("Refresh")) {
                        script_plugins.refreshProjectScriptPlugins();
                    }

                    const ScriptPluginCandidate* selected_candidate = script_plugins.getSelectedScriptPluginCandidate();
                    ui.sameLine();
                    if (selected_candidate != nullptr) {
                        ui.text(std::string("Selected: ") + selected_candidate->Manifest.DisplayName);
                    } else {
                        ui.text("Selected: <none>");
                    }

                    const std::string& status = script_plugins.getScriptPluginStatus();
                    if (!status.empty()) {
                        ui.spacing();
                        ui.textWrapped(status);
                    }

                    const auto& candidates = script_plugins.getDiscoveredScriptPlugins();
                    ui.separator();
                    if (candidates.empty()) {
                        ui.text("No script plugin candidates were discovered.");
                        return;
                    }

                    const TableFlags table_flags = TableFlag::RowBg | TableFlag::BordersInnerH |
                                                   TableFlag::SizingStretchProp | TableFlag::ScrollY;
                    if (!ui.beginTable("##ScriptPluginsTable", 2, table_flags)) {
                        return;
                    }

                    ui.tableSetupColumn("Plugin",
                                        static_cast<TableColumnFlags>(TableColumnFlag::WidthFixed),
                                        220.0f);
                    ui.tableSetupColumn("Details",
                                        static_cast<TableColumnFlags>(TableColumnFlag::WidthStretch),
                                        1.0f);
                    ui.tableHeadersRow();

                    for (const auto& candidate : candidates) {
                        const bool is_selected = selected_candidate != nullptr &&
                                                 selected_candidate->Manifest.PluginId == candidate.Manifest.PluginId;

                        ui.tableNextRow();

                        ui.tableNextColumn();
                        if (ui.selectable(std::string(candidate.Manifest.DisplayName) + "##" +
                                              candidate.Manifest.PluginId,
                                          is_selected)) {
                            (void) script_plugins.selectScriptPlugin(&candidate);
                        }
                        if (is_selected) {
                            ui.setItemDefaultFocus();
                        }

                        ui.tableNextColumn();
                        ui.textDisabled(std::string(candidate.Manifest.Language) + " | " +
                                        candidate.Manifest.BackendName + " | " +
                                        scriptPluginScopeToString(candidate.Scope));
                        if (!candidate.Manifest.SupportedExtensions.empty()) {
                            ui.textDisabled(std::string("Extensions: ") +
                                           joinScriptExtensions(candidate.Manifest.SupportedExtensions));
                        }
                        ui.textWrapped(std::string("Id: ") + candidate.Manifest.PluginId);
                        if (!candidate.Manifest.Version.empty()) {
                            ui.textWrapped(std::string("Version: ") + candidate.Manifest.Version);
                        }
                        if (!candidate.ResolvedEntryPath.empty()) {
                            ui.textWrapped(std::string("Entry: ") + candidate.ResolvedEntryPath.string());
                            if (!candidate.EntryExists) {
                                ui.textDisabled("Entry file not found yet.");
                            }
                        }
                    }

                    ui.endTable();
                },
        });
    }

    void onUnload(Host& host) override
    {
        host.windows().unregisterWindow(kWindowId);
    }
};

std::unique_ptr<Plugin> createScriptPluginsPlugin()
{
    return std::make_unique<ScriptPluginsPlugin>();
}

} // namespace luna::editor
