#pragma once

#include <ostream>
#include <string>

namespace luna {

enum class EventType : int {
    None,
    WindowClose,
    WindowResize,
    WindowFocus,
    WindowLostFocus,
    WindowMoved,
    AppTick,
    AppUpdate,
    AppRender,
    KeyPressed,
    KeyReleased,
    KeyTyped,
    MouseButtonPressed,
    MouseButtonReleased,
    MouseMoved,
    MouseScrolled
};

enum class EventCategory : int {
    None = 0,
    EventCategoryApplication = 1 << 0,
    EventCategoryInput = 1 << 1,
    EventCategoryKeyboard = 1 << 2,
    EventCategoryMouse = 1 << 3,
    EventCategoryMouseButton = 1 << 4
};

class Event {
public:
    virtual ~Event() = default;

    bool handled = false;

    virtual EventType getEventType() const = 0;
    virtual const char* getName() const = 0;
    virtual int getCategoryFlags() const = 0;

    virtual std::string toString() const
    {
        return getName();
    }

    bool isInCategory(EventCategory category) const
    {
        return (getCategoryFlags() & static_cast<int>(category)) != 0;
    }
};

class EventDispatcher {
public:
    explicit EventDispatcher(Event& event)
        : m_event(event)
    {}

    template <typename T, typename F>
    bool dispatch(const F& func)
    {
        if (m_event.getEventType() == T::getStaticType()) {
            m_event.handled |= func(static_cast<T&>(m_event));
            return true;
        }

        return false;
    }

private:
    Event& m_event;
};

inline std::ostream& operator<<(std::ostream& os, const Event& event)
{
    return os << event.toString();
}

} // namespace luna
