#ifndef CACAO_BINDLESS_DESCRIPTORS_H
#define CACAO_BINDLESS_DESCRIPTORS_H
#include "Buffer.h"
#include "Core.h"
#include "Sampler.h"
#include "Texture.h"

#include <memory>

namespace Cacao {
struct BindlessPoolCreateInfo {
    uint32_t MaxTextures = 1'024;
    uint32_t MaxBuffers = 1'024;
    uint32_t MaxSamplers = 256;
};

class CACAO_API BindlessDescriptorPool {
public:
    virtual ~BindlessDescriptorPool() = default;
    virtual uint32_t AddTexture(const Ref<CacaoTextureView>& view) = 0;
    virtual uint32_t AddBuffer(const Ref<Buffer>& buffer, uint64_t offset = 0, uint64_t size = UINT64_MAX) = 0;
    virtual uint32_t AddSampler(const Ref<Sampler>& sampler) = 0;
    virtual void RemoveTexture(uint32_t index) = 0;
    virtual void RemoveBuffer(uint32_t index) = 0;
    virtual void RemoveSampler(uint32_t index) = 0;
    virtual void Update() = 0;
};
} // namespace Cacao

#endif
