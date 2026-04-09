#include "Renderer/ImageLoader.h"

// Copyright(c) 2021, #Momo
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met :
// 
// 1. Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and /or other materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string_view>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYDDSLOADER_IMPLEMENTATION
#include "third_party/tinyddsloader/tinyddsloader.h"

namespace VulkanAbstractionLayer
{
    namespace
    {
        bool IsDDSImage(const std::string& filepath)
        {
            return std::filesystem::path{ filepath }.extension() == ".dds";
        }

        bool IsZLIBImage(const std::string& filepath)
        {
            return std::filesystem::path{ filepath }.extension() == ".zlib";
        }

        bool IsDDSMimeType(const std::string_view mimeType)
        {
            return mimeType == "image/vnd-ms.dds";
        }

        bool IsDDSData(const uint8_t* data, const std::size_t size)
        {
            return size >= 4 &&
                data[0] == 'D' &&
                data[1] == 'D' &&
                data[2] == 'S' &&
                data[3] == ' ';
        }

        Format DDSFormatToImageFormat(const tinyddsloader::DDSFile::DXGIFormat format)
        {
            switch (format)
            {
            case tinyddsloader::DDSFile::DXGIFormat::Unknown:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::R32G32B32A32_Typeless:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::R32G32B32A32_Float:
                return Format::R32G32B32A32_SFLOAT;
            case tinyddsloader::DDSFile::DXGIFormat::R32G32B32A32_UInt:
                return Format::R32G32B32A32_UINT;
            case tinyddsloader::DDSFile::DXGIFormat::R32G32B32A32_SInt:
                return Format::R32G32B32A32_SINT;
            case tinyddsloader::DDSFile::DXGIFormat::R32G32B32_Typeless:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::R32G32B32_Float:
                return Format::R32G32B32_SFLOAT;
            case tinyddsloader::DDSFile::DXGIFormat::R32G32B32_UInt:
                return Format::R32G32B32_UINT;
            case tinyddsloader::DDSFile::DXGIFormat::R32G32B32_SInt:
                return Format::R32G32B32_SINT;
            case tinyddsloader::DDSFile::DXGIFormat::R16G16B16A16_Typeless:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::R16G16B16A16_Float:
                return Format::R16G16B16A16_SFLOAT;
            case tinyddsloader::DDSFile::DXGIFormat::R16G16B16A16_UNorm:
                return Format::R16G16B16A16_UNORM;
            case tinyddsloader::DDSFile::DXGIFormat::R16G16B16A16_UInt:
                return Format::R16G16B16A16_UINT;
            case tinyddsloader::DDSFile::DXGIFormat::R16G16B16A16_SNorm:
                return Format::R16G16B16A16_SNORM;
            case tinyddsloader::DDSFile::DXGIFormat::R16G16B16A16_SInt:
                return Format::R16G16B16A16_SINT;
            case tinyddsloader::DDSFile::DXGIFormat::R32G32_Typeless:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::R32G32_Float:
                return Format::R32G32_SFLOAT;
            case tinyddsloader::DDSFile::DXGIFormat::R32G32_UInt:
                return Format::R32G32_UINT;
            case tinyddsloader::DDSFile::DXGIFormat::R32G32_SInt:
                return Format::R32G32_SINT;
            case tinyddsloader::DDSFile::DXGIFormat::R32G8X24_Typeless:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::D32_Float_S8X24_UInt:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::R32_Float_X8X24_Typeless:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::X32_Typeless_G8X24_UInt:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::R10G10B10A2_Typeless:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::R10G10B10A2_UNorm:
                return Format::A2R10G10B10_UNORM_PACK_32;
            case tinyddsloader::DDSFile::DXGIFormat::R10G10B10A2_UInt:
                return Format::A2R10G10B10_UINT_PACK_32;
            case tinyddsloader::DDSFile::DXGIFormat::R11G11B10_Float:
                return Format::B10G11R11_UFLOAT_PACK_32;
            case tinyddsloader::DDSFile::DXGIFormat::R8G8B8A8_Typeless:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::R8G8B8A8_UNorm:
                return Format::R8G8B8A8_UNORM;
            case tinyddsloader::DDSFile::DXGIFormat::R8G8B8A8_UNorm_SRGB:
                return Format::R8G8B8A8_SRGB;
            case tinyddsloader::DDSFile::DXGIFormat::R8G8B8A8_UInt:
                return Format::R8G8B8A8_UINT;
            case tinyddsloader::DDSFile::DXGIFormat::R8G8B8A8_SNorm:
                return Format::R8G8B8A8_SNORM;
            case tinyddsloader::DDSFile::DXGIFormat::R8G8B8A8_SInt:
                return Format::R8G8B8A8_SINT;
            case tinyddsloader::DDSFile::DXGIFormat::R16G16_Typeless:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::R16G16_Float:
                return Format::R16G16_SFLOAT;
            case tinyddsloader::DDSFile::DXGIFormat::R16G16_UNorm:
                return Format::R16G16_UNORM;
            case tinyddsloader::DDSFile::DXGIFormat::R16G16_UInt:
                return Format::R16G16_UINT;
            case tinyddsloader::DDSFile::DXGIFormat::R16G16_SNorm:
                return Format::R16G16_SNORM;
            case tinyddsloader::DDSFile::DXGIFormat::R16G16_SInt:
                return Format::R16G16_SINT;
            case tinyddsloader::DDSFile::DXGIFormat::R32_Typeless:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::D32_Float:
                return Format::D32_SFLOAT;
            case tinyddsloader::DDSFile::DXGIFormat::R32_Float:
                return Format::R32_SFLOAT;
            case tinyddsloader::DDSFile::DXGIFormat::R32_UInt:
                return Format::R32_UINT;
            case tinyddsloader::DDSFile::DXGIFormat::R32_SInt:
                return Format::R32_SINT;
            case tinyddsloader::DDSFile::DXGIFormat::R24G8_Typeless:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::D24_UNorm_S8_UInt:
                return Format::D24_UNORM_S8_UINT;
            case tinyddsloader::DDSFile::DXGIFormat::R24_UNorm_X8_Typeless:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::X24_Typeless_G8_UInt:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::R8G8_Typeless:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::R8G8_UNorm:
                return Format::R8G8_UNORM;
            case tinyddsloader::DDSFile::DXGIFormat::R8G8_UInt:
                return Format::R8G8_UINT;
            case tinyddsloader::DDSFile::DXGIFormat::R8G8_SNorm:
                return Format::R8G8_SNORM;
            case tinyddsloader::DDSFile::DXGIFormat::R8G8_SInt:
                return Format::R8G8_SINT;
            case tinyddsloader::DDSFile::DXGIFormat::R16_Typeless:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::R16_Float:
                return Format::R16_SFLOAT;
            case tinyddsloader::DDSFile::DXGIFormat::D16_UNorm:
                return Format::D16_UNORM;
            case tinyddsloader::DDSFile::DXGIFormat::R16_UNorm:
                return Format::R16_UNORM;
            case tinyddsloader::DDSFile::DXGIFormat::R16_UInt:
                return Format::R16_UINT;
            case tinyddsloader::DDSFile::DXGIFormat::R16_SNorm:
                return Format::R16_SNORM;
            case tinyddsloader::DDSFile::DXGIFormat::R16_SInt:
                return Format::R16_SINT;
            case tinyddsloader::DDSFile::DXGIFormat::R8_Typeless:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::R8_UNorm:
                return Format::R8_UNORM;
            case tinyddsloader::DDSFile::DXGIFormat::R8_UInt:
                return Format::R8_UINT;
            case tinyddsloader::DDSFile::DXGIFormat::R8_SNorm:
                return Format::R8_SNORM;
            case tinyddsloader::DDSFile::DXGIFormat::R8_SInt:
                return Format::R8_SINT;
            case tinyddsloader::DDSFile::DXGIFormat::A8_UNorm:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::R1_UNorm:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::R9G9B9E5_SHAREDEXP:
                return Format::E5B9G9R9_UFLOAT_PACK_32;
            case tinyddsloader::DDSFile::DXGIFormat::R8G8_B8G8_UNorm:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::G8R8_G8B8_UNorm:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::BC1_Typeless:
            case tinyddsloader::DDSFile::DXGIFormat::BC1_UNorm:
            case tinyddsloader::DDSFile::DXGIFormat::BC1_UNorm_SRGB:
            case tinyddsloader::DDSFile::DXGIFormat::BC2_Typeless:
            case tinyddsloader::DDSFile::DXGIFormat::BC2_UNorm:
            case tinyddsloader::DDSFile::DXGIFormat::BC2_UNorm_SRGB:
            case tinyddsloader::DDSFile::DXGIFormat::BC3_Typeless:
            case tinyddsloader::DDSFile::DXGIFormat::BC3_UNorm:
            case tinyddsloader::DDSFile::DXGIFormat::BC3_UNorm_SRGB:
            case tinyddsloader::DDSFile::DXGIFormat::BC4_Typeless:
            case tinyddsloader::DDSFile::DXGIFormat::BC4_UNorm:
            case tinyddsloader::DDSFile::DXGIFormat::BC4_SNorm:
            case tinyddsloader::DDSFile::DXGIFormat::BC5_Typeless:
            case tinyddsloader::DDSFile::DXGIFormat::BC5_UNorm:
            case tinyddsloader::DDSFile::DXGIFormat::BC5_SNorm:
            case tinyddsloader::DDSFile::DXGIFormat::BC6H_Typeless:
            case tinyddsloader::DDSFile::DXGIFormat::BC6H_UF16:
            case tinyddsloader::DDSFile::DXGIFormat::BC6H_SF16:
            case tinyddsloader::DDSFile::DXGIFormat::BC7_Typeless:
            case tinyddsloader::DDSFile::DXGIFormat::BC7_UNorm:
            case tinyddsloader::DDSFile::DXGIFormat::BC7_UNorm_SRGB:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::B5G6R5_UNorm:
                return Format::B5G6R5_UNORM_PACK_16;
            case tinyddsloader::DDSFile::DXGIFormat::B5G5R5A1_UNorm:
                return Format::B5G5R5A1_UNORM_PACK_16;
            case tinyddsloader::DDSFile::DXGIFormat::B8G8R8A8_UNorm:
                return Format::B8G8R8A8_UNORM;
            case tinyddsloader::DDSFile::DXGIFormat::B8G8R8X8_UNorm:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::R10G10B10_XR_BIAS_A2_UNorm:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::B8G8R8A8_Typeless:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::B8G8R8A8_UNorm_SRGB:
                return Format::B8G8R8A8_SRGB;
            case tinyddsloader::DDSFile::DXGIFormat::B8G8R8X8_Typeless:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::B8G8R8X8_UNorm_SRGB:
                return Format::UNDEFINED;
            case tinyddsloader::DDSFile::DXGIFormat::AYUV:
            case tinyddsloader::DDSFile::DXGIFormat::Y410:
            case tinyddsloader::DDSFile::DXGIFormat::Y416:
            case tinyddsloader::DDSFile::DXGIFormat::NV12:
            case tinyddsloader::DDSFile::DXGIFormat::P010:
            case tinyddsloader::DDSFile::DXGIFormat::P016:
            case tinyddsloader::DDSFile::DXGIFormat::YUV420_OPAQUE:
            case tinyddsloader::DDSFile::DXGIFormat::YUY2:
            case tinyddsloader::DDSFile::DXGIFormat::Y210:
            case tinyddsloader::DDSFile::DXGIFormat::Y216:
            case tinyddsloader::DDSFile::DXGIFormat::NV11:
            case tinyddsloader::DDSFile::DXGIFormat::AI44:
            case tinyddsloader::DDSFile::DXGIFormat::IA44:
            case tinyddsloader::DDSFile::DXGIFormat::P8:
            case tinyddsloader::DDSFile::DXGIFormat::A8P8:
            case tinyddsloader::DDSFile::DXGIFormat::P208:
            case tinyddsloader::DDSFile::DXGIFormat::V208:
            case tinyddsloader::DDSFile::DXGIFormat::V408:
            default:
                return Format::UNDEFINED;
            }
        }

        ImageData LoadImageUsingDDSLoader(const std::string& filepath)
        {
            ImageData image;
            tinyddsloader::DDSFile dds;
            if (dds.Load(filepath.c_str()) != tinyddsloader::Result::Success)
            {
                return image;
            }

            dds.Flip();
            auto imageData = dds.GetImageData();
            image.Width = imageData->m_width;
            image.Height = imageData->m_height;
            image.ImageFormat = DDSFormatToImageFormat(dds.GetFormat());
            image.ByteData.resize(imageData->m_memSlicePitch);
            std::copy_n(static_cast<const uint8_t*>(imageData->m_mem), imageData->m_memSlicePitch, image.ByteData.begin());

            for (uint32_t i = 1; i < dds.GetMipCount(); ++i)
            {
                auto& mipLevelData = image.MipLevels.emplace_back();
                auto mipImageData = dds.GetImageData(i);
                mipLevelData.resize(mipImageData->m_memSlicePitch);
                std::copy_n(static_cast<const uint8_t*>(mipImageData->m_mem), mipImageData->m_memSlicePitch, mipLevelData.begin());
            }

            return image;
        }

        ImageData LoadImageUsingDDSLoader(const uint8_t* data, const std::size_t size)
        {
            ImageData image;
            tinyddsloader::DDSFile dds;
            if (dds.Load(data, size) != tinyddsloader::Result::Success)
            {
                return image;
            }

            dds.Flip();
            auto imageData = dds.GetImageData();
            image.Width = imageData->m_width;
            image.Height = imageData->m_height;
            image.ImageFormat = DDSFormatToImageFormat(dds.GetFormat());
            image.ByteData.resize(imageData->m_memSlicePitch);
            std::copy_n(static_cast<const uint8_t*>(imageData->m_mem), imageData->m_memSlicePitch, image.ByteData.begin());

            for (uint32_t i = 1; i < dds.GetMipCount(); ++i)
            {
                auto& mipLevelData = image.MipLevels.emplace_back();
                auto mipImageData = dds.GetImageData(i);
                mipLevelData.resize(mipImageData->m_memSlicePitch);
                std::copy_n(static_cast<const uint8_t*>(mipImageData->m_mem), mipImageData->m_memSlicePitch, mipLevelData.begin());
            }

            return image;
        }

        std::string ConvertZLIBToDDS(const std::string& filepath)
        {
            std::ifstream file(filepath, std::ios::binary);
            if (!file.good())
            {
                return filepath;
            }

            std::vector<char> compressedData{
                std::istreambuf_iterator<char>(file),
                std::istreambuf_iterator<char>()
            };

            int decompressedSize = 0;
            char* decompressedData = stbi_zlib_decode_malloc(compressedData.data(), static_cast<int>(compressedData.size()), &decompressedSize);
            if (decompressedData == nullptr || decompressedSize <= 0)
            {
                return filepath;
            }

            std::filesystem::path path{ filepath };
            auto ddsFilepath = (path.parent_path() / path.stem()).string() + ".dds";
            std::ofstream ddsFile(ddsFilepath, std::ios::binary);
            ddsFile.write(decompressedData, decompressedSize);
            std::free(decompressedData);

            return ddsFilepath;
        }

        ImageData LoadImageUsingZLIBLoader(const std::string& filepath)
        {
            return LoadImageUsingDDSLoader(ConvertZLIBToDDS(filepath));
        }

        ImageData LoadImageUsingSTBLoader(const std::string& filepath)
        {
            int width = 0;
            int height = 0;
            int channels = 0;
            constexpr int ActualChannels = 4;

            stbi_set_flip_vertically_on_load(true);
            uint8_t* data = stbi_load(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
            stbi_set_flip_vertically_on_load(false);
            if (data == nullptr || width <= 0 || height <= 0)
            {
                return {};
            }

            std::vector<uint8_t> vecData(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * ActualChannels);
            std::copy_n(data, vecData.size(), vecData.begin());
            stbi_image_free(data);
            return ImageData{ std::move(vecData), Format::R8G8B8A8_UNORM, static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
        }

        ImageData LoadImageUsingSTBLoader(const uint8_t* data, const std::size_t size)
        {
            int width = 0;
            int height = 0;
            int channels = 0;
            constexpr int ActualChannels = 4;

            stbi_set_flip_vertically_on_load(true);
            uint8_t* decoded = stbi_load_from_memory(data, static_cast<int>(size), &width, &height, &channels, STBI_rgb_alpha);
            stbi_set_flip_vertically_on_load(false);
            if (decoded == nullptr || width <= 0 || height <= 0)
            {
                return {};
            }

            std::vector<uint8_t> vecData(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * ActualChannels);
            std::copy_n(decoded, vecData.size(), vecData.begin());
            stbi_image_free(decoded);
            return ImageData{ std::move(vecData), Format::R8G8B8A8_UNORM, static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
        }

        std::vector<uint8_t> ExtractCubemapFace(const ImageData& image, const size_t faceWidth, const size_t faceHeight, const size_t channelCount, const size_t sliceX, const size_t sliceY)
        {
            std::vector<uint8_t> result(image.ByteData.size() / 6);

            for (size_t i = 0; i < faceHeight; ++i)
            {
                const size_t y = (faceHeight - i - 1) + sliceY * faceHeight;
                const size_t x = sliceX * faceWidth;
                const size_t bytesInRow = faceWidth * channelCount;

                std::memcpy(result.data() + i * bytesInRow, image.ByteData.data() + (y * image.Width + x) * channelCount, bytesInRow);
            }
            return result;
        }

        CubemapData CreateCubemapFromSingleImage(const ImageData& image)
        {
            CubemapData cubemapData;
            cubemapData.FaceFormat = image.ImageFormat;
            cubemapData.FaceWidth = image.Width / 4;
            cubemapData.FaceHeight = image.Height / 3;
            assert(cubemapData.FaceWidth == cubemapData.FaceHeight);
            assert(cubemapData.FaceFormat == Format::R8G8B8A8_UNORM);
            constexpr size_t ChannelCount = 4;

            cubemapData.Faces[0] = ExtractCubemapFace(image, cubemapData.FaceWidth, cubemapData.FaceHeight, ChannelCount, 2, 1);
            cubemapData.Faces[1] = ExtractCubemapFace(image, cubemapData.FaceWidth, cubemapData.FaceHeight, ChannelCount, 0, 1);
            cubemapData.Faces[2] = ExtractCubemapFace(image, cubemapData.FaceWidth, cubemapData.FaceHeight, ChannelCount, 1, 2);
            cubemapData.Faces[3] = ExtractCubemapFace(image, cubemapData.FaceWidth, cubemapData.FaceHeight, ChannelCount, 1, 0);
            cubemapData.Faces[4] = ExtractCubemapFace(image, cubemapData.FaceWidth, cubemapData.FaceHeight, ChannelCount, 1, 1);
            cubemapData.Faces[5] = ExtractCubemapFace(image, cubemapData.FaceWidth, cubemapData.FaceHeight, ChannelCount, 3, 1);
            return cubemapData;
        }
    }

    ImageData ImageLoader::LoadImageFromFile(const std::string& filepath)
    {
        if (IsDDSImage(filepath))
        {
            return LoadImageUsingDDSLoader(filepath);
        }
        if (IsZLIBImage(filepath))
        {
            return LoadImageUsingZLIBLoader(filepath);
        }
        return LoadImageUsingSTBLoader(filepath);
    }

    ImageData ImageLoader::LoadImageFromMemory(const uint8_t* data, const std::size_t size, const std::string& mimeType)
    {
        if (data == nullptr || size == 0)
        {
            return {};
        }

        if (IsDDSMimeType(mimeType) || IsDDSData(data, size))
        {
            return LoadImageUsingDDSLoader(data, size);
        }

        return LoadImageUsingSTBLoader(data, size);
    }

    CubemapData ImageLoader::LoadCubemapImageFromFile(const std::string& filepath)
    {
        return CreateCubemapFromSingleImage(ImageLoader::LoadImageFromFile(filepath));
    }
}
