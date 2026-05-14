#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace luna::editor {

class Host;

using CommandValue = std::variant<bool, int64_t, uint64_t, double, std::string>;
using CommandSubject = std::optional<CommandValue>;

struct CommandDescriptor {
    std::string id;
    std::string label;
    std::string description;
    std::string shortcut;
    std::string owner_id;
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
    virtual bool execute(std::string_view id, CommandSubject subject) = 0;
    virtual CommandSubject takeSubject(std::string_view id) = 0;
    virtual bool canExecute(std::string_view id) const = 0;
    virtual bool isChecked(std::string_view id) const = 0;
};

} // namespace luna::editor
