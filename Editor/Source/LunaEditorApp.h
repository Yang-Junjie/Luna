#pragma once

#include "Core/Application.h"
#include "EditorEnginePaths.h"

#include <Instance.h>

namespace luna {

class LunaEditorApplication final : public Application {
public:
    LunaEditorApplication(luna::RHI::BackendType backend, editor::EditorEnginePaths engine_paths);

    luna::RHI::BackendType getBackend() const;
    const editor::EditorEnginePaths& enginePaths() const noexcept;

protected:
    Renderer::InitializationOptions getRendererInitializationOptions() override;
    void onInit() override;

private:
    luna::RHI::BackendType m_backend{luna::RHI::BackendType::Auto};
    editor::EditorEnginePaths m_engine_paths;
};

Application* createApplication(int argc, char** argv);

} // namespace luna
