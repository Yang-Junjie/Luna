#pragma once

#include "EditorApi/EditorTypes.h"

#include <functional>
#include <string>
#include <string_view>

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
    std::function<void(WindowDrawContext&)> draw;
};

class WindowService {
public:
    virtual ~WindowService() = default;

    virtual bool registerWindow(WindowDescriptor descriptor) = 0;
    virtual void unregisterWindow(std::string_view id) = 0;
    virtual bool isWindowOpen(std::string_view id) const = 0;
    virtual void setWindowOpen(std::string_view id, bool open) = 0;
};

} // namespace luna::editor
