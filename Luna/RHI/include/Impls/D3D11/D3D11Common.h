#ifndef CACAO_D3D11COMMON_H
#define CACAO_D3D11COMMON_H

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

#include <Core.h>
#include <Buffer.h>

namespace Cacao
{
    inline DXGI_FORMAT D3D11_ToDXGIFormat(Format format)
    {
        switch (format)
        {
        case Format::R8_UNORM:            return DXGI_FORMAT_R8_UNORM;
        case Format::RG8_UNORM:           return DXGI_FORMAT_R8G8_UNORM;
        case Format::RGBA8_UNORM:         return DXGI_FORMAT_R8G8B8A8_UNORM;
        case Format::RGBA8_SRGB:          return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case Format::BGRA8_UNORM:         return DXGI_FORMAT_B8G8R8A8_UNORM;
        case Format::BGRA8_SRGB:          return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case Format::R16_FLOAT:           return DXGI_FORMAT_R16_FLOAT;
        case Format::RG16_FLOAT:          return DXGI_FORMAT_R16G16_FLOAT;
        case Format::RGBA16_FLOAT:        return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case Format::R32_FLOAT:           return DXGI_FORMAT_R32_FLOAT;
        case Format::RG32_FLOAT:          return DXGI_FORMAT_R32G32_FLOAT;
        case Format::RGB32_FLOAT:         return DXGI_FORMAT_R32G32B32_FLOAT;
        case Format::RGBA32_FLOAT:        return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case Format::R32_UINT:            return DXGI_FORMAT_R32_UINT;
        case Format::R16_UINT:            return DXGI_FORMAT_R16_UINT;
        case Format::D16_UNORM:           return DXGI_FORMAT_D16_UNORM;
        case Format::D24_UNORM_S8_UINT:   return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case Format::D32_FLOAT:           return DXGI_FORMAT_D32_FLOAT;
        case Format::D32_FLOAT_S8_UINT:   return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        case Format::BC1_RGB_UNORM:       return DXGI_FORMAT_BC1_UNORM;
        case Format::BC1_RGB_SRGB:        return DXGI_FORMAT_BC1_UNORM_SRGB;
        case Format::BC3_UNORM:           return DXGI_FORMAT_BC3_UNORM;
        case Format::BC3_SRGB:            return DXGI_FORMAT_BC3_UNORM_SRGB;
        case Format::BC5_UNORM:           return DXGI_FORMAT_BC5_UNORM;
        case Format::BC7_UNORM:           return DXGI_FORMAT_BC7_UNORM;
        case Format::BC7_SRGB:            return DXGI_FORMAT_BC7_UNORM_SRGB;
        default:                          return DXGI_FORMAT_UNKNOWN;
        }
    }

    inline D3D11_USAGE D3D11_ToUsage(BufferMemoryUsage usage)
    {
        switch (usage)
        {
        case BufferMemoryUsage::GpuOnly:  return D3D11_USAGE_DEFAULT;
        case BufferMemoryUsage::CpuToGpu: return D3D11_USAGE_DYNAMIC;
        case BufferMemoryUsage::GpuToCpu: return D3D11_USAGE_STAGING;
        case BufferMemoryUsage::CpuOnly:  return D3D11_USAGE_STAGING;
        default:                          return D3D11_USAGE_DEFAULT;
        }
    }
}
#endif
