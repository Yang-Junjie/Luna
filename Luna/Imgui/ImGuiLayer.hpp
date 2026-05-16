#pragma once

#include "Core/Layer.h"
#include "Renderer/Renderer.h"

#include <filesystem>

namespace luna {

class Event;

struct ImGuiFontConfig {
    std::filesystem::path font_path;
    float size_pixels{16.0f};
};

class ImGuiLayer final : public Layer {
public:
    ImGuiLayer(Renderer& renderer, bool enable_multi_viewport, ImGuiFontConfig font_config = {});
    ~ImGuiLayer() override = default;

    void onAttach() override;
    void onDetach() override;
    void onEvent(Event& event) override;

    void onImGuiRender() override {}

    void startFrame();

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
    bool m_block_events = true;
    bool m_attached = false;
    bool m_enable_multi_viewport = false;
    ImGuiFontConfig m_font_config;
    Renderer* m_renderer = nullptr;
};

} // namespace luna
