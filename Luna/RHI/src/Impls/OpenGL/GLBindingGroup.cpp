#include "DescriptorSetLayout.h"
#include "Impls/OpenGL/GLBindingGroup.h"
#include "Impls/OpenGL/GLBuffer.h"
#include "Impls/OpenGL/GLSampler.h"
#include "Impls/OpenGL/GLTexture.h"

namespace luna::RHI {
void GLBindingGroup::AddUniformBuffer(uint32_t binding, GLuint buffer)
{
    GLBindingEntry entry;
    entry.binding = binding;
    entry.type = GLBindingEntry::Type::UniformBuffer;
    entry.resource = buffer;
    m_entries.push_back(entry);
}

void GLBindingGroup::AddStorageBuffer(uint32_t binding, GLuint buffer)
{
    GLBindingEntry entry;
    entry.binding = binding;
    entry.type = GLBindingEntry::Type::StorageBuffer;
    entry.resource = buffer;
    m_entries.push_back(entry);
}

void GLBindingGroup::AddTexture(uint32_t binding, GLuint texture, GLenum target, uint32_t textureUnit)
{
    GLBindingEntry entry;
    entry.binding = binding;
    entry.type = GLBindingEntry::Type::Texture;
    entry.resource = texture;
    entry.textureTarget = target;
    entry.uniformLocation = -1;
    entry.textureUnit = textureUnit;
    m_entries.push_back(entry);
}

void GLBindingGroup::AddSampler(uint32_t binding, GLuint sampler, uint32_t textureUnit)
{
    GLBindingEntry entry;
    entry.binding = binding;
    entry.type = GLBindingEntry::Type::Sampler;
    entry.resource = sampler;
    entry.uniformLocation = -1;
    entry.textureUnit = textureUnit;
    m_entries.push_back(entry);
}

void GLBindingGroup::AddImage(
    uint32_t binding, GLuint texture, uint32_t level, bool layered, uint32_t layer, GLenum access, GLenum format)
{
    GLBindingEntry entry;
    entry.binding = binding;
    entry.type = GLBindingEntry::Type::Image;
    entry.resource = texture;
    entry.imageLevel = level;
    entry.imageLayered = layered;
    entry.imageLayer = layer;
    entry.imageAccess = access;
    entry.imageFormat = format;
    m_entries.push_back(entry);
}

void GLBindingGroup::Bind(GLuint program) const
{
    for (const auto& entry : m_entries) {
        switch (entry.type) {
            case GLBindingEntry::Type::UniformBuffer:
                glBindBufferBase(GL_UNIFORM_BUFFER, entry.binding, entry.resource);
                break;
            case GLBindingEntry::Type::StorageBuffer:
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, entry.binding, entry.resource);
                break;
            case GLBindingEntry::Type::Texture:
                glActiveTexture(GL_TEXTURE0 + entry.textureUnit);
                glBindTexture(entry.textureTarget, entry.resource);
                break;
            case GLBindingEntry::Type::Sampler:
                for (auto& texEntry : m_entries) {
                    if (texEntry.type == GLBindingEntry::Type::Texture) {
                        glBindSampler(texEntry.textureUnit, entry.resource);
                    }
                }
                break;
            case GLBindingEntry::Type::Image:
                glBindImageTexture(entry.binding,
                                   entry.resource,
                                   entry.imageLevel,
                                   entry.imageLayered ? GL_TRUE : GL_FALSE,
                                   entry.imageLayer,
                                   entry.imageAccess,
                                   entry.imageFormat);
                break;
        }
    }
}

void GLBindingGroup::WriteBuffer(const BufferWriteInfo& info)
{
    auto glBuf = std::dynamic_pointer_cast<GLBuffer>(info.Buffer);
    if (!glBuf) {
        return;
    }

    if (info.Type == DescriptorType::UniformBuffer || info.Type == DescriptorType::UniformBufferDynamic) {
        AddUniformBuffer(info.Binding, glBuf->GetHandle());
    } else if (info.Type == DescriptorType::StorageBuffer || info.Type == DescriptorType::StorageBufferDynamic) {
        AddStorageBuffer(info.Binding, glBuf->GetHandle());
    } else {
        throw std::runtime_error("[Luna RHI] GL WriteBuffer: unsupported DescriptorType " +
                                 std::to_string(static_cast<int>(info.Type)));
    }
}

void GLBindingGroup::WriteBuffers(const BufferWriteInfos& infos)
{
    for (size_t i = 0; i < infos.Buffers.size(); i++) {
        BufferWriteInfo single;
        single.Binding = infos.Binding;
        single.Buffer = infos.Buffers[i];
        single.Type = infos.Type;
        WriteBuffer(single);
    }
}

void GLBindingGroup::WriteTexture(const TextureWriteInfo& info)
{
    if (auto glView = std::dynamic_pointer_cast<GLTextureView>(info.TextureView)) {
        AddTexture(info.Binding, glView->GetHandle(), glView->GetTarget(), info.Binding);
    }
    if (auto glSamp = std::dynamic_pointer_cast<GLSampler>(info.Sampler)) {
        AddSampler(info.Binding, glSamp->GetHandle(), info.Binding);
    }
}

void GLBindingGroup::WriteTextures(const TextureWriteInfos& infos)
{
    for (size_t i = 0; i < infos.TextureViews.size(); i++) {
        TextureWriteInfo single;
        single.Binding = infos.Binding;
        single.TextureView = infos.TextureViews[i];
        single.Type = infos.Type;
        if (i < infos.Samplers.size()) {
            single.Sampler = infos.Samplers[i];
        }
        WriteTexture(single);
    }
}

void GLBindingGroup::WriteSampler(const SamplerWriteInfo& info)
{
    if (auto glSamp = std::dynamic_pointer_cast<GLSampler>(info.Sampler)) {
        AddSampler(info.Binding, glSamp->GetHandle(), info.Binding);
    }
}

void GLBindingGroup::WriteSamplers(const SamplerWriteInfos& infos)
{
    for (size_t i = 0; i < infos.Samplers.size(); i++) {
        SamplerWriteInfo single;
        single.Binding = infos.Binding;
        single.Sampler = infos.Samplers[i];
        WriteSampler(single);
    }
}

void GLBindingGroup::WriteAccelerationStructure(const AccelerationStructureWriteInfo&) {}

void GLBindingGroup::WriteAccelerationStructures(const AccelerationStructureWriteInfos&) {}

void GLBindingGroup::Update() {}
} // namespace luna::RHI
