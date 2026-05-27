#pragma once
#include "Core/MouseCodes.h"
#include "Event.h"

#include <sstream>
#include <string>

namespace luna {
class MouseMovedEvent : public Event {
public:
    MouseMovedEvent(const float x, const float y)
        : m_mouse_x(x),
          m_mouse_y(y)
    {}

    float getX() const
    {
        return m_mouse_x;
    }

    float getY() const
    {
        return m_mouse_y;
    }

    std::string toString() const override
    {
        std::stringstream ss;
        ss << "MouseMovedEvent: " << m_mouse_x << ", " << m_mouse_y;
        return ss.str();
    }

    static EventType getStaticType()
    {
        return EventType::MouseMoved;
    }

    virtual EventType getEventType() const override
    {
        return getStaticType();
    }

    virtual const char* getName() const override
    {
        return "MouseMoved";
    }

    virtual int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::EventCategoryMouse) |
               static_cast<int>(EventCategory::EventCategoryInput);
    }

private:
    float m_mouse_x, m_mouse_y;
};

class MouseScrolledEvent : public Event {
public:
    MouseScrolledEvent(const float x_offset, const float y_offset)
        : m_x_offset(x_offset),
          m_y_offset(y_offset)
    {}

    float getXOffset() const
    {
        return m_x_offset;
    }

    float getYOffset() const
    {
        return m_y_offset;
    }

    std::string toString() const override
    {
        std::stringstream ss;
        ss << "MouseScrolledEvent: " << getXOffset() << ", " << getYOffset();
        return ss.str();
    }

    static EventType getStaticType()
    {
        return EventType::MouseScrolled;
    }

    virtual EventType getEventType() const override
    {
        return getStaticType();
    }

    virtual const char* getName() const override
    {
        return "MouseScrolled";
    }

    virtual int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::EventCategoryMouse) |
               static_cast<int>(EventCategory::EventCategoryInput);
    }

private:
    float m_x_offset, m_y_offset;
};

class MouseButtonEvent : public Event {
public:
    MouseCode getMouseButton() const
    {
        return m_button;
    }

    virtual int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::EventCategoryMouse) |
               static_cast<int>(EventCategory::EventCategoryInput) |
               static_cast<int>(EventCategory::EventCategoryMouseButton);
    }

protected:
    MouseButtonEvent(const MouseCode button)
        : m_button(button)
    {}

    MouseCode m_button;
};

class MouseButtonPressedEvent : public MouseButtonEvent {
public:
    MouseButtonPressedEvent(const MouseCode button)
        : MouseButtonEvent(button)
    {}

    std::string toString() const override
    {
        std::stringstream ss;
        ss << "MouseButtonPressedEvent: " << static_cast<int>(m_button);
        return ss.str();
    }

    static EventType getStaticType()
    {
        return EventType::MouseButtonPressed;
    }

    virtual EventType getEventType() const override
    {
        return getStaticType();
    }

    virtual const char* getName() const override
    {
        return "MouseButtonPressed";
    }
};

class MouseButtonReleasedEvent : public MouseButtonEvent {
public:
    MouseButtonReleasedEvent(const MouseCode button)
        : MouseButtonEvent(button)
    {}

    std::string toString() const override
    {
        std::stringstream ss;
        ss << "MouseButtonReleasedEvent: " << static_cast<int>(m_button);
        return ss.str();
    }

    static EventType getStaticType()
    {
        return EventType::MouseButtonReleased;
    }

    virtual EventType getEventType() const override
    {
        return getStaticType();
    }

    virtual const char* getName() const override
    {
        return "MouseButtonReleased";
    }
};
} // namespace luna
