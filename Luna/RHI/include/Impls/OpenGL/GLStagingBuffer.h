#ifndef CACAO_GLSTAGINGBUFFER_H
#define CACAO_GLSTAGINGBUFFER_H
#include "GLCommon.h"
#include <cstdint>

namespace Cacao
{
    struct GLStagingAllocation
    {
        uint64_t offset = 0;
        uint64_t size = 0;
        void* mappedPtr = nullptr;
        GLuint buffer = 0;
    };

    class CACAO_API GLStagingBuffer
    {
    public:
        GLStagingBuffer(uint64_t capacity);
        ~GLStagingBuffer();

        GLStagingAllocation Allocate(uint64_t size, uint64_t alignment = 256);
        void Reset();
        GLuint GetHandle() const { return m_buffer; }
        uint64_t GetCapacity() const { return m_capacity; }
        uint64_t GetUsed() const { return m_offset; }

    private:
        GLuint m_buffer = 0;
        void* m_mappedPtr = nullptr;
        uint64_t m_capacity;
        uint64_t m_offset = 0;
    };
}

#endif
