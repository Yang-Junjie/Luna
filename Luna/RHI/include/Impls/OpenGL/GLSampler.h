#ifndef CACAO_GLSAMPLER_H
#define CACAO_GLSAMPLER_H
#include "Sampler.h"
#include "GLCommon.h"

namespace Cacao
{
    class CACAO_API GLSampler final : public Sampler
    {
    public:
        GLSampler(const SamplerCreateInfo& info);
        static Ref<GLSampler> Create(const SamplerCreateInfo& info);
        ~GLSampler() override;

        const SamplerCreateInfo& GetInfo() const override { return m_createInfo; }
        GLuint GetHandle() const { return m_sampler; }

    private:
        GLuint m_sampler = 0;
        SamplerCreateInfo m_createInfo;
    };
}

#endif
