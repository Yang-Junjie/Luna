#include "ExampleToolPlugin.h"

#include "EditorApi/EditorApi.h"
#include "Shell/EditorBuiltinPluginRegistry.h"

#include <string>

namespace {

constexpr const char* kPluginId = "luna.source.example-tool";
constexpr const char* kWindowId = "luna.source.example-tool.window";

class SourceExampleToolPlugin final : public luna::editor::Plugin {
public:
    [[nodiscard]] luna::editor::PluginDescriptor descriptor() const override
    {
        return luna::editor::PluginDescriptor{
            .id = kPluginId,
            .display_name = "Source Example Tool",
            .version = "0.1.0",
        };
    }

    bool onLoad(luna::editor::Host& host) override
    {
        return host.windows().registerWindow(luna::editor::WindowDescriptor{
            .id = kWindowId,
            .title = "Source Example Tool",
            .default_open = true,
            .default_size = luna::editor::Vec2{.x = 420.0f, .y = 260.0f},
            .draw =
                [this](luna::editor::WindowDrawContext& context) {
                    drawWindow(context);
                },
        });
    }

    void onUnload(luna::editor::Host& host) override
    {
        host.windows().unregisterWindow(kWindowId);
    }

private:
    void drawWindow(luna::editor::WindowDrawContext& context)
    {
        luna::editor::Ui& ui = context.ui();
        luna::editor::Host& host = context.host();

        ui.textWrapped("This window is provided by a source-level editor plugin under Plugins/Editor.");
        ui.separatorText("Package Asset");

        if (!m_asset_loaded) {
            m_asset_text = host.pluginAssets().readText(kPluginId, "welcome.txt").value_or("Asset text missing.");
            m_asset_loaded = true;
        }
        ui.textWrapped(m_asset_text);

        ui.separatorText("State");
        if (ui.button("Increment Counter", luna::editor::ButtonVariant::Primary)) {
            ++m_counter;
        }
        ui.text("Counter: " + std::to_string(m_counter));
    }

    std::string m_asset_text;
    int m_counter{0};
    bool m_asset_loaded{false};
};

} // namespace

namespace luna::editor {

std::unique_ptr<Plugin> createSourceExampleToolPlugin()
{
    return std::make_unique<SourceExampleToolPlugin>();
}

namespace {

const EditorBuiltinPluginFactoryRegistration kSourceExampleToolRegistration{
    kPluginId,
    createSourceExampleToolPlugin,
};

} // namespace

} // namespace luna::editor
