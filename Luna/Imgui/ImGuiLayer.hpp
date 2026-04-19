#pragma once

#include "Core/Layer.h"
#include "Renderer/Renderer.h"

#include <functional>

namespace luna {

class Event;

class ImGuiLayer final : public Layer {
public:
    using MenuBarCallback = std::function<void()>;
    ImGuiLayer(Renderer& renderer, bool enable_multi_viewport);
    ~ImGuiLayer() override = default;

    void onAttach() override;
    void onDetach() override;
    void onEvent(Event& event) override;

    void onImGuiRender() override {}

    void begin();
    void end();

    void setMenuBarCallback(MenuBarCallback callback)
    {
        m_menu_bar_callback = std::move(callback);
    }

    void renderPlatformWindows();

    void blockEvents(bool block)
    {
        m_block_events = block;
    }

    bool isInitialized() const
    {
        return m_attached;
    }

    bool viewportsEnabled() const;

private:
    void setImGuiWidgetStyle();
    void setDarkThemeColors();

private:
    bool m_block_events = true;
    bool m_attached = false;
    bool m_enable_multi_viewport = false;
    Renderer* m_renderer = nullptr;
    MenuBarCallback m_menu_bar_callback;
};

} // namespace luna
