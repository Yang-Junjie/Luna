#include "Renderer/ImageLoader.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stb_image.h>
#include <string_view>

#define TINYDDSLOADER_IMPLEMENTATION
#include "third_party/tinyddsloader/tinyddsloader.h"

namespace luna::rhi {
namespace {

bool isDDSImage(const std::string& filepath)
{
    return std::filesystem::path(filepath).extension() == ".dds";
}

bool isHDRImage(const std::string& filepath)
{
    return std::filesystem::path(filepath).extension() == ".hdr";
}

bool isZLIBImage(const std::string& filepath)
{
    return std::filesystem::path(filepath).extension() == ".zlib";
}

bool isDDSMimeType(const std::string_view mime_type)
{
    return mime_type == "image/vnd-ms.dds";
}

bool isDDSData(const uint8_t* data, std::size_t size)
{
    return data != nullptr && size >= 4 && data[0] == 'D' && data[1] == 'D' && data[2] == 'S' && data[3] == ' ';
}

luna::RHI::Format toImageFormat(tinyddsloader::DDSFile::DXGIFormat format)
{
    using DXGIFormat = tinyddsloader::DDSFile::DXGIFormat;
    using Format = luna::RHI::Format;

    switch (format) {
        case DXGIFormat::R32G32B32A32_Float:
            return Format::RGBA32_FLOAT;
        case DXGIFormat::R32G32B32A32_UInt:
            return Format::RGBA32_UINT;
        case DXGIFormat::R32G32B32A32_SInt:
            return Format::RGBA32_SINT;
        case DXGIFormat::R32G32B32_Float:
            return Format::RGB32_FLOAT;
        case DXGIFormat::R32G32B32_UInt:
            return Format::RGB32_UINT;
        case DXGIFormat::R32G32B32_SInt:
            return Format::RGB32_SINT;
        case DXGIFormat::R16G16B16A16_Float:
            return Format::RGBA16_FLOAT;
        case DXGIFormat::R16G16B16A16_UNorm:
            return Format::RGBA16_UNORM;
        case DXGIFormat::R16G16B16A16_UInt:
            return Format::RGBA16_UINT;
        case DXGIFormat::R16G16B16A16_SNorm:
            return Format::RGBA16_SNORM;
        case DXGIFormat::R16G16B16A16_SInt:
            return Format::RGBA16_SINT;
        case DXGIFormat::R32G32_Float:
            return Format::RG32_FLOAT;
        case DXGIFormat::R32G32_UInt:
            return Format::RG32_UINT;
        case DXGIFormat::R32G32_SInt:
            return Format::RG32_SINT;
        case DXGIFormat::R10G10B10A2_UNorm:
            return Format::RGB10A2_UNORM;
        case DXGIFormat::R10G10B10A2_UInt:
            return Format::RGB10A2_UINT;
        case DXGIFormat::R11G11B10_Float:
            return Format::RG11B10_FLOAT;
        case DXGIFormat::R8G8B8A8_UNorm:
            return Format::RGBA8_UNORM;
        case DXGIFormat::R8G8B8A8_UNorm_SRGB:
            return Format::RGBA8_SRGB;
        case DXGIFormat::R8G8B8A8_UInt:
            return Format::RGBA8_UINT;
        case DXGIFormat::R8G8B8A8_SNorm:
            return Format::RGBA8_SNORM;
        case DXGIFormat::R8G8B8A8_SInt:
            return Format::RGBA8_SINT;
        case DXGIFormat::R16G16_Float:
            return Format::RG16_FLOAT;
        case DXGIFormat::R16G16_UNorm:
            return Format::RG16_UNORM;
        case DXGIFormat::R16G16_UInt:
            return Format::RG16_UINT;
        case DXGIFormat::R16G16_SNorm:
            return Format::RG16_SNORM;
        case DXGIFormat::R16G16_SInt:
            return Format::RG16_SINT;
        case DXGIFormat::D32_Float:
            return Format::D32_FLOAT;
        case DXGIFormat::R32_Float:
            return Format::R32_FLOAT;
        case DXGIFormat::R32_UInt:
            return Format::R32_UINT;
        case DXGIFormat::R32_SInt:
            return Format::R32_SINT;
        case DXGIFormat::D24_UNorm_S8_UInt:
            return Format::D24_UNORM_S8_UINT;
        case DXGIFormat::R8G8_UNorm:
            return Format::RG8_UNORM;
        case DXGIFormat::R8G8_UInt:
            return Format::RG8_UINT;
        case DXGIFormat::R8G8_SNorm:
            return Format::RG8_SNORM;
        case DXGIFormat::R8G8_SInt:
            return Format::RG8_SINT;
        case DXGIFormat::R16_Float:
            return Format::R16_FLOAT;
        case DXGIFormat::D16_UNorm:
            return Format::D16_UNORM;
        case DXGIFormat::R16_UNorm:
            return Format::R16_UNORM;
        case DXGIFormat::R16_UInt:
            return Format::R16_UINT;
        case DXGIFormat::R16_SNorm:
            return Format::R16_SNORM;
        case DXGIFormat::R16_SInt:
            return Format::R16_SINT;
        case DXGIFormat::R8_UNorm:
            return Format::R8_UNORM;
        case DXGIFormat::R8_UInt:
            return Format::R8_UINT;
        case DXGIFormat::R8_SNorm:
            return Format::R8_SNORM;
        case DXGIFormat::R8_SInt:
            return Format::R8_SINT;
        case DXGIFormat::R9G9B9E5_SHAREDEXP:
            return Format::RGB9E5_FLOAT;
        case DXGIFormat::B8G8R8A8_UNorm:
            return Format::BGRA8_UNORM;
        case DXGIFormat::B8G8R8A8_UNorm_SRGB:
            return Format::BGRA8_SRGB;
        case DXGIFormat::BC1_UNorm:
            return Format::BC1_RGBA_UNORM;
        case DXGIFormat::BC1_UNorm_SRGB:
            return Format::BC1_RGBA_SRGB;
        case DXGIFormat::BC2_UNorm:
            return Format::BC2_UNORM;
        case DXGIFormat::BC2_UNorm_SRGB:
            return Format::BC2_SRGB;
        case DXGIFormat::BC3_UNorm:
            return Format::BC3_UNORM;
        case DXGIFormat::BC3_UNorm_SRGB:
            return Format::BC3_SRGB;
        case DXGIFormat::BC4_UNorm:
            return Format::BC4_UNORM;
        case DXGIFormat::BC4_SNorm:
            return Format::BC4_SNORM;
        case DXGIFormat::BC5_UNorm:
            return Format::BC5_UNORM;
        case DXGIFormat::BC5_SNorm:
            return Format::BC5_SNORM;
        case DXGIFormat::BC6H_UF16:
            return Format::BC6H_UFLOAT;
        case DXGIFormat::BC6H_SF16:
            return Format::BC6H_SFLOAT;
        case DXGIFormat::BC7_UNorm:
            return Format::BC7_UNORM;
        case DXGIFormat::BC7_UNorm_SRGB:
            return Format::BC7_SRGB;
        default:
            return Format::UNDEFINED;
    }
}

ImageData loadImageUsingDDSLoader(const std::string& filepath)
{
    ImageData image;
    tinyddsloader::DDSFile dds;
    if (dds.Load(filepath.c_str()) != tinyddsloader::Result::Success) {
        return image;
    }

    dds.Flip();
    const auto* image_data = dds.GetImageData();
    if (image_data == nullptr) {
        return image;
    }

    image.Width = image_data->m_width;
    image.Height = image_data->m_height;
    image.ImageFormat = toImageFormat(dds.GetFormat());
    image.ByteData.resize(image_data->m_memSlicePitch);
    std::copy_n(static_cast<const uint8_t*>(image_data->m_mem), image.ByteData.size(), image.ByteData.begin());

    for (uint32_t mip = 1; mip < dds.GetMipCount(); ++mip) {
        const auto* mip_image_data = dds.GetImageData(mip);
        if (mip_image_data == nullptr) {
            continue;
        }

        auto& mip_level = image.MipLevels.emplace_back();
        mip_level.resize(mip_image_data->m_memSlicePitch);
        std::copy_n(static_cast<const uint8_t*>(mip_image_data->m_mem), mip_level.size(), mip_level.begin());
    }

    return image;
}

ImageData loadImageUsingDDSLoader(const uint8_t* data, std::size_t size)
{
    ImageData image;
    tinyddsloader::DDSFile dds;
    if (dds.Load(data, size) != tinyddsloader::Result::Success) {
        return image;
    }

    dds.Flip();
    const auto* image_data = dds.GetImageData();
    if (image_data == nullptr) {
        return image;
    }

    image.Width = image_data->m_width;
    image.Height = image_data->m_height;
    image.ImageFormat = toImageFormat(dds.GetFormat());
    image.ByteData.resize(image_data->m_memSlicePitch);
    std::copy_n(static_cast<const uint8_t*>(image_data->m_mem), image.ByteData.size(), image.ByteData.begin());

    for (uint32_t mip = 1; mip < dds.GetMipCount(); ++mip) {
        const auto* mip_image_data = dds.GetImageData(mip);
        if (mip_image_data == nullptr) {
            continue;
        }

        auto& mip_level = image.MipLevels.emplace_back();
        mip_level.resize(mip_image_data->m_memSlicePitch);
        std::copy_n(static_cast<const uint8_t*>(mip_image_data->m_mem), mip_level.size(), mip_level.begin());
    }

    return image;
}

std::string convertZLIBToDDS(const std::string& filepath)
{
    std::ifstream file(filepath, std::ios::binary);
    if (!file.good()) {
        return filepath;
    }

    std::vector<char> compressed_data{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};

    int decompressed_size = 0;
    char* decompressed_data =
        stbi_zlib_decode_malloc(compressed_data.data(), static_cast<int>(compressed_data.size()), &decompressed_size);
    if (decompressed_data == nullptr || decompressed_size <= 0) {
        return filepath;
    }

    const std::filesystem::path path(filepath);
    const auto dds_filepath = (path.parent_path() / path.stem()).string() + ".dds";
    std::ofstream dds_file(dds_filepath, std::ios::binary);
    dds_file.write(decompressed_data, decompressed_size);
    std::free(decompressed_data);

    return dds_filepath;
}

ImageData loadImageUsingSTBLoader(const std::string& filepath)
{
    int width = 0;
    int height = 0;
    int channels = 0;

    stbi_uc* pixels = stbi_load(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr || width <= 0 || height <= 0) {
        return {};
    }

    constexpr int actual_channels = 4;
    std::vector<uint8_t> byte_data(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) *
                                   actual_channels);
    std::copy_n(pixels, byte_data.size(), byte_data.begin());
    stbi_image_free(pixels);

    return ImageData{
        .ByteData = std::move(byte_data),
        .ImageFormat = luna::RHI::Format::RGBA8_UNORM,
        .Width = static_cast<uint32_t>(width),
        .Height = static_cast<uint32_t>(height),
    };
}

ImageData loadHDRImageUsingSTBLoader(const std::string& filepath)
{
    int width = 0;
    int height = 0;
    int channels = 0;

    stbi_set_flip_vertically_on_load(false);
    float* pixels = stbi_loadf(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr || width <= 0 || height <= 0) {
        return {};
    }

    constexpr int actual_channels = 4;
    std::vector<uint8_t> byte_data(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) *
                                   actual_channels * sizeof(float));
    std::memcpy(byte_data.data(), pixels, byte_data.size());
    stbi_image_free(pixels);

    return ImageData{
        .ByteData = std::move(byte_data),
        .ImageFormat = luna::RHI::Format::RGBA32_FLOAT,
        .Width = static_cast<uint32_t>(width),
        .Height = static_cast<uint32_t>(height),
    };
}

ImageData loadImageUsingSTBLoader(const uint8_t* data, std::size_t size)
{
    int width = 0;
    int height = 0;
    int channels = 0;

    stbi_uc* pixels = stbi_load_from_memory(data, static_cast<int>(size), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr || width <= 0 || height <= 0) {
        return {};
    }

    constexpr int actual_channels = 4;
    std::vector<uint8_t> byte_data(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) *
                                   actual_channels);
    std::copy_n(pixels, byte_data.size(), byte_data.begin());
    stbi_image_free(pixels);

    return ImageData{
        .ByteData = std::move(byte_data),
        .ImageFormat = luna::RHI::Format::RGBA8_UNORM,
        .Width = static_cast<uint32_t>(width),
        .Height = static_cast<uint32_t>(height),
    };
}

ImageData loadHDRImageUsingSTBLoader(const uint8_t* data, std::size_t size)
{
    int width = 0;
    int height = 0;
    int channels = 0;

    stbi_set_flip_vertically_on_load(false);
    float* pixels = stbi_loadf_from_memory(data, static_cast<int>(size), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr || width <= 0 || height <= 0) {
        return {};
    }

    constexpr int actual_channels = 4;
    std::vector<uint8_t> byte_data(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) *
                                   actual_channels * sizeof(float));
    std::memcpy(byte_data.data(), pixels, byte_data.size());
    stbi_image_free(pixels);

    return ImageData{
        .ByteData = std::move(byte_data),
        .ImageFormat = luna::RHI::Format::RGBA32_FLOAT,
        .Width = static_cast<uint32_t>(width),
        .Height = static_cast<uint32_t>(height),
    };
}

std::vector<uint8_t> extractCubemapFace(const ImageData& image,
                                        std::size_t face_width,
                                        std::size_t face_height,
                                        std::size_t channel_count,
                                        std::size_t slice_x,
                                        std::size_t slice_y)
{
    std::vector<uint8_t> result(face_width * face_height * channel_count);

    for (std::size_t row = 0; row < face_height; ++row) {
        const std::size_t image_y = (face_height - row - 1) + slice_y * face_height;
        const std::size_t image_x = slice_x * face_width;
        const std::size_t bytes_in_row = face_width * channel_count;

        std::memcpy(result.data() + row * bytes_in_row,
                    image.ByteData.data() + (image_y * image.Width + image_x) * channel_count,
                    bytes_in_row);
    }

    return result;
}

CubemapData createCubemapFromSingleImage(const ImageData& image)
{
    CubemapData cubemap_data;
    cubemap_data.FaceFormat = image.ImageFormat;
    cubemap_data.FaceWidth = image.Width / 4;
    cubemap_data.FaceHeight = image.Height / 3;

    assert(cubemap_data.FaceWidth == cubemap_data.FaceHeight);
    assert(cubemap_data.FaceFormat == luna::RHI::Format::RGBA8_UNORM);

    constexpr std::size_t channel_count = 4;
    cubemap_data.Faces[0] =
        extractCubemapFace(image, cubemap_data.FaceWidth, cubemap_data.FaceHeight, channel_count, 2, 1);
    cubemap_data.Faces[1] =
        extractCubemapFace(image, cubemap_data.FaceWidth, cubemap_data.FaceHeight, channel_count, 0, 1);
    cubemap_data.Faces[2] =
        extractCubemapFace(image, cubemap_data.FaceWidth, cubemap_data.FaceHeight, channel_count, 1, 2);
    cubemap_data.Faces[3] =
        extractCubemapFace(image, cubemap_data.FaceWidth, cubemap_data.FaceHeight, channel_count, 1, 0);
    cubemap_data.Faces[4] =
        extractCubemapFace(image, cubemap_data.FaceWidth, cubemap_data.FaceHeight, channel_count, 1, 1);
    cubemap_data.Faces[5] =
        extractCubemapFace(image, cubemap_data.FaceWidth, cubemap_data.FaceHeight, channel_count, 3, 1);
    return cubemap_data;
}

} // namespace

ImageData ImageLoader::LoadImageFromFile(const std::string& filepath)
{
    if (isDDSImage(filepath)) {
        return loadImageUsingDDSLoader(filepath);
    }
    if (isHDRImage(filepath)) {
        return loadHDRImageUsingSTBLoader(filepath);
    }
    if (isZLIBImage(filepath)) {
        return loadImageUsingDDSLoader(convertZLIBToDDS(filepath));
    }
    return loadImageUsingSTBLoader(filepath);
}

ImageData ImageLoader::LoadImageFromMemory(const uint8_t* data, std::size_t size, const std::string& mimeType)
{
    if (data == nullptr || size == 0) {
        return {};
    }

    if (isDDSMimeType(mimeType) || isDDSData(data, size)) {
        return loadImageUsingDDSLoader(data, size);
    }
    if (stbi_is_hdr_from_memory(data, static_cast<int>(size)) != 0) {
        return loadHDRImageUsingSTBLoader(data, size);
    }

    return loadImageUsingSTBLoader(data, size);
}

CubemapData ImageLoader::LoadCubemapImageFromFile(const std::string& filepath)
{
    return createCubemapFromSingleImage(LoadImageFromFile(filepath));
}

} // namespace luna::rhi
