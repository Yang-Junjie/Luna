#pragma once

#include "EditorApi/EditorTypes.h"

#include <cstdint>

#include <filesystem>
#include <optional>
#include <string>

namespace luna::editor {

enum class MaterialBlendMode : uint32_t {
    Opaque = 0,
    Masked = 1,
    Transparent = 2,
    Additive = 3,
};

struct MaterialTextureSet {
    AssetHandle base_color{0};
    AssetHandle normal{0};
    AssetHandle metallic_roughness{0};
    AssetHandle metallic{0};
    AssetHandle roughness{0};
    AssetHandle emissive{0};
    AssetHandle occlusion{0};
};

struct MaterialSurfaceProperties {
    Vec4 base_color_factor{1.0f, 1.0f, 1.0f, 1.0f};
    Vec3 emissive_factor{0.0f, 0.0f, 0.0f};
    float metallic_factor{0.0f};
    float roughness_factor{1.0f};
    float normal_scale{1.0f};
    float occlusion_strength{1.0f};
    float alpha_cutoff{0.5f};
    MaterialBlendMode blend_mode{MaterialBlendMode::Opaque};
    bool double_sided{false};
    bool unlit{false};
};

struct MaterialDocument {
    AssetHandle handle{0};
    std::string name;
    std::filesystem::path project_path;
    std::filesystem::path absolute_path;
    MaterialTextureSet textures;
    MaterialSurfaceProperties surface;
    bool exists{false};
    bool editable{false};
    bool builtin{false};
    bool memory_only{false};
    bool dirty{false};
    uint64_t version{0};
};

struct MaterialEditResult {
    bool success{false};
    bool project_loaded{false};
    bool dirty{false};
    std::string message;
};

struct MaterialCreateRequest {
    std::string name{"New Material"};
    std::filesystem::path project_path;
    MaterialTextureSet textures;
    MaterialSurfaceProperties surface;
    bool overwrite{false};
};

struct MaterialCreateResult {
    bool success{false};
    bool project_loaded{false};
    AssetHandle handle{0};
    std::filesystem::path project_path;
    std::filesystem::path absolute_path;
    std::string message;
};

class MaterialService {
public:
    virtual ~MaterialService() = default;

    virtual MaterialCreateResult createMaterial(const MaterialCreateRequest& request) = 0;
    virtual bool canEditMaterial(AssetHandle handle) const = 0;
    virtual std::optional<MaterialDocument> readMaterial(AssetHandle handle) = 0;
    virtual bool setMaterialTextures(AssetHandle handle, const MaterialTextureSet& textures) = 0;
    virtual bool setMaterialSurface(AssetHandle handle, const MaterialSurfaceProperties& surface) = 0;
    virtual MaterialEditResult saveMaterial(AssetHandle handle) = 0;
    virtual MaterialEditResult revertMaterial(AssetHandle handle) = 0;
    virtual bool isMaterialDirty(AssetHandle handle) const = 0;
};

} // namespace luna::editor
