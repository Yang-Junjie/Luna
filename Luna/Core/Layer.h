#pragma once
#include "Timestep.h"

#include <string>

namespace luna {
class Event;

class Layer {
public:
    Layer(const std::string& name = "Layer")
        : m_name(name)
    {}

    virtual ~Layer() = default;

    virtual void onAttach() {}

    virtual void onDetach() {}

    virtual void onUpdate(Timestep dt) {}

    virtual void onEvent(Event& event) {}

    virtual void onImGuiRender() {}

    virtual void onRender() {}

    const std::string& getName() const
    {
        return m_name;
    }

protected:
    std::string m_name;
};
} // namespace luna
