#include "Asset/AssetManager.h"
#include "Asset/AssetTypes.h"
#include "AssetLoadingPlugin.h"
#include "EditorApi/EditorApi.h"
#include "Luna/Editor/EditorBuiltinPluginRegistration.h"

#include <string>
#include <vector>

namespace {

constexpr const char* kPluginId = "luna.editor.asset-loading";
constexpr const char* kWindowId = "luna.editor.asset-loading.window";

class AssetLoadingPlugin final : public luna::editor::Plugin {
public:
    [[nodiscard]] luna::editor::PluginDescriptor descriptor() const override
    {
        return luna::editor::PluginDescriptor{
            .id = kPluginId,
            .display_name = "Asset Loading",
            .version = "0.1.0",
        };
    }

    bool onLoad(luna::editor::Host& host) override
    {
        return host.windows().registerWindow(luna::editor::WindowDescriptor{
            .id = kWindowId,
            .title = "Asset Loading",
            .default_open = true,
            .default_size = luna::editor::Vec2{.x = 420.0f, .y = 220.0f},
            .draw =
                [](luna::editor::WindowDrawContext& context) {
                    luna::editor::Ui& ui = context.ui();
                    const std::vector<luna::AssetManager::LoadingAssetInfo> loading_assets =
                        luna::AssetManager::get().getLoadingAssetsSnapshot();

                    ui.heading("Asset Loading");
                    ui.metric("Loading Assets",
                              std::to_string(loading_assets.size()),
                              loading_assets.empty() ? "Queue is idle" : "Imports or loads in flight",
                              loading_assets.empty() ? luna::editor::StatusVariant::Success
                                                     : luna::editor::StatusVariant::Info);
                    ui.spacing();

                    if (loading_assets.empty()) {
                        ui.emptyState("No assets are loading", "The asset queue is currently idle.");
                        return;
                    }

                    const luna::editor::TableFlags table_flags =
                        luna::editor::TableFlag::RowBg | luna::editor::TableFlag::BordersInnerH |
                        luna::editor::TableFlag::SizingStretchProp | luna::editor::TableFlag::ScrollY;

                    if (!ui.beginTable("##AssetLoadingTable", 4, table_flags)) {
                        return;
                    }

                    ui.tableSetupColumn(
                        "Type",
                        static_cast<luna::editor::TableColumnFlags>(luna::editor::TableColumnFlag::WidthFixed),
                        90.0f);
                    ui.tableSetupColumn(
                        "Name",
                        static_cast<luna::editor::TableColumnFlags>(luna::editor::TableColumnFlag::WidthStretch),
                        0.25f);
                    ui.tableSetupColumn(
                        "Path",
                        static_cast<luna::editor::TableColumnFlags>(luna::editor::TableColumnFlag::WidthStretch),
                        0.55f);
                    ui.tableSetupColumn(
                        "Handle",
                        static_cast<luna::editor::TableColumnFlags>(luna::editor::TableColumnFlag::WidthStretch),
                        0.20f);
                    ui.tableHeadersRow();

                    for (const luna::AssetManager::LoadingAssetInfo& info : loading_assets) {
                        ui.tableNextRow();

                        ui.tableNextColumn();
                        ui.badge(luna::AssetUtils::AssetTypeToString(info.Type), luna::editor::StatusVariant::Info);

                        ui.tableNextColumn();
                        ui.text(info.Name.empty() ? "Unnamed Asset" : info.Name);

                        ui.tableNextColumn();
                        ui.text(info.FilePath.empty() ? std::string("Unknown Path") : info.FilePath.generic_string());

                        ui.tableNextColumn();
                        ui.textDisabled(info.Handle.toString());
                    }

                    ui.endTable();
                },
        });
    }

    void onUnload(luna::editor::Host& host) override
    {
        host.windows().unregisterWindow(kWindowId);
    }
};

} // namespace

namespace luna::editor {

std::unique_ptr<Plugin> createAssetLoadingPlugin()
{
    return std::make_unique<AssetLoadingPlugin>();
}

namespace {

const EditorBuiltinPluginFactoryRegistration kAssetLoadingPluginRegistration{
    kPluginId,
    createAssetLoadingPlugin,
};

} // namespace

} // namespace luna::editor
