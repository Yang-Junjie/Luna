#pragma once

#include "ContentBrowserPanel.h"
#include "Core/Layer.h"
#include "InspectorPanel.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "SceneHierarchyPanel.h"

#include <filesystem>
#include <string>

namespace luna {

class LunaEditorApplication;

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
    bool openSceneFile(const std::filesystem::path& scene_file_path);
    Entity createEntityFromMeshAsset(AssetHandle mesh_handle, Entity parent = {});
    void applyMeshAssetToEntity(Entity entity, AssetHandle mesh_handle);

private:
    void onImGuiMenuBar();
    void drawViewport();
    void resetEditorState();
    void createScene();
    bool syncProjectAssets();
    bool openProject(const std::filesystem::path& project_file_path);
    bool openScene();
    bool openScene(const std::filesystem::path& scene_file_path, bool update_project_start_scene);
    bool saveScene();
    bool saveSceneAs();
    bool saveSceneAs(const std::filesystem::path& scene_file_path);
    std::filesystem::path sceneDialogDefaultPath() const;
    void updateSceneLabel();
    void syncProjectStartScene(const std::filesystem::path& scene_file_path);
    bool hasProjectLoaded() const;

private:
    LunaEditorApplication* m_application{nullptr};
    Scene m_scene;
    Entity m_selected_entity;
    std::filesystem::path m_scene_file_path;
    std::string m_asset_label{"No scene loaded"};
    SceneHierarchyPanel m_scene_hierarchy_panel;
    InspectorPanel m_inspector_panel;
    ContentBrowserPanel m_content_browser_panel;
};

} // namespace luna
