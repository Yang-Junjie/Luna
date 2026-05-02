#pragma once

#include "Core/Application.h"

#include <Instance.h>

namespace luna {

class LunaEditorApplication final : public Application {
public:
    explicit LunaEditorApplication(luna::RHI::BackendType backend);

    luna::RHI::BackendType getBackend() const;

protected:
    Renderer::InitializationOptions getRendererInitializationOptions() override;
    void onInit() override;

private:
    luna::RHI::BackendType m_backend{luna::RHI::BackendType::Auto};
};

Application* createApplication(int argc, char** argv);

} // namespace luna
