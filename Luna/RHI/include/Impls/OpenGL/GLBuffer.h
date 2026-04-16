#ifndef CACAO_GLBUFFER_H
#define CACAO_GLBUFFER_H
#include "Buffer.h"
#include "GLCommon.h"

namespace Cacao
{
    class Device;

    class CACAO_API GLBuffer final : public Buffer
    {
    public:
        GLBuffer(const Ref<Device>& device, const BufferCreateInfo& info);
        static Ref<GLBuffer> Create(const Ref<Device>& device, const BufferCreateInfo& info);
        ~GLBuffer() override;

        uint64_t GetSize() const override;
        BufferUsageFlags GetUsage() const override;
        BufferMemoryUsage GetMemoryUsage() const override;
        void* Map() override;
        void Unmap() override;
        void Flush(uint64_t offset, uint64_t size) override;
        uint64_t GetDeviceAddress() const override;

        GLuint GetHandle() const { return m_buffer; }
        const void* GetMappedPtr() const { return m_cpuData.empty() ? nullptr : m_cpuData.data(); }

    private:
        GLuint m_buffer = 0;
        GLenum m_target = GL_ARRAY_BUFFER;
        Ref<Device> m_device;
        BufferCreateInfo m_createInfo;
        void* m_mappedPtr = nullptr;
        std::vector<uint8_t> m_cpuData;
    };
}

#endif
