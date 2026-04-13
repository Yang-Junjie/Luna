#include "Renderer/SceneRenderer.h"

#include "Core/Log.h"
#include "Imgui/ImGuiRenderPass.h"
#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/ShaderLoader.h"
#include "Vulkan/CommandBuffer.h"
#include "Vulkan/GraphicShader.h"
#include "Vulkan/VulkanContext.h"

#include <filesystem>
#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

struct MeshPushConstants {
    glm::mat4 model{1.0f};
    glm::mat4 view_projection{1.0f};
};

std::filesystem::path projectRoot()
{
    return std::filesystem::path(LUNA_PROJECT_ROOT);
}

std::shared_ptr<luna::val::GraphicShader> loadGraphicsShader(const std::filesystem::path& vertex_path,
                                                             const std::filesystem::path& fragment_path)
{
    const auto vertex_shader_data = luna::val::ShaderLoader::LoadFromSourceFile(
        vertex_path.string(), luna::val::ShaderType::VERTEX, luna::val::ShaderLanguage::GLSL);
    const auto fragment_shader_data = luna::val::ShaderLoader::LoadFromSourceFile(
        fragment_path.string(), luna::val::ShaderType::FRAGMENT, luna::val::ShaderLanguage::GLSL);

    if (vertex_shader_data.Bytecode.empty() || fragment_shader_data.Bytecode.empty()) {
        return {};
    }

    auto shader = std::make_shared<luna::val::GraphicShader>();
    shader->Init(vertex_shader_data, fragment_shader_data);
    return shader;
}

glm::mat4 buildViewProjection(const Camera& camera, float aspect_ratio)
{
    const float clamped_aspect_ratio = std::max(aspect_ratio, 0.001f);
    const glm::mat4 projection = glm::perspective(glm::radians(50.0f), clamped_aspect_ratio, 0.05f, 200.0f);
    return projection * camera.getViewMatrix();
}

void updateCombinedImageSamplerDescriptor(const vk::DescriptorSet& descriptor_set,
                                          uint32_t binding,
                                          const luna::val::Image& image,
                                          const luna::val::Sampler& sampler)
{
    const vk::DescriptorImageInfo descriptor_image_info{
        sampler.GetNativeHandle(),
        image.GetNativeView(luna::val::ImageView::NATIVE),
        luna::val::ImageUsageToImageLayout(luna::val::ImageUsage::SHADER_READ),
    };

    vk::WriteDescriptorSet write_descriptor_set;
    write_descriptor_set.setDstSet(descriptor_set)
        .setDstBinding(binding)
        .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
        .setDescriptorCount(1)
        .setPImageInfo(&descriptor_image_info);

    luna::val::GetCurrentVulkanContext().GetDevice().updateDescriptorSets(write_descriptor_set, {});
}

} // namespace

namespace luna {

class SceneGeometryPass final : public val::RenderPass {
public:
    SceneGeometryPass(SceneRenderer& scene_renderer, val::Format surface_format, uint32_t width, uint32_t height)
        : m_scene_renderer(scene_renderer)
        , m_surface_format(surface_format)
        , m_width(width)
        , m_height(height)
    {}

    void SetupPipeline(val::PipelineState pipeline) override
    {
        pipeline.Shader = m_scene_renderer.m_geometry_shader;
        pipeline.VertexBindings = {
            {val::VertexBinding::Rate::PER_VERTEX, val::VertexBinding::BindingRangeAll},
        };
        pipeline.DeclareAttachment("gbuffer_albedo", val::Format::R8G8B8A8_UNORM, m_width, m_height);
        pipeline.DeclareAttachment("gbuffer_normal", val::Format::R16G16B16A16_SFLOAT, m_width, m_height);
        pipeline.DeclareAttachment("gbuffer_depth", val::Format::D32_SFLOAT, m_width, m_height);
        pipeline.AddOutputAttachment("gbuffer_albedo", val::ClearColor{0.0f, 0.0f, 0.0f, 0.0f});
        pipeline.AddOutputAttachment("gbuffer_normal", val::ClearColor{0.5f, 0.5f, 1.0f, 0.0f});
        pipeline.AddOutputAttachment("gbuffer_depth", val::ClearDepthStencil{1.0f, 0});
    }

    void BeforeRender(val::RenderPassState state) override
    {
        m_scene_renderer.getOrCreateUploadedMaterial(m_scene_renderer.m_default_material);
        for (const auto& draw_command : m_scene_renderer.m_submitted_meshes) {
            if (!draw_command.mesh || !draw_command.mesh->isValid()) {
                continue;
            }

            (void) m_scene_renderer.getOrCreateUploadedMesh(*draw_command.mesh);

            const Material& material =
                draw_command.material != nullptr ? *draw_command.material : m_scene_renderer.m_default_material;
            auto& uploaded_material = m_scene_renderer.getOrCreateUploadedMaterial(material);
            m_scene_renderer.uploadMaterialIfNeeded(uploaded_material, state.Commands);
        }
    }

    void OnRender(val::RenderPassState state) override
    {
        const auto& gbuffer_albedo = state.GetAttachment("gbuffer_albedo");
        const float aspect_ratio =
            static_cast<float>(gbuffer_albedo.GetWidth()) / static_cast<float>(gbuffer_albedo.GetHeight());
        const glm::mat4 view_projection = buildViewProjection(m_scene_renderer.m_camera, aspect_ratio);

        state.Commands.SetRenderArea(gbuffer_albedo);

        for (const auto& draw_command : m_scene_renderer.m_submitted_meshes) {
            if (!draw_command.mesh || !draw_command.mesh->isValid()) {
                continue;
            }

            const auto uploaded_mesh_it = m_scene_renderer.m_uploaded_meshes.find(draw_command.mesh.get());
            if (uploaded_mesh_it == m_scene_renderer.m_uploaded_meshes.end()) {
                continue;
            }

            const Material& material =
                draw_command.material != nullptr ? *draw_command.material : m_scene_renderer.m_default_material;
            const auto uploaded_material_it = m_scene_renderer.m_uploaded_materials.find(&material);
            if (uploaded_material_it == m_scene_renderer.m_uploaded_materials.end()) {
                continue;
            }

            const auto& uploaded_mesh = uploaded_mesh_it->second;
            const auto& uploaded_material = uploaded_material_it->second;

            MeshPushConstants push_constants;
            push_constants.model = draw_command.transform;
            push_constants.view_projection = view_projection;

            updateCombinedImageSamplerDescriptor(
                state.Pass.DescriptorSet, 0, uploaded_material.albedo_image, uploaded_material.albedo_sampler);

            state.Commands.BindVertexBuffers(uploaded_mesh.vertex_buffer);
            state.Commands.BindIndexBufferUInt32(uploaded_mesh.index_buffer);
            state.Commands.PushConstants(state.Pass, &push_constants);
            state.Commands.DrawIndexed(uploaded_mesh.index_count, 1);
        }
    }

private:
    SceneRenderer& m_scene_renderer;
    val::Format m_surface_format{val::Format::UNDEFINED};
    uint32_t m_width{0};
    uint32_t m_height{0};
};

class SceneLightingPass final : public val::RenderPass {
public:
    SceneLightingPass(SceneRenderer& scene_renderer, val::Format surface_format, uint32_t width, uint32_t height)
        : m_scene_renderer(scene_renderer)
        , m_surface_format(surface_format)
        , m_width(width)
        , m_height(height)
    {}

    void SetupPipeline(val::PipelineState pipeline) override
    {
        pipeline.Shader = m_scene_renderer.m_lighting_shader;
        pipeline.DeclareAttachment("scene_color", m_surface_format, m_width, m_height);
        pipeline.AddOutputAttachment("scene_color", val::ClearColor{0.10f, 0.10f, 0.12f, 1.0f});
        pipeline.DescriptorBindings.Bind(
            0, "gbuffer_albedo", m_scene_renderer.m_gbuffer_sampler, val::UniformType::COMBINED_IMAGE_SAMPLER);
        pipeline.DescriptorBindings.Bind(
            1, "gbuffer_normal", m_scene_renderer.m_gbuffer_sampler, val::UniformType::COMBINED_IMAGE_SAMPLER);
    }

    void OnRender(val::RenderPassState state) override
    {
        state.Commands.SetRenderArea(state.GetAttachment("scene_color"));
        state.Commands.Draw(3, 1);
    }

private:
    SceneRenderer& m_scene_renderer;
    val::Format m_surface_format{val::Format::UNDEFINED};
    uint32_t m_width{0};
    uint32_t m_height{0};
};

class SceneClearPass final : public val::RenderPass {
public:
    SceneClearPass(val::Format surface_format, uint32_t width, uint32_t height)
        : m_surface_format(surface_format)
        , m_width(width)
        , m_height(height)
    {}

    void SetupPipeline(val::PipelineState pipeline) override
    {
        pipeline.DeclareAttachment("scene_color", m_surface_format, m_width, m_height);
        pipeline.AddOutputAttachment("scene_color", val::ClearColor{0.10f, 0.10f, 0.12f, 1.0f});
    }

private:
    val::Format m_surface_format{val::Format::UNDEFINED};
    uint32_t m_width{0};
    uint32_t m_height{0};
};

SceneRenderer::~SceneRenderer()
{
    shutdown();
}

void SceneRenderer::setShaderPaths(ShaderPaths shader_paths)
{
    m_shader_paths = std::move(shader_paths);
    m_geometry_shader.reset();
    m_lighting_shader.reset();
    m_core_resources_initialized = false;
}

void SceneRenderer::shutdown()
{
    clearSubmittedMeshes();
    m_uploaded_materials.clear();
    m_uploaded_meshes.clear();
    m_geometry_shader.reset();
    m_lighting_shader.reset();
    m_gbuffer_sampler = val::Sampler{};
    m_core_resources_initialized = false;
}

void SceneRenderer::beginScene(const Camera& camera)
{
    m_camera = camera;
    m_submitted_meshes.clear();
}

void SceneRenderer::clearSubmittedMeshes()
{
    m_submitted_meshes.clear();
}

void SceneRenderer::submitStaticMesh(const glm::mat4& transform,
                                     std::shared_ptr<Mesh> mesh,
                                     std::shared_ptr<Material> material)
{
    if (!mesh || !mesh->isValid()) {
        return;
    }

    m_submitted_meshes.push_back(StaticMeshDrawCommand{
        .transform = transform,
        .mesh = std::move(mesh),
        .material = std::move(material),
    });
}

std::unique_ptr<val::RenderGraph> SceneRenderer::buildRenderGraph(
    val::Format surface_format, uint32_t framebuffer_width, uint32_t framebuffer_height, bool include_imgui_pass)
{
    ensureCoreResources();

    val::RenderGraphBuilder builder;
    if (!m_core_resources_initialized) {
        builder.AddRenderPass("scene_clear", std::make_unique<SceneClearPass>(surface_format, framebuffer_width, framebuffer_height));
        if (include_imgui_pass) {
            builder.AddRenderPass("imgui", std::make_unique<val::ImGuiRenderPass>("scene_color"));
        }
        builder.SetOutputName("scene_color");
        return builder.Build();
    }

    builder.AddRenderPass(
        "scene_geometry", std::make_unique<SceneGeometryPass>(*this, surface_format, framebuffer_width, framebuffer_height));
    builder.AddRenderPass(
        "scene_lighting", std::make_unique<SceneLightingPass>(*this, surface_format, framebuffer_width, framebuffer_height));

    if (include_imgui_pass) {
        builder.AddRenderPass("imgui", std::make_unique<val::ImGuiRenderPass>("scene_color"));
    }

    builder.SetOutputName("scene_color");
    return builder.Build();
}

SceneRenderer::ShaderPaths SceneRenderer::getDefaultShaderPaths()
{
    const std::filesystem::path shader_root = projectRoot() / "Luna" / "Renderer" / "Shaders";
    return ShaderPaths{
        .geometry_vertex_path = shader_root / "SceneGeometry.vert",
        .geometry_fragment_path = shader_root / "SceneGeometry.frag",
        .lighting_vertex_path = shader_root / "SceneLighting.vert",
        .lighting_fragment_path = shader_root / "SceneLighting.frag",
    };
}

val::ImageData SceneRenderer::createFallbackImageData(const glm::vec4& albedo_color)
{
    const glm::vec4 clamped_color = glm::clamp(albedo_color, glm::vec4(0.0f), glm::vec4(1.0f));
    auto to_byte = [](float channel) {
        return static_cast<uint8_t>(std::lround(channel * 255.0f));
    };

    return val::ImageData{
        .ByteData = {to_byte(clamped_color.r), to_byte(clamped_color.g), to_byte(clamped_color.b), to_byte(clamped_color.a)},
        .ImageFormat = val::Format::R8G8B8A8_UNORM,
        .Width = 1,
        .Height = 1,
    };
}

SceneRenderer::ShaderPaths SceneRenderer::resolveShaderPaths() const
{
    if (m_shader_paths.isComplete()) {
        return m_shader_paths;
    }

    return getDefaultShaderPaths();
}

void SceneRenderer::ensureCoreResources()
{
    if (m_core_resources_initialized) {
        return;
    }

    const ShaderPaths shader_paths = resolveShaderPaths();

    m_geometry_shader = loadGraphicsShader(shader_paths.geometry_vertex_path, shader_paths.geometry_fragment_path);
    m_lighting_shader = loadGraphicsShader(shader_paths.lighting_vertex_path, shader_paths.lighting_fragment_path);

    if (!m_geometry_shader || !m_lighting_shader) {
        LUNA_CORE_ERROR("Failed to load scene renderer shaders: '{}', '{}', '{}', '{}'",
                        shader_paths.geometry_vertex_path.string(),
                        shader_paths.geometry_fragment_path.string(),
                        shader_paths.lighting_vertex_path.string(),
                        shader_paths.lighting_fragment_path.string());
        return;
    }

    m_gbuffer_sampler = val::Sampler(val::Sampler::MinFilter::LINEAR,
                                     val::Sampler::MagFilter::LINEAR,
                                     val::Sampler::AddressMode::CLAMP_TO_EDGE,
                                     val::Sampler::MipFilter::LINEAR);
    m_core_resources_initialized = true;
}

SceneRenderer::UploadedMesh& SceneRenderer::getOrCreateUploadedMesh(const Mesh& mesh)
{
    const auto it = m_uploaded_meshes.find(&mesh);
    if (it != m_uploaded_meshes.end()) {
        return it->second;
    }

    auto [inserted_it, _] = m_uploaded_meshes.emplace(&mesh, UploadedMesh{});
    auto& uploaded_mesh = inserted_it->second;

    uploaded_mesh.vertex_buffer = val::Buffer(
        mesh.getVertices().size() * sizeof(StaticMeshVertex), val::BufferUsage::VERTEX_BUFFER, val::MemoryUsage::CPU_TO_GPU);
    uploaded_mesh.index_buffer =
        val::Buffer(mesh.getIndices().size() * sizeof(uint32_t), val::BufferUsage::INDEX_BUFFER, val::MemoryUsage::CPU_TO_GPU);
    uploaded_mesh.index_count = static_cast<uint32_t>(mesh.getIndices().size());

    uploaded_mesh.vertex_buffer.CopyData(
        reinterpret_cast<const uint8_t*>(mesh.getVertices().data()), mesh.getVertices().size() * sizeof(StaticMeshVertex), 0);
    uploaded_mesh.index_buffer.CopyData(
        reinterpret_cast<const uint8_t*>(mesh.getIndices().data()), mesh.getIndices().size() * sizeof(uint32_t), 0);

    return uploaded_mesh;
}

SceneRenderer::UploadedMaterial& SceneRenderer::getOrCreateUploadedMaterial(const Material& material)
{
    const auto it = m_uploaded_materials.find(&material);
    if (it != m_uploaded_materials.end()) {
        return it->second;
    }

    const val::ImageData source_image = material.hasAlbedoTexture() ? material.getAlbedoImageData()
                                                                    : createFallbackImageData(material.getAlbedoColor());

    auto [inserted_it, _] = m_uploaded_materials.emplace(&material, UploadedMaterial{});
    auto& uploaded_material = inserted_it->second;

    uploaded_material.albedo_image =
        val::Image(source_image.Width,
                   source_image.Height,
                   source_image.ImageFormat,
                   val::ImageUsage::TRANSFER_DISTINATION | val::ImageUsage::SHADER_READ | val::ImageUsage::TRANSFER_SOURCE,
                   val::MemoryUsage::GPU_ONLY,
                   val::ImageOptions::MIPMAPS);
    uploaded_material.albedo_sampler = val::Sampler(val::Sampler::MinFilter::LINEAR,
                                                    val::Sampler::MagFilter::LINEAR,
                                                    val::Sampler::AddressMode::REPEAT,
                                                    val::Sampler::MipFilter::LINEAR);
    uploaded_material.staging_buffer =
        val::Buffer(source_image.ByteData.size(), val::BufferUsage::TRANSFER_SOURCE, val::MemoryUsage::CPU_TO_GPU);
    uploaded_material.staging_buffer.CopyData(source_image.ByteData.data(), source_image.ByteData.size(), 0);

    return uploaded_material;
}

void SceneRenderer::uploadMaterialIfNeeded(UploadedMaterial& uploaded_material, val::CommandBuffer& commands)
{
    if (uploaded_material.uploaded) {
        return;
    }

    commands.CopyBufferToImage(val::BufferInfo{std::cref(uploaded_material.staging_buffer), 0},
                               val::ImageInfo{std::cref(uploaded_material.albedo_image), val::ImageUsage::UNKNOWN, 0, 0});

    if (uploaded_material.albedo_image.GetMipLevelCount() > 1) {
        commands.GenerateMipLevels(
            uploaded_material.albedo_image, val::ImageUsage::TRANSFER_DISTINATION, val::BlitFilter::LINEAR);
    } else {
        commands.TransferLayout(
            uploaded_material.albedo_image, val::ImageUsage::TRANSFER_DISTINATION, val::ImageUsage::SHADER_READ);
    }

    uploaded_material.uploaded = true;
}

} // namespace luna
