#pragma once

// Caches GPU-side scene assets needed by draw submission.
// Converts meshes, materials, and textures into uploaded resources and descriptor sets,
// then reuses them across frames until pipeline/device state changes.

#include "Renderer/RenderFlow/DefaultScene/DrawQueue.h"
#include "Renderer/RenderFlow/DefaultScene/Support.h"

#include "Asset/Asset.h"

#include <memory>
#include <span>
#include <unordered_map>

namespace luna::RHI {
class DescriptorPool;
class DescriptorSet;
class DescriptorSetLayout;
class Device;
} // namespace luna::RHI

namespace luna::render_flow::default_scene {

class AssetCache final {
public:
    struct Bindings {
        luna::RHI::Ref<luna::RHI::Device> device;
        luna::RHI::Ref<luna::RHI::DescriptorPool> descriptor_pool;
        luna::RHI::Ref<luna::RHI::DescriptorSetLayout> material_layout;

        [[nodiscard]] bool isValid() const noexcept
        {
            return device && descriptor_pool && material_layout;
        }
    };

    struct DrawResources {
        luna::RHI::Ref<luna::RHI::Buffer> vertex_buffer;
        luna::RHI::Ref<luna::RHI::Buffer> index_buffer;
        luna::RHI::Ref<luna::RHI::DescriptorSet> material_descriptor_set;
        uint32_t index_count{0};

        [[nodiscard]] bool isValid() const noexcept
        {
            return vertex_buffer && index_buffer && material_descriptor_set && index_count > 0;
        }
    };

    enum class ClearMode : uint8_t {
        MaterialsAndTextures,
        All,
    };

    void clear(ClearMode mode);
    void prepareDraws(luna::RHI::CommandBufferEncoder& commands,
                      std::span<const DrawCommand> draw_commands,
                      const Material& default_material,
                      const Bindings& bindings);
    [[nodiscard]] DrawResources resolveDrawResources(const DrawCommand& draw_command,
                                                     const Material& default_material) const;

private:
    struct UploadedSubMesh {
        luna::RHI::Ref<luna::RHI::Buffer> vertex_buffer;
        luna::RHI::Ref<luna::RHI::Buffer> index_buffer;
        uint32_t index_count{0};
    };

    struct UploadedMesh {
        std::vector<UploadedSubMesh> sub_meshes;
    };

    struct UploadedMaterial {
        std::shared_ptr<render_flow::default_scene_detail::PendingTextureUpload> base_color_texture;
        std::shared_ptr<render_flow::default_scene_detail::PendingTextureUpload> normal_texture;
        std::shared_ptr<render_flow::default_scene_detail::PendingTextureUpload> metallic_roughness_texture;
        std::shared_ptr<render_flow::default_scene_detail::PendingTextureUpload> emissive_texture;
        std::shared_ptr<render_flow::default_scene_detail::PendingTextureUpload> occlusion_texture;
        luna::RHI::Ref<luna::RHI::Buffer> params_buffer;
        luna::RHI::Ref<luna::RHI::DescriptorSet> descriptor_set;
        uint64_t uploaded_version{0};
    };

    UploadedMesh& getOrCreateUploadedMesh(const Mesh& mesh, const Bindings& bindings);
    std::shared_ptr<render_flow::default_scene_detail::PendingTextureUpload>
        getOrCreateUploadedTexture(const std::shared_ptr<Texture>& texture, const Bindings& bindings);
    UploadedMaterial& getOrCreateUploadedMaterial(const Material& material, const Bindings& bindings);
    static void uploadMaterialParamsIfNeeded(const Material& material, UploadedMaterial& uploaded_material);
    void uploadMaterialIfNeeded(luna::RHI::CommandBufferEncoder& commands, UploadedMaterial& uploaded_material);
    [[nodiscard]] static const Material& resolveMaterial(const std::shared_ptr<Material>& material,
                                                         const Material& default_material);

private:
    std::unordered_map<AssetHandle, UploadedMesh> m_uploaded_meshes;
    std::unordered_map<const Texture*, std::shared_ptr<render_flow::default_scene_detail::PendingTextureUpload>>
        m_uploaded_textures;
    std::unordered_map<const Material*, UploadedMaterial> m_uploaded_materials;
};

} // namespace luna::render_flow::default_scene





