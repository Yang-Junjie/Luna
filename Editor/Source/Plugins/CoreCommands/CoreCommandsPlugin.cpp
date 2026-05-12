#include "Plugins/CoreCommands/CoreCommandsPlugin.h"

#include "EditorApi/EditorApi.h"

namespace {

constexpr const char* kPluginId = "luna.editor.core-commands";

class CoreCommandsPlugin final : public luna::editor::Plugin {
public:
    [[nodiscard]] luna::editor::PluginDescriptor descriptor() const override
    {
        return luna::editor::PluginDescriptor{
            .id = kPluginId,
            .display_name = "Core Editor Commands",
            .version = "0.1.0",
        };
    }

    bool onLoad(luna::editor::Host& host) override
    {
        const bool undo_registered = host.commands().registerCommand(luna::editor::CommandDescriptor{
            .id = luna::editor::commands::kUndo,
            .label = "Undo",
            .description = "Undo the last scene authoring operation.",
            .shortcut = "Ctrl+Z",
            .can_execute = [](luna::editor::Host& host) {
                return host.history().canUndo();
            },
            .execute =
                [](luna::editor::Host& host) {
                    (void) host.history().undo();
                },
        });

        const bool redo_registered = host.commands().registerCommand(luna::editor::CommandDescriptor{
            .id = luna::editor::commands::kRedo,
            .label = "Redo",
            .description = "Redo the next scene authoring operation.",
            .shortcut = "Ctrl+Y",
            .can_execute = [](luna::editor::Host& host) {
                return host.history().canRedo();
            },
            .execute =
                [](luna::editor::Host& host) {
                    (void) host.history().redo();
                },
        });

        const bool runtime_viewport_registered =
            host.commands().registerCommand(luna::editor::CommandDescriptor{
                .id = luna::editor::commands::kToggleRuntimeViewport,
                .label = "Runtime Viewport",
                .description = "Toggle the viewport between editor rendering and runtime scene simulation.",
                .can_execute = [](luna::editor::Host&) {
                    return true;
                },
                .is_checked = [](luna::editor::Host& host) {
                    return host.runtimeViewport().isRuntimeViewportRequested();
                },
                .execute =
                    [](luna::editor::Host& host) {
                        const bool requested = host.runtimeViewport().isRuntimeViewportRequested();
                        host.runtimeViewport().setRuntimeViewportRequested(!requested);
                    },
            });

        const bool undo_menu_registered = host.menus().addMenuItem(luna::editor::MenuItemDescriptor{
            .menu_path = "Edit",
            .command_id = luna::editor::commands::kUndo,
        });
        const bool redo_menu_registered = host.menus().addMenuItem(luna::editor::MenuItemDescriptor{
            .menu_path = "Edit",
            .command_id = luna::editor::commands::kRedo,
        });
        const bool runtime_viewport_menu_registered = host.menus().addMenuItem(luna::editor::MenuItemDescriptor{
            .menu_path = "Viewport",
            .command_id = luna::editor::commands::kToggleRuntimeViewport,
        });

        if (!undo_registered || !redo_registered || !runtime_viewport_registered || !undo_menu_registered ||
            !redo_menu_registered || !runtime_viewport_menu_registered) {
            onUnload(host);
            return false;
        }

        return true;
    }

    void onUnload(luna::editor::Host& host) override
    {
        host.menus().removeMenuItemsForCommand(luna::editor::commands::kToggleRuntimeViewport);
        host.menus().removeMenuItemsForCommand(luna::editor::commands::kRedo);
        host.menus().removeMenuItemsForCommand(luna::editor::commands::kUndo);
        host.commands().unregisterCommand(luna::editor::commands::kToggleRuntimeViewport);
        host.commands().unregisterCommand(luna::editor::commands::kRedo);
        host.commands().unregisterCommand(luna::editor::commands::kUndo);
    }
};

} // namespace

namespace luna::editor {

std::unique_ptr<Plugin> createCoreCommandsPlugin()
{
    return std::make_unique<CoreCommandsPlugin>();
}

} // namespace luna::editor
