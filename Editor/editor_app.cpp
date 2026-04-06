#include "Editor/editor_app.h"

#include "Editor/editor_layer.h"
#include "Core/log.h"
#include "Platform/GLFWWindow.hpp"
#include "Renderer/SceneRenderPipeline.h"

#include <GLFW/glfw3.h>

#include <array>
#include <string_view>
#include <utility>

namespace luna::editor {

namespace {

bool parse_arguments(int argc, char** argv, EditorAppOptions* options)
{
    if (options == nullptr) {
        return false;
    }

    constexpr std::string_view kSelfTestName = "phase3_editor_resize";
    constexpr std::string_view kSelfTestPrefix = "--self-test=";
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--self-test") {
            options->runResizeSelfTest = true;
            continue;
        }

        if (argument.substr(0, kSelfTestPrefix.size()) == kSelfTestPrefix) {
            if (argument.substr(kSelfTestPrefix.size()) != kSelfTestName) {
                LUNA_CORE_ERROR("Unknown self-test '{}'", argument.substr(kSelfTestPrefix.size()));
                return false;
            }

            options->runResizeSelfTest = true;
            continue;
        }

        LUNA_CORE_ERROR("Unknown argument '{}'", argument);
        return false;
    }

    return true;
}

class EditorResizeSelfTestLayer final : public Layer {
public:
    explicit EditorResizeSelfTestLayer(EditorApp& app)
        : Layer("EditorResizeSelfTestLayer"),
          m_app(app)
    {}

    void onAttach() override
    {
        LUNA_CORE_INFO("LunaApp self-test begin: phase3_editor_resize");
    }

    void onUpdate(Timestep dt) override
    {
        m_elapsedSeconds += dt.getSeconds();
        GLFWwindow* const window = GLFWWindow::getActiveNativeWindow();
        if (window == nullptr) {
            finish(false);
            return;
        }

        if (m_elapsedSeconds >= m_nextResizeTimeSeconds) {
            static constexpr std::array<std::pair<int, int>, 8> kSizes = {
                std::pair<int, int>{1700, 900},
                std::pair<int, int>{1280, 720},
                std::pair<int, int>{1580, 980},
                std::pair<int, int>{1160, 720},
                std::pair<int, int>{1440, 840},
                std::pair<int, int>{1024, 768},
                std::pair<int, int>{1360, 900},
                std::pair<int, int>{1200, 700},
            };

            const auto& [width, height] = kSizes[m_resizeCount % kSizes.size()];
            glfwSetWindowSize(window, width, height);
            ++m_resizeCount;
            LUNA_CORE_INFO("LunaApp resize self-test step {} -> {}x{}", m_resizeCount, width, height);
            m_nextResizeTimeSeconds += 0.5f;
        }

        if (m_elapsedSeconds >= 10.0f) {
            finish(m_resizeCount >= 18);
        }
    }

private:
    void finish(bool passed)
    {
        if (m_finished) {
            return;
        }

        m_finished = true;
        m_app.setSelfTestPassed(passed);
        if (passed) {
            LUNA_CORE_INFO("LunaApp self-test PASS resizeSteps={} elapsed={:.2f}s",
                           m_resizeCount,
                           m_elapsedSeconds);
        } else {
            LUNA_CORE_ERROR("LunaApp self-test FAIL resizeSteps={} elapsed={:.2f}s",
                            m_resizeCount,
                            m_elapsedSeconds);
        }

        Application::get().close();
    }

private:
    EditorApp& m_app;
    float m_elapsedSeconds = 0.0f;
    float m_nextResizeTimeSeconds = 0.0f;
    int m_resizeCount = 0;
    bool m_finished = false;
};

} // namespace

EditorApp::EditorApp(const EditorAppOptions& options)
    : Application(ApplicationSpecification{
          .name = "Luna Editor",
          .windowWidth = 1'700,
          .windowHeight = 900,
          .maximized = false,
          .renderService =
              {
                  .applicationName = "Luna Editor",
                  .backend = luna::RHIBackend::Vulkan,
                  .renderPipeline = luna::CreateDefaultSceneRenderPipeline(),
              },
      }),
      m_options(options)
{}

void EditorApp::onInit()
{
    pushLayer(std::make_unique<EditorLayer>());
    if (m_options.runResizeSelfTest) {
        pushLayer(std::make_unique<EditorResizeSelfTestLayer>(*this));
    }
}

} // namespace luna::editor

namespace luna {

Application* createApplication(int argc, char** argv)
{
    editor::EditorAppOptions options;
    if (!editor::parse_arguments(argc, argv, &options)) {
        return nullptr;
    }

    return new editor::EditorApp(options);
}

} // namespace luna
