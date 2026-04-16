#include "Impls/OpenGL/GLCommon.h"
#include "Impls/OpenGL/GLPipeline.h"
#include "Impls/OpenGL/GLShaderModule.h"

namespace Cacao {
static GLenum CompareOpToGL(CompareOp op)
{
    switch (op) {
        case CompareOp::Never:
            return GL_NEVER;
        case CompareOp::Less:
            return GL_LESS;
        case CompareOp::Equal:
            return GL_EQUAL;
        case CompareOp::LessOrEqual:
            return GL_LEQUAL;
        case CompareOp::Greater:
            return GL_GREATER;
        case CompareOp::NotEqual:
            return GL_NOTEQUAL;
        case CompareOp::GreaterOrEqual:
            return GL_GEQUAL;
        case CompareOp::Always:
            return GL_ALWAYS;
        default:
            return GL_LESS;
    }
}

static GLenum StencilOpToGL(StencilOp op)
{
    switch (op) {
        case StencilOp::Keep:
            return GL_KEEP;
        case StencilOp::Zero:
            return GL_ZERO;
        case StencilOp::Replace:
            return GL_REPLACE;
        case StencilOp::IncrementAndClamp:
            return GL_INCR;
        case StencilOp::DecrementAndClamp:
            return GL_DECR;
        case StencilOp::Invert:
            return GL_INVERT;
        case StencilOp::IncrementWrap:
            return GL_INCR_WRAP;
        case StencilOp::DecrementWrap:
            return GL_DECR_WRAP;
        default:
            return GL_KEEP;
    }
}

static GLenum BlendFactorToGL(BlendFactor factor)
{
    switch (factor) {
        case BlendFactor::Zero:
            return GL_ZERO;
        case BlendFactor::One:
            return GL_ONE;
        case BlendFactor::SrcColor:
            return GL_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor:
            return GL_ONE_MINUS_SRC_COLOR;
        case BlendFactor::DstColor:
            return GL_DST_COLOR;
        case BlendFactor::OneMinusDstColor:
            return GL_ONE_MINUS_DST_COLOR;
        case BlendFactor::SrcAlpha:
            return GL_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha:
            return GL_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DstAlpha:
            return GL_DST_ALPHA;
        case BlendFactor::OneMinusDstAlpha:
            return GL_ONE_MINUS_DST_ALPHA;
        case BlendFactor::ConstantColor:
            return GL_CONSTANT_COLOR;
        case BlendFactor::SrcAlphaSaturate:
            return GL_SRC_ALPHA_SATURATE;
        default:
            return GL_ONE;
    }
}

static GLenum BlendOpToGL(BlendOp op)
{
    switch (op) {
        case BlendOp::Add:
            return GL_FUNC_ADD;
        case BlendOp::Subtract:
            return GL_FUNC_SUBTRACT;
        case BlendOp::ReverseSubtract:
            return GL_FUNC_REVERSE_SUBTRACT;
        case BlendOp::Min:
            return GL_MIN;
        case BlendOp::Max:
            return GL_MAX;
        default:
            return GL_FUNC_ADD;
    }
}

GLGraphicsPipeline::GLGraphicsPipeline(const GraphicsPipelineCreateInfo& info)
{
    m_program = glCreateProgram();
    for (const auto& shader : info.Shaders) {
        if (auto glShader = std::dynamic_pointer_cast<GLShaderModule>(shader)) {
            GLuint oldProg = glShader->GetProgram();
            GLint shaderCount = 0;
            glGetProgramiv(oldProg, GL_ATTACHED_SHADERS, &shaderCount);
            if (shaderCount > 0) {
                std::vector<GLuint> shaders(shaderCount);
                glGetAttachedShaders(oldProg, shaderCount, nullptr, shaders.data());
                for (GLuint s : shaders) {
                    glAttachShader(m_program, s);
                }
            }
        }
    }
    glLinkProgram(m_program);
    GLint linked = GL_FALSE;
    glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint len = 0;
        glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetProgramInfoLog(m_program, len, nullptr, log.data());
        fprintf(stderr, "[GL] Pipeline link error: %s\n", log.c_str());
    }

    if (!info.ColorBlend.Attachments.empty() && info.ColorBlend.Attachments[0].BlendEnable) {
        auto& att = info.ColorBlend.Attachments[0];
        m_state.blendEnabled = true;
        m_state.blendSrcRGB = BlendFactorToGL(att.SrcColorBlendFactor);
        m_state.blendDstRGB = BlendFactorToGL(att.DstColorBlendFactor);
        m_state.blendSrcAlpha = BlendFactorToGL(att.SrcAlphaBlendFactor);
        m_state.blendDstAlpha = BlendFactorToGL(att.DstAlphaBlendFactor);
        m_state.blendOpRGB = BlendOpToGL(att.ColorBlendOp);
        m_state.blendOpAlpha = BlendOpToGL(att.AlphaBlendOp);
    }

    m_state.depthTestEnabled = info.DepthStencil.DepthTestEnable;
    m_state.depthWriteEnabled = info.DepthStencil.DepthWriteEnable;
    m_state.depthFunc = CompareOpToGL(info.DepthStencil.DepthCompareOp);

    if (info.DepthStencil.StencilTestEnable) {
        m_state.stencilTestEnabled = true;
        m_state.stencilFrontFunc = CompareOpToGL(info.DepthStencil.Front.CompareOp);
        m_state.stencilFrontFailOp = StencilOpToGL(info.DepthStencil.Front.FailOp);
        m_state.stencilFrontDepthFailOp = StencilOpToGL(info.DepthStencil.Front.DepthFailOp);
        m_state.stencilFrontPassOp = StencilOpToGL(info.DepthStencil.Front.PassOp);
        m_state.stencilBackFunc = CompareOpToGL(info.DepthStencil.Back.CompareOp);
        m_state.stencilBackFailOp = StencilOpToGL(info.DepthStencil.Back.FailOp);
        m_state.stencilBackDepthFailOp = StencilOpToGL(info.DepthStencil.Back.DepthFailOp);
        m_state.stencilBackPassOp = StencilOpToGL(info.DepthStencil.Back.PassOp);
    }

    m_state.topology = PrimitiveTopologyToGL(static_cast<uint32_t>(info.InputAssembly.Topology));

    if (info.Rasterizer.CullMode != CullMode::None) {
        m_state.cullEnabled = true;
        switch (info.Rasterizer.CullMode) {
            case CullMode::Front:
                m_state.cullMode = GL_FRONT;
                break;
            case CullMode::Back:
                m_state.cullMode = GL_BACK;
                break;
            case CullMode::FrontAndBack:
                m_state.cullMode = GL_FRONT_AND_BACK;
                break;
            default:
                m_state.cullMode = GL_BACK;
                break;
        }
    }
    m_state.frontFace = (info.Rasterizer.FrontFace == FrontFace::CounterClockwise) ? GL_CCW : GL_CW;
}

Ref<GLGraphicsPipeline> GLGraphicsPipeline::Create(const GraphicsPipelineCreateInfo& info)
{
    return std::make_shared<GLGraphicsPipeline>(info);
}

void GLGraphicsPipeline::Bind() const
{

    glUseProgram(m_program);
    if (m_state.blendEnabled) {
        glEnable(GL_BLEND);
        glBlendFuncSeparate(m_state.blendSrcRGB, m_state.blendDstRGB, m_state.blendSrcAlpha, m_state.blendDstAlpha);
    } else {
        glDisable(GL_BLEND);
    }

    if (m_state.depthTestEnabled) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(m_state.depthFunc);
        glDepthMask(m_state.depthWriteEnabled ? GL_TRUE : GL_FALSE);
    } else {
        glDisable(GL_DEPTH_TEST);
    }

    if (m_state.cullEnabled) {
        glEnable(GL_CULL_FACE);
        glCullFace(m_state.cullMode);
        glFrontFace(m_state.frontFace);
    } else {
        glDisable(GL_CULL_FACE);
    }
}

// --- Compute ---

GLComputePipeline::GLComputePipeline(const ComputePipelineCreateInfo& info)
{
    if (auto glShader = std::dynamic_pointer_cast<GLShaderModule>(info.ComputeShader)) {
        m_program = glShader->GetProgram();
    }
}

Ref<GLComputePipeline> GLComputePipeline::Create(const ComputePipelineCreateInfo& info)
{
    return std::make_shared<GLComputePipeline>(info);
}
} // namespace Cacao
