#pragma once

#include <string>
#include <string_view>

namespace luna::editor {

struct MenuItemDescriptor {
    std::string menu_path;
    std::string command_id;
    std::string label;
    std::string shortcut;
    std::string owner_id;
};

class MenuService {
public:
    virtual ~MenuService() = default;

    virtual bool addMenuItem(MenuItemDescriptor descriptor) = 0;
    virtual void removeMenuItem(std::string_view menu_path, std::string_view command_id) = 0;
    virtual void removeMenuItemsForCommand(std::string_view command_id) = 0;
};

} // namespace luna::editor
