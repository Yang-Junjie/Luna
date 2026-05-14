#pragma once

#include "Core/KeyCodes.h"

#include <string>
#include <string_view>

namespace luna::editor {

struct ShortcutChord {
    KeyCode key{KeyCode::None};
    bool primary{false};
    bool ctrl{false};
    bool shift{false};
    bool alt{false};
    bool super{false};
    bool allow_repeat{false};
};

struct ShortcutDescriptor {
    std::string id;
    std::string command_id;
    std::string owner_id;
    ShortcutChord chord;
    std::string display_text;
};

class ShortcutService {
public:
    virtual ~ShortcutService() = default;

    virtual bool registerShortcut(ShortcutDescriptor descriptor) = 0;
    virtual void unregisterShortcut(std::string_view id) = 0;
    virtual std::string shortcutText(std::string_view id) const = 0;
    virtual std::string commandShortcutText(std::string_view command_id) const = 0;
};

} // namespace luna::editor
