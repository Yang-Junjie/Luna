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
    bool isAutoRotateEnabled() const;
    void setAutoRotateEnabled(bool enabled);
    float getSpinSpeed() const;
    void setSpinSpeed(float speed);
    void resetCamera();

protected:
    Renderer::InitializationOptions getRendererInitializationOptions() override;
    void onInit() override;
    void onUpdate(Timestep timestep) override;

private:
    void buildScene();
    bool tryLoadDefaultAsset();
    void createFallbackAsset();
    void updateDemoTransform(float delta_time);

private:
    Scene m_scene;
    Entity m_demo_entity;
    std::shared_ptr<Mesh> m_demo_mesh;
    std::shared_ptr<Material> m_demo_material;
    std::string m_asset_label{"Procedural cube"};
    luna::RHI::BackendType m_backend{luna::RHI::BackendType::Vulkan};
    float m_spin_speed{0.85f};
    bool m_auto_rotate{true};
};

Application* createApplication(int argc, char** argv);

} // namespace luna
