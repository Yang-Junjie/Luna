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

class LunaRuntimeApplication final : public Application {
public:
    explicit LunaRuntimeApplication(luna::RHI::BackendType backend);

protected:
    Renderer::InitializationOptions getRendererInitializationOptions() override;
    void onInit() override;
    void onUpdate(Timestep timestep) override;

private:
    void resetCamera();
    void buildScene();
    bool tryLoadDefaultAsset();
    void createFallbackAsset();
    void updateDemoTransform(float delta_time);

private:
    Scene m_scene;
    Entity m_demo_entity;
    std::shared_ptr<Mesh> m_demo_mesh;
    std::shared_ptr<Material> m_demo_material;
    AssetHandle m_demo_mesh_handle{0};
    AssetHandle m_demo_material_handle{0};
    bool m_demo_mesh_memory_only{true};
    MemoryMeshType m_demo_memory_mesh_type{MemoryMeshType::None};
    std::string m_asset_label{"Procedural cube"};
    luna::RHI::BackendType m_backend{luna::RHI::BackendType::Vulkan};
    float m_spin_speed{0.85f};
    bool m_auto_rotate{true};
};

Application* createApplication(int argc, char** argv);

} // namespace luna
