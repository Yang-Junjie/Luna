#pragma once

#include "Luna/Editor/Native/NativeTypes.h"

namespace luna::editor::native {

class Menus final {
public:
    constexpr Menus() noexcept = default;
    explicit constexpr Menus(const LunaEditorMenuApi* api) noexcept
        : api_(api)
    {
    }

    [[nodiscard]] bool canAdd() const noexcept
    {
        return api_ != nullptr && api_->add_menu_item != nullptr;
    }

    [[nodiscard]] bool addItem(const LunaEditorMenuItemDescriptor& descriptor) const noexcept
    {
        return canAdd() && api_->add_menu_item(api_->api_user_data, &descriptor) != 0;
    }

    [[nodiscard]] bool addItem(const MenuItemDescriptor& descriptor) const noexcept
    {
        const LunaEditorMenuItemDescriptor native_descriptor = descriptor.native();
        return addItem(native_descriptor);
    }

    void removeItem(const char* menu_path, const char* command_id) const noexcept
    {
        if (api_ != nullptr && api_->remove_menu_item != nullptr) {
            api_->remove_menu_item(api_->api_user_data, menu_path, command_id);
        }
    }

    void removeItemsForCommand(const char* command_id) const noexcept
    {
        if (api_ != nullptr && api_->remove_menu_items_for_command != nullptr) {
            api_->remove_menu_items_for_command(api_->api_user_data, command_id);
        }
    }

    [[nodiscard]] const LunaEditorMenuApi* native() const noexcept
    {
        return api_;
    }

private:
    const LunaEditorMenuApi* api_{};
};

} // namespace luna::editor::native
