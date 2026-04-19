#pragma once

#include "Renderer/ImageLoader.h"
#include "Renderer/ModelLoader.h"

#include <cstdint>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <string>

namespace luna {

class Material {
public:
    enum class BlendMode : uint8_t {
        Opaque,
        Masked,
        Transparent,
    };

    struct TextureSet {
        rhi::ImageData BaseColor;
        rhi::ImageData Normal;
        rhi::ImageData MetallicRoughness;
        rhi::ImageData Emissive;
        rhi::ImageData Occlusion;
    };

    struct SurfaceProperties {
        glm::vec4 BaseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
        glm::vec3 EmissiveFactor{0.0f, 0.0f, 0.0f};
        float MetallicFactor{0.0f};
        float RoughnessFactor{1.0f};
        float NormalScale{1.0f};
        float OcclusionStrength{1.0f};
        BlendMode BlendModeValue{BlendMode::Opaque};
        float AlphaCutoff{0.5f};
        bool DoubleSided{false};
        bool Unlit{false};
    };

    Material();
    Material(std::string name, TextureSet textures);
    Material(std::string name, TextureSet textures, SurfaceProperties surface);

    static std::shared_ptr<Material> create();
    static std::shared_ptr<Material> create(std::string name, TextureSet textures);
    static std::shared_ptr<Material> create(std::string name, TextureSet textures, SurfaceProperties surface);
    static std::shared_ptr<Material> createFromModelMaterial(const rhi::ModelData::Material& material_data);

    const std::string& getName() const;

    const rhi::ImageData& getBaseColorImageData() const;
    const rhi::ImageData& getNormalImageData() const;
    const rhi::ImageData& getMetallicRoughnessImageData() const;
    const rhi::ImageData& getEmissiveImageData() const;
    const rhi::ImageData& getOcclusionImageData() const;

    const glm::vec4& getBaseColorFactor() const;
    const glm::vec3& getEmissiveFactor() const;

    float getMetallicFactor() const;
    float getRoughnessFactor() const;
    float getNormalScale() const;
    float getOcclusionStrength() const;
    BlendMode getBlendMode() const;
    bool isTransparent() const;
    bool isMasked() const;
    bool isDoubleSided() const;
    bool isUnlit() const;
    float getAlphaCutoff() const;
    bool hasBaseColorTexture() const;
    bool hasNormalTexture() const;
    bool hasMetallicRoughnessTexture() const;
    bool hasEmissiveTexture() const;
    bool hasOcclusionTexture() const;

    const rhi::ImageData& getAlbedoImageData() const;
    const glm::vec4& getAlbedoColor() const;
    bool hasAlbedoTexture() const;

private:
    std::string m_name;
    rhi::ImageData m_base_color_image_data;
    rhi::ImageData m_normal_image_data;
    rhi::ImageData m_metallic_roughness_image_data;
    rhi::ImageData m_emissive_image_data;
    rhi::ImageData m_occlusion_image_data;
    glm::vec4 m_base_color_factor{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec3 m_emissive_factor{0.0f, 0.0f, 0.0f};
    BlendMode m_blend_mode{BlendMode::Opaque};

    float m_alpha_cutoff{0.5f};

    float m_metallic_factor{0.0f};
    float m_roughness_factor{1.0f};
    float m_normal_scale{1.0f};
    float m_occlusion_strength{1.0f};
    bool m_double_sided{false};
    bool m_unlit{false};
};

} // namespace luna
