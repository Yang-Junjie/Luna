#pragma once

#include "Editor/EditorPanel.h"

#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace luna::editor {

class EditorRegistry {
public:
    using PanelFactory = std::function<std::unique_ptr<EditorPanel>()>;
    using CommandCallback = std::function<void()>;

    struct PanelRegistration {
        std::string m_id;
        std::string m_display_name;
        PanelFactory m_factory;
        bool m_open_by_default = true;
    };

    struct CommandRegistration {
        std::string m_id;
        std::string m_display_name;
        CommandCallback m_callback;
    };

    void addPanel(std::string id, std::string display_name, PanelFactory factory, bool open_by_default = true);

    template <typename PanelT> void addPanel(std::string id, std::string display_name, bool open_by_default = true)
    {
        static_assert(std::is_base_of_v<EditorPanel, PanelT>, "PanelT must derive from EditorPanel");
        static_assert(std::is_default_constructible_v<PanelT>, "PanelT must be default constructible");

        addPanel(
            std::move(id),
            std::move(display_name),
            [] {
                return std::make_unique<PanelT>();
            },
            open_by_default);
    }

    void addCommand(std::string id, std::string display_name, CommandCallback callback);
    bool invokeCommand(const std::string& id) const;

    const std::vector<PanelRegistration>& panels() const
    {
        return m_panels;
    }

    const std::vector<CommandRegistration>& commands() const
    {
        return m_commands;
    }

private:
    std::vector<PanelRegistration> m_panels;
    std::vector<CommandRegistration> m_commands;
};

} // namespace luna::editor
