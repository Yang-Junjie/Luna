#pragma once

#include "Asset/Asset.h"
#include "Core/Layer.h"
#include "InspectorPanel.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "SceneHierarchyPanel.h"

#include <filesystem>
#include <memory>
#include <string>

namespace luna {

class LunaEditorApplication;
class Material;
class Mesh;

class LunaEditorLayer final : public Layer {
public:
    explicit LunaEditorLayer(LunaEditorApplication& application);

    void onAttach() override;
    void onDetach() override;
    void onUpdate(Timestep dt) override;
    void onImGuiRender() override;

    const std::string& getAssetLabel() const;
    Scene& getScene();
    Entity getSelectedEntity() const;
    void setSelectedEntity(Entity entity);

private:
    void onImGuiMenuBar();
    void drawViewport();
    void resetEditorState();
    bool openProject(const std::filesystem::path& project_file_path);
    void buildScene();
    bool tryLoadDefaultAsset();

private:
    LunaEditorApplication* m_application{nullptr};
    Scene m_scene;
    Entity m_selected_entity;
    std::shared_ptr<Mesh> m_demo_mesh;
    std::shared_ptr<Material> m_demo_material;
    AssetHandle m_demo_mesh_handle{0};
    AssetHandle m_demo_material_handle{0};
    std::string m_asset_label{"No project loaded"};
    SceneHierarchyPanel m_scene_hierarchy_panel;
    InspectorPanel m_inspector_panel;
};

} // namespace luna
