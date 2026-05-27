#pragma once

#include "Luna/Editor/Native/NativeRegistration.h"
#include "Luna/Editor/Native/NativeTypes.h"

namespace luna::editor::native {

class Windows final {
public:
    constexpr Windows() noexcept = default;

    explicit constexpr Windows(const LunaEditorWindowApi* api) noexcept
        : api_(api)
    {}

    [[nodiscard]] bool canRegister() const noexcept
    {
        return api_ != nullptr && api_->register_window != nullptr;
    }

    [[nodiscard]] bool canSetOpen() const noexcept
    {
        return api_ != nullptr && api_->set_window_open != nullptr;
    }

    [[nodiscard]] bool registerWindow(const LunaEditorWindowDescriptor& descriptor) const noexcept
    {
        return canRegister() && api_->register_window(api_->api_user_data, &descriptor) != 0;
    }

    [[nodiscard]] bool registerWindow(const WindowDescriptor& descriptor) const noexcept
    {
        const LunaEditorWindowDescriptor native_descriptor = descriptor.native();
        return registerWindow(native_descriptor);
    }

    [[nodiscard]] RegisteredWindow registerScoped(const WindowDescriptor& descriptor) const
    {
        return registerWindow(descriptor) ? RegisteredWindow(api_, descriptor.id) : RegisteredWindow{};
    }

    [[nodiscard]] RegisteredWindow registerScoped(const LunaEditorWindowDescriptor& descriptor) const
    {
        return registerWindow(descriptor) ? RegisteredWindow(api_, descriptor.id) : RegisteredWindow{};
    }

    void unregisterWindow(const char* id) const noexcept
    {
        if (api_ != nullptr && api_->unregister_window != nullptr) {
            api_->unregister_window(api_->api_user_data, id);
        }
    }

    [[nodiscard]] bool isOpen(const char* id) const noexcept
    {
        return api_ != nullptr && api_->is_window_open != nullptr && api_->is_window_open(api_->api_user_data, id) != 0;
    }

    void setOpen(const char* id, bool open) const noexcept
    {
        if (api_ != nullptr && api_->set_window_open != nullptr) {
            api_->set_window_open(api_->api_user_data, id, open ? 1 : 0);
        }
    }

    [[nodiscard]] const LunaEditorWindowApi* native() const noexcept
    {
        return api_;
    }

private:
    const LunaEditorWindowApi* api_{};
};

} // namespace luna::editor::native
