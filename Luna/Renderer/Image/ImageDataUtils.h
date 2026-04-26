#pragma once

#include "Asset/Editor/ImageLoader.h"

#include <array>
#include <glm/vec4.hpp>

namespace luna::renderer_detail {

luna::ImageData createFallbackColorImageData(const glm::vec4& color);
luna::ImageData createFallbackMetallicRoughnessImageData(float roughness, float metallic);
luna::ImageData createFallbackFloatImageData(const glm::vec4& value, luna::RHI::Format format);
std::array<glm::vec4, 9> computeDiffuseIrradianceSH(const luna::ImageData& image, luna::RHI::Format expected_format);
luna::ImageData generateEnvironmentMipChain(const luna::ImageData& source, luna::RHI::Format expected_format);

} // namespace luna::renderer_detail
