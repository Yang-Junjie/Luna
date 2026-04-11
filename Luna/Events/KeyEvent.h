#pragma once
#include "Core/KeyCodes.h"
#include "Event.h"

#include <sstream>

namespace luna {
class KeyEvent : public Event {
public:
    KeyCode getKeyCode() const
    {
        return m_key_code;
    }

    virtual int getCategoryFlags() const override
    {
        return static_cast<int>(EventCategory::EventCategoryKeyboard) |
               static_cast<int>(EventCategory::EventCategoryInput);
    }

protected:
    KeyEvent(const KeyCode key_code)
        : m_key_code(key_code)
    {}

    KeyCode m_key_code;
};

class KeyPressedEvent : public KeyEvent {
public:
    KeyPressedEvent(const KeyCode key_code, bool is_repeat = false)
        : KeyEvent(key_code),
          m_is_repeat(is_repeat)
    {}

    bool isRepeat() const
    {
        return m_is_repeat;
    }

    std::string toString() const override
    {
        std::stringstream ss;
        ss << "KeyPressedEvent: " << static_cast<int>(m_key_code) << " (repeat = " << m_is_repeat << ")";
        return ss.str();
    }

    static EventType getStaticType()
    {
        return EventType::KeyPressed;
    }

    virtual EventType getEventType() const override
    {
        return getStaticType();
    }

    virtual const char* getName() const override
    {
        return "KeyPressed";
    }

private:
    bool m_is_repeat;
};

class KeyReleasedEvent : public KeyEvent {
public:
    KeyReleasedEvent(const KeyCode key_code)
        : KeyEvent(key_code)
    {}

    std::string toString() const override
    {
        std::stringstream ss;
        ss << "KeyReleasedEvent: " << static_cast<int>(m_key_code);
        return ss.str();
    }

    static EventType getStaticType()
    {
        return EventType::KeyReleased;
    }

    virtual EventType getEventType() const override
    {
        return getStaticType();
    }

    virtual const char* getName() const override
    {
        return "KeyReleased";
    }
};

class KeyTypedEvent : public KeyEvent {
public:
    KeyTypedEvent(const KeyCode key_code)
        : KeyEvent(key_code)
    {}

    std::string toString() const override
    {
        std::stringstream ss;
        ss << "KeyTypedEvent: " << static_cast<int>(m_key_code);
        return ss.str();
    }

    static EventType getStaticType()
    {
        return EventType::KeyTyped;
    }

    virtual EventType getEventType() const override
    {
        return getStaticType();
    }

    virtual const char* getName() const override
    {
        return "KeyTyped";
    }
};
} // namespace luna
