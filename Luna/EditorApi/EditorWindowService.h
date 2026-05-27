#pragma once

#include "EditorApi/EditorTypes.h"

#include <cstdint>

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace luna::editor {

class Host;
class Ui;

class WindowDrawContext {
public:
    WindowDrawContext(Host& host, Ui& ui)
        : m_host(&host),
          m_ui(&ui)
    {}

    [[nodiscard]] Host& host() const noexcept
    {
        return *m_host;
    }

    [[nodiscard]] Ui& ui() const noexcept
    {
        return *m_ui;
    }

private:
    Host* m_host{nullptr};
    Ui* m_ui{nullptr};
};

struct WindowDescriptor {
    std::string id;
    std::string title;
    bool default_open{false};
    Vec2 default_size{};
    WindowFlags flags{static_cast<WindowFlags>(WindowFlag::None)};
    std::string owner_id;
    std::string dockspace_id;
    bool show_in_window_menu{true};
    std::function<void(WindowDrawContext&)> draw;
};

enum class DockSplitDirection : uint8_t {
    Left,
    Right,
    Up,
    Down,
};

struct DockedWindowDescriptor {
    std::string window_id;
    DockSplitDirection direction{DockSplitDirection::Right};
    float ratio{0.5f};
};

struct DockspaceWindowDescriptor {
    std::string id;
    std::string title;
    bool default_open{false};
    Vec2 default_size{};
    WindowFlags flags{static_cast<WindowFlags>(WindowFlag::None)};
    std::string owner_id;
    std::vector<DockedWindowDescriptor> docked_windows;
    bool show_in_window_menu{true};
};

class WindowService {
public:
    virtual ~WindowService() = default;

    virtual bool registerWindow(WindowDescriptor descriptor) = 0;
    virtual bool registerDockspaceWindow(DockspaceWindowDescriptor descriptor) = 0;
    virtual void unregisterWindow(std::string_view id) = 0;
    virtual bool isWindowOpen(std::string_view id) const = 0;
    virtual void setWindowOpen(std::string_view id, bool open) = 0;
};

} // namespace luna::editor
