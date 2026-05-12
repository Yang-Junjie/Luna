#include "Plugins/ViewportPlugin.h"

#include "EditorApi/EditorApi.h"

namespace {

constexpr const char* kPluginId = "luna.editor.viewport";
constexpr const char* kWindowId = "luna.editor.viewport.window";

class ViewportPlugin final : public luna::editor::Plugin {
public:
    [[nodiscard]] luna::editor::PluginDescriptor descriptor() const override
    {
        return luna::editor::PluginDescriptor{
            .id = kPluginId,
            .display_name = "Viewport",
            .version = "0.1.0",
        };
    }

    bool onLoad(luna::editor::Host& host) override
    {
        return host.windows().registerWindow(luna::editor::WindowDescriptor{
            .id = kWindowId,
            .title = "Viewport",
            .default_open = true,
            .default_size = luna::editor::Vec2{.x = 960.0f, .y = 640.0f},
            .flags = static_cast<luna::editor::WindowFlags>(luna::editor::WindowFlag::NoPadding),
            .draw =
                [](luna::editor::WindowDrawContext& context) {
                    context.host().viewport().drawDefaultSceneViewport(context.ui());
                },
        });
    }

    void onUnload(luna::editor::Host& host) override
    {
        host.windows().unregisterWindow(kWindowId);
    }
};

} // namespace

namespace luna::editor {

std::unique_ptr<Plugin> createViewportPlugin()
{
    return std::make_unique<ViewportPlugin>();
}

} // namespace luna::editor
