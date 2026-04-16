#include "Impls/OpenGL/GLSampler.h"

namespace Cacao
{
    static GLenum FilterToGL(Filter mode)
    {
        return (mode == Filter::Nearest) ? GL_NEAREST : GL_LINEAR;
    }

    static GLenum WrapToGL(SamplerAddressMode mode)
    {
        switch (mode)
        {
        case SamplerAddressMode::Repeat:            return GL_REPEAT;
        case SamplerAddressMode::MirroredRepeat:    return GL_MIRRORED_REPEAT;
        case SamplerAddressMode::ClampToEdge:       return GL_CLAMP_TO_EDGE;
        case SamplerAddressMode::ClampToBorder:     return GL_CLAMP_TO_BORDER;
        case SamplerAddressMode::MirrorClampToEdge: return GL_MIRROR_CLAMP_TO_EDGE;
        default:                                    return GL_REPEAT;
        }
    }

    GLSampler::GLSampler(const SamplerCreateInfo& info)
        : m_createInfo(info)
    {
        glGenSamplers(1, &m_sampler);
        glSamplerParameteri(m_sampler, GL_TEXTURE_MIN_FILTER, FilterToGL(info.MinFilter));
        glSamplerParameteri(m_sampler, GL_TEXTURE_MAG_FILTER, FilterToGL(info.MagFilter));
        glSamplerParameteri(m_sampler, GL_TEXTURE_WRAP_S, WrapToGL(info.AddressModeU));
        glSamplerParameteri(m_sampler, GL_TEXTURE_WRAP_T, WrapToGL(info.AddressModeV));
        glSamplerParameteri(m_sampler, GL_TEXTURE_WRAP_R, WrapToGL(info.AddressModeW));

        if (info.AnisotropyEnable && info.MaxAnisotropy > 1.0f)
            glSamplerParameterf(m_sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, info.MaxAnisotropy);

        if (info.CompareEnable)
        {
            glSamplerParameteri(m_sampler, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glSamplerParameteri(m_sampler, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        }
    }

    Ref<GLSampler> GLSampler::Create(const SamplerCreateInfo& info)
    {
        return std::make_shared<GLSampler>(info);
    }

    GLSampler::~GLSampler()
    {
        if (m_sampler)
            glDeleteSamplers(1, &m_sampler);
    }
}
