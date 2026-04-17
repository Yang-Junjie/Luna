#ifndef LUNA_RHI_GLSAMPLER_H
#define LUNA_RHI_GLSAMPLER_H
#include "GLCommon.h"
#include "Sampler.h"

namespace luna::RHI {
class LUNA_RHI_API GLSampler final : public Sampler {
public:
    GLSampler(const SamplerCreateInfo& info);
    static Ref<GLSampler> Create(const SamplerCreateInfo& info);
    ~GLSampler() override;

    const SamplerCreateInfo& GetInfo() const override
    {
        return m_createInfo;
    }

    GLuint GetHandle() const
    {
        return m_sampler;
    }

private:
    GLuint m_sampler = 0;
    SamplerCreateInfo m_createInfo;
};
} // namespace luna::RHI

#endif
