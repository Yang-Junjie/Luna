#pragma once

#include "Renderer/Camera.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/RenderGraph.h"
#include "Vulkan/Buffer.h"
#include "Vulkan/Image.h"
#include "Vulkan/Sampler.h"

#include <filesystem>
#include <glm/mat4x4.hpp>

#include <memory>
#include <unordered_map>
#include <vector>

namespace luna::val {
class GraphicShader;
class CommandBuffer;
} // namespace luna::val

namespace luna {

class SceneGeometryPass;
class SceneLightingPass;

class SceneRenderer {
public:
    struct ShaderPaths {
        std::filesystem::path geometry_vertex_path;
        std::filesystem::path geometry_fragment_path;
        std::filesystem::path lighting_vertex_path;
        std::filesystem::path lighting_fragment_path;

        bool isComplete() const
        {
            return !geometry_vertex_path.empty() && !geometry_fragment_path.empty() && !lighting_vertex_path.empty() &&
                   !lighting_fragment_path.empty();
        }
    };

    SceneRenderer() = default;
    ~SceneRenderer();

    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;

    void shutdown();

    void beginScene(const Camera& camera);
    void clearSubmittedMeshes();
    void setShaderPaths(ShaderPaths shader_paths);
    void submitStaticMesh(const glm::mat4& transform,
                          std::shared_ptr<Mesh> mesh,
                          std::shared_ptr<Material> material = {});

    std::unique_ptr<val::RenderGraph>
        buildRenderGraph(val::Format surface_format, uint32_t framebuffer_width, uint32_t framebuffer_height, bool include_imgui_pass);

private:
    struct StaticMeshDrawCommand {
        glm::mat4 transform{1.0f};
        std::shared_ptr<Mesh> mesh;
        std::shared_ptr<Material> material;
    };

    struct UploadedMesh {
        val::Buffer vertex_buffer;
        val::Buffer index_buffer;
        uint32_t index_count{0};
    };

    struct UploadedMaterial {
        val::Image albedo_image;
        val::Buffer staging_buffer;
        val::Sampler albedo_sampler;
        bool uploaded{false};
    };

    static ShaderPaths getDefaultShaderPaths();
    static val::ImageData createFallbackImageData(const glm::vec4& albedo_color);

    void ensureCoreResources();
    ShaderPaths resolveShaderPaths() const;
    UploadedMesh& getOrCreateUploadedMesh(const Mesh& mesh);
    UploadedMaterial& getOrCreateUploadedMaterial(const Material& material);
    void uploadMaterialIfNeeded(UploadedMaterial& uploaded_material, val::CommandBuffer& commands);

private:
    Camera m_camera{};
    std::vector<StaticMeshDrawCommand> m_submitted_meshes;
    std::unordered_map<const Mesh*, UploadedMesh> m_uploaded_meshes;
    std::unordered_map<const Material*, UploadedMaterial> m_uploaded_materials;
    Material m_default_material;
    std::shared_ptr<val::GraphicShader> m_geometry_shader;
    std::shared_ptr<val::GraphicShader> m_lighting_shader;
    val::Sampler m_gbuffer_sampler;
    ShaderPaths m_shader_paths{};
    bool m_core_resources_initialized{false};

    friend class SceneGeometryPass;
    friend class SceneLightingPass;
};

} // namespace luna
