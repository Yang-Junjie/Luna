#pragma once

#include "Renderer/ImageLoader.h"
#include "Renderer/ModelLoader.h"

#include <cstdint>
#include <glm/vec4.hpp>
#include <memory>
#include <string>
#include <utility>

namespace luna {

class Material {
public:
    enum class BlendMode : uint8_t {
        Opaque,
        Masked,
        Transparent,
    };

    Material();
    Material(std::string name,
             rhi::ImageData albedo_image_data,
             glm::vec4 albedo_color = glm::vec4(1.0f),
             BlendMode blend_mode = BlendMode::Opaque,
             float alpha_cutoff = 0.5f);

    static std::shared_ptr<Material> create(std::string name = "Material",
                                            rhi::ImageData albedo_image_data = {},
                                            glm::vec4 albedo_color = glm::vec4(1.0f),
                                            BlendMode blend_mode = BlendMode::Opaque,
                                            float alpha_cutoff = 0.5f);
    static std::shared_ptr<Material> createFromModelMaterial(const rhi::ModelData::Material& material_data);

    const std::string& getName() const
    {
        return m_name;
    }

    const rhi::ImageData& getAlbedoImageData() const
    {
        return m_albedo_image_data;
    }

    const glm::vec4& getAlbedoColor() const
    {
        return m_albedo_color;
    }

    BlendMode getBlendMode() const
    {
        return m_blend_mode;
    }

    bool isTransparent() const
    {
        return m_blend_mode == BlendMode::Transparent;
    }

    bool isMasked() const
    {
        return m_blend_mode == BlendMode::Masked;
    }

    float getAlphaCutoff() const
    {
        return m_alpha_cutoff;
    }

    bool hasAlbedoTexture() const
    {
        return !m_albedo_image_data.ByteData.empty() && m_albedo_image_data.Width > 0 && m_albedo_image_data.Height > 0;
    }

private:
    std::string m_name;
    rhi::ImageData m_albedo_image_data;
    glm::vec4 m_albedo_color{1.0f, 1.0f, 1.0f, 1.0f};
    BlendMode m_blend_mode{BlendMode::Opaque};
    float m_alpha_cutoff{0.5f};
};

} // namespace luna
