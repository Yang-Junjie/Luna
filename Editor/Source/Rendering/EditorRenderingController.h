#pragma once

#include "EditorApi/EditorRenderingService.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace luna {

class Renderer;

class EditorRenderingController final {
public:
    EditorRenderingController() = default;
    explicit EditorRenderingController(Renderer& renderer) noexcept;

    void bindRenderer(Renderer* renderer) noexcept;

    [[nodiscard]] std::string backendName() const;
    [[nodiscard]] editor::RenderingBackendCapabilities backendCapabilities() const;
    [[nodiscard]] editor::RenderGraphProfileSnapshot renderGraphProfile() const;
    [[nodiscard]] bool isRenderGraphProfilingEnabled() const noexcept;
    void setRenderGraphProfilingEnabled(bool enabled);
    [[nodiscard]] std::filesystem::path defaultRenderProfileExportPath(std::string_view backend_name = {}) const;
    bool exportRenderGraphProfileChromeTraceJson(const editor::RenderGraphProfileSnapshot& profile,
                                                 const std::filesystem::path& output_path,
                                                 std::string* error_message = nullptr) const;
    [[nodiscard]] std::vector<editor::RenderFeatureInfo> defaultRenderFeatureInfos() const;
    [[nodiscard]] std::vector<editor::RenderFeatureParameterInfo>
        defaultRenderFeatureParameters(std::string_view feature_name) const;
    bool setDefaultRenderFeatureEnabled(std::string_view feature_name, bool enabled);
    bool setDefaultRenderFeatureParameter(std::string_view feature_name,
                                          std::string_view parameter_name,
                                          const editor::RenderFeatureParameterValue& value);
    [[nodiscard]] std::vector<editor::RenderDebugViewModeInfo> renderDebugViewModes() const;
    [[nodiscard]] editor::RenderDebugViewMode renderDebugViewMode() const noexcept;
    void setRenderDebugViewMode(editor::RenderDebugViewMode mode);
    [[nodiscard]] float renderDebugVelocityScale() const noexcept;
    void setRenderDebugVelocityScale(float scale);
    [[nodiscard]] editor::TextureView renderDebugTextureView() const;
    [[nodiscard]] float frameTimeMilliseconds() const noexcept;
    [[nodiscard]] float framesPerSecond() const noexcept;
    [[nodiscard]] editor::UVec2 sceneOutputSize() const noexcept;

private:
    Renderer* m_renderer{nullptr};
};

} // namespace luna
