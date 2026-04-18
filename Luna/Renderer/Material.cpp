#include "Renderer/Material.h"

#include <utility>

namespace luna {
namespace {

bool isImageDataValid(const rhi::ImageData& image_data)
{
    return image_data.isValid();
}

Material::BlendMode toBlendMode(rhi::ModelData::Material::AlphaMode alpha_mode)
{
    switch (alpha_mode) {
        case rhi::ModelData::Material::AlphaMode::Opaque:
            return Material::BlendMode::Opaque;
        case rhi::ModelData::Material::AlphaMode::Mask:
            return Material::BlendMode::Masked;
        case rhi::ModelData::Material::AlphaMode::Blend:
            return Material::BlendMode::Transparent;
        default:
            return Material::BlendMode::Opaque;
    }
}

} // namespace

Material::Material()
    : Material("Material", TextureSet{}, SurfaceProperties{})
{}

Material::Material(std::string name, TextureSet textures)
    : Material(std::move(name), std::move(textures), SurfaceProperties{})
{}

Material::Material(std::string name, TextureSet textures, SurfaceProperties surface)
    : m_name(std::move(name)),
      m_base_color_image_data(std::move(textures.BaseColor)),
      m_normal_image_data(std::move(textures.Normal)),
      m_metallic_roughness_image_data(std::move(textures.MetallicRoughness)),
      m_emissive_image_data(std::move(textures.Emissive)),
      m_occlusion_image_data(std::move(textures.Occlusion)),
      m_base_color_factor(surface.BaseColorFactor),
      m_emissive_factor(surface.EmissiveFactor),
      m_blend_mode(surface.BlendModeValue),
      m_alpha_cutoff(surface.AlphaCutoff),
      m_metallic_factor(surface.MetallicFactor),
      m_roughness_factor(surface.RoughnessFactor),
      m_normal_scale(surface.NormalScale),
      m_occlusion_strength(surface.OcclusionStrength),
      m_double_sided(surface.DoubleSided),
      m_unlit(surface.Unlit)
{}

std::shared_ptr<Material> Material::create()
{
    return std::make_shared<Material>();
}

std::shared_ptr<Material> Material::create(std::string name, TextureSet textures)
{
    return std::make_shared<Material>(std::move(name), std::move(textures));
}

std::shared_ptr<Material> Material::create(std::string name, TextureSet textures, SurfaceProperties surface)
{
    return std::make_shared<Material>(std::move(name), std::move(textures), surface);
}

std::shared_ptr<Material> Material::createFromModelMaterial(const rhi::ModelData::Material& material_data)
{
    TextureSet textures;
    textures.BaseColor = material_data.BaseColorTexture;
    textures.Normal = material_data.NormalTexture;
    textures.MetallicRoughness = material_data.MetallicRoughnessTexture;
    textures.Emissive = material_data.EmissiveTexture;
    textures.Occlusion = material_data.OcclusionTexture;

    SurfaceProperties surface;
    surface.BaseColorFactor = material_data.BaseColorFactor;
    surface.EmissiveFactor = material_data.EmissiveFactor;
    surface.MetallicFactor = material_data.MetallicFactor;
    surface.RoughnessFactor = material_data.RoughnessFactor;
    surface.NormalScale = material_data.NormalScale;
    surface.OcclusionStrength = material_data.OcclusionStrength;
    surface.BlendModeValue = toBlendMode(material_data.AlphaModeValue);
    surface.AlphaCutoff = material_data.AlphaCutoff;
    surface.DoubleSided = material_data.DoubleSided;
    surface.Unlit = material_data.Unlit;

    return Material::create(material_data.Name, std::move(textures), surface);
}

const std::string& Material::getName() const
{
    return m_name;
}

const rhi::ImageData& Material::getBaseColorImageData() const
{
    return m_base_color_image_data;
}

const rhi::ImageData& Material::getNormalImageData() const
{
    return m_normal_image_data;
}

const rhi::ImageData& Material::getMetallicRoughnessImageData() const
{
    return m_metallic_roughness_image_data;
}

const rhi::ImageData& Material::getEmissiveImageData() const
{
    return m_emissive_image_data;
}

const rhi::ImageData& Material::getOcclusionImageData() const
{
    return m_occlusion_image_data;
}

const glm::vec4& Material::getBaseColorFactor() const
{
    return m_base_color_factor;
}

const glm::vec3& Material::getEmissiveFactor() const
{
    return m_emissive_factor;
}

float Material::getMetallicFactor() const
{
    return m_metallic_factor;
}

float Material::getRoughnessFactor() const
{
    return m_roughness_factor;
}

float Material::getNormalScale() const
{
    return m_normal_scale;
}

float Material::getOcclusionStrength() const
{
    return m_occlusion_strength;
}

Material::BlendMode Material::getBlendMode() const
{
    return m_blend_mode;
}

bool Material::isTransparent() const
{
    return m_blend_mode == BlendMode::Transparent;
}

bool Material::isMasked() const
{
    return m_blend_mode == BlendMode::Masked;
}

bool Material::isDoubleSided() const
{
    return m_double_sided;
}

bool Material::isUnlit() const
{
    return m_unlit;
}

float Material::getAlphaCutoff() const
{
    return m_alpha_cutoff;
}

bool Material::hasBaseColorTexture() const
{
    return isImageDataValid(m_base_color_image_data);
}

bool Material::hasNormalTexture() const
{
    return isImageDataValid(m_normal_image_data);
}

bool Material::hasMetallicRoughnessTexture() const
{
    return isImageDataValid(m_metallic_roughness_image_data);
}

bool Material::hasEmissiveTexture() const
{
    return isImageDataValid(m_emissive_image_data);
}

bool Material::hasOcclusionTexture() const
{
    return isImageDataValid(m_occlusion_image_data);
}

const rhi::ImageData& Material::getAlbedoImageData() const
{
    return getBaseColorImageData();
}

const glm::vec4& Material::getAlbedoColor() const
{
    return getBaseColorFactor();
}

bool Material::hasAlbedoTexture() const
{
    return hasBaseColorTexture();
}

} // namespace luna
