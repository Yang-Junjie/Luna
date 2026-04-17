#include "Impls/OpenGL/GLBuffer.h"

namespace luna::RHI {
GLBuffer::GLBuffer(const Ref<Device>& device, const BufferCreateInfo& info)
    : m_device(device),
      m_createInfo(info)
{
    glGenBuffers(1, &m_buffer);

    GLenum target = GL_ARRAY_BUFFER;
    uint32_t usage = static_cast<uint32_t>(info.Usage);
    if (usage & static_cast<uint32_t>(BufferUsageFlags::StorageBuffer)) {
        target = GL_SHADER_STORAGE_BUFFER;
    } else if (usage & static_cast<uint32_t>(BufferUsageFlags::IndexBuffer)) {
        target = GL_ELEMENT_ARRAY_BUFFER;
    } else if (usage & static_cast<uint32_t>(BufferUsageFlags::UniformBuffer)) {
        target = GL_UNIFORM_BUFFER;
    }

    m_target = target;
    glBindBuffer(target, m_buffer);

    bool isMappable =
        (info.MemoryUsage == BufferMemoryUsage::CpuToGpu || info.MemoryUsage == BufferMemoryUsage::GpuToCpu);

    glBufferData(target, static_cast<GLsizeiptr>(info.Size), nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(target, 0);
}

Ref<GLBuffer> GLBuffer::Create(const Ref<Device>& device, const BufferCreateInfo& info)
{
    return std::make_shared<GLBuffer>(device, info);
}

GLBuffer::~GLBuffer()
{
    if (m_buffer) {
        glDeleteBuffers(1, &m_buffer);
    }
}

uint64_t GLBuffer::GetSize() const
{
    return m_createInfo.Size;
}

BufferUsageFlags GLBuffer::GetUsage() const
{
    return m_createInfo.Usage;
}

BufferMemoryUsage GLBuffer::GetMemoryUsage() const
{
    return m_createInfo.MemoryUsage;
}

void* GLBuffer::Map()
{
    if (m_mappedPtr) {
        return m_mappedPtr;
    }

    if (m_cpuData.empty()) {
        m_cpuData.resize(m_createInfo.Size, 0);
    }
    m_mappedPtr = m_cpuData.data();
    return m_mappedPtr;
}

void GLBuffer::Unmap()
{
    if (!m_mappedPtr) {
        return;
    }
    glBindBuffer(m_target, m_buffer);
    glBufferSubData(m_target, 0, static_cast<GLsizeiptr>(m_createInfo.Size), m_cpuData.data());
    glBindBuffer(m_target, 0);
}

void GLBuffer::Flush(uint64_t offset, uint64_t size)
{
    if (m_cpuData.empty()) {
        return;
    }
    uint64_t actualSize = (size == UINT64_MAX || size == 0) ? m_createInfo.Size : size;
    glBindBuffer(m_target, m_buffer);
    glBufferSubData(
        m_target, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(actualSize), m_cpuData.data() + offset);
    glBindBuffer(m_target, 0);
}

uint64_t GLBuffer::GetDeviceAddress() const
{
    // GL doesn't have device addresses in the same sense; return handle as a proxy
    return static_cast<uint64_t>(m_buffer);
}
} // namespace luna::RHI
