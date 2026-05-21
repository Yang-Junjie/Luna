#pragma once

#include "EditorApi/EditorApi.h"

#include <optional>
#include <string>

namespace luna::editor {

class MaterialEditorPlugin final : public Plugin {
public:
    [[nodiscard]] PluginDescriptor descriptor() const override;
    bool onLoad(Host& host) override;
    void onUnload(Host& host) override;

private:
    void open(Host& host);
    void drawToolsWindow(WindowDrawContext& context);
    void drawPreviewWindow(WindowDrawContext& context);
    void selectMaterial(Host& host, AssetHandle handle);
    std::optional<MaterialDocument> readSelectedMaterial(Host& host);
    void drawToolbar(Host& host, Ui& ui, const std::optional<MaterialDocument>& document);
    void drawNewMaterialPopup(Host& host, Ui& ui);
    void drawPreviewControls(Host& host, Ui& ui);
    void drawPreviewViewport(Host& host, Ui& ui, const MaterialDocument& document);
    void resetPreviewCamera();
    void drawMaterialBody(Host& host, Ui& ui, MaterialDocument document);
    void applyTextureChanges(Host& host, const MaterialTextureSet& textures);
    void applySurfaceChanges(Host& host, const MaterialSurfaceProperties& surface);

    AssetHandle m_selected_material{0};
    ViewportId m_preview_viewport{kInvalidViewportId};
    AssetHandle m_preview_environment_map{0};
    int m_preview_background{0};
    float m_preview_sky_intensity{1.0f};
    float m_preview_light_intensity{1.0f};
    OrbitCameraController m_preview_camera;
    std::string m_status;
    std::string m_new_material_name{"New Material"};
    std::string m_new_material_path{"Assets/Materials/New Material.lunamat"};
    int m_new_material_template{0};
};

std::unique_ptr<Plugin> createMaterialEditorPlugin();

} // namespace luna::editor
