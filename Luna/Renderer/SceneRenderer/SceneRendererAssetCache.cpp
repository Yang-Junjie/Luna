#include "Renderer/SceneRenderer/SceneRendererAssetCache.h"

#include "Core/Log.h"
#include "Renderer/Mesh.h"
#include "Renderer/RendererUtilities.h"

#include <Builders.h>
#include <DescriptorPool.h>
#include <DescriptorSet.h>
#include <DescriptorSetLayout.h>
#include <Device.h>

#include <cstring>

namespace luna::scene_renderer {

void AssetCache::clear(ClearMode mode)
{
    const bool clear_meshes = mode == ClearMode::All;
    if (!m_uploaded_materials.empty() || !m_uploaded_textures.empty() || (clear_meshes && !m_uploaded_meshes.empty())) {
        LUNA_RENDERER_DEBUG("Clearing scene renderer asset cache: materials={} textures={} meshes={} clear_meshes={}",
                            m_uploaded_materials.size(),
                            m_uploaded_textures.size(),
                            m_uploaded_meshes.size(),
                            clear_meshes);
    }

    m_uploaded_materials.clear();
    m_uploaded_textures.clear();
    if (clear_meshes) {
        m_uploaded_meshes.clear();
    }
}

void AssetCache::prepareDraws(luna::RHI::CommandBufferEncoder& commands,
                              std::span<const DrawCommand> draw_commands,
                              const Material& default_material,
                              const Bindings& bindings)
{
    LUNA_RENDERER_FRAME_TRACE("Preparing {} draw command(s) for GPU submission", draw_commands.size());
    (void) getOrCreateUploadedMaterial(default_material, bindings);

    for (const auto& draw_command : draw_commands) {
        if (!draw_command.mesh || !draw_command.mesh->isValid()) {
            LUNA_RENDERER_WARN("Skipping draw preparation because mesh is null or invalid");
            continue;
        }

        (void) getOrCreateUploadedMesh(*draw_command.mesh, bindings);
        auto& uploaded_material = getOrCreateUploadedMaterial(resolveMaterial(draw_command.material, default_material), bindings);
        uploadMaterialIfNeeded(commands, uploaded_material);
    }
}

AssetCache::DrawResources AssetCache::resolveDrawResources(const DrawCommand& draw_command,
                                                           const Material& default_material) const
{
    DrawResources resolved;
    if (!draw_command.mesh || !draw_command.mesh->isValid()) {
        LUNA_RENDERER_FRAME_TRACE("Cannot resolve draw resources because mesh is null or invalid");
        return resolved;
    }

    const auto uploaded_mesh_it = m_uploaded_meshes.find(draw_command.mesh.get());
    if (uploaded_mesh_it == m_uploaded_meshes.end()) {
        LUNA_RENDERER_FRAME_TRACE("Cannot resolve draw resources for mesh '{}' because it has not been uploaded",
                                  draw_command.mesh->getName().empty() ? "<unnamed>" : draw_command.mesh->getName());
        return resolved;
    }

    const Material& material = resolveMaterial(draw_command.material, default_material);
    const auto uploaded_material_it = m_uploaded_materials.find(&material);
    if (uploaded_material_it == m_uploaded_materials.end() || !uploaded_material_it->second.descriptor_set) {
        LUNA_RENDERER_FRAME_TRACE("Cannot resolve draw resources for material '{}' because descriptor set is unavailable",
                                  material.getName().empty() ? "Material" : material.getName());
        return resolved;
    }

    const auto& uploaded_mesh = uploaded_mesh_it->second;
    if (draw_command.sub_mesh_index >= uploaded_mesh.sub_meshes.size()) {
        LUNA_RENDERER_FRAME_TRACE("Cannot resolve draw resources because submesh index {} is out of range ({})",
                                  draw_command.sub_mesh_index,
                                  uploaded_mesh.sub_meshes.size());
        return resolved;
    }

    const auto& uploaded_sub_mesh = uploaded_mesh.sub_meshes[draw_command.sub_mesh_index];
    resolved.vertex_buffer = uploaded_sub_mesh.vertex_buffer;
    resolved.index_buffer = uploaded_sub_mesh.index_buffer;
    resolved.material_descriptor_set = uploaded_material_it->second.descriptor_set;
    resolved.index_count = uploaded_sub_mesh.index_count;
    return resolved;
}

AssetCache::UploadedMesh& AssetCache::getOrCreateUploadedMesh(const Mesh& mesh, const Bindings& bindings)
{
    const auto it = m_uploaded_meshes.find(&mesh);
    if (it != m_uploaded_meshes.end()) {
        LUNA_RENDERER_FRAME_TRACE("Reusing uploaded mesh '{}'", mesh.getName().empty() ? "<unnamed>" : mesh.getName());
        return it->second;
    }

    auto [inserted_it, _] = m_uploaded_meshes.emplace(&mesh, UploadedMesh{});
    auto& uploaded_mesh = inserted_it->second;
    const auto& sub_meshes = mesh.getSubMeshes();
    uploaded_mesh.sub_meshes.resize(sub_meshes.size());
    LUNA_RENDERER_DEBUG("Uploading mesh '{}' with {} submesh(es)",
                        mesh.getName().empty() ? "<unnamed>" : mesh.getName(),
                        sub_meshes.size());

    if (!bindings.device) {
        LUNA_RENDERER_WARN("Cannot upload mesh '{}' because device is unavailable",
                           mesh.getName().empty() ? "<unnamed>" : mesh.getName());
        return uploaded_mesh;
    }

    for (size_t sub_mesh_index = 0; sub_mesh_index < sub_meshes.size(); ++sub_mesh_index) {
        const auto& sub_mesh = sub_meshes[sub_mesh_index];
        auto& uploaded_sub_mesh = uploaded_mesh.sub_meshes[sub_mesh_index];

        if (sub_mesh.Vertices.empty() || sub_mesh.Indices.empty()) {
            LUNA_RENDERER_WARN("Skipping upload for empty submesh {} in mesh '{}'",
                               sub_mesh_index,
                               mesh.getName().empty() ? "<unnamed>" : mesh.getName());
            continue;
        }

        const std::string sub_mesh_name =
            sub_mesh.Name.empty() ? mesh.getName() + "_SubMesh_" + std::to_string(sub_mesh_index) : sub_mesh.Name;

        uploaded_sub_mesh.vertex_buffer = bindings.device->CreateBuffer(luna::RHI::BufferBuilder()
                                                                            .SetSize(sub_mesh.Vertices.size() *
                                                                                     sizeof(StaticMeshVertex))
                                                                            .SetUsage(luna::RHI::BufferUsageFlags::VertexBuffer)
                                                                            .SetMemoryUsage(luna::RHI::BufferMemoryUsage::CpuToGpu)
                                                                            .SetName(sub_mesh_name + "_VertexBuffer")
                                                                            .Build());
        uploaded_sub_mesh.index_buffer = bindings.device->CreateBuffer(luna::RHI::BufferBuilder()
                                                                           .SetSize(sub_mesh.Indices.size() * sizeof(uint32_t))
                                                                           .SetUsage(luna::RHI::BufferUsageFlags::IndexBuffer)
                                                                           .SetMemoryUsage(luna::RHI::BufferMemoryUsage::CpuToGpu)
                                                                           .SetName(sub_mesh_name + "_IndexBuffer")
                                                                           .Build());
        uploaded_sub_mesh.index_count = static_cast<uint32_t>(sub_mesh.Indices.size());

        if (uploaded_sub_mesh.vertex_buffer) {
            if (void* vertex_memory = uploaded_sub_mesh.vertex_buffer->Map()) {
                std::memcpy(vertex_memory, sub_mesh.Vertices.data(), sub_mesh.Vertices.size() * sizeof(StaticMeshVertex));
                uploaded_sub_mesh.vertex_buffer->Flush();
                uploaded_sub_mesh.vertex_buffer->Unmap();
            } else {
                LUNA_RENDERER_WARN("Failed to map vertex buffer for mesh '{}' submesh '{}'",
                                   mesh.getName().empty() ? "<unnamed>" : mesh.getName(),
                                   sub_mesh_name);
            }
        } else {
            LUNA_RENDERER_WARN("Failed to create vertex buffer for mesh '{}' submesh '{}'",
                               mesh.getName().empty() ? "<unnamed>" : mesh.getName(),
                               sub_mesh_name);
        }

        if (uploaded_sub_mesh.index_buffer) {
            if (void* index_memory = uploaded_sub_mesh.index_buffer->Map()) {
                std::memcpy(index_memory, sub_mesh.Indices.data(), sub_mesh.Indices.size() * sizeof(uint32_t));
                uploaded_sub_mesh.index_buffer->Flush();
                uploaded_sub_mesh.index_buffer->Unmap();
            } else {
                LUNA_RENDERER_WARN("Failed to map index buffer for mesh '{}' submesh '{}'",
                                   mesh.getName().empty() ? "<unnamed>" : mesh.getName(),
                                   sub_mesh_name);
            }
        } else {
            LUNA_RENDERER_WARN("Failed to create index buffer for mesh '{}' submesh '{}'",
                               mesh.getName().empty() ? "<unnamed>" : mesh.getName(),
                               sub_mesh_name);
        }
    }

    return uploaded_mesh;
}

std::shared_ptr<scene_renderer_detail::PendingTextureUpload>
    AssetCache::getOrCreateUploadedTexture(const std::shared_ptr<Texture>& texture, const Bindings& bindings)
{
    if (texture == nullptr || !texture->isValid()) {
        LUNA_RENDERER_WARN("Ignoring texture upload request because texture is null or invalid");
        return {};
    }

    const auto it = m_uploaded_textures.find(texture.get());
    if (it != m_uploaded_textures.end()) {
        LUNA_RENDERER_FRAME_TRACE("Reusing uploaded texture '{}'",
                                  texture->getName().empty() ? "Texture" : texture->getName());
        return it->second;
    }

    const std::string debug_name = texture->getName().empty() ? std::string("Texture") : texture->getName();
    auto uploaded_texture = std::make_shared<scene_renderer_detail::PendingTextureUpload>(
        scene_renderer_detail::createTextureUpload(
            bindings.device, texture->getImageData(), texture->getSamplerSettings(), debug_name));
    m_uploaded_textures.emplace(texture.get(), uploaded_texture);
    return uploaded_texture;
}

AssetCache::UploadedMaterial& AssetCache::getOrCreateUploadedMaterial(const Material& material, const Bindings& bindings)
{
    const auto it = m_uploaded_materials.find(&material);
    if (it != m_uploaded_materials.end()) {
        LUNA_RENDERER_FRAME_TRACE("Reusing uploaded material '{}'",
                                  material.getName().empty() ? "Material" : material.getName());
        return it->second;
    }

    const auto& textures = material.getTextures();
    const auto& surface = material.getSurface();
    const std::string material_name = material.getName().empty() ? "Material" : material.getName();
    const Texture::SamplerSettings default_sampler_settings{};

    auto [inserted_it, _] = m_uploaded_materials.emplace(&material, UploadedMaterial{});
    auto& uploaded_material = inserted_it->second;
    LUNA_RENDERER_DEBUG("Uploading material '{}'", material_name);

    const auto create_material_texture =
        [&](const std::shared_ptr<Texture>& texture,
            const ImageData& fallback_image,
            std::string_view suffix) -> std::shared_ptr<scene_renderer_detail::PendingTextureUpload> {
        if (texture != nullptr && texture->isValid()) {
            return getOrCreateUploadedTexture(texture, bindings);
        }

        const std::string texture_name = material_name + "_" + std::string(suffix);
        return std::make_shared<scene_renderer_detail::PendingTextureUpload>(
            scene_renderer_detail::createTextureUpload(bindings.device, fallback_image, default_sampler_settings, texture_name));
    };

    uploaded_material.base_color_texture =
        create_material_texture(textures.BaseColor, scene_renderer_detail::createFallbackColorImageData(glm::vec4(1.0f)), "BaseColor");
    uploaded_material.normal_texture = create_material_texture(
        textures.Normal, scene_renderer_detail::createFallbackColorImageData(glm::vec4(0.5f, 0.5f, 1.0f, 1.0f)), "Normal");
    uploaded_material.metallic_roughness_texture = create_material_texture(
        textures.MetallicRoughness,
        scene_renderer_detail::createFallbackMetallicRoughnessImageData(1.0f, 0.0f),
        "MetallicRoughness");
    uploaded_material.emissive_texture = create_material_texture(
        textures.Emissive, scene_renderer_detail::createFallbackColorImageData(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)), "Emissive");
    uploaded_material.occlusion_texture =
        create_material_texture(textures.Occlusion, scene_renderer_detail::createFallbackColorImageData(glm::vec4(1.0f)), "Occlusion");

    if (bindings.device) {
        uploaded_material.params_buffer = bindings.device->CreateBuffer(luna::RHI::BufferBuilder()
                                                                            .SetSize(sizeof(scene_renderer_detail::MaterialGpuParams))
                                                                            .SetUsage(luna::RHI::BufferUsageFlags::UniformBuffer)
                                                                            .SetMemoryUsage(luna::RHI::BufferMemoryUsage::CpuToGpu)
                                                                            .SetName(material_name + "_Params")
                                                                            .Build());
    }
    if (!uploaded_material.params_buffer) {
        LUNA_RENDERER_WARN("Failed to create parameter buffer for material '{}'", material_name);
    } else if (void* mapped = uploaded_material.params_buffer->Map()) {
        const scene_renderer_detail::MaterialGpuParams params{
            .base_color_factor = surface.BaseColorFactor,
            .emissive_factor_normal_scale = glm::vec4(surface.EmissiveFactor, surface.NormalScale),
            .material_factors = glm::vec4(
                surface.MetallicFactor, surface.RoughnessFactor, surface.OcclusionStrength, surface.AlphaCutoff),
            .material_flags = glm::vec4(scene_renderer_detail::materialBlendModeToFloat(material.getBlendMode()),
                                        surface.Unlit ? 1.0f : 0.0f,
                                        surface.DoubleSided ? 1.0f : 0.0f,
                                        0.0f),
        };
        std::memcpy(mapped, &params, sizeof(params));
        uploaded_material.params_buffer->Flush();
        uploaded_material.params_buffer->Unmap();
    } else {
        LUNA_RENDERER_WARN("Failed to map parameter buffer for material '{}'", material_name);
    }

    if (!bindings.isValid() || !uploaded_material.base_color_texture || !uploaded_material.normal_texture ||
        !uploaded_material.metallic_roughness_texture || !uploaded_material.emissive_texture ||
        !uploaded_material.occlusion_texture || !uploaded_material.base_color_texture->texture ||
        !uploaded_material.normal_texture->texture || !uploaded_material.metallic_roughness_texture->texture ||
        !uploaded_material.emissive_texture->texture || !uploaded_material.occlusion_texture->texture ||
        !uploaded_material.base_color_texture->sampler || !uploaded_material.normal_texture->sampler ||
        !uploaded_material.metallic_roughness_texture->sampler || !uploaded_material.emissive_texture->sampler ||
        !uploaded_material.occlusion_texture->sampler || !uploaded_material.params_buffer) {
        LUNA_RENDERER_WARN(
            "Material '{}' upload is incomplete: bindings={} base={} normal={} metallic_roughness={} emissive={} occlusion={} params_buffer={}",
            material_name,
            bindings.isValid(),
            static_cast<bool>(uploaded_material.base_color_texture && uploaded_material.base_color_texture->texture &&
                              uploaded_material.base_color_texture->sampler),
            static_cast<bool>(uploaded_material.normal_texture && uploaded_material.normal_texture->texture &&
                              uploaded_material.normal_texture->sampler),
            static_cast<bool>(uploaded_material.metallic_roughness_texture &&
                              uploaded_material.metallic_roughness_texture->texture &&
                              uploaded_material.metallic_roughness_texture->sampler),
            static_cast<bool>(uploaded_material.emissive_texture && uploaded_material.emissive_texture->texture &&
                              uploaded_material.emissive_texture->sampler),
            static_cast<bool>(uploaded_material.occlusion_texture && uploaded_material.occlusion_texture->texture &&
                              uploaded_material.occlusion_texture->sampler),
            static_cast<bool>(uploaded_material.params_buffer));
        return uploaded_material;
    }

    uploaded_material.descriptor_set = bindings.descriptor_pool->AllocateDescriptorSet(bindings.material_layout);
    if (!uploaded_material.descriptor_set) {
        LUNA_RENDERER_WARN("Failed to allocate descriptor set for material '{}'", material_name);
        return uploaded_material;
    }

    uploaded_material.descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 0,
        .TextureView = uploaded_material.base_color_texture->texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    uploaded_material.descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
        .Binding = 1,
        .Sampler = uploaded_material.base_color_texture->sampler,
    });
    uploaded_material.descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 2,
        .TextureView = uploaded_material.normal_texture->texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    uploaded_material.descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
        .Binding = 3,
        .Sampler = uploaded_material.normal_texture->sampler,
    });
    uploaded_material.descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 4,
        .TextureView = uploaded_material.metallic_roughness_texture->texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    uploaded_material.descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
        .Binding = 5,
        .Sampler = uploaded_material.metallic_roughness_texture->sampler,
    });
    uploaded_material.descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 6,
        .TextureView = uploaded_material.emissive_texture->texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    uploaded_material.descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
        .Binding = 7,
        .Sampler = uploaded_material.emissive_texture->sampler,
    });
    uploaded_material.descriptor_set->WriteTexture(luna::RHI::TextureWriteInfo{
        .Binding = 8,
        .TextureView = uploaded_material.occlusion_texture->texture->GetDefaultView(),
        .Layout = luna::RHI::ResourceState::ShaderRead,
        .Type = luna::RHI::DescriptorType::SampledImage,
    });
    uploaded_material.descriptor_set->WriteSampler(luna::RHI::SamplerWriteInfo{
        .Binding = 9,
        .Sampler = uploaded_material.occlusion_texture->sampler,
    });
    uploaded_material.descriptor_set->WriteBuffer(luna::RHI::BufferWriteInfo{
        .Binding = 10,
        .Buffer = uploaded_material.params_buffer,
        .Offset = 0,
        .Stride = sizeof(scene_renderer_detail::MaterialGpuParams),
        .Size = sizeof(scene_renderer_detail::MaterialGpuParams),
        .Type = luna::RHI::DescriptorType::UniformBuffer,
    });
    uploaded_material.descriptor_set->Update();
    return uploaded_material;
}

void AssetCache::uploadMaterialIfNeeded(luna::RHI::CommandBufferEncoder& commands, UploadedMaterial& uploaded_material)
{
    LUNA_RENDERER_FRAME_TRACE("Ensuring material textures are uploaded");
    if (uploaded_material.base_color_texture) {
        scene_renderer_detail::uploadTextureIfNeeded(commands, *uploaded_material.base_color_texture);
    }
    if (uploaded_material.normal_texture) {
        scene_renderer_detail::uploadTextureIfNeeded(commands, *uploaded_material.normal_texture);
    }
    if (uploaded_material.metallic_roughness_texture) {
        scene_renderer_detail::uploadTextureIfNeeded(commands, *uploaded_material.metallic_roughness_texture);
    }
    if (uploaded_material.emissive_texture) {
        scene_renderer_detail::uploadTextureIfNeeded(commands, *uploaded_material.emissive_texture);
    }
    if (uploaded_material.occlusion_texture) {
        scene_renderer_detail::uploadTextureIfNeeded(commands, *uploaded_material.occlusion_texture);
    }
}

const Material& AssetCache::resolveMaterial(const std::shared_ptr<Material>& material, const Material& default_material)
{
    return material != nullptr ? *material : default_material;
}

} // namespace luna::scene_renderer
