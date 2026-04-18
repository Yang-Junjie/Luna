#include "Renderer/Material.h"

#include <utility>

namespace luna {

Material::Material()
    : Material("Material", {}, glm::vec4(1.0f), BlendMode::Opaque, 0.5f)
{}

Material::Material(std::string name,
                   rhi::ImageData albedo_image_data,
                   glm::vec4 albedo_color,
                   BlendMode blend_mode,
                   float alpha_cutoff)
    : m_name(std::move(name)),
      m_albedo_image_data(std::move(albedo_image_data)),
      m_albedo_color(albedo_color),
      m_blend_mode(blend_mode),
      m_alpha_cutoff(alpha_cutoff)
{}

std::shared_ptr<Material>
    Material::create(std::string name,
                     rhi::ImageData albedo_image_data,
                     glm::vec4 albedo_color,
                     BlendMode blend_mode,
                     float alpha_cutoff)
{
    return std::make_shared<Material>(
        std::move(name), std::move(albedo_image_data), albedo_color, blend_mode, alpha_cutoff);
}

std::shared_ptr<Material> Material::createFromModelMaterial(const rhi::ModelData::Material& material_data)
{
    BlendMode blend_mode = BlendMode::Opaque;
    switch (material_data.AlphaModeValue) {
        case rhi::ModelData::Material::AlphaMode::Opaque:
            blend_mode = BlendMode::Opaque;
            break;
        case rhi::ModelData::Material::AlphaMode::Mask:
            blend_mode = BlendMode::Masked;
            break;
        case rhi::ModelData::Material::AlphaMode::Blend:
            blend_mode = BlendMode::Transparent;
            break;
        default:
            blend_mode = BlendMode::Opaque;
            break;
    }

    return Material::create(
        material_data.Name, material_data.AlbedoTexture, glm::vec4(1.0f), blend_mode, material_data.AlphaCutoff);
}

} // namespace luna
