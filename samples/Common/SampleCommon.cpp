#include "samples/Common/SampleCommon.h"

#include "Renderer/ShaderLoader.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/VectorMath.h"

#include <algorithm>
#include <functional>

namespace {

VulkanAbstractionLayer::ImageData makeFallbackImageData()
{
    VulkanAbstractionLayer::ImageData image_data;
    image_data.ImageFormat = VulkanAbstractionLayer::Format::R8G8B8A8_UNORM;
    image_data.Width = 1;
    image_data.Height = 1;
    image_data.ByteData = {255, 255, 255, 255};
    return image_data;
}

} // namespace

namespace luna::samples {

void UploadedTexture::uploadIfNeeded(VulkanAbstractionLayer::CommandBuffer& commands)
{
    if (m_uploaded || m_staging_buffer.GetByteSize() == 0) {
        return;
    }

    commands.CopyBufferToImage(
        VulkanAbstractionLayer::BufferInfo{std::cref(m_staging_buffer), 0},
        VulkanAbstractionLayer::ImageInfo{std::cref(m_image), VulkanAbstractionLayer::ImageUsage::UNKNOWN, 0, 0});

    if (m_image.GetMipLevelCount() > 1) {
        commands.GenerateMipLevels(
            m_image,
            VulkanAbstractionLayer::ImageUsage::TRANSFER_DISTINATION,
            VulkanAbstractionLayer::BlitFilter::LINEAR);
    }

    commands.TransferLayout(
        m_image,
        VulkanAbstractionLayer::ImageUsage::TRANSFER_DISTINATION,
        VulkanAbstractionLayer::ImageUsage::SHADER_READ);

    m_staging_buffer = {};
    m_uploaded = true;
}

std::filesystem::path projectRoot()
{
    return std::filesystem::path(LUNA_PROJECT_ROOT);
}

std::filesystem::path assetPath(std::string_view relative_path)
{
    return projectRoot() / "assets" / std::filesystem::path(relative_path);
}

std::shared_ptr<VulkanAbstractionLayer::GraphicShader> loadGraphicsShader(
    const std::filesystem::path& vertex_path,
    const std::filesystem::path& fragment_path)
{
    auto shader = std::make_shared<VulkanAbstractionLayer::GraphicShader>();
    shader->Init(
        VulkanAbstractionLayer::ShaderLoader::LoadFromSourceFile(
            vertex_path.string(),
            VulkanAbstractionLayer::ShaderType::VERTEX,
            VulkanAbstractionLayer::ShaderLanguage::GLSL),
        VulkanAbstractionLayer::ShaderLoader::LoadFromSourceFile(
            fragment_path.string(),
            VulkanAbstractionLayer::ShaderType::FRAGMENT,
            VulkanAbstractionLayer::ShaderLanguage::GLSL));
    return shader;
}

glm::mat4 buildViewProjection(
    const Camera& camera,
    float aspect_ratio,
    float fov_degrees,
    float near_plane,
    float far_plane)
{
    const float clamped_aspect_ratio = std::max(aspect_ratio, 0.001f);
    const glm::mat4 projection = VulkanAbstractionLayer::MakePerspectiveMatrix(
        VulkanAbstractionLayer::ToRadians(fov_degrees),
        clamped_aspect_ratio,
        near_plane,
        far_plane);
    return projection * camera.getViewMatrix();
}

UploadedTexture createTexture(const VulkanAbstractionLayer::ImageData& image_data, bool generate_mipmaps)
{
    const VulkanAbstractionLayer::ImageData& source_image =
        image_data.ByteData.empty() ? makeFallbackImageData() : image_data;

    const VulkanAbstractionLayer::ImageOptions::Value image_options =
        generate_mipmaps ? VulkanAbstractionLayer::ImageOptions::MIPMAPS : VulkanAbstractionLayer::ImageOptions::DEFAULT;
    const VulkanAbstractionLayer::ImageUsage::Value image_usage =
        VulkanAbstractionLayer::ImageUsage::TRANSFER_DISTINATION |
        VulkanAbstractionLayer::ImageUsage::SHADER_READ |
        (generate_mipmaps ? VulkanAbstractionLayer::ImageUsage::TRANSFER_SOURCE : 0u);

    UploadedTexture texture{
        .m_image = VulkanAbstractionLayer::Image(
            source_image.Width,
            source_image.Height,
            source_image.ImageFormat,
            image_usage,
            VulkanAbstractionLayer::MemoryUsage::GPU_ONLY,
            image_options),
        .m_sampler = VulkanAbstractionLayer::Sampler(
            VulkanAbstractionLayer::Sampler::MinFilter::LINEAR,
            VulkanAbstractionLayer::Sampler::MagFilter::LINEAR,
            VulkanAbstractionLayer::Sampler::AddressMode::REPEAT,
            VulkanAbstractionLayer::Sampler::MipFilter::LINEAR),
        .m_staging_buffer = VulkanAbstractionLayer::Buffer(
            source_image.ByteData.size(),
            VulkanAbstractionLayer::BufferUsage::TRANSFER_SOURCE,
            VulkanAbstractionLayer::MemoryUsage::CPU_TO_GPU),
    };

    texture.m_staging_buffer.CopyData(source_image.ByteData.data(), source_image.ByteData.size(), 0);
    return texture;
}

void updateCombinedImageSamplerDescriptor(
    const vk::DescriptorSet& descriptor_set,
    uint32_t binding,
    const VulkanAbstractionLayer::Image& image,
    const VulkanAbstractionLayer::Sampler& sampler)
{
    const vk::DescriptorImageInfo descriptor_image_info{
        sampler.GetNativeHandle(),
        image.GetNativeView(VulkanAbstractionLayer::ImageView::NATIVE),
        VulkanAbstractionLayer::ImageUsageToImageLayout(VulkanAbstractionLayer::ImageUsage::SHADER_READ),
    };

    vk::WriteDescriptorSet write_descriptor_set;
    write_descriptor_set
        .setDstSet(descriptor_set)
        .setDstBinding(binding)
        .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
        .setDescriptorCount(1)
        .setPImageInfo(&descriptor_image_info);

    VulkanAbstractionLayer::GetCurrentVulkanContext().GetDevice().updateDescriptorSets(write_descriptor_set, {});
}

} // namespace luna::samples
