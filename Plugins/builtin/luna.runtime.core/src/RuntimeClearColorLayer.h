#pragma once

#include "Core/Application.h"
#include "Core/Layer.h"
#include "Core/Log.h"

#include <cmath>

namespace luna::runtime {

class RuntimeClearColorLayer final : public Layer {
public:
    RuntimeClearColorLayer()
        : Layer("RuntimeClearColorLayer")
    {}

    void onAttach() override
    {
        LUNA_RUNTIME_INFO("Runtime clear-color layer attached");
    }

    void onUpdate(Timestep dt) override
    {
        m_elapsed_seconds += dt.getSeconds();

        auto& clear_color = luna::Application::get().getRenderer().getClearColor();
        clear_color.r = 0.10f + 0.08f * (0.5f + 0.5f * std::sin(m_elapsed_seconds * 0.7f));
        clear_color.g = 0.10f + 0.10f * (0.5f + 0.5f * std::sin(m_elapsed_seconds * 1.1f + 1.2f));
        clear_color.b = 0.14f + 0.12f * (0.5f + 0.5f * std::sin(m_elapsed_seconds * 0.9f + 2.4f));
        clear_color.a = 1.0f;
    }

private:
    float m_elapsed_seconds = 0.0f;
};

} // namespace luna::runtime
