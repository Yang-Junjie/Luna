#ifndef CACAO_D3D12COMMON_H
#define CACAO_D3D12COMMON_H

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

#include <Core.h>
#include <Barrier.h>
#include <Buffer.h>
#include <Adapter.h>

namespace Cacao
{
    inline DXGI_FORMAT ToDXGIFormat(Format format)
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
        default:                          return DXGI_FORMAT_UNKNOWN;
        }
    }

    inline D3D12_RESOURCE_STATES ToD3D12ResourceState(ResourceState state)
    {
        switch (state)
        {
        case ResourceState::Undefined:      return D3D12_RESOURCE_STATE_COMMON;
        case ResourceState::General:        return D3D12_RESOURCE_STATE_COMMON;
        case ResourceState::RenderTarget:   return D3D12_RESOURCE_STATE_RENDER_TARGET;
        case ResourceState::DepthWrite:     return D3D12_RESOURCE_STATE_DEPTH_WRITE;
        case ResourceState::DepthRead:      return D3D12_RESOURCE_STATE_DEPTH_READ;
        case ResourceState::ShaderRead:     return D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
        case ResourceState::CopySource:     return D3D12_RESOURCE_STATE_COPY_SOURCE;
        case ResourceState::CopyDest:       return D3D12_RESOURCE_STATE_COPY_DEST;
        case ResourceState::Present:        return D3D12_RESOURCE_STATE_PRESENT;
        case ResourceState::UnorderedAccess:  return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        default:                            return D3D12_RESOURCE_STATE_COMMON;
        }
    }

    inline D3D12_COMMAND_LIST_TYPE ToD3D12CommandListType(QueueType type)
    {
        switch (type)
        {
        case QueueType::Graphics: return D3D12_COMMAND_LIST_TYPE_DIRECT;
        case QueueType::Compute:  return D3D12_COMMAND_LIST_TYPE_COMPUTE;
        case QueueType::Transfer: return D3D12_COMMAND_LIST_TYPE_COPY;
        default:                  return D3D12_COMMAND_LIST_TYPE_DIRECT;
        }
    }

    inline uint32_t D3D12GetFormatBytesPerPixel(DXGI_FORMAT format)
    {
        switch (format)
        {
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_R8_SNORM:       return 1;
        case DXGI_FORMAT_R8G8_UNORM:
        case DXGI_FORMAT_R16_FLOAT:
        case DXGI_FORMAT_R16_UINT:       return 2;
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_R16G16_FLOAT:
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_R32_UINT:
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_D24_UNORM_S8_UINT: return 4;
        case DXGI_FORMAT_R16G16B16A16_FLOAT: return 8;
        case DXGI_FORMAT_R32G32_FLOAT:   return 8;
        case DXGI_FORMAT_R32G32B32_FLOAT: return 12;
        case DXGI_FORMAT_R32G32B32A32_FLOAT: return 16;
        default:                         return 4;
        }
    }

    inline D3D12_HEAP_TYPE ToD3D12HeapType(BufferMemoryUsage usage)
    {
        switch (usage)
        {
        case BufferMemoryUsage::GpuOnly:   return D3D12_HEAP_TYPE_DEFAULT;
        case BufferMemoryUsage::CpuToGpu:  return D3D12_HEAP_TYPE_UPLOAD;
        case BufferMemoryUsage::GpuToCpu:  return D3D12_HEAP_TYPE_READBACK;
        default:                           return D3D12_HEAP_TYPE_DEFAULT;
        }
    }
}

#endif
