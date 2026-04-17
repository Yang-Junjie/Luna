#pragma once

#include "Renderer/Camera.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"

#include <Core.h>
#include <filesystem>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <unordered_map>
#include <vector>

namespace luna::RHI {
class Buffer;
class CommandBufferEncoder;
class DescriptorPool;
class DescriptorSet;
class DescriptorSetLayout;
class Device;
class GraphicsPipeline;
class PipelineLayout;
class Sampler;
class ShaderModule;
class Texture;
} // namespace luna::RHI

namespace luna {

class SceneRenderer {
public:
    struct ShaderPaths {
        std::filesystem::path geometry_vertex_path;
        std::filesystem::path geometry_fragment_path;
        std::filesystem::path lighting_vertex_path;
        std::filesystem::path lighting_fragment_path;

        bool isComplete() const
        {
            return !geometry_vertex_path.empty() && !geometry_fragment_path.empty();
        }
    };

    struct RenderContext {
        luna::RHI::Ref<luna::RHI::Device> device;
        luna::RHI::Ref<luna::RHI::CommandBufferEncoder> command_buffer;
        luna::RHI::Ref<luna::RHI::Texture> color_target;
        luna::RHI::Format color_format{luna::RHI::Format::UNDEFINED};
        glm::vec4 clear_color{0.10f, 0.10f, 0.12f, 1.0f};
        uint32_t framebuffer_width{0};
        uint32_t framebuffer_height{0};
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
    void render(const RenderContext& context);

private:
    struct StaticMeshDrawCommand {
        glm::mat4 transform{1.0f};
        std::shared_ptr<Mesh> mesh;
        std::shared_ptr<Material> material;
    };

    struct UploadedMesh {
        luna::RHI::Ref<luna::RHI::Buffer> vertex_buffer;
        luna::RHI::Ref<luna::RHI::Buffer> index_buffer;
        uint32_t index_count{0};
    };

    struct UploadedMaterial {
        luna::RHI::Ref<luna::RHI::Texture> albedo_texture;
        luna::RHI::Ref<luna::RHI::Buffer> staging_buffer;
        luna::RHI::Ref<luna::RHI::DescriptorSet> descriptor_set;
        bool uploaded{false};
    };

    static ShaderPaths getDefaultShaderPaths();
    static rhi::ImageData createFallbackImageData(const glm::vec4& albedo_color);

    void ensurePipeline(const RenderContext& context);
    void ensureFrameResources(const RenderContext& context);
    void createOrResizeDepthTexture(uint32_t width, uint32_t height);
    ShaderPaths resolveShaderPaths() const;
    UploadedMesh& getOrCreateUploadedMesh(const Mesh& mesh);
    UploadedMaterial& getOrCreateUploadedMaterial(const Material& material);
    void uploadMaterialIfNeeded(luna::RHI::CommandBufferEncoder& commands, UploadedMaterial& uploaded_material);

private:
    Camera m_camera{};
    std::vector<StaticMeshDrawCommand> m_submitted_meshes;
    std::unordered_map<const Mesh*, UploadedMesh> m_uploaded_meshes;
    std::unordered_map<const Material*, UploadedMaterial> m_uploaded_materials;
    Material m_default_material;
    ShaderPaths m_shader_paths{};
    luna::RHI::Ref<luna::RHI::Device> m_device;
    luna::RHI::Ref<luna::RHI::GraphicsPipeline> m_pipeline;
    luna::RHI::Ref<luna::RHI::PipelineLayout> m_pipeline_layout;
    luna::RHI::Ref<luna::RHI::DescriptorSetLayout> m_material_layout;
    luna::RHI::Ref<luna::RHI::DescriptorPool> m_descriptor_pool;
    luna::RHI::Ref<luna::RHI::Sampler> m_material_sampler;
    luna::RHI::Ref<luna::RHI::ShaderModule> m_vertex_shader;
    luna::RHI::Ref<luna::RHI::ShaderModule> m_fragment_shader;
    luna::RHI::Ref<luna::RHI::Texture> m_depth_texture;
    luna::RHI::Format m_surface_format{luna::RHI::Format::UNDEFINED};
    uint32_t m_framebuffer_width{0};
    uint32_t m_framebuffer_height{0};
    bool m_depth_texture_initialized{false};
};

} // namespace luna
