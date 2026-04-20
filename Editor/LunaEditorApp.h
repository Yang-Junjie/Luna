#pragma once

#include "Core/Application.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

#include <Instance.h>
#include <memory>
#include <string>

namespace luna {

class Material;
class Mesh;

class LunaEditorApplication final : public Application {
public:
    explicit LunaEditorApplication(luna::RHI::BackendType backend);

    const std::string& getAssetLabel() const;
    luna::RHI::BackendType getBackend() const;
    Scene& getScene();
    const Scene& getScene() const;
    Entity getSelectedEntity() const;
    void setSelectedEntity(Entity entity);

protected:
    Renderer::InitializationOptions getRendererInitializationOptions() override;
    void onInit() override;
    void onUpdate(Timestep timestep) override;

private:
    void buildScene();
    bool tryLoadDefaultAsset();
    void createFallbackAsset();

private:
    Scene m_scene;
    Entity m_demo_entity;
    Entity m_selected_entity;
    std::shared_ptr<Mesh> m_demo_mesh;
    std::shared_ptr<Material> m_demo_material;
    AssetHandle m_demo_mesh_handle{0};
    AssetHandle m_demo_material_handle{0};
    bool m_demo_mesh_memory_only{true};
    MemoryMeshType m_demo_memory_mesh_type{MemoryMeshType::None};
    std::string m_asset_label{"Procedural cube"};
    luna::RHI::BackendType m_backend{luna::RHI::BackendType::Vulkan};
};

Application* createApplication(int argc, char** argv);

} // namespace luna
