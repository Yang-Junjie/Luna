#include "Renderer/Material.h"

#include <utility>

namespace luna {

Material::Material()
    : Material("Material", {}, glm::vec4(1.0f))
{}

Material::Material(std::string name, rhi::ImageData albedo_image_data, glm::vec4 albedo_color)
    : m_name(std::move(name))
    , m_albedo_image_data(std::move(albedo_image_data))
    , m_albedo_color(albedo_color)
{}

std::shared_ptr<Material>
    Material::create(std::string name, rhi::ImageData albedo_image_data, glm::vec4 albedo_color)
{
    return std::make_shared<Material>(std::move(name), std::move(albedo_image_data), albedo_color);
}

std::shared_ptr<Material> Material::createFromModelMaterial(const rhi::ModelData::Material& material_data)
{
    return Material::create(material_data.Name, material_data.AlbedoTexture, glm::vec4(1.0f));
}

} // namespace luna
