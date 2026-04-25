#include "Core/Log.h"
#include "Material.h"

#include <utility>

namespace luna {

namespace {

const char* blendModeToString(Material::BlendMode blend_mode)
{
    switch (blend_mode) {
        case Material::BlendMode::Opaque:
            return "Opaque";
        case Material::BlendMode::Masked:
            return "Masked";
        case Material::BlendMode::Transparent:
            return "Transparent";
        case Material::BlendMode::Additive:
            return "Additive";
        default:
            return "Unknown";
    }
}

} // namespace

Material::Material(std::string name, TextureSet textures, SurfaceProperties surface)
    : m_name(std::move(name)),
      m_textures(std::move(textures)),
      m_surface(surface),
      m_default_surface(surface),
      m_blend_mode(surface.BlendModeValue)
{
    LUNA_RENDERER_DEBUG("Created material '{}' blend={} textures: base_color={} normal={} metallic_roughness={} "
                        "emissive={} occlusion={}",
                        m_name.empty() ? "<unnamed>" : m_name,
                        blendModeToString(m_blend_mode),
                        static_cast<bool>(m_textures.BaseColor),
                        static_cast<bool>(m_textures.Normal),
                        static_cast<bool>(m_textures.MetallicRoughness),
                        static_cast<bool>(m_textures.Emissive),
                        static_cast<bool>(m_textures.Occlusion));
}

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

const Material::SurfaceProperties& Material::getDefaultSurface() const
{
    return m_default_surface;
}

uint64_t Material::getVersion() const
{
    return m_version;
}

void Material::setSurface(const SurfaceProperties& surface)
{
    m_surface = surface;
    m_blend_mode = m_surface.BlendModeValue;
    ++m_version;
}

void Material::resetSurface()
{
    setSurface(m_default_surface);
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




