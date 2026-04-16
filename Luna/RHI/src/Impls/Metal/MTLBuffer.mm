#ifdef __APPLE__
#import <Metal/Metal.h>
#include "Impls/Metal/MTLBuffer.h"
#include <cstring>

namespace Cacao
{
    MTLBufferImpl::MTLBufferImpl(id device, const BufferCreateInfo& info)
        : m_createInfo(info)
    {
        id<MTLDevice> mtlDevice = (id<MTLDevice>)device;

        MTLResourceOptions options;
        switch (info.MemoryUsage)
        {
        case BufferMemoryUsage::GpuOnly:
            options = MTLResourceStorageModePrivate;
            break;
        case BufferMemoryUsage::CpuToGpu:
        case BufferMemoryUsage::CpuOnly:
            options = MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined;
            break;
        case BufferMemoryUsage::GpuToCpu:
            options = MTLResourceStorageModeShared;
            break;
        default:
            options = MTLResourceStorageModeShared;
            break;
        }

        if (info.InitialData && info.MemoryUsage != BufferMemoryUsage::GpuOnly)
        {
            m_buffer = [mtlDevice newBufferWithBytes:info.InitialData
                                              length:info.Size
                                             options:options];
        }
        else
        {
            m_buffer = [mtlDevice newBufferWithLength:info.Size options:options];
            if (info.InitialData && m_buffer)
            {
                id<MTLBuffer> buf = (id<MTLBuffer>)m_buffer;
                memcpy([buf contents], info.InitialData, info.Size);
            }
        }

        if (!info.Name.empty() && m_buffer)
        {
            id<MTLBuffer> buf = (id<MTLBuffer>)m_buffer;
            [buf setLabel:[NSString stringWithUTF8String:info.Name.c_str()]];
        }
    }

    MTLBufferImpl::~MTLBufferImpl()
    {
        m_buffer = nil;
    }

    void* MTLBufferImpl::Map()
    {
        if (!m_buffer) return nullptr;
        id<MTLBuffer> buf = (id<MTLBuffer>)m_buffer;
        return [buf contents];
    }

    void MTLBufferImpl::Unmap()
    {
        // Metal shared buffers don't need explicit unmap
    }

    void MTLBufferImpl::Flush(uint64_t offset, uint64_t size)
    {
#if TARGET_OS_OSX
        if (!m_buffer) return;
        id<MTLBuffer> buf = (id<MTLBuffer>)m_buffer;
        if (buf.storageMode == MTLStorageModeManaged)
        {
            uint64_t actualSize = (size == UINT64_MAX) ? m_createInfo.Size - offset : size;
            [buf didModifyRange:NSMakeRange(offset, actualSize)];
        }
#endif
    }
}
#endif // __APPLE__
