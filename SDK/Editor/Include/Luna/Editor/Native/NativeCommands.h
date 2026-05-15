#pragma once

#include "Luna/Editor/Native/NativeTypes.h"

namespace luna::editor::native {

class Commands final {
public:
    constexpr Commands() noexcept = default;
    explicit constexpr Commands(const LunaEditorCommandApi* api) noexcept
        : api_(api)
    {
    }

    [[nodiscard]] bool canRegister() const noexcept
    {
        return api_ != nullptr && api_->register_command != nullptr;
    }

    [[nodiscard]] bool registerCommand(const LunaEditorCommandDescriptor& descriptor) const noexcept
    {
        return canRegister() && api_->register_command(api_->api_user_data, &descriptor) != 0;
    }

    [[nodiscard]] bool registerCommand(const CommandDescriptor& descriptor) const noexcept
    {
        const LunaEditorCommandDescriptor native_descriptor = descriptor.native();
        return registerCommand(native_descriptor);
    }

    void unregisterCommand(const char* id) const noexcept
    {
        if (api_ != nullptr && api_->unregister_command != nullptr) {
            api_->unregister_command(api_->api_user_data, id);
        }
    }

    [[nodiscard]] bool execute(const char* id) const noexcept
    {
        return api_ != nullptr && api_->execute_command != nullptr &&
               api_->execute_command(api_->api_user_data, id) != 0;
    }

    [[nodiscard]] bool canExecute(const char* id) const noexcept
    {
        return api_ != nullptr && api_->can_execute_command != nullptr &&
               api_->can_execute_command(api_->api_user_data, id) != 0;
    }

    [[nodiscard]] bool isChecked(const char* id) const noexcept
    {
        return api_ != nullptr && api_->is_command_checked != nullptr &&
               api_->is_command_checked(api_->api_user_data, id) != 0;
    }

    [[nodiscard]] const LunaEditorCommandApi* native() const noexcept
    {
        return api_;
    }

private:
    const LunaEditorCommandApi* api_{};
};

} // namespace luna::editor::native
