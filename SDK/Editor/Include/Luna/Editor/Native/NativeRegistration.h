#pragma once

#include "Luna/Editor/Native/NativeTypes.h"

#include <string>
#include <utility>

namespace luna::editor::native {

class RegisteredCommand final {
public:
    RegisteredCommand() noexcept = default;

    RegisteredCommand(const LunaEditorCommandApi* api, const char* id)
        : api_(api),
          id_(id != nullptr ? id : "")
    {}

    RegisteredCommand(const RegisteredCommand&) = delete;
    RegisteredCommand& operator=(const RegisteredCommand&) = delete;

    RegisteredCommand(RegisteredCommand&& other) noexcept
    {
        moveFrom(other);
    }

    RegisteredCommand& operator=(RegisteredCommand&& other) noexcept
    {
        if (this != &other) {
            reset();
            moveFrom(other);
        }
        return *this;
    }

    ~RegisteredCommand()
    {
        reset();
    }

    [[nodiscard]] bool active() const noexcept
    {
        return api_ != nullptr && !id_.empty();
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return active();
    }

    [[nodiscard]] const std::string& id() const noexcept
    {
        return id_;
    }

    void reset() noexcept
    {
        if (api_ != nullptr && api_->unregister_command != nullptr && !id_.empty()) {
            api_->unregister_command(api_->api_user_data, id_.c_str());
        }
        release();
    }

    void release() noexcept
    {
        api_ = nullptr;
        id_.clear();
    }

private:
    void moveFrom(RegisteredCommand& other) noexcept
    {
        api_ = std::exchange(other.api_, nullptr);
        id_ = std::move(other.id_);
        other.id_.clear();
    }

    const LunaEditorCommandApi* api_{};
    std::string id_;
};

class RegisteredWindow final {
public:
    RegisteredWindow() noexcept = default;

    RegisteredWindow(const LunaEditorWindowApi* api, const char* id)
        : api_(api),
          id_(id != nullptr ? id : "")
    {}

    RegisteredWindow(const RegisteredWindow&) = delete;
    RegisteredWindow& operator=(const RegisteredWindow&) = delete;

    RegisteredWindow(RegisteredWindow&& other) noexcept
    {
        moveFrom(other);
    }

    RegisteredWindow& operator=(RegisteredWindow&& other) noexcept
    {
        if (this != &other) {
            reset();
            moveFrom(other);
        }
        return *this;
    }

    ~RegisteredWindow()
    {
        reset();
    }

    [[nodiscard]] bool active() const noexcept
    {
        return api_ != nullptr && !id_.empty();
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return active();
    }

    [[nodiscard]] const std::string& id() const noexcept
    {
        return id_;
    }

    void reset() noexcept
    {
        if (api_ != nullptr && api_->unregister_window != nullptr && !id_.empty()) {
            api_->unregister_window(api_->api_user_data, id_.c_str());
        }
        release();
    }

    void release() noexcept
    {
        api_ = nullptr;
        id_.clear();
    }

private:
    void moveFrom(RegisteredWindow& other) noexcept
    {
        api_ = std::exchange(other.api_, nullptr);
        id_ = std::move(other.id_);
        other.id_.clear();
    }

    const LunaEditorWindowApi* api_{};
    std::string id_;
};

class RegisteredMenuItem final {
public:
    RegisteredMenuItem() noexcept = default;

    RegisteredMenuItem(const LunaEditorMenuApi* api, const char* menu_path, const char* command_id)
        : api_(api),
          menu_path_(menu_path != nullptr ? menu_path : ""),
          command_id_(command_id != nullptr ? command_id : "")
    {}

    RegisteredMenuItem(const RegisteredMenuItem&) = delete;
    RegisteredMenuItem& operator=(const RegisteredMenuItem&) = delete;

    RegisteredMenuItem(RegisteredMenuItem&& other) noexcept
    {
        moveFrom(other);
    }

    RegisteredMenuItem& operator=(RegisteredMenuItem&& other) noexcept
    {
        if (this != &other) {
            reset();
            moveFrom(other);
        }
        return *this;
    }

    ~RegisteredMenuItem()
    {
        reset();
    }

    [[nodiscard]] bool active() const noexcept
    {
        return api_ != nullptr && !menu_path_.empty() && !command_id_.empty();
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return active();
    }

    void reset() noexcept
    {
        if (api_ != nullptr && api_->remove_menu_item != nullptr && !menu_path_.empty() && !command_id_.empty()) {
            api_->remove_menu_item(api_->api_user_data, menu_path_.c_str(), command_id_.c_str());
        }
        release();
    }

    void release() noexcept
    {
        api_ = nullptr;
        menu_path_.clear();
        command_id_.clear();
    }

private:
    void moveFrom(RegisteredMenuItem& other) noexcept
    {
        api_ = std::exchange(other.api_, nullptr);
        menu_path_ = std::move(other.menu_path_);
        command_id_ = std::move(other.command_id_);
        other.menu_path_.clear();
        other.command_id_.clear();
    }

    const LunaEditorMenuApi* api_{};
    std::string menu_path_;
    std::string command_id_;
};

class RegisteredMenuItemsForCommand final {
public:
    RegisteredMenuItemsForCommand() noexcept = default;

    RegisteredMenuItemsForCommand(const LunaEditorMenuApi* api, const char* command_id)
        : api_(api),
          command_id_(command_id != nullptr ? command_id : "")
    {}

    RegisteredMenuItemsForCommand(const RegisteredMenuItemsForCommand&) = delete;
    RegisteredMenuItemsForCommand& operator=(const RegisteredMenuItemsForCommand&) = delete;

    RegisteredMenuItemsForCommand(RegisteredMenuItemsForCommand&& other) noexcept
    {
        moveFrom(other);
    }

    RegisteredMenuItemsForCommand& operator=(RegisteredMenuItemsForCommand&& other) noexcept
    {
        if (this != &other) {
            reset();
            moveFrom(other);
        }
        return *this;
    }

    ~RegisteredMenuItemsForCommand()
    {
        reset();
    }

    [[nodiscard]] bool active() const noexcept
    {
        return api_ != nullptr && !command_id_.empty();
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return active();
    }

    void reset() noexcept
    {
        if (api_ != nullptr && api_->remove_menu_items_for_command != nullptr && !command_id_.empty()) {
            api_->remove_menu_items_for_command(api_->api_user_data, command_id_.c_str());
        }
        release();
    }

    void release() noexcept
    {
        api_ = nullptr;
        command_id_.clear();
    }

private:
    void moveFrom(RegisteredMenuItemsForCommand& other) noexcept
    {
        api_ = std::exchange(other.api_, nullptr);
        command_id_ = std::move(other.command_id_);
        other.command_id_.clear();
    }

    const LunaEditorMenuApi* api_{};
    std::string command_id_;
};

class SceneViewportHandle final {
public:
    SceneViewportHandle() noexcept = default;

    SceneViewportHandle(const LunaEditorViewportApi* api, ViewportId id) noexcept
        : api_(api),
          id_(id)
    {}

    SceneViewportHandle(const SceneViewportHandle&) = delete;
    SceneViewportHandle& operator=(const SceneViewportHandle&) = delete;

    SceneViewportHandle(SceneViewportHandle&& other) noexcept
    {
        moveFrom(other);
    }

    SceneViewportHandle& operator=(SceneViewportHandle&& other) noexcept
    {
        if (this != &other) {
            reset();
            moveFrom(other);
        }
        return *this;
    }

    ~SceneViewportHandle()
    {
        reset();
    }

    [[nodiscard]] bool active() const noexcept
    {
        return api_ != nullptr && id_ != 0u;
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return active();
    }

    [[nodiscard]] ViewportId id() const noexcept
    {
        return id_;
    }

    void reset() noexcept
    {
        if (api_ != nullptr && api_->destroy_scene_viewport != nullptr && id_ != 0u) {
            api_->destroy_scene_viewport(api_->api_user_data, id_);
        }
        (void) release();
    }

    [[nodiscard]] ViewportId release() noexcept
    {
        api_ = nullptr;
        return std::exchange(id_, 0u);
    }

private:
    void moveFrom(SceneViewportHandle& other) noexcept
    {
        api_ = std::exchange(other.api_, nullptr);
        id_ = std::exchange(other.id_, 0u);
    }

    const LunaEditorViewportApi* api_{};
    ViewportId id_{};
};

} // namespace luna::editor::native
