#include "Asset/Editor/TextureLoader.h"

#include "Project/ProjectManager.h"
#include "TextureImporter.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <glm/vec4.hpp>

namespace luna::texture_loader_detail {

using FilterMode = rhi::Texture::FilterMode;
using ImportSettings = rhi::Texture::ImportSettings;
using MipFilterMode = rhi::Texture::MipFilterMode;
using WrapMode = rhi::Texture::WrapMode;

FilterMode parseFilterMode(const YAML::Node& node, FilterMode default_value)
{
    if (!node) {
        return default_value;
    }

    return node.as<std::string>() == "Nearest" ? FilterMode::Nearest : FilterMode::Linear;
}

MipFilterMode parseMipFilterMode(const YAML::Node& node, MipFilterMode default_value)
{
    if (!node) {
        return default_value;
    }

    const std::string value = node.as<std::string>();
    if (value == "None") {
        return MipFilterMode::None;
    }
    if (value == "Nearest") {
        return MipFilterMode::Nearest;
    }
    return MipFilterMode::Linear;
}

WrapMode parseWrapMode(const YAML::Node& node, WrapMode default_value)
{
    if (!node) {
        return default_value;
    }

    const std::string value = node.as<std::string>();
    if (value == "MirroredRepeat") {
        return WrapMode::MirroredRepeat;
    }
    if (value == "ClampToEdge") {
        return WrapMode::ClampToEdge;
    }
    if (value == "ClampToBorder") {
        return WrapMode::ClampToBorder;
    }
    if (value == "MirrorClampToEdge") {
        return WrapMode::MirrorClampToEdge;
    }
    return WrapMode::Repeat;
}

bool isSrgbCapableFormat(luna::RHI::Format format)
{
    using Format = luna::RHI::Format;

    switch (format) {
        case Format::RGBA8_UNORM:
        case Format::RGBA8_SRGB:
        case Format::BGRA8_UNORM:
        case Format::BGRA8_SRGB:
        case Format::BC1_RGB_UNORM:
        case Format::BC1_RGB_SRGB:
        case Format::BC1_RGBA_UNORM:
        case Format::BC1_RGBA_SRGB:
        case Format::BC2_UNORM:
        case Format::BC2_SRGB:
        case Format::BC3_UNORM:
        case Format::BC3_SRGB:
        case Format::BC7_UNORM:
        case Format::BC7_SRGB:
            return true;
        default:
            return false;
    }
}

luna::RHI::Format toSrgbFormat(luna::RHI::Format format)
{
    using Format = luna::RHI::Format;

    switch (format) {
        case Format::RGBA8_UNORM:
            return Format::RGBA8_SRGB;
        case Format::BGRA8_UNORM:
            return Format::BGRA8_SRGB;
        case Format::BC1_RGB_UNORM:
            return Format::BC1_RGB_SRGB;
        case Format::BC1_RGBA_UNORM:
            return Format::BC1_RGBA_SRGB;
        case Format::BC2_UNORM:
            return Format::BC2_SRGB;
        case Format::BC3_UNORM:
            return Format::BC3_SRGB;
        case Format::BC7_UNORM:
            return Format::BC7_SRGB;
        default:
            return format;
    }
}

luna::RHI::Format toLinearFormat(luna::RHI::Format format)
{
    using Format = luna::RHI::Format;

    switch (format) {
        case Format::RGBA8_SRGB:
            return Format::RGBA8_UNORM;
        case Format::BGRA8_SRGB:
            return Format::BGRA8_UNORM;
        case Format::BC1_RGB_SRGB:
            return Format::BC1_RGB_UNORM;
        case Format::BC1_RGBA_SRGB:
            return Format::BC1_RGBA_UNORM;
        case Format::BC2_SRGB:
            return Format::BC2_UNORM;
        case Format::BC3_SRGB:
            return Format::BC3_UNORM;
        case Format::BC7_SRGB:
            return Format::BC7_UNORM;
        default:
            return format;
    }
}

ImportSettings parseImportSettings(const YAML::Node& config, const std::filesystem::path& texture_path)
{
    const YAML::Node default_config = texture_importer_detail::makeDefaultTextureConfig(texture_path);

    auto get_node = [&](const char* key) -> YAML::Node {
        if (config && config[key]) {
            return config[key];
        }
        return default_config[key];
    };

    ImportSettings settings;
    settings.GenerateMipmaps = get_node("GenerateMipmaps").as<bool>();
    settings.SRGB = get_node("SRGB").as<bool>();
    settings.Sampler.MinFilter = parseFilterMode(get_node("MinFilter"), settings.Sampler.MinFilter);
    settings.Sampler.MagFilter = parseFilterMode(get_node("MagFilter"), settings.Sampler.MagFilter);
    settings.Sampler.MipFilter = parseMipFilterMode(get_node("MipFilter"), settings.Sampler.MipFilter);
    settings.Sampler.WrapU = parseWrapMode(get_node("WrapU"), settings.Sampler.WrapU);
    settings.Sampler.WrapV = parseWrapMode(get_node("WrapV"), settings.Sampler.WrapV);
    settings.Sampler.WrapW = parseWrapMode(get_node("WrapW"), settings.Sampler.WrapW);
    return settings;
}

AssetMetadata resolveTextureMetadata(const std::filesystem::path& texture_path, std::string_view asset_name)
{
    TextureImporter importer;
    const auto meta_path = importer_detail::getMetadataPath(texture_path);
    AssetMetadata metadata =
        std::filesystem::exists(meta_path) ? importer.deserializeMetadata(meta_path) : importer.import(texture_path);

    if (!asset_name.empty()) {
        metadata.Name = std::string(asset_name);
    }
    if (!metadata.SpecializedConfig || !metadata.SpecializedConfig.IsDefined()) {
        metadata.SpecializedConfig = texture_importer_detail::makeDefaultTextureConfig(texture_path);
    }
    return metadata;
}

bool isByteColorFormat(luna::RHI::Format format)
{
    using Format = luna::RHI::Format;

    switch (format) {
        case Format::RGBA8_UNORM:
        case Format::RGBA8_SRGB:
        case Format::BGRA8_UNORM:
        case Format::BGRA8_SRGB:
            return true;
        default:
            return false;
    }
}

bool isFloatColorFormat(luna::RHI::Format format)
{
    return format == luna::RHI::Format::RGBA32_FLOAT;
}

rhi::ImageData generateMipChain(const rhi::ImageData& source)
{
    if (!source.isValid()) {
        return source;
    }

    if (!isByteColorFormat(source.ImageFormat) && !isFloatColorFormat(source.ImageFormat)) {
        return source;
    }

    rhi::ImageData result = source;
    result.MipLevels.clear();

    if (isByteColorFormat(source.ImageFormat)) {
        uint32_t previous_width = source.Width;
        uint32_t previous_height = source.Height;
        std::vector<uint8_t> previous_level = source.ByteData;

        while (previous_width > 1 || previous_height > 1) {
            const uint32_t next_width = (std::max)(previous_width / 2, 1u);
            const uint32_t next_height = (std::max)(previous_height / 2, 1u);
            std::vector<uint8_t> next_level(static_cast<size_t>(next_width) * static_cast<size_t>(next_height) * 4u, 0);

            for (uint32_t y = 0; y < next_height; ++y) {
                for (uint32_t x = 0; x < next_width; ++x) {
                    uint32_t sum[4] = {0, 0, 0, 0};
                    for (uint32_t sample_y = 0; sample_y < 2; ++sample_y) {
                        for (uint32_t sample_x = 0; sample_x < 2; ++sample_x) {
                            const uint32_t source_x = (std::min)(previous_width - 1, x * 2 + sample_x);
                            const uint32_t source_y = (std::min)(previous_height - 1, y * 2 + sample_y);
                            const size_t source_index =
                                (static_cast<size_t>(source_y) * previous_width + source_x) * static_cast<size_t>(4);
                            for (size_t channel = 0; channel < 4; ++channel) {
                                sum[channel] += previous_level[source_index + channel];
                            }
                        }
                    }

                    const size_t dest_index = (static_cast<size_t>(y) * next_width + x) * static_cast<size_t>(4);
                    for (size_t channel = 0; channel < 4; ++channel) {
                        next_level[dest_index + channel] = static_cast<uint8_t>(sum[channel] / 4u);
                    }
                }
            }

            result.MipLevels.push_back(next_level);
            previous_level = std::move(next_level);
            previous_width = next_width;
            previous_height = next_height;
        }

        return result;
    }

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

rhi::ImageData applyImportSettings(rhi::ImageData image_data, const ImportSettings& import_settings)
{
    if (!image_data.isValid()) {
        return image_data;
    }

    if (isSrgbCapableFormat(image_data.ImageFormat)) {
        image_data.ImageFormat = import_settings.SRGB ? toSrgbFormat(image_data.ImageFormat)
                                                      : toLinearFormat(image_data.ImageFormat);
    }

    if (!import_settings.GenerateMipmaps) {
        image_data.MipLevels.clear();
        return image_data;
    }

    if (image_data.MipLevels.empty()) {
        image_data = generateMipChain(image_data);
    }

    return image_data;
}

} // namespace luna::texture_loader_detail

namespace luna {

std::shared_ptr<Asset> TextureLoader::load(const AssetMetadata& meta_data)
{
    const auto project_root_path = ProjectManager::instance().getProjectRootPath();
    if (!project_root_path) {
        return {};
    }

    return loadFromMetadata(*project_root_path / meta_data.FilePath, meta_data);
}

std::shared_ptr<rhi::Texture> TextureLoader::loadFromFile(const std::filesystem::path& path, std::string asset_name)
{
    return loadFromMetadata(path, texture_loader_detail::resolveTextureMetadata(path, asset_name));
}

std::shared_ptr<rhi::Texture> TextureLoader::loadFromMetadata(const std::filesystem::path& path,
                                                              const AssetMetadata& meta_data)
{
    rhi::ImageData image_data = rhi::ImageLoader::LoadImageFromFile(path.string());
    if (!image_data.isValid()) {
        return {};
    }

    const auto import_settings = texture_loader_detail::parseImportSettings(meta_data.SpecializedConfig, path);
    image_data = texture_loader_detail::applyImportSettings(std::move(image_data), import_settings);

    return rhi::Texture::create(meta_data.Name.empty() ? path.stem().string() : meta_data.Name,
                                std::move(image_data),
                                import_settings);
}

} // namespace luna
