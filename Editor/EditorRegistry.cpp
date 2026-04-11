#include "Editor/EditorRegistry.h"

#include "Core/Log.h"

#include <algorithm>
#include <utility>

namespace luna::editor {

void EditorRegistry::addPanel(std::string id, std::string display_name, PanelFactory factory, bool open_by_default)
{
    if (!factory) {
        LUNA_EDITOR_WARN("Ignoring editor panel '{}' because it has no factory", id);
        return;
    }

    const auto duplicate = std::find_if(m_panels.begin(), m_panels.end(), [&id](const PanelRegistration& panel) {
        return panel.m_id == id;
    });

    if (duplicate != m_panels.end()) {
        LUNA_EDITOR_WARN("Ignoring duplicate editor panel '{}'", id);
        return;
    }

    m_panels.push_back(PanelRegistration{.m_id = std::move(id),
                                         .m_display_name = std::move(display_name),
                                         .m_factory = std::move(factory),
                                         .m_open_by_default = open_by_default});
}

void EditorRegistry::addCommand(std::string id, std::string display_name, CommandCallback callback)
{
    if (!callback) {
        LUNA_EDITOR_WARN("Ignoring editor command '{}' because it has no callback", id);
        return;
    }

    const auto duplicate = std::find_if(m_commands.begin(), m_commands.end(), [&id](const CommandRegistration& cmd) {
        return cmd.m_id == id;
    });

    if (duplicate != m_commands.end()) {
        LUNA_EDITOR_WARN("Ignoring duplicate editor command '{}'", id);
        return;
    }

    m_commands.push_back(CommandRegistration{
        .m_id = std::move(id),
        .m_display_name = std::move(display_name),
        .m_callback = std::move(callback),
    });
}

bool EditorRegistry::invokeCommand(const std::string& id) const
{
    const auto command = std::find_if(m_commands.begin(), m_commands.end(), [&id](const CommandRegistration& entry) {
        return entry.m_id == id;
    });

    if (command == m_commands.end() || !command->m_callback) {
        return false;
    }

    command->m_callback();
    return true;
}

} // namespace luna::editor
