#pragma once

#include "Renderer/ImageLoader.h"
#include "Renderer/ShaderLoader.h"
#include "Vulkan/Buffer.h"
#include "Vulkan/CommandBuffer.h"
#include "Vulkan/GraphicShader.h"
#include "Vulkan/Image.h"
#include "Vulkan/Sampler.h"

#include "Renderer/Camera.h"

#include <filesystem>
#include <memory>
#include <string_view>

namespace luna::samples {

struct MeshPushConstants {
    glm::mat4 m_model{1.0f};
    glm::mat4 m_view_projection{1.0f};
};

struct UploadedTexture {
    VulkanAbstractionLayer::Image m_image;
    VulkanAbstractionLayer::Sampler m_sampler;
    VulkanAbstractionLayer::Buffer m_staging_buffer;
    bool m_uploaded{false};

    void uploadIfNeeded(VulkanAbstractionLayer::CommandBuffer& commands);
};

std::filesystem::path projectRoot();
std::filesystem::path assetPath(std::string_view relative_path);

std::shared_ptr<VulkanAbstractionLayer::GraphicShader> loadGraphicsShader(
    const std::filesystem::path& vertex_path,
    const std::filesystem::path& fragment_path);

glm::mat4 buildViewProjection(
    const Camera& camera,
    float aspect_ratio,
    float fov_degrees = 45.0f,
    float near_plane = 0.1f,
    float far_plane = 100.0f);

UploadedTexture createTexture(const VulkanAbstractionLayer::ImageData& image_data, bool generate_mipmaps = true);
void updateCombinedImageSamplerDescriptor(
    const vk::DescriptorSet& descriptor_set,
    uint32_t binding,
    const VulkanAbstractionLayer::Image& image,
    const VulkanAbstractionLayer::Sampler& sampler);

} // namespace luna::samples
