#include "Core/application.h"
#include "Core/log.h"
#include "Editor/editor_app.h"

#include <memory>

int main(int argc, char** argv)
{
#ifndef NDEBUG
    constexpr luna::Logger::Level kLogLevel = luna::Logger::Level::Trace;
#else
    constexpr luna::Logger::Level kLogLevel = luna::Logger::Level::Info;
#endif

    luna::Logger::init("logs/luna.log", kLogLevel);
    LUNA_CORE_INFO("Application starting");

    std::unique_ptr<luna::Application> app(luna::createApplication(argc, argv));
    if (app == nullptr) {
        LUNA_CORE_FATAL("Application creation returned null");
        luna::Logger::shutdown();
        return 1;
    }

    if (!app->isInitialized()) {
        LUNA_CORE_FATAL("Application initialization failed");
        luna::Logger::shutdown();
        return 1;
    }

    int exitCode = 0;
    app->run();
    if (auto* editorApp = dynamic_cast<luna::editor::EditorApp*>(app.get());
        editorApp != nullptr && editorApp->isSelfTestMode() && !editorApp->selfTestPassed()) {
        exitCode = 1;
    }

    app.reset();
    LUNA_CORE_INFO("Application shutdown cleanly");
    luna::Logger::shutdown();
    return exitCode;
}
