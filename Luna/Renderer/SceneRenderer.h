#pragma once

#include "Renderer/Camera.h"
#include "Renderer/RenderGraphBuilder.h"

#include <Core.h>
#include <Instance.h>
#include <filesystem>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <vector>

namespace luna::RHI {
class Device;
class ShaderCompiler;
} // namespace luna::RHI

namespace luna {

class Mesh;
class Material;

class SceneRenderer {
public:
    struct ShaderPaths {
        std::filesystem::path geometry_vertex_path;
        std::filesystem::path geometry_fragment_path;
        std::filesystem::path lighting_vertex_path;
        std::filesystem::path lighting_fragment_path;

        [[nodiscard]] bool isComplete() const
        {
            return !geometry_vertex_path.empty() && !geometry_fragment_path.empty() &&
                   !lighting_vertex_path.empty() && !lighting_fragment_path.empty();
        }
    };

    struct RenderContext {
        luna::RHI::Ref<luna::RHI::Device> device;
        luna::RHI::Ref<luna::RHI::ShaderCompiler> compiler;
        luna::RHI::BackendType backend_type{luna::RHI::BackendType::Vulkan};
        rhi::RenderGraphTextureHandle color_target;
        rhi::RenderGraphTextureHandle depth_target;
        luna::RHI::Format color_format{luna::RHI::Format::UNDEFINED};
        glm::vec4 clear_color{0.10f, 0.10f, 0.12f, 1.0f};
        uint32_t framebuffer_width{0};
        uint32_t framebuffer_height{0};

        [[nodiscard]] bool isValid() const
        {
            return device && color_target.isValid() && depth_target.isValid() && framebuffer_width > 0 &&
                   framebuffer_height > 0;
        }
    };

    SceneRenderer();
    ~SceneRenderer();

    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;

    void beginScene(const Camera& camera);
    void submitStaticMesh(const glm::mat4& transform,
                          std::shared_ptr<Mesh> mesh,
                          std::shared_ptr<Material> material = {});
    void submitStaticMesh(const glm::mat4& transform,
                          std::shared_ptr<Mesh> mesh,
                          const std::vector<std::shared_ptr<Material>>& submesh_materials);
    void buildRenderGraph(rhi::RenderGraphBuilder& graph, const RenderContext& context);
    void clearSubmittedMeshes();
    void setShaderPaths(ShaderPaths shader_paths);
    void shutdown();

private:
    class Implementation;
    std::unique_ptr<Implementation> m_impl;
};

} // namespace luna
