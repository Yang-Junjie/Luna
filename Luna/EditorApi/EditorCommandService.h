#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace luna::editor {

class Host;

struct CommandDescriptor {
    std::string id;
    std::string label;
    std::string description;
    std::string shortcut;
    std::function<bool(Host&)> can_execute;
    std::function<bool(Host&)> is_checked;
    std::function<void(Host&)> execute;
};

class CommandService {
public:
    virtual ~CommandService() = default;

    virtual bool registerCommand(CommandDescriptor descriptor) = 0;
    virtual void unregisterCommand(std::string_view id) = 0;
    virtual bool execute(std::string_view id) = 0;
    virtual bool canExecute(std::string_view id) const = 0;
    virtual bool isChecked(std::string_view id) const = 0;
};

} // namespace luna::editor
