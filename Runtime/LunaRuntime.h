#pragma once

#include "Core/Application.h"
#include "Scene/Scene.h"
#include "Scene/SceneRuntime.h"

#include <filesystem>
#include <Instance.h>
#include <memory>
#include <string>

namespace luna {

class LunaRuntimeApplication final : public Application {
public:
    LunaRuntimeApplication(luna::RHI::BackendType backend, std::filesystem::path project_file_path);

protected:
    Renderer::InitializationOptions getRendererInitializationOptions() override;
    void onInit() override;
    void onUpdate(Timestep timestep) override;
    void onShutdown() override;

private:
    bool loadStartupScene();

private:
    Scene m_scene;
    std::unique_ptr<SceneRuntime> m_scene_runtime;
    std::filesystem::path m_project_file_path;
    std::filesystem::path m_scene_file_path;
    luna::RHI::BackendType m_backend{luna::RHI::BackendType::Auto};
};

Application* createApplication(int argc, char** argv);

} // namespace luna
