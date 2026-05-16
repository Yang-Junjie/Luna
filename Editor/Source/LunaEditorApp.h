#pragma once

#include "Core/Application.h"
#include "EditorEnginePaths.h"
#include "EditorSettings.h"

#include <Instance.h>

namespace luna {

class LunaEditorApplication final : public Application {
public:
    LunaEditorApplication(luna::RHI::BackendType backend, editor::EditorEnginePaths engine_paths);

    luna::RHI::BackendType getBackend() const;
    const editor::EditorEnginePaths& enginePaths() const noexcept;
    editor::EditorSettingsStore& editorSettings() noexcept;
    const editor::EditorSettingsStore& editorSettings() const noexcept;

protected:
    Renderer::InitializationOptions getRendererInitializationOptions() override;
    void onInit() override;

private:
    LunaEditorApplication(luna::RHI::BackendType backend,
                          editor::EditorEnginePaths engine_paths,
                          editor::EditorSettingsStore editor_settings);

    luna::RHI::BackendType m_backend{luna::RHI::BackendType::Auto};
    editor::EditorEnginePaths m_engine_paths;
    editor::EditorSettingsStore m_editor_settings;
};

Application* createApplication(int argc, char** argv);

} // namespace luna
