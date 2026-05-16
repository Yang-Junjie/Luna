#include "PluginManagerPlugin.h"

#include "EditorApi/EditorApi.h"
#include "Luna/Editor/EditorBuiltinPluginRegistration.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr const char* kPluginId = "luna.editor.plugin-manager";
constexpr const char* kWindowId = "luna.editor.plugin-manager.window";

std::string runtimeLabel(luna::editor::PluginRuntimeKind runtime)
{
    switch (runtime) {
        case luna::editor::PluginRuntimeKind::BuiltinNative:
            return "BuiltinNative";
        case luna::editor::PluginRuntimeKind::Native:
            return "Native";
        case luna::editor::PluginRuntimeKind::Lua:
            return "Lua";
    }
    return "Unknown";
}

std::string sourceLabel(luna::editor::PluginSourceKind source)
{
    switch (source) {
        case luna::editor::PluginSourceKind::Official:
            return "Official";
        case luna::editor::PluginSourceKind::Installed:
            return "Installed";
        case luna::editor::PluginSourceKind::Development:
            return "Development";
        case luna::editor::PluginSourceKind::Unknown:
            break;
    }
    return "Unknown";
}

std::string categoryLabel(luna::editor::PluginCategoryKind category)
{
    switch (category) {
        case luna::editor::PluginCategoryKind::Core:
            return "Core";
        case luna::editor::PluginCategoryKind::Tool:
            return "Tool";
        case luna::editor::PluginCategoryKind::Diagnostics:
            return "Diagnostics";
    }
    return "Tool";
}

std::string stateLabel(luna::editor::PluginLoadState state)
{
    switch (state) {
        case luna::editor::PluginLoadState::Registered:
            return "Registered";
        case luna::editor::PluginLoadState::Loaded:
            return "Loaded";
        case luna::editor::PluginLoadState::Disabled:
            return "Disabled";
        case luna::editor::PluginLoadState::Failed:
            return "Failed";
    }
    return "Unknown";
}

std::string pathText(const std::filesystem::path& path)
{
    return path.empty() ? std::string("-") : path.generic_string();
}

std::string dependenciesText(const std::vector<std::string>& dependencies)
{
    if (dependencies.empty()) {
        return "-";
    }

    std::string result;
    for (const std::string& dependency : dependencies) {
        if (!result.empty()) {
            result += ", ";
        }
        result += dependency;
    }
    return result;
}

bool matchesFilter(const luna::editor::PluginInfo& plugin, bool show_loaded, bool show_failed, bool show_disabled)
{
    switch (plugin.state) {
        case luna::editor::PluginLoadState::Loaded:
            return show_loaded;
        case luna::editor::PluginLoadState::Failed:
            return show_failed;
        case luna::editor::PluginLoadState::Disabled:
            return show_disabled;
        case luna::editor::PluginLoadState::Registered:
            return true;
    }
    return true;
}

class PluginManagerPlugin final : public luna::editor::Plugin {
public:
    [[nodiscard]] luna::editor::PluginDescriptor descriptor() const override
    {
        return luna::editor::PluginDescriptor{
            .id = kPluginId,
            .display_name = "Plugin Manager",
            .version = "0.1.0",
        };
    }

    bool onLoad(luna::editor::Host& host) override
    {
        return host.windows().registerWindow(luna::editor::WindowDescriptor{
            .id = kWindowId,
            .title = "Plugin Manager",
            .default_open = false,
            .default_size = luna::editor::Vec2{.x = 1040.0f, .y = 560.0f},
            .draw =
                [this](luna::editor::WindowDrawContext& context) {
                    drawWindow(context);
                },
        });
    }

    void onUnload(luna::editor::Host& host) override
    {
        host.windows().unregisterWindow(kWindowId);
    }

private:
    void drawWindow(luna::editor::WindowDrawContext& context)
    {
        luna::editor::Ui& ui = context.ui();
        std::vector<luna::editor::PluginInfo> plugins = context.host().plugins().plugins();
        std::sort(plugins.begin(), plugins.end(), [](const auto& lhs, const auto& rhs) {
            if (lhs.state != rhs.state) {
                return static_cast<int>(lhs.state) < static_cast<int>(rhs.state);
            }
            return lhs.id < rhs.id;
        });

        ui.checkbox("Loaded", m_show_loaded);
        ui.sameLine();
        ui.checkbox("Failed", m_show_failed);
        ui.sameLine();
        ui.checkbox("Disabled", m_show_disabled);

        size_t loaded_count = 0;
        size_t failed_count = 0;
        size_t disabled_count = 0;
        for (const auto& plugin : plugins) {
            loaded_count += plugin.state == luna::editor::PluginLoadState::Loaded ? 1u : 0u;
            failed_count += plugin.state == luna::editor::PluginLoadState::Failed ? 1u : 0u;
            disabled_count += plugin.state == luna::editor::PluginLoadState::Disabled ? 1u : 0u;
        }

        ui.text("Discovered: " + std::to_string(plugins.size()) + "  Loaded: " + std::to_string(loaded_count) +
                "  Failed: " + std::to_string(failed_count) + "  Disabled: " + std::to_string(disabled_count));
        ui.separator();

        const luna::editor::TableFlags table_flags =
            luna::editor::TableFlag::RowBg | luna::editor::TableFlag::BordersInnerH |
            luna::editor::TableFlag::SizingStretchProp;
        if (!ui.beginTable("##EditorPluginDiagnostics", 6, table_flags, ui.contentRegionAvail())) {
            return;
        }

        ui.tableSetupColumn("State",
                            static_cast<luna::editor::TableColumnFlags>(
                                luna::editor::TableColumnFlag::WidthFixed),
                            92.0f);
        ui.tableSetupColumn("Plugin",
                            static_cast<luna::editor::TableColumnFlags>(
                                luna::editor::TableColumnFlag::WidthStretch),
                            1.5f);
        ui.tableSetupColumn("Runtime",
                            static_cast<luna::editor::TableColumnFlags>(
                                luna::editor::TableColumnFlag::WidthFixed),
                            112.0f);
        ui.tableSetupColumn("Source",
                            static_cast<luna::editor::TableColumnFlags>(
                                luna::editor::TableColumnFlag::WidthFixed),
                            112.0f);
        ui.tableSetupColumn("Category",
                            static_cast<luna::editor::TableColumnFlags>(
                                luna::editor::TableColumnFlag::WidthFixed),
                            118.0f);
        ui.tableSetupColumn("Details",
                            static_cast<luna::editor::TableColumnFlags>(
                                luna::editor::TableColumnFlag::WidthStretch),
                            2.0f);
        ui.tableHeadersRow();

        for (const luna::editor::PluginInfo& plugin : plugins) {
            if (!matchesFilter(plugin, m_show_loaded, m_show_failed, m_show_disabled)) {
                continue;
            }

            ui.tableNextRow();
            ui.tableNextColumn();
            ui.text(stateLabel(plugin.state));

            ui.tableNextColumn();
            ui.text(plugin.display_name.empty() ? plugin.id : plugin.display_name);
            ui.textDisabled(plugin.id);

            ui.tableNextColumn();
            ui.text(runtimeLabel(plugin.runtime));

            ui.tableNextColumn();
            ui.text(sourceLabel(plugin.source));

            ui.tableNextColumn();
            ui.text(categoryLabel(plugin.category));

            ui.tableNextColumn();
            ui.textWrapped(plugin.status.empty() ? stateLabel(plugin.state) : plugin.status);
            ui.textDisabled("Version: " + (plugin.version.empty() ? std::string("-") : plugin.version));
            ui.textWrapped("Dependencies: " + dependenciesText(plugin.dependencies));
            ui.textWrapped("Root: " + pathText(plugin.root_path));
            if (!plugin.entry_path.empty() || !plugin.resolved_entry_path.empty()) {
                ui.textWrapped("Entry: " + pathText(plugin.entry_path));
                ui.textWrapped("Resolved: " + pathText(plugin.resolved_entry_path));
            }
        }

        ui.endTable();
    }

    bool m_show_loaded{true};
    bool m_show_failed{true};
    bool m_show_disabled{true};
};

} // namespace

namespace luna::editor {

std::unique_ptr<Plugin> createPluginManagerPlugin()
{
    return std::make_unique<PluginManagerPlugin>();
}

namespace {

const EditorBuiltinPluginFactoryRegistration kPluginManagerPluginRegistration{
    kPluginId,
    createPluginManagerPlugin,
};

} // namespace

} // namespace luna::editor
