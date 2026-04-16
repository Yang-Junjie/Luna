#ifndef CACAO_GLPIPELINE_H
#define CACAO_GLPIPELINE_H
#include "GLCommon.h"
#include "Pipeline.h"

namespace Cacao {
struct GLPipelineState {
    bool blendEnabled = false;
    GLenum blendSrcRGB = GL_ONE;
    GLenum blendDstRGB = GL_ZERO;
    GLenum blendSrcAlpha = GL_ONE;
    GLenum blendDstAlpha = GL_ZERO;
    GLenum blendOpRGB = GL_FUNC_ADD;
    GLenum blendOpAlpha = GL_FUNC_ADD;

    bool depthTestEnabled = false;
    bool depthWriteEnabled = true;
    GLenum depthFunc = GL_LESS;

    bool stencilTestEnabled = false;
    GLenum stencilFrontFunc = GL_ALWAYS;
    GLenum stencilFrontFailOp = GL_KEEP;
    GLenum stencilFrontDepthFailOp = GL_KEEP;
    GLenum stencilFrontPassOp = GL_KEEP;
    GLenum stencilBackFunc = GL_ALWAYS;
    GLenum stencilBackFailOp = GL_KEEP;
    GLenum stencilBackDepthFailOp = GL_KEEP;
    GLenum stencilBackPassOp = GL_KEEP;

    GLenum cullMode = GL_BACK;
    bool cullEnabled = true;
    GLenum frontFace = GL_CCW;

    GLenum polygonMode = GL_FILL;
    GLenum topology = GL_TRIANGLES;
};

class CACAO_API GLGraphicsPipeline final : public GraphicsPipeline {
public:
    GLGraphicsPipeline(const GraphicsPipelineCreateInfo& info);
    static Ref<GLGraphicsPipeline> Create(const GraphicsPipelineCreateInfo& info);

    void Bind() const;

    const GLPipelineState& GetState() const
    {
        return m_state;
    }

    GLuint GetProgram() const
    {
        return m_program;
    }

    Ref<PipelineLayout> GetLayout() const override
    {
        return m_layout;
    }

private:
    GLPipelineState m_state;
    GLuint m_program = 0;
    Ref<PipelineLayout> m_layout;
};

class CACAO_API GLComputePipeline final : public ComputePipeline {
public:
    GLComputePipeline(const ComputePipelineCreateInfo& info);
    static Ref<GLComputePipeline> Create(const ComputePipelineCreateInfo& info);

    GLuint GetProgram() const
    {
        return m_program;
    }

    Ref<PipelineLayout> GetLayout() const override
    {
        return m_layout;
    }

private:
    GLuint m_program = 0;
    Ref<PipelineLayout> m_layout;
};
} // namespace Cacao

#endif
