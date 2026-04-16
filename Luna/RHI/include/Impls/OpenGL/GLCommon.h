#ifndef CACAO_GLCOMMON_H
#define CACAO_GLCOMMON_H

#ifdef CACAO_GLES
#include <GLES3/gl32.h>
#include <GLES2/gl2ext.h>
#else
#include <glad/glad.h>
#endif

#include <Core.h>
#include <Barrier.h>

namespace Cacao
{
    struct GLFormatInfo
    {
        GLenum internalFormat;
        GLenum format;
        GLenum type;
    };

    inline GLFormatInfo FormatToGL(Format fmt)
    {
        switch (fmt)
        {
        case Format::R8_UNORM:            return {GL_R8, GL_RED, GL_UNSIGNED_BYTE};
        case Format::R8_SNORM:            return {GL_R8_SNORM, GL_RED, GL_BYTE};
        case Format::R16_FLOAT:           return {GL_R16F, GL_RED, GL_HALF_FLOAT};
        case Format::R32_FLOAT:           return {GL_R32F, GL_RED, GL_FLOAT};
        case Format::RG8_UNORM:           return {GL_RG8, GL_RG, GL_UNSIGNED_BYTE};
        case Format::RG16_FLOAT:          return {GL_RG16F, GL_RG, GL_HALF_FLOAT};
        case Format::RG32_FLOAT:          return {GL_RG32F, GL_RG, GL_FLOAT};
        case Format::RGBA8_UNORM:         return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};
        case Format::RGBA8_SNORM:         return {GL_RGBA8_SNORM, GL_RGBA, GL_BYTE};
        case Format::RGBA16_FLOAT:        return {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT};
        case Format::RGBA32_FLOAT:        return {GL_RGBA32F, GL_RGBA, GL_FLOAT};
        case Format::BGRA8_UNORM:         return {GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE};
        case Format::D16_UNORM:           return {GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT};
        case Format::D32_FLOAT:           return {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT};
        case Format::D24_UNORM_S8_UINT:   return {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8};
        default:                          return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};
        }
    }

    inline GLenum BufferUsageToGL(uint32_t usage)
    {
        return GL_DYNAMIC_DRAW;
    }

    inline GLenum PrimitiveTopologyToGL(uint32_t topology)
    {
        switch (topology)
        {
        case 0:  return GL_TRIANGLES;
        case 1:  return GL_TRIANGLE_STRIP;
        case 2:  return GL_LINES;
        case 3:  return GL_LINE_STRIP;
        case 4:  return GL_POINTS;
        default: return GL_TRIANGLES;
        }
    }

    inline void TransitionResourceState(GLenum, GLenum) {}
}

#endif
