#pragma once

#include "Core/Application.h"

#include <Instance.h>

#include <memory>

namespace luna::render_flow {
struct ScreenSpaceAmbientOcclusionOptions;
}

namespace luna {

class LunaEditorApplication final : public Application {
public:
    explicit LunaEditorApplication(luna::RHI::BackendType backend);

    luna::RHI::BackendType getBackend() const;
    [[nodiscard]] bool isScreenSpaceAmbientOcclusionEnabled() const;
    void setScreenSpaceAmbientOcclusionEnabled(bool enabled);

protected:
    Renderer::InitializationOptions getRendererInitializationOptions() override;
    void onInit() override;

private:
    luna::RHI::BackendType m_backend{luna::RHI::BackendType::Vulkan};
    std::shared_ptr<render_flow::ScreenSpaceAmbientOcclusionOptions> m_ssao_options;
};

Application* createApplication(int argc, char** argv);

} // namespace luna
