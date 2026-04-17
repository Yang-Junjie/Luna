#ifndef LUNA_RHI_GLBINDINGGROUP_H
#define LUNA_RHI_GLBINDINGGROUP_H
#include "DescriptorSet.h"
#include "GLCommon.h"

#include <string>
#include <vector>

namespace luna::RHI {
struct GLBindingEntry {
    uint32_t binding = 0;
    enum class Type {
        UniformBuffer,
        StorageBuffer,
        Texture,
        Sampler,
        Image
    } type;
    GLuint resource = 0;
    GLint uniformLocation = -1;
    uint32_t textureUnit = 0;
    uint32_t imageLevel = 0;
    bool imageLayered = false;
    uint32_t imageLayer = 0;
    GLenum imageAccess = GL_READ_WRITE;
    GLenum imageFormat = GL_RGBA8;
};

class LUNA_RHI_API GLBindingGroup final : public DescriptorSet {
public:
    void AddUniformBuffer(uint32_t binding, GLuint buffer);
    void AddStorageBuffer(uint32_t binding, GLuint buffer);
    void AddTexture(uint32_t binding, GLuint texture, uint32_t textureUnit);
    void AddSampler(uint32_t binding, GLuint sampler, uint32_t textureUnit);
    void AddImage(
        uint32_t binding, GLuint texture, uint32_t level, bool layered, uint32_t layer, GLenum access, GLenum format);

    void Bind(GLuint program) const;

    void WriteBuffer(const BufferWriteInfo& info) override;
    void WriteBuffers(const BufferWriteInfos& infos) override;
    void WriteTexture(const TextureWriteInfo& info) override;
    void WriteTextures(const TextureWriteInfos& infos) override;
    void WriteSampler(const SamplerWriteInfo& info) override;
    void WriteSamplers(const SamplerWriteInfos& infos) override;
    void WriteAccelerationStructure(const AccelerationStructureWriteInfo& info) override;
    void WriteAccelerationStructures(const AccelerationStructureWriteInfos& infos) override;
    void Update() override;

private:
    std::vector<GLBindingEntry> m_entries;
};
} // namespace luna::RHI

#endif
