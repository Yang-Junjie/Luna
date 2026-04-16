#ifndef CACAO_CACAOBUFFER_H
#define CACAO_CACAOBUFFER_H
#include "Core.h"
#include <cstring>
#include <string>
namespace Cacao
{
    enum class BufferUsageFlags : uint32_t
    {
        None = 0,
        TransferSrc = 1 << 0, 
        TransferDst = 1 << 1, 
        UniformBuffer = 1 << 2, 
        StorageBuffer = 1 << 3,
        StorageBufferReadOnly = 1 << 3,
        StorageBufferReadWrite = (1 << 3) | (1 << 9),
        IndexBuffer = 1 << 4, 
        VertexBuffer = 1 << 5, 
        IndirectBuffer = 1 << 6, 
        ShaderDeviceAddress = 1 << 7, 
        AccelerationStructure = 1 << 8, 
    };
    inline BufferUsageFlags operator|(BufferUsageFlags a, BufferUsageFlags b)
    {
        return static_cast<BufferUsageFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }
    inline bool operator &(BufferUsageFlags a, BufferUsageFlags b)
    {
        return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
    }
    enum class BufferMemoryUsage
    {
        GpuOnly,
        CpuToGpu,
        GpuToCpu,
        CpuOnly,
    };
    struct BufferCreateInfo
    {
        uint64_t Size = 0;
        BufferUsageFlags Usage = BufferUsageFlags::UniformBuffer;
        BufferMemoryUsage MemoryUsage = BufferMemoryUsage::CpuToGpu;
        std::string Name;
        const void* InitialData = nullptr;
    };
    class CACAO_API Buffer
    {
    public:
        virtual ~Buffer() = default;
        virtual uint64_t GetSize() const = 0;
        virtual BufferUsageFlags GetUsage() const = 0;
        virtual BufferMemoryUsage GetMemoryUsage() const = 0;
        virtual void* Map() = 0;
        virtual void Unmap() = 0;
        virtual void Flush(uint64_t offset = 0, uint64_t size = UINT64_MAX) = 0;
        template <typename T>
        void Write(const T& data, uint64_t offset = 0)
        {
            void* ptr = Map();
            if (ptr)
            {
                uint8_t* bytePtr = static_cast<uint8_t*>(ptr);
                memcpy(bytePtr + offset, &data, sizeof(T));
            }
        }
        template <typename T>
        void Write(const std::vector<T>& data, uint64_t offset = 0)
        {
            void* ptr = Map();
            if (ptr)
            {
                uint8_t* bytePtr = static_cast<uint8_t*>(ptr);
                memcpy(bytePtr + offset, data.data(), data.size() * sizeof(T));
            }
        }
        virtual uint64_t GetDeviceAddress() const = 0;
    };
}
#endif 
