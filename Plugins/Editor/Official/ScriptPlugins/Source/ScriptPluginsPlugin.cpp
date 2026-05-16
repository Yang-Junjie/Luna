#include "EditorApi/EditorApi.h"
#include "Luna/Editor/EditorBuiltinPluginRegistration.h"
#include "ScriptPluginsPlugin.h"

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

std::string describeAssetRefresh(const luna::editor::AssetRefreshResult& result)
{
    std::string message = result.message.empty() ? (result.project_loaded ? std::string("Asset sync completed.")
                                                                          : std::string("Asset sync did not run."))
                                                 : result.message;
    if (!result.project_loaded) {
        return message;
    }

    message += " Discovered " + std::to_string(result.discovered_assets) + " asset(s), imported " +
               std::to_string(result.imported_missing_assets) + ".";
    if (result.script_files_skipped_no_plugin > 0u) {
        message += " Skipped " + std::to_string(result.script_files_skipped_no_plugin) +
                   " script file(s) because no usable script plugin is loaded.";
    }
    if (result.script_files_skipped_unsupported_language > 0u) {
        message += " Skipped " + std::to_string(result.script_files_skipped_unsupported_language) +
                   " script file(s) because they are not supported by the selected script plugin.";
    }
    if (result.failed_assets > 0u || result.missing_metadata_after_sync > 0u) {
        message += " Failed " + std::to_string(result.failed_assets) + " asset(s), missing metadata for " +
                   std::to_string(result.missing_metadata_after_sync) + ".";
    }

    return message;
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
                [this](WindowDrawContext& context) {
                    Host& host = context.host();
                    Ui& ui = context.ui();
                    ScriptPluginService& script_plugins = host.scriptPlugins();

                    if (!host.project().hasProjectLoaded()) {
                        ui.emptyState("No project loaded", "Open a project to configure script plugins.");
                        return;
                    }

                    ui.heading("Script Plugins");
                    ui.beginPanel("##ScriptPluginsToolbar");
                    if (ui.button("Refresh", luna::editor::ButtonVariant::Subtle)) {
                        script_plugins.refreshProjectScriptPlugins();
                        m_asset_refresh_status = describeAssetRefresh(host.assets().refreshAssets());
                    }

                    const ScriptPluginCandidate* selected_candidate = script_plugins.getSelectedScriptPluginCandidate();
                    ui.sameLine();
                    if (selected_candidate != nullptr) {
                        ui.badge(selected_candidate->Manifest.DisplayName, luna::editor::StatusVariant::Success);
                    } else {
                        ui.badge("None selected", luna::editor::StatusVariant::Warning);
                    }
                    ui.endPanel();

                    const std::string& status = script_plugins.getScriptPluginStatus();
                    if (!status.empty()) {
                        ui.spacing();
                        ui.textWrapped(status);
                    }
                    if (!m_asset_refresh_status.empty()) {
                        ui.spacing();
                        ui.textWrapped(m_asset_refresh_status);
                    }

                    const auto& candidates = script_plugins.getDiscoveredScriptPlugins();
                    if (candidates.empty()) {
                        ui.emptyState("No script plugins found", "Install or enable a script plugin for this project.");
                        return;
                    }

                    const TableFlags table_flags =
                        TableFlag::RowBg | TableFlag::BordersInnerH | TableFlag::SizingStretchProp | TableFlag::ScrollY;
                    if (!ui.beginTable("##ScriptPluginsTable", 2, table_flags)) {
                        return;
                    }

                    ui.tableSetupColumn("Plugin", static_cast<TableColumnFlags>(TableColumnFlag::WidthFixed), 220.0f);
                    ui.tableSetupColumn("Details", static_cast<TableColumnFlags>(TableColumnFlag::WidthStretch), 1.0f);
                    ui.tableHeadersRow();

                    for (const auto& candidate : candidates) {
                        const bool is_selected = selected_candidate != nullptr &&
                                                 selected_candidate->Manifest.PluginId == candidate.Manifest.PluginId;

                        ui.tableNextRow();

                        ui.tableNextColumn();
                        if (ui.selectable(std::string(candidate.Manifest.DisplayName) + "##" +
                                              candidate.Manifest.PluginId,
                                          is_selected)) {
                            if (script_plugins.selectScriptPlugin(&candidate)) {
                                m_asset_refresh_status = describeAssetRefresh(host.assets().refreshAssets());
                            } else {
                                m_asset_refresh_status = "Failed to select script plugin.";
                            }
                        }
                        if (is_selected) {
                            ui.setItemDefaultFocus();
                        }

                        ui.tableNextColumn();
                        ui.badge(candidate.Manifest.Language, luna::editor::StatusVariant::Info);
                        ui.sameLine();
                        ui.badge(candidate.Manifest.BackendName, luna::editor::StatusVariant::Neutral);
                        ui.sameLine();
                        ui.badge(scriptPluginScopeToString(candidate.Scope),
                                 candidate.Scope == luna::ScriptPluginScope::Project
                                     ? luna::editor::StatusVariant::Success
                                     : luna::editor::StatusVariant::Neutral);
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

private:
    std::string m_asset_refresh_status;
};

std::unique_ptr<Plugin> createScriptPluginsPlugin()
{
    return std::make_unique<ScriptPluginsPlugin>();
}

namespace {

const EditorBuiltinPluginFactoryRegistration kScriptPluginsPluginRegistration{
    kPluginId,
    createScriptPluginsPlugin,
};

} // namespace

} // namespace luna::editor
