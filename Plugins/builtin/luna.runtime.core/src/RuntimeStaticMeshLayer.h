#pragma once

#include "Core/Application.h"
#include "Core/Layer.h"
#include "Core/Log.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/SceneRenderer.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

#include <filesystem>
#include <memory>
#include <vector>

namespace luna::runtime {

class RuntimeStaticMeshLayer final : public Layer {
public:
    RuntimeStaticMeshLayer()
        : Layer("RuntimeStaticMeshLayer")
    {}

    void onAttach() override
    {
        configureRenderer();
        createScene();
        LUNA_RUNTIME_INFO("Runtime static-mesh layer attached");
    }

    void onDetach() override
    {
        m_scene.reset();
        m_mesh_entity = {};
    }

    void onUpdate(Timestep dt) override
    {
        m_elapsed_seconds += dt.getSeconds();

        if (m_mesh_entity) {
            auto& transform_component = m_mesh_entity.getComponent<TransformComponent>();
            transform_component.rotation.x = 0.35f;
            transform_component.rotation.y += dt.getSeconds() * 0.9f;
        }

        if (m_scene != nullptr) {
            m_scene->onUpdateRuntime();
        }
    }

private:
    static std::filesystem::path projectRoot()
    {
        return std::filesystem::path(LUNA_PROJECT_ROOT);
    }

    static SceneRenderer::ShaderPaths getPluginShaderPaths()
    {
        const std::filesystem::path shader_root = projectRoot() / "Plugins" / "builtin" / "luna.runtime.core" / "Shaders";
        return SceneRenderer::ShaderPaths{
            .geometry_vertex_path = shader_root / "RuntimeSceneGeometry.vert",
            .geometry_fragment_path = shader_root / "RuntimeSceneGeometry.frag",
            .lighting_vertex_path = shader_root / "RuntimeSceneLighting.vert",
            .lighting_fragment_path = shader_root / "RuntimeSceneLighting.frag",
        };
    }

    static std::shared_ptr<Mesh> createCubeMesh()
    {
        using Vertex = StaticMeshVertex;

        const std::vector<Vertex> vertices = {
            {{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
            {{-1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},

            {{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
            {{-1.0f, -1.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
            {{-1.0f, 1.0f, -1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
            {{1.0f, 1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},

            {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
            {{-1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
            {{-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
            {{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},

            {{1.0f, -1.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{1.0f, -1.0f, -1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{1.0f, 1.0f, -1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
            {{1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},

            {{-1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{1.0f, 1.0f, -1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
            {{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},

            {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
            {{1.0f, -1.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
            {{1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
            {{-1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
        };

        const std::vector<uint32_t> indices = {
            0, 1, 2, 0, 2, 3,
            4, 5, 6, 4, 6, 7,
            8, 9, 10, 8, 10, 11,
            12, 13, 14, 12, 14, 15,
            16, 17, 18, 16, 18, 19,
            20, 21, 22, 20, 22, 23,
        };

        return Mesh::create("runtime_cube", vertices, indices);
    }

    void configureRenderer()
    {
        auto& renderer = Application::get().getRenderer();
        renderer.getSceneRenderer().setShaderPaths(getPluginShaderPaths());
        renderer.requestRenderGraphRebuild();

        auto& camera = renderer.getMainCamera();
        camera.m_position = glm::vec3(0.0f, 0.0f, 4.5f);
        camera.m_pitch = 0.0f;
        camera.m_yaw = 0.0f;
    }

    void createScene()
    {
        m_scene = std::make_unique<Scene>();

        m_mesh_entity = m_scene->createEntity("runtime_cube");
        auto& transform_component = m_mesh_entity.getComponent<TransformComponent>();
        transform_component.scale = glm::vec3(0.85f, 0.85f, 0.85f);
        transform_component.rotation = glm::vec3(0.35f, 0.25f, 0.0f);

        const auto material = Material::create("runtime_cube_material", {}, glm::vec4(0.85f, 0.65f, 0.30f, 1.0f));
        m_mesh_entity.addComponent<StaticMeshComponent>(createCubeMesh(), material);
    }

private:
    std::unique_ptr<Scene> m_scene;
    Entity m_mesh_entity;
    float m_elapsed_seconds = 0.0f;
};

} // namespace luna::runtime
