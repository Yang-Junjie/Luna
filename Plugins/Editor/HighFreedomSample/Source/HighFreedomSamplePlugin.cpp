#include "HighFreedomSamplePlugin.h"

#include "EditorApi/EditorApi.h"
#include "Luna/Editor/EditorBuiltinPluginRegistration.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {

constexpr const char* kPluginId = "luna.source.high-freedom-sample";
constexpr const char* kWindowId = "luna.source.high-freedom-sample.window";
constexpr const char* kOpenCommandId = "luna.source.high-freedom-sample.open";
constexpr const char* kCreateEntityCommandId = "luna.source.high-freedom-sample.create-entity";

std::string entityLabel(luna::editor::EntityId entity_id)
{
    return entity_id.isValid() ? entity_id.toString() : std::string("None");
}

std::string assetTypeLabel(luna::AssetType type)
{
    switch (type) {
        case luna::AssetType::Texture:
            return "Texture";
        case luna::AssetType::Mesh:
            return "Mesh";
        case luna::AssetType::Material:
            return "Material";
        case luna::AssetType::Model:
            return "Model";
        case luna::AssetType::Scene:
            return "Scene";
        case luna::AssetType::Script:
            return "Script";
        case luna::AssetType::None:
        default:
            return "None";
    }
}

constexpr luna::editor::TableColumnFlags tableColumnFlag(luna::editor::TableColumnFlag flag) noexcept
{
    return static_cast<luna::editor::TableColumnFlags>(flag);
}

class HighFreedomSamplePlugin final : public luna::editor::Plugin {
public:
    [[nodiscard]] luna::editor::PluginDescriptor descriptor() const override
    {
        return luna::editor::PluginDescriptor{
            .id = kPluginId,
            .display_name = "High Freedom Sample",
            .version = "0.1.0",
        };
    }

    bool onLoad(luna::editor::Host& host) override
    {
        const bool window_registered = host.windows().registerWindow(luna::editor::WindowDescriptor{
            .id = kWindowId,
            .title = "High Freedom Sample",
            .default_open = true,
            .default_size = luna::editor::Vec2{.x = 520.0f, .y = 640.0f},
            .draw =
                [this](luna::editor::WindowDrawContext& context) {
                    drawWindow(context);
                },
        });

        const bool open_command_registered = host.commands().registerCommand(luna::editor::CommandDescriptor{
            .id = kOpenCommandId,
            .label = "Open High Freedom Sample",
            .description = "Opens the high freedom editor plugin sample window.",
            .is_checked = [](luna::editor::Host& host) {
                return host.windows().isWindowOpen(kWindowId);
            },
            .execute = [](luna::editor::Host& host) {
                host.windows().setWindowOpen(kWindowId, true);
            },
        });

        const bool create_command_registered = host.commands().registerCommand(luna::editor::CommandDescriptor{
            .id = kCreateEntityCommandId,
            .label = "Create High Freedom Entity",
            .description = "Creates and selects an entity through public editor APIs.",
            .can_execute = [](luna::editor::Host& host) {
                return host.scene().canEditScene();
            },
            .execute =
                [this](luna::editor::Host& host) {
                    createEntityFromCommand(host);
                },
        });

        const bool open_menu_registered = host.menus().addMenuItem(luna::editor::MenuItemDescriptor{
            .menu_path = "Tools/High Freedom Sample",
            .command_id = kOpenCommandId,
        });
        const bool create_menu_registered = host.menus().addMenuItem(luna::editor::MenuItemDescriptor{
            .menu_path = "Tools/High Freedom Sample",
            .command_id = kCreateEntityCommandId,
        });

        if (!window_registered || !open_command_registered || !create_command_registered || !open_menu_registered ||
            !create_menu_registered) {
            onUnload(host);
            return false;
        }

        return true;
    }

    void onUnload(luna::editor::Host& host) override
    {
        if (m_preview_viewport_id != luna::editor::kInvalidViewportId) {
            host.viewport().destroySceneViewport(m_preview_viewport_id);
            m_preview_viewport_id = luna::editor::kInvalidViewportId;
        }
        host.menus().removeMenuItemsForCommand(kCreateEntityCommandId);
        host.menus().removeMenuItemsForCommand(kOpenCommandId);
        host.commands().unregisterCommand(kCreateEntityCommandId);
        host.commands().unregisterCommand(kOpenCommandId);
        host.windows().unregisterWindow(kWindowId);
    }

private:
    void drawWindow(luna::editor::WindowDrawContext& context)
    {
        luna::editor::Host& host = context.host();
        luna::editor::Ui& ui = context.ui();

        ui.textWrapped("A normal editor plugin can create tools, menus, commands, assets, scene edits, and viewport content using only Luna editor APIs.");
        ui.separatorText("Plugin Asset");
        drawPluginAsset(host, ui);

        ui.separatorText("Scene Workflow");
        drawSceneWorkflow(host, ui);

        ui.separatorText("Selected Entity");
        drawSelectedEntity(host, ui);

        ui.separatorText("Project Assets");
        drawAssetSummary(host, ui);

        ui.separatorText("Plugin Viewport");
        drawPluginViewport(host, ui);
    }

    void drawPluginAsset(luna::editor::Host& host, luna::editor::Ui& ui)
    {
        if (!m_asset_loaded) {
            m_asset_text = host.pluginAssets().readText(kPluginId, "welcome.txt").value_or("Missing welcome.txt.");
            m_asset_loaded = true;
        }
        ui.textWrapped(m_asset_text);
    }

    void drawSceneWorkflow(luna::editor::Host& host, luna::editor::Ui& ui)
    {
        ui.text("Scene: " + host.scene().sceneLabel());
        ui.text("Entities: " + std::to_string(host.scene().entityCount()));
        ui.text("Selected: " + entityLabel(host.selection().selectedEntityId()));

        if (!host.scene().canEditScene()) {
            ui.textDisabled("Scene editing is disabled while runtime viewport owns the scene.");
            return;
        }

        ui.inputTextWithHint("Entity Name", "Plugin Entity", m_entity_name, 128);
        if (ui.button("Create Empty", luna::editor::ButtonVariant::Primary)) {
            createEntity(host, false);
        }
        ui.sameLine();
        if (ui.button("Create Camera")) {
            createEntity(host, true);
        }
        ui.sameLine();
        if (ui.button("Clear Selection", luna::editor::ButtonVariant::Subtle)) {
            host.selection().clearSelection();
        }
    }

    void drawSelectedEntity(luna::editor::Host& host, luna::editor::Ui& ui)
    {
        const luna::editor::EntityId selected_entity_id = host.selection().selectedEntityId();
        if (!selected_entity_id.isValid()) {
            ui.textDisabled("No entity selected.");
            return;
        }

        const std::optional<luna::editor::SceneEntityDetails> details = host.scene().entityDetails(selected_entity_id);
        if (!details) {
            ui.textDisabled("Selected entity no longer exists.");
            return;
        }

        if (m_tracked_entity_id != selected_entity_id) {
            m_tracked_entity_id = selected_entity_id;
            m_selected_entity_name = details->name;
        }

        ui.text("ID: " + selected_entity_id.toString());
        if (ui.inputText("Name", m_selected_entity_name, 128)) {
            (void) host.scene().setEntityName(selected_entity_id, m_selected_entity_name);
        }

        if (ui.beginTable("high-freedom-components", 2, luna::editor::TableFlag::RowBg |
                                                           luna::editor::TableFlag::SizingStretchProp)) {
            ui.tableSetupColumn("Component", tableColumnFlag(luna::editor::TableColumnFlag::WidthFixed), 120.0f);
            ui.tableSetupColumn("State", tableColumnFlag(luna::editor::TableColumnFlag::WidthStretch), 1.0f);
            ui.tableHeadersRow();
            drawComponentRow(ui, "Transform", details->components.transform);
            drawComponentRow(ui, "Camera", details->components.camera);
            drawComponentRow(ui, "Light", details->components.light);
            drawComponentRow(ui, "Mesh", details->components.mesh);
            drawComponentRow(ui, "Script", details->components.script);
            ui.endTable();
        }

        if (host.scene().canEditScene()) {
            if (!details->components.camera && ui.button("Add Camera Component")) {
                (void) host.scene().addComponent(selected_entity_id, luna::editor::SceneComponentKind::Camera);
                (void) host.scene().setCameraComponent(selected_entity_id, luna::editor::SceneCameraComponent{});
            }
            if (!details->components.light) {
                ui.sameLine();
                if (ui.button("Add Point Light")) {
                    luna::editor::SceneLightComponent light{};
                    light.type = luna::editor::SceneLightType::Point;
                    light.intensity = 8.0f;
                    light.range = 12.0f;
                    (void) host.scene().addComponent(selected_entity_id, luna::editor::SceneComponentKind::Light);
                    (void) host.scene().setLightComponent(selected_entity_id, light);
                }
            }
        }
    }

    void drawAssetSummary(luna::editor::Host& host, luna::editor::Ui& ui)
    {
        ui.text("Asset revision: " + std::to_string(host.assets().assetRevision()));
        const std::vector<luna::editor::AssetInfo> assets = host.assets().listAssets(luna::AssetType::None, true);
        if (assets.empty()) {
            ui.textDisabled("No assets discovered yet.");
            return;
        }

        const std::size_t row_count = (std::min)(assets.size(), static_cast<std::size_t>(6));
        if (ui.beginTable("high-freedom-assets", 3, luna::editor::TableFlag::RowBg |
                                                  luna::editor::TableFlag::BordersInnerH |
                                                  luna::editor::TableFlag::SizingStretchProp)) {
            ui.tableSetupColumn("Name", tableColumnFlag(luna::editor::TableColumnFlag::WidthStretch), 1.0f);
            ui.tableSetupColumn("Type", tableColumnFlag(luna::editor::TableColumnFlag::WidthFixed), 92.0f);
            ui.tableSetupColumn("State", tableColumnFlag(luna::editor::TableColumnFlag::WidthFixed), 72.0f);
            ui.tableHeadersRow();
            for (std::size_t index = 0; index < row_count; ++index) {
                const luna::editor::AssetInfo& asset = assets[index];
                ui.tableNextRow();
                ui.tableNextColumn();
                ui.text(asset.label.empty() ? asset.project_path.generic_string() : asset.label);
                ui.tableNextColumn();
                ui.text(assetTypeLabel(asset.type));
                ui.tableNextColumn();
                ui.text(asset.loading ? "Loading" : "Ready");
            }
            ui.endTable();
        }
    }

    void drawPluginViewport(luna::editor::Host& host, luna::editor::Ui& ui)
    {
        if (ui.checkbox("Show independent scene viewport", m_show_preview_viewport) && !m_show_preview_viewport) {
            if (m_preview_viewport_id != luna::editor::kInvalidViewportId) {
                host.viewport().destroySceneViewport(m_preview_viewport_id);
                m_preview_viewport_id = luna::editor::kInvalidViewportId;
            }
        }

        if (!m_show_preview_viewport) {
            ui.textDisabled("Enable this to let the plugin own a separate viewport instance.");
            return;
        }

        if (m_preview_viewport_id == luna::editor::kInvalidViewportId ||
            !host.viewport().isSceneViewportValid(m_preview_viewport_id)) {
            m_preview_viewport_id = host.viewport().createSceneViewport("HighFreedomSampleViewport");
        }

        const luna::editor::Vec2 available = ui.contentRegionAvail();
        const uint32_t width = static_cast<uint32_t>((std::max)(available.x, 256.0f));
        const uint32_t height = static_cast<uint32_t>((std::max)((std::min)(available.y, 260.0f), 180.0f));
        const luna::editor::SceneViewportDrawResult result =
            host.viewport().drawSceneViewport(ui,
                                              m_preview_viewport_id,
                                              luna::editor::SceneViewportDrawOptions{
                                                  .requested_size = luna::editor::Vec2{
                                                      .x = static_cast<float>(width),
                                                      .y = static_cast<float>(height),
                                                  },
                                              });

        if (!result.drawn) {
            ui.textDisabled("Viewport texture is not ready.");
            return;
        }

        if (result.hovered) {
            ui.setTooltip("This image is drawn from a plugin-owned viewport instance.");
        }
    }

    void drawComponentRow(luna::editor::Ui& ui, std::string_view name, bool enabled)
    {
        ui.tableNextRow();
        ui.tableNextColumn();
        ui.text(name);
        ui.tableNextColumn();
        ui.text(enabled ? "Present" : "Missing");
    }

    void createEntityFromCommand(luna::editor::Host& host)
    {
        if (!host.scene().canEditScene()) {
            return;
        }
        createEntity(host, false);
        host.windows().setWindowOpen(kWindowId, true);
    }

    void createEntity(luna::editor::Host& host, bool camera)
    {
        const std::string name = m_entity_name.empty() ? std::string("High Freedom Entity") : m_entity_name;
        luna::editor::EntityId entity_id{};
        if (camera) {
            entity_id = host.scene().createEntity(luna::editor::SceneEntityCreateRequest{
                .kind = luna::editor::SceneEntityCreateKind::Camera,
                .name = name,
            });
        } else {
            entity_id = host.scene().createEntity(name);
        }

        if (entity_id.isValid()) {
            host.selection().selectEntity(entity_id);
            m_tracked_entity_id = entity_id;
            m_selected_entity_name = name;
        }
    }

    std::string m_asset_text;
    std::string m_entity_name{"High Freedom Entity"};
    std::string m_selected_entity_name;
    luna::editor::EntityId m_tracked_entity_id{};
    luna::editor::ViewportId m_preview_viewport_id{luna::editor::kInvalidViewportId};
    bool m_asset_loaded{false};
    bool m_show_preview_viewport{false};
};

} // namespace

namespace luna::editor {

std::unique_ptr<Plugin> createHighFreedomSamplePlugin()
{
    return std::make_unique<HighFreedomSamplePlugin>();
}

namespace {

const EditorBuiltinPluginFactoryRegistration kHighFreedomSamplePluginRegistration{
    kPluginId,
    createHighFreedomSamplePlugin,
};

} // namespace

} // namespace luna::editor
