#include "Renderer/Image/ImageDataUtils.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <glm/common.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>

namespace luna::renderer_detail {

luna::ImageData createFallbackColorImageData(const glm::vec4& color)
{
    const glm::vec4 clamped_color = glm::clamp(color, glm::vec4(0.0f), glm::vec4(1.0f));
    auto to_byte = [](float channel) {
        return static_cast<uint8_t>(std::lround(channel * 255.0f));
    };

    return luna::ImageData{
        .ByteData = {to_byte(clamped_color.r),
                     to_byte(clamped_color.g),
                     to_byte(clamped_color.b),
                     to_byte(clamped_color.a)},
        .ImageFormat = luna::RHI::Format::RGBA8_UNORM,
        .Width = 1,
        .Height = 1,
    };
}

luna::ImageData createFallbackMetallicRoughnessImageData(float roughness, float metallic)
{
    return createFallbackColorImageData(glm::vec4(0.0f, roughness, metallic, 1.0f));
}

luna::ImageData createFallbackFloatImageData(const glm::vec4& value, luna::RHI::Format format)
{
    std::vector<uint8_t> bytes(sizeof(float) * 4u, 0);
    std::memcpy(bytes.data(), &value[0], bytes.size());
    return luna::ImageData{
        .ByteData = std::move(bytes),
        .ImageFormat = format,
        .Width = 1,
        .Height = 1,
    };
}

std::array<glm::vec4, 9> computeDiffuseIrradianceSH(const luna::ImageData& image, luna::RHI::Format expected_format)
{
    std::array<glm::vec4, 9> result{};
    if (!image.isValid() || image.ImageFormat != expected_format ||
        image.ByteData.size() != static_cast<size_t>(image.Width) * static_cast<size_t>(image.Height) * 4u * sizeof(float)) {
        return result;
    }

    const auto* pixels = reinterpret_cast<const float*>(image.ByteData.data());
    std::array<glm::dvec3, 9> coefficients{};
    const double width = static_cast<double>(image.Width);
    const double height = static_cast<double>(image.Height);
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kTwoPi = kPi * 2.0;
    constexpr double kFourPi = kPi * 4.0;
    const double d_phi = kTwoPi / width;
    const double d_theta = kPi / height;
    double total_weight = 0.0;

    for (uint32_t y = 0; y < image.Height; ++y) {
        const double theta = kPi * (static_cast<double>(y) + 0.5) / height;
        const double sin_theta = std::sin(theta);
        const double cos_theta = std::cos(theta);

        for (uint32_t x = 0; x < image.Width; ++x) {
            const double phi = kTwoPi * (static_cast<double>(x) + 0.5) / width - kPi;
            const double sin_phi = std::sin(phi);
            const double cos_phi = std::cos(phi);
            const glm::dvec3 direction(cos_phi * sin_theta, cos_theta, sin_phi * sin_theta);
            const double weight = sin_theta * d_theta * d_phi;
            const size_t pixel_index = (static_cast<size_t>(y) * image.Width + x) * 4u;
            const glm::dvec3 radiance(static_cast<double>(pixels[pixel_index + 0]),
                                      static_cast<double>(pixels[pixel_index + 1]),
                                      static_cast<double>(pixels[pixel_index + 2]));

            const std::array<double, 9> basis{
                0.282095,
                0.488603 * direction.y,
                0.488603 * direction.z,
                0.488603 * direction.x,
                1.092548 * direction.x * direction.y,
                1.092548 * direction.y * direction.z,
                0.315392 * (3.0 * direction.z * direction.z - 1.0),
                1.092548 * direction.x * direction.z,
                0.546274 * (direction.x * direction.x - direction.y * direction.y),
            };

            for (size_t basis_index = 0; basis_index < basis.size(); ++basis_index) {
                coefficients[basis_index] += radiance * (basis[basis_index] * weight);
            }
            total_weight += weight;
        }
    }

    if (total_weight <= 0.0) {
        return result;
    }

    const double normalization = kFourPi / total_weight;
    constexpr std::array<double, 9> lambert_band_scale{
        kPi,
        kTwoPi / 3.0,
        kTwoPi / 3.0,
        kTwoPi / 3.0,
        kPi / 4.0,
        kPi / 4.0,
        kPi / 4.0,
        kPi / 4.0,
        kPi / 4.0,
    };

    for (size_t coefficient_index = 0; coefficient_index < coefficients.size(); ++coefficient_index) {
        const glm::dvec3 irradiance = coefficients[coefficient_index] * (normalization * lambert_band_scale[coefficient_index]);
        result[coefficient_index] = glm::vec4(glm::vec3(irradiance), 0.0f);
    }

    return result;
}

luna::ImageData generateEnvironmentMipChain(const luna::ImageData& source, luna::RHI::Format expected_format)
{
    if (!source.isValid() || source.ImageFormat != expected_format ||
        source.ByteData.size() !=
            static_cast<size_t>(source.Width) * static_cast<size_t>(source.Height) * 4u * sizeof(float)) {
        return source;
    }

    luna::ImageData result = source;
    result.MipLevels.clear();

    uint32_t previous_width = source.Width;
    uint32_t previous_height = source.Height;
    std::vector<float> previous_level(source.ByteData.size() / sizeof(float), 0.0f);
    std::memcpy(previous_level.data(), source.ByteData.data(), source.ByteData.size());

    while (previous_width > 1 || previous_height > 1) {
        const uint32_t next_width = (std::max)(previous_width / 2, 1u);
        const uint32_t next_height = (std::max)(previous_height / 2, 1u);
        std::vector<float> next_level(static_cast<size_t>(next_width) * static_cast<size_t>(next_height) * 4u, 0.0f);

        for (uint32_t y = 0; y < next_height; ++y) {
            for (uint32_t x = 0; x < next_width; ++x) {
                glm::vec4 sum(0.0f);
                for (uint32_t sample_y = 0; sample_y < 2; ++sample_y) {
                    for (uint32_t sample_x = 0; sample_x < 2; ++sample_x) {
                        const uint32_t source_x = (std::min)(previous_width - 1, x * 2 + sample_x);
                        const uint32_t source_y = (std::min)(previous_height - 1, y * 2 + sample_y);
                        const size_t source_index =
                            (static_cast<size_t>(source_y) * previous_width + source_x) * static_cast<size_t>(4);
                        sum += glm::vec4(previous_level[source_index + 0],
                                         previous_level[source_index + 1],
                                         previous_level[source_index + 2],
                                         previous_level[source_index + 3]);
                    }
                }

                const glm::vec4 averaged = sum * 0.25f;
                const size_t dest_index = (static_cast<size_t>(y) * next_width + x) * static_cast<size_t>(4);
                next_level[dest_index + 0] = averaged.x;
                next_level[dest_index + 1] = averaged.y;
                next_level[dest_index + 2] = averaged.z;
                next_level[dest_index + 3] = averaged.w;
            }
        }

        auto& mip_bytes = result.MipLevels.emplace_back(next_level.size() * sizeof(float), uint8_t{0});
        std::memcpy(mip_bytes.data(), next_level.data(), mip_bytes.size());

        previous_level = std::move(next_level);
        previous_width = next_width;
        previous_height = next_height;
    }

    return result;
}

} // namespace luna::renderer_detail
