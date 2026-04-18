#pragma once

#include "Renderer/Camera.h"
#include "Renderer/Material.h"
#include "Renderer/RenderGraphBuilder.h"

#include <CommandBufferEncoder.h>
#include <Core.h>
#include <filesystem>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <Instance.h>
#include <memory>
#include <unordered_map>
#include <vector>

namespace luna::RHI {
class Buffer;
class DescriptorPool;
class DescriptorSet;
class DescriptorSetLayout;
class Device;
class GraphicsPipeline;
class PipelineLayout;
class Sampler;
class ShaderCompiler;
class ShaderModule;
class Texture;
} // namespace luna::RHI

namespace luna {

class Mesh;

class SceneRenderer {
public:
    struct ShaderPaths {
        std::filesystem::path geometry_vertex_path;
        std::filesystem::path geometry_fragment_path;
        std::filesystem::path lighting_vertex_path;
        std::filesystem::path lighting_fragment_path;

        [[nodiscard]] bool isComplete() const
        {
            return !geometry_vertex_path.empty() && !geometry_fragment_path.empty();
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

    SceneRenderer() = default;
    ~SceneRenderer();

    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;

    void beginScene(const Camera& camera);
    void submitStaticMesh(const glm::mat4& transform,
                          std::shared_ptr<Mesh> mesh,
                          std::shared_ptr<Material> material = {});
    void buildRenderGraph(rhi::RenderGraphBuilder& graph, const RenderContext& context);
    void clearSubmittedMeshes();
    void setShaderPaths(ShaderPaths shader_paths);
    void shutdown();

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

    struct UploadedTexture {
        luna::RHI::Ref<luna::RHI::Texture> texture;
        luna::RHI::Ref<luna::RHI::Buffer> staging_buffer;
        std::vector<luna::RHI::BufferImageCopy> copy_regions;
        bool uploaded{false};
    };

    struct UploadedMaterial {
        UploadedTexture base_color_texture;
        UploadedTexture normal_texture;
        UploadedTexture metallic_roughness_texture;
        UploadedTexture emissive_texture;
        UploadedTexture occlusion_texture;
        luna::RHI::Ref<luna::RHI::Buffer> params_buffer;
        luna::RHI::Ref<luna::RHI::DescriptorSet> descriptor_set;
    };

    struct DrawQueueState {
        Camera camera{};
        std::vector<StaticMeshDrawCommand> opaque_draw_commands;
        std::vector<StaticMeshDrawCommand> transparent_draw_commands;
    };

    struct UploadCacheState {
        std::unordered_map<const Mesh*, UploadedMesh> uploaded_meshes;
        std::unordered_map<const Material*, UploadedMaterial> uploaded_materials;
    };

    struct GpuState {
        luna::RHI::Ref<luna::RHI::Device> device;

        luna::RHI::Ref<luna::RHI::GraphicsPipeline> geometry_pipeline;
        luna::RHI::Ref<luna::RHI::GraphicsPipeline> lighting_pipeline;
        luna::RHI::Ref<luna::RHI::GraphicsPipeline> transparent_pipeline;

        luna::RHI::Ref<luna::RHI::PipelineLayout> geometry_pipeline_layout;
        luna::RHI::Ref<luna::RHI::PipelineLayout> lighting_pipeline_layout;
        luna::RHI::Ref<luna::RHI::PipelineLayout> transparent_pipeline_layout;

        luna::RHI::Ref<luna::RHI::DescriptorSetLayout> material_layout;
        luna::RHI::Ref<luna::RHI::DescriptorSetLayout> gbuffer_layout;
        luna::RHI::Ref<luna::RHI::DescriptorSetLayout> scene_layout;

        luna::RHI::Ref<luna::RHI::DescriptorPool> descriptor_pool;

        luna::RHI::Ref<luna::RHI::Sampler> material_sampler;
        luna::RHI::Ref<luna::RHI::Sampler> gbuffer_sampler;
        luna::RHI::Ref<luna::RHI::Sampler> environment_sampler;

        luna::RHI::Ref<luna::RHI::DescriptorSet> gbuffer_descriptor_set;
        luna::RHI::Ref<luna::RHI::DescriptorSet> scene_descriptor_set;
        luna::RHI::Ref<luna::RHI::Buffer> scene_params_buffer;
        UploadedTexture environment_texture;

        luna::RHI::Ref<luna::RHI::ShaderModule> geometry_vertex_shader;
        luna::RHI::Ref<luna::RHI::ShaderModule> geometry_fragment_shader;
        luna::RHI::Ref<luna::RHI::ShaderModule> lighting_vertex_shader;
        luna::RHI::Ref<luna::RHI::ShaderModule> lighting_fragment_shader;
        luna::RHI::Ref<luna::RHI::ShaderModule> transparent_fragment_shader;

        luna::RHI::Format surface_format{luna::RHI::Format::UNDEFINED};
    };

    static ShaderPaths getDefaultShaderPaths();
    static rhi::ImageData createFallbackImageData(const glm::vec4& albedo_color);
    static std::filesystem::path getDefaultEnvironmentPath();

    const Material& resolveMaterial(const std::shared_ptr<Material>& material) const;
    void resetPipelineState();
    void ensurePipelines(const RenderContext& context);
    ShaderPaths resolveShaderPaths() const;
    UploadedMesh& getOrCreateUploadedMesh(const Mesh& mesh);
    UploadedMaterial& getOrCreateUploadedMaterial(const Material& material);
    UploadedTexture createUploadedTexture(const rhi::ImageData& image, const std::string& debug_name) const;
    void uploadTextureIfNeeded(luna::RHI::CommandBufferEncoder& commands, UploadedTexture& uploaded_texture);
    void uploadMaterialIfNeeded(luna::RHI::CommandBufferEncoder& commands, UploadedMaterial& uploaded_material);
    void ensureSceneResources();
    void updateSceneParameters(const RenderContext& context);
    void executeGeometryPass(rhi::RenderGraphRasterPassContext& pass_context, const RenderContext& context);
    void executeLightingPass(rhi::RenderGraphRasterPassContext& pass_context,
                             const RenderContext& context,
                             rhi::RenderGraphTextureHandle gbuffer_base_color_handle,
                             rhi::RenderGraphTextureHandle gbuffer_normal_metallic_handle,
                             rhi::RenderGraphTextureHandle gbuffer_world_position_roughness_handle,
                             rhi::RenderGraphTextureHandle gbuffer_emissive_ao_handle);
    void executeTransparentPass(rhi::RenderGraphRasterPassContext& pass_context, const RenderContext& context);
    void sortTransparentDrawCommands();

private:
    DrawQueueState m_draw_queue{};
    UploadCacheState m_upload_cache{};
    Material m_default_material;
    ShaderPaths m_shader_paths{};
    GpuState m_gpu{};
};

} // namespace luna
