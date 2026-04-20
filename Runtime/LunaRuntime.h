#pragma once

#include "Core/Application.h"
#include "Scene/Scene.h"

#include <Instance.h>
#include <filesystem>
#include <string>

namespace luna {

class LunaRuntimeApplication final : public Application {
public:
    LunaRuntimeApplication(luna::RHI::BackendType backend, std::filesystem::path project_file_path);

protected:
    Renderer::InitializationOptions getRendererInitializationOptions() override;
    void onInit() override;
    void onUpdate(Timestep timestep) override;

private:
    void resetCamera();
    bool loadStartupScene();

private:
    Scene m_scene;
    std::filesystem::path m_project_file_path;
    std::filesystem::path m_scene_file_path;
    luna::RHI::BackendType m_backend{luna::RHI::BackendType::Vulkan};
};

Application* createApplication(int argc, char** argv);

} // namespace luna
