#ifndef LUNA_RHI_GLSHADERMODULE_H
#define LUNA_RHI_GLSHADERMODULE_H
#include "GLCommon.h"
#include "ShaderModule.h"

namespace luna::RHI {
class LUNA_RHI_API GLShaderModule final : public ShaderModule {
public:
    GLShaderModule(const ShaderBlob& blob, const ShaderCreateInfo& info);
    static Ref<GLShaderModule> Create(const ShaderBlob& blob, const ShaderCreateInfo& info);
    ~GLShaderModule() override;

    const std::string& GetEntryPoint() const override
    {
        return m_entryPoint;
    }

    ShaderStage GetStage() const override
    {
        return m_stage;
    }

    const ShaderBlob& GetBlob() const override
    {
        return m_blob;
    }

    GLuint GetProgram() const
    {
        return m_program;
    }

    bool IsValid() const
    {
        return m_valid;
    }

private:
    GLuint m_program = 0;
    GLuint m_vertShader = 0;
    GLuint m_fragShader = 0;
    GLuint m_compShader = 0;
    bool m_valid = false;
    ShaderBlob m_blob;
    ShaderStage m_stage = ShaderStage::Vertex;
    std::string m_entryPoint = "main";

    bool CompileShader(GLenum type, const char* source, GLuint& outShader);
    bool LinkProgram();
};
} // namespace luna::RHI

#endif
