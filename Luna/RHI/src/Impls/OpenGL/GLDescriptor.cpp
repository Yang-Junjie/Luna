#include "Impls/OpenGL/GLBindingGroup.h"
#include "Impls/OpenGL/GLBuffer.h"
#include "Impls/OpenGL/GLDescriptor.h"
#include "Impls/OpenGL/GLSampler.h"
#include "Impls/OpenGL/GLTexture.h"

namespace luna::RHI {
void GLDescriptorSet::WriteBuffer(const BufferWriteInfo& info)
{
    auto* glBuf = static_cast<GLBuffer*>(info.Buffer.get());
    if (!glBuf) {
        return;
    }
    if (info.Type == DescriptorType::StorageBuffer || info.Type == DescriptorType::StorageBufferDynamic) {
        m_bindingGroup.AddStorageBuffer(info.Binding, glBuf->GetHandle());
    } else {
        m_bindingGroup.AddUniformBuffer(info.Binding, glBuf->GetHandle());
    }
}

void GLDescriptorSet::WriteTexture(const TextureWriteInfo& info)
{
    if (auto glView = std::dynamic_pointer_cast<GLTextureView>(info.TextureView)) {
        m_bindingGroup.AddTexture(info.Binding, glView->GetHandle(), glView->GetTarget(), info.Binding);
    }
}

void GLDescriptorSet::WriteSampler(const SamplerWriteInfo& info)
{
    auto* glSampler = static_cast<GLSampler*>(info.Sampler.get());
    if (!glSampler) {
        return;
    }
    m_bindingGroup.AddSampler(info.Binding, glSampler->GetHandle(), info.Binding);
}

void GLDescriptorSet::WriteBuffers(const BufferWriteInfos& infos)
{
    for (size_t i = 0; i < infos.Buffers.size(); ++i) {
        BufferWriteInfo info;
        info.Binding = infos.Binding;
        info.Buffer = infos.Buffers[i];
        info.Type = infos.Type;
        WriteBuffer(info);
    }
}

void GLDescriptorSet::WriteTextures(const TextureWriteInfos& infos) {}

void GLDescriptorSet::WriteSamplers(const SamplerWriteInfos& infos)
{
    for (size_t i = 0; i < infos.Samplers.size(); ++i) {
        SamplerWriteInfo info;
        info.Binding = infos.Binding;
        info.Sampler = infos.Samplers[i];
        WriteSampler(info);
    }
}

Ref<DescriptorSet> GLDescriptorPool::AllocateDescriptorSet(const Ref<DescriptorSetLayout>& layout)
{
    return std::make_shared<GLBindingGroup>();
}
} // namespace luna::RHI
