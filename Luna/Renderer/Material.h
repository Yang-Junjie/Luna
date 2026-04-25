#pragma once

// Defines material assets consumed by scene render flows.
// Stores texture references, surface parameters, and blend behavior,
// but does not perform any GPU upload work by itself.

#include "Asset/Asset.h"

#include <cstdint>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <string>

namespace luna {

class Texture;

class Material final : public Asset {
public:
    enum class BlendMode : uint8_t {
        Opaque,
        Masked,
        Transparent,
        Additive
    };

    struct TextureSet {
        std::shared_ptr<Texture> BaseColor;
        std::shared_ptr<Texture> Normal;
        std::shared_ptr<Texture> MetallicRoughness;
        std::shared_ptr<Texture> Emissive;
        std::shared_ptr<Texture> Occlusion;
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

public:
    Material() = default;

    Material(std::string name, TextureSet textures, SurfaceProperties surface);

    static std::shared_ptr<Material> create(std::string name, TextureSet textures, SurfaceProperties surface);

    const std::string& getName() const;

    const TextureSet& getTextures() const;
    const SurfaceProperties& getSurface() const;
    const SurfaceProperties& getDefaultSurface() const;
    uint64_t getVersion() const;

    void setSurface(const SurfaceProperties& surface);
    void resetSurface();

    AssetType getAssetsType() const override
    {
        return AssetType::Material;
    }

    BlendMode getBlendMode() const;

    bool isTransparent() const;
    bool isMasked() const;

    bool hasBaseColorTexture() const;
    bool hasNormalTexture() const;
    bool hasMetallicRoughnessTexture() const;
    bool hasEmissiveTexture() const;
    bool hasOcclusionTexture() const;

private:
    std::string m_name;

    TextureSet m_textures;
    SurfaceProperties m_surface;
    SurfaceProperties m_default_surface;

    BlendMode m_blend_mode{BlendMode::Opaque};
    uint64_t m_version{1};
};

} // namespace luna




