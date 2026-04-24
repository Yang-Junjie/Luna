#include "Asset/Editor/ImageLoader.h"

#include <cstring>

#include <filesystem>
#include <stb_image.h>
#include <string_view>

#define TINYDDSLOADER_IMPLEMENTATION
#include "third_party/tinyddsloader/tinyddsloader.h"

namespace luna {
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
        case DXGIFormat::R32_Float:
            return Format::R32_FLOAT;
        case DXGIFormat::R32_UInt:
            return Format::R32_UINT;
        case DXGIFormat::R32_SInt:
            return Format::R32_SINT;
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
        case DXGIFormat::B8G8R8A8_UNorm:
            return Format::BGRA8_UNORM;
        case DXGIFormat::B8G8R8A8_UNorm_SRGB:
            return Format::BGRA8_SRGB;
        default:
            return Format::UNDEFINED;
    }
}

ImageData loadImageUsingDDSLoader(const std::string& filepath)
{
    tinyddsloader::DDSFile dds;
    ImageData image;

    if (dds.Load(filepath.c_str()) != tinyddsloader::Result::Success) {
        return image;
    }

    const auto* image_data = dds.GetImageData();
    if (image_data == nullptr || image_data->m_memSlicePitch == 0) {
        return image;
    }

    image.ImageFormat = toImageFormat(dds.GetFormat());
    image.Width = image_data->m_width;
    image.Height = image_data->m_height;
    const auto* base_memory = static_cast<const uint8_t*>(image_data->m_mem);
    image.ByteData.assign(base_memory, base_memory + image_data->m_memSlicePitch);

    for (uint32_t mip = 1; mip < dds.GetMipCount(); ++mip) {
        const auto* mip_image_data = dds.GetImageData(mip);
        if (mip_image_data == nullptr || mip_image_data->m_memSlicePitch == 0) {
            continue;
        }

        const auto* mip_memory = static_cast<const uint8_t*>(mip_image_data->m_mem);
        image.MipLevels.emplace_back(mip_memory, mip_memory + mip_image_data->m_memSlicePitch);
    }

    return image;
}

ImageData loadImageUsingDDSLoader(const uint8_t* data, std::size_t size)
{
    tinyddsloader::DDSFile dds;
    ImageData image;

    if (dds.Load(data, size) != tinyddsloader::Result::Success) {
        return image;
    }

    const auto* image_data = dds.GetImageData();
    if (image_data == nullptr || image_data->m_memSlicePitch == 0) {
        return image;
    }

    image.ImageFormat = toImageFormat(dds.GetFormat());
    image.Width = image_data->m_width;
    image.Height = image_data->m_height;
    const auto* base_memory = static_cast<const uint8_t*>(image_data->m_mem);
    image.ByteData.assign(base_memory, base_memory + image_data->m_memSlicePitch);

    for (uint32_t mip = 1; mip < dds.GetMipCount(); ++mip) {
        const auto* mip_image_data = dds.GetImageData(mip);
        if (mip_image_data == nullptr || mip_image_data->m_memSlicePitch == 0) {
            continue;
        }

        const auto* mip_memory = static_cast<const uint8_t*>(mip_image_data->m_mem);
        image.MipLevels.emplace_back(mip_memory, mip_memory + mip_image_data->m_memSlicePitch);
    }

    return image;
}

ImageData loadImageUsingSTBLoader(const std::string& filepath)
{
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr) {
        return {};
    }

    const size_t byte_size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
    std::vector<uint8_t> bytes(byte_size);
    std::memcpy(bytes.data(), pixels, byte_size);
    stbi_image_free(pixels);

    return ImageData{
        .ByteData = std::move(bytes),
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
    float* pixels = stbi_loadf(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr) {
        return {};
    }

    const size_t byte_size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u * sizeof(float);
    std::vector<uint8_t> bytes(byte_size);
    std::memcpy(bytes.data(), pixels, byte_size);
    stbi_image_free(pixels);

    return ImageData{
        .ByteData = std::move(bytes),
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
    if (pixels == nullptr) {
        return {};
    }

    const size_t byte_size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
    std::vector<uint8_t> bytes(byte_size);
    std::memcpy(bytes.data(), pixels, byte_size);
    stbi_image_free(pixels);

    return ImageData{
        .ByteData = std::move(bytes),
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
    float* pixels = stbi_loadf_from_memory(data, static_cast<int>(size), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr) {
        return {};
    }

    const size_t byte_size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u * sizeof(float);
    std::vector<uint8_t> bytes(byte_size);
    std::memcpy(bytes.data(), pixels, byte_size);
    stbi_image_free(pixels);

    return ImageData{
        .ByteData = std::move(bytes),
        .ImageFormat = luna::RHI::Format::RGBA32_FLOAT,
        .Width = static_cast<uint32_t>(width),
        .Height = static_cast<uint32_t>(height),
    };
}

std::vector<uint8_t> extractCubemapFace(const ImageData& image,
                                        uint32_t face_x,
                                        uint32_t face_y,
                                        uint32_t face_width,
                                        uint32_t face_height)
{
    const size_t pixel_stride =
        image.ImageFormat == luna::RHI::Format::RGBA32_FLOAT ? sizeof(float) * 4u : sizeof(uint8_t) * 4u;
    std::vector<uint8_t> face_bytes(static_cast<size_t>(face_width) * static_cast<size_t>(face_height) * pixel_stride);

    for (uint32_t row = 0; row < face_height; ++row) {
        const size_t source_offset = (static_cast<size_t>(face_y + row) * image.Width + face_x) * pixel_stride;
        const size_t dest_offset = static_cast<size_t>(row) * face_width * pixel_stride;
        std::memcpy(face_bytes.data() + dest_offset, image.ByteData.data() + source_offset, face_width * pixel_stride);
    }

    return face_bytes;
}

CubemapData createCubemapFromSingleImage(const ImageData& image)
{
    CubemapData cubemap_data;
    if (!image.isValid() || image.Width % 4 != 0 || image.Height % 3 != 0) {
        return cubemap_data;
    }

    const uint32_t face_width = image.Width / 4;
    const uint32_t face_height = image.Height / 3;
    if (face_width != face_height) {
        return cubemap_data;
    }

    cubemap_data.FaceFormat = image.ImageFormat;
    cubemap_data.FaceWidth = face_width;
    cubemap_data.FaceHeight = face_height;

    cubemap_data.Faces[0] = extractCubemapFace(image, face_width * 2, face_height, face_width, face_height);
    cubemap_data.Faces[1] = extractCubemapFace(image, 0, face_height, face_width, face_height);
    cubemap_data.Faces[2] = extractCubemapFace(image, face_width, 0, face_width, face_height);
    cubemap_data.Faces[3] = extractCubemapFace(image, face_width, face_height * 2, face_width, face_height);
    cubemap_data.Faces[4] = extractCubemapFace(image, face_width, face_height, face_width, face_height);
    cubemap_data.Faces[5] = extractCubemapFace(image, face_width * 3, face_height, face_width, face_height);

    return cubemap_data;
}

} // namespace

ImageData ImageLoader::LoadImageFromFile(const std::string& filepath)
{
    if (isDDSImage(filepath) || isZLIBImage(filepath)) {
        return loadImageUsingDDSLoader(filepath);
    }

    if (isHDRImage(filepath)) {
        return loadHDRImageUsingSTBLoader(filepath);
    }

    return loadImageUsingSTBLoader(filepath);
}

ImageData ImageLoader::LoadImageFromMemory(const uint8_t* data, std::size_t size, const std::string& mimeType)
{
    if (isDDSMimeType(mimeType) || isDDSData(data, size)) {
        return loadImageUsingDDSLoader(data, size);
    }

    if (mimeType == "image/vnd.radiance") {
        return loadHDRImageUsingSTBLoader(data, size);
    }

    return loadImageUsingSTBLoader(data, size);
}

CubemapData ImageLoader::LoadCubemapImageFromFile(const std::string& filepath)
{
    return createCubemapFromSingleImage(LoadImageFromFile(filepath));
}

} // namespace luna
