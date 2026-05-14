#include "EditorApiSamplePlugin.h"

#include "EditorApi/EditorApi.h"

#include <string>

namespace {

constexpr const char* kPluginId = "luna.editor.api-sample";
constexpr const char* kWindowId = "luna.editor.api-sample.window";
constexpr const char* kCreateEntityCommandId = "luna.editor.api-sample.create-entity";

class EditorApiSamplePlugin final : public luna::editor::Plugin {
public:
    [[nodiscard]] luna::editor::PluginDescriptor descriptor() const override
    {
        return luna::editor::PluginDescriptor{
            .id = kPluginId,
            .display_name = "Editor API Sample",
            .version = "0.1.0",
        };
    }

    bool onLoad(luna::editor::Host& host) override
    {
        const bool window_registered = host.windows().registerWindow(luna::editor::WindowDescriptor{
            .id = kWindowId,
            .title = "Editor API Sample",
            .default_open = false,
            .draw =
                [](luna::editor::WindowDrawContext& context) {
                    luna::editor::Ui& ui = context.ui();
                    luna::editor::Host& host = context.host();

                    ui.text("This official plugin only uses Luna/EditorApi.");
                    ui.separator();
                    ui.text("Scene: " + host.scene().sceneLabel());
                    ui.text("Entities: " + std::to_string(host.scene().entityCount()));

                    const luna::editor::EntityId selected_entity_id = host.selection().selectedEntityId();
                    ui.text("Selected: " +
                            (selected_entity_id.isValid() ? selected_entity_id.toString() : std::string("None")));

                    if (!host.scene().canEditScene()) {
                        ui.textDisabled("Runtime viewport is active; scene editing is disabled.");
                        return;
                    }

                    if (ui.button("Create Entity")) {
                        const luna::editor::EntityId entity_id = host.scene().createEntity("Editor API Entity");
                        if (entity_id.isValid()) {
                            host.selection().selectEntity(entity_id);
                        }
                    }
                    ui.sameLine();
                    if (ui.button("Clear Selection")) {
                        host.selection().clearSelection();
                    }
                },
        });

        const bool command_registered = host.commands().registerCommand(luna::editor::CommandDescriptor{
            .id = kCreateEntityCommandId,
            .label = "Create Editor API Entity",
            .description = "Creates an entity through the public editor plugin API.",
            .shortcut = "",
            .can_execute = [](luna::editor::Host& host) {
                return host.scene().canEditScene();
            },
            .execute =
                [](luna::editor::Host& host) {
                    const luna::editor::EntityId entity_id = host.scene().createEntity("Editor API Entity");
                    if (entity_id.isValid()) {
                        host.selection().selectEntity(entity_id);
                    }
                },
        });

        const bool menu_registered = host.menus().addMenuItem(luna::editor::MenuItemDescriptor{
            .menu_path = "Tools/Editor API Sample",
            .command_id = kCreateEntityCommandId,
        });

        if (!window_registered || !command_registered || !menu_registered) {
            onUnload(host);
            return false;
        }

        return true;
    }

    void onUnload(luna::editor::Host& host) override
    {
        host.menus().removeMenuItemsForCommand(kCreateEntityCommandId);
        host.commands().unregisterCommand(kCreateEntityCommandId);
        host.windows().unregisterWindow(kWindowId);
    }
};

} // namespace

namespace luna::editor {

std::unique_ptr<Plugin> createEditorApiSamplePlugin()
{
    return std::make_unique<EditorApiSamplePlugin>();
}

} // namespace luna::editor
