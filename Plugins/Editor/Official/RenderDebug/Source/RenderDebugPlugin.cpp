#include "RenderDebugPlugin.h"

#include "EditorApi/EditorApi.h"

#include <cstddef>
#include <string_view>
#include <vector>

namespace {

constexpr const char* kPluginId = "luna.editor.render-debug";
constexpr const char* kWindowId = "luna.editor.render-debug.window";

int modeIndex(luna::editor::RenderDebugViewMode mode,
              const std::vector<luna::editor::RenderDebugViewModeInfo>& modes)
{
    for (int index = 0; index < static_cast<int>(modes.size()); ++index) {
        if (modes[static_cast<size_t>(index)].mode == mode) {
            return index;
        }
    }
    return 0;
}

void drawRenderDebugWindow(luna::editor::WindowDrawContext& context, luna::editor::ViewportId texture_viewport)
{
    luna::editor::Host& host = context.host();
    luna::editor::Ui& ui = context.ui();
    const std::vector<luna::editor::RenderDebugViewModeInfo> modes = host.rendering().renderDebugViewModes();
    if (modes.empty()) {
        ui.text("No render debug views registered.");
        return;
    }

    const luna::editor::RenderDebugViewMode current_mode = host.rendering().renderDebugViewMode();
    int selected_mode = modeIndex(current_mode, modes);
    ui.setNextItemWidth(220.0f);
    if (ui.beginCombo("View", modes[static_cast<size_t>(selected_mode)].label)) {
        for (int index = 0; index < static_cast<int>(modes.size()); ++index) {
            const auto& item = modes[static_cast<size_t>(index)];
            const bool selected = index == selected_mode;
            if (ui.selectable(item.label, selected)) {
                selected_mode = index;
                host.rendering().setRenderDebugViewMode(item.mode);
            }
            if (selected) {
                ui.setItemDefaultFocus();
            }
        }
        ui.endCombo();
    }

    if (host.rendering().renderDebugViewMode() == luna::editor::RenderDebugViewMode::Velocity) {
        float velocity_scale = host.rendering().renderDebugVelocityScale();
        ui.setNextItemWidth(220.0f);
        if (ui.sliderFloat("Velocity Scale", velocity_scale, 1.0f, 200.0f, "%.1f")) {
            host.rendering().setRenderDebugVelocityScale(velocity_scale);
        }
    }

    ui.separator();

    if (host.rendering().renderDebugViewMode() == luna::editor::RenderDebugViewMode::None) {
        ui.text("Select a debug view to render a preview.");
        return;
    }

    const luna::editor::TextureView debug_texture = host.rendering().renderDebugTextureView();
    if (!debug_texture.valid()) {
        ui.text("Debug texture will appear after the next rendered frame.");
        return;
    }

    (void) host.viewport().drawTextureViewport(ui, texture_viewport, debug_texture);
}

class RenderDebugPlugin final : public luna::editor::Plugin {
public:
    [[nodiscard]] luna::editor::PluginDescriptor descriptor() const override
    {
        return luna::editor::PluginDescriptor{
            .id = kPluginId,
            .display_name = "Render Debug",
            .version = "0.1.0",
        };
    }

    bool onLoad(luna::editor::Host& host) override
    {
        m_texture_viewport = host.viewport().createTextureViewport("Render Debug Preview");
        if (m_texture_viewport == luna::editor::kInvalidViewportId) {
            return false;
        }

        const bool window_registered = host.windows().registerWindow(luna::editor::WindowDescriptor{
            .id = kWindowId,
            .title = "Render Debug",
            .default_open = false,
            .default_size = luna::editor::Vec2{.x = 640.0f, .y = 420.0f},
            .draw =
                [this](luna::editor::WindowDrawContext& context) {
                    drawRenderDebugWindow(context, m_texture_viewport);
                },
        });
        if (!window_registered) {
            host.viewport().destroyTextureViewport(m_texture_viewport);
            m_texture_viewport = luna::editor::kInvalidViewportId;
            return false;
        }

        return true;
    }

    void onUnload(luna::editor::Host& host) override
    {
        host.rendering().setRenderDebugViewMode(luna::editor::RenderDebugViewMode::None);
        host.windows().unregisterWindow(kWindowId);
        if (m_texture_viewport != luna::editor::kInvalidViewportId) {
            host.viewport().destroyTextureViewport(m_texture_viewport);
            m_texture_viewport = luna::editor::kInvalidViewportId;
        }
    }

    void onUpdate(luna::editor::Host& host, float) override
    {
        if (!host.windows().isWindowOpen(kWindowId) &&
            host.rendering().renderDebugViewMode() != luna::editor::RenderDebugViewMode::None) {
            host.rendering().setRenderDebugViewMode(luna::editor::RenderDebugViewMode::None);
        }
    }

private:
    luna::editor::ViewportId m_texture_viewport{luna::editor::kInvalidViewportId};
};

} // namespace

namespace luna::editor {

std::unique_ptr<Plugin> createRenderDebugPlugin()
{
    return std::make_unique<RenderDebugPlugin>();
}

} // namespace luna::editor
