#include "Impls/OpenGL/GLStagingBuffer.h"

#include <cstring>

namespace luna::RHI {
GLStagingBuffer::GLStagingBuffer(uint64_t capacity)
    : m_capacity(capacity)
{
    glGenBuffers(1, &m_buffer);
    glBindBuffer(GL_COPY_READ_BUFFER, m_buffer);

#ifndef LUNA_RHI_GLES
    GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    glBufferStorage(GL_COPY_READ_BUFFER, static_cast<GLsizeiptr>(capacity), nullptr, flags | GL_DYNAMIC_STORAGE_BIT);
    m_mappedPtr = glMapBufferRange(GL_COPY_READ_BUFFER, 0, static_cast<GLsizeiptr>(capacity), flags);
#else
    glBufferData(GL_COPY_READ_BUFFER, static_cast<GLsizeiptr>(capacity), nullptr, GL_STREAM_DRAW);
    m_mappedPtr = glMapBufferRange(
        GL_COPY_READ_BUFFER, 0, static_cast<GLsizeiptr>(capacity), GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
#endif

    glBindBuffer(GL_COPY_READ_BUFFER, 0);
}

GLStagingBuffer::~GLStagingBuffer()
{
    if (m_buffer) {
        glBindBuffer(GL_COPY_READ_BUFFER, m_buffer);
        glUnmapBuffer(GL_COPY_READ_BUFFER);
        glBindBuffer(GL_COPY_READ_BUFFER, 0);
        glDeleteBuffers(1, &m_buffer);
    }
}

GLStagingAllocation GLStagingBuffer::Allocate(uint64_t size, uint64_t alignment)
{
    uint64_t aligned = (m_offset + alignment - 1) & ~(alignment - 1);
    if (aligned + size > m_capacity) {
        return {};
    }

    GLStagingAllocation alloc;
    alloc.offset = aligned;
    alloc.size = size;
    alloc.buffer = m_buffer;
    alloc.mappedPtr = static_cast<uint8_t*>(m_mappedPtr) + aligned;

    m_offset = aligned + size;
    return alloc;
}

void GLStagingBuffer::Reset()
{
    m_offset = 0;
}
} // namespace luna::RHI
