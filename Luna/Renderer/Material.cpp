#include "Material.h"

#include <utility>

namespace luna {

Material::Material(std::string name, TextureSet textures, SurfaceProperties surface)
    : m_name(std::move(name)),
      m_textures(std::move(textures)),
      m_surface(surface),
      m_blend_mode(surface.BlendModeValue)
{}

std::shared_ptr<Material> Material::create(std::string name, TextureSet textures, SurfaceProperties surface)
{
    return std::make_shared<Material>(std::move(name), std::move(textures), surface);
}

const std::string& Material::getName() const
{
    return m_name;
}

const Material::TextureSet& Material::getTextures() const
{
    return m_textures;
}

const Material::SurfaceProperties& Material::getSurface() const
{
    return m_surface;
}

Material::BlendMode Material::getBlendMode() const
{
    return m_blend_mode;
}

bool Material::isTransparent() const
{
    return m_blend_mode == BlendMode::Transparent || m_blend_mode == BlendMode::Additive;
}

bool Material::isMasked() const
{
    return m_blend_mode == BlendMode::Masked;
}

bool Material::hasBaseColorTexture() const
{
    return m_textures.BaseColor != nullptr;
}

bool Material::hasNormalTexture() const
{
    return m_textures.Normal != nullptr;
}

bool Material::hasMetallicRoughnessTexture() const
{
    return m_textures.MetallicRoughness != nullptr;
}

bool Material::hasEmissiveTexture() const
{
    return m_textures.Emissive != nullptr;
}

bool Material::hasOcclusionTexture() const
{
    return m_textures.Occlusion != nullptr;
}

} // namespace luna
