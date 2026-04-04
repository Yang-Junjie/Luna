#include "Core/application.h"
#include "Core/input.h"
#include "Core/log.h"
#include "Renderer/SceneRenderPipeline.h"

#include <memory>

namespace {

class LegacySceneApp final : public luna::Application {
public:
    LegacySceneApp()
        : luna::Application(make_specification())
    {}

protected:
    void onInit() override
    {
        auto& renderService = getRenderService();
        auto& effects = renderService.getBackgroundEffects();
        LUNA_CORE_INFO("LegacyScene controls: WASD move, Q/E vertical, Left/Right switch background effect");
        LUNA_CORE_INFO("LegacyScene scene loaded={}, backgroundEffects={}",
                       renderService.findLoadedScene("basicmesh") != nullptr,
                       effects.size());
    }

    void onUpdate(luna::Timestep dt) override
    {
        update_camera(dt);
        update_background_effect();
    }

private:
    static luna::ApplicationSpecification make_specification()
    {
        luna::ApplicationSpecification specification{};
        specification.name = "LegacyScene";
        specification.windowWidth = 1440;
        specification.windowHeight = 900;
        specification.maximized = false;
        specification.enableImGui = false;
        specification.enableMultiViewport = false;
        specification.renderService.applicationName = "LegacyScene";
        specification.renderService.backend = luna::RHIBackend::Vulkan;
        specification.renderService.renderPipeline = luna::CreateDefaultSceneRenderPipeline();
        return specification;
    }

    void update_camera(luna::Timestep dt)
    {
        auto& camera = getRenderService().getMainCamera();
        float speed = 3.5f;
        if (luna::Input::isKeyPressed(luna::KeyCode::LeftShift) ||
            luna::Input::isKeyPressed(luna::KeyCode::RightShift)) {
            speed *= 3.0f;
        }

        const float step = speed * static_cast<float>(dt);
        if (luna::Input::isKeyPressed(luna::KeyCode::W)) {
            camera.position.z -= step;
        }
        if (luna::Input::isKeyPressed(luna::KeyCode::S)) {
            camera.position.z += step;
        }
        if (luna::Input::isKeyPressed(luna::KeyCode::A)) {
            camera.position.x -= step;
        }
        if (luna::Input::isKeyPressed(luna::KeyCode::D)) {
            camera.position.x += step;
        }
        if (luna::Input::isKeyPressed(luna::KeyCode::Q)) {
            camera.position.y -= step;
        }
        if (luna::Input::isKeyPressed(luna::KeyCode::E)) {
            camera.position.y += step;
        }
    }

    void update_background_effect()
    {
        auto& renderService = getRenderService();
        auto& effects = renderService.getBackgroundEffects();
        if (effects.empty()) {
            return;
        }

        auto& currentEffectIndex = renderService.getCurrentBackgroundEffect();
        const bool leftPressed = luna::Input::isKeyPressed(luna::KeyCode::Left);
        const bool rightPressed = luna::Input::isKeyPressed(luna::KeyCode::Right);

        if (leftPressed && !m_previousLeftPressed) {
            currentEffectIndex =
                currentEffectIndex <= 0 ? static_cast<int>(effects.size()) - 1 : currentEffectIndex - 1;
            LUNA_CORE_INFO("LegacyScene background effect -> {}",
                           effects[static_cast<size_t>(currentEffectIndex)].name);
        }

        if (rightPressed && !m_previousRightPressed) {
            currentEffectIndex = (currentEffectIndex + 1) % static_cast<int>(effects.size());
            LUNA_CORE_INFO("LegacyScene background effect -> {}",
                           effects[static_cast<size_t>(currentEffectIndex)].name);
        }

        m_previousLeftPressed = leftPressed;
        m_previousRightPressed = rightPressed;
    }

private:
    bool m_previousLeftPressed = false;
    bool m_previousRightPressed = false;
};

} // namespace

int main()
{
#ifndef NDEBUG
    constexpr luna::Logger::Level kLogLevel = luna::Logger::Level::Trace;
#else
    constexpr luna::Logger::Level kLogLevel = luna::Logger::Level::Info;
#endif

    luna::Logger::init("logs/luna.log", kLogLevel);

    std::unique_ptr<luna::Application> app = std::make_unique<LegacySceneApp>();
    if (!app->isInitialized()) {
        app.reset();
        luna::Logger::shutdown();
        return 1;
    }

    app->run();
    app.reset();
    luna::Logger::shutdown();
    return 0;
}
