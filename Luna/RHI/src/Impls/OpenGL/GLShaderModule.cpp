#include "Impls/OpenGL/GLShaderModule.h"
#include "Logging.h"

#include <cstring>

#include <string>

namespace luna::RHI {
static std::string FixupVulkanGLSLForOpenGL(const char* source)
{
    std::string glsl(source);

    auto replaceAll = [](std::string& str, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = str.find(from, pos)) != std::string::npos) {
            str.replace(pos, from.length(), to);
            pos += to.length();
        }
    };

    // Strip Slang-generated $ identifiers (e.g. $Global, $Globals_0)
    // Replace $identifier with _identifier
    {
        size_t pos = 0;
        while ((pos = glsl.find('$', pos)) != std::string::npos) {
            glsl[pos] = '_';
            pos++;
        }
    }

    // Vulkan builtins -> GL builtins
    replaceAll(glsl, "gl_VertexIndex", "gl_VertexID");
    replaceAll(glsl, "gl_InstanceIndex", "gl_InstanceID");

    // Slang may emit set(N) which GL doesn't support; strip set= from layout
    // layout(set = 0, binding = N) -> layout(binding = N)
    {
        size_t pos = 0;
        while ((pos = glsl.find("set = ", pos)) != std::string::npos) {
            size_t end = glsl.find(',', pos);
            if (end != std::string::npos) {
                // "set = N, " -> ""
                glsl.erase(pos, end - pos + 2);
            } else {
                // "set = N" -> ""
                size_t numEnd = pos + 6;
                while (numEnd < glsl.size() && (glsl[numEnd] >= '0' && glsl[numEnd] <= '9')) {
                    numEnd++;
                }
                glsl.erase(pos, numEnd - pos);
            }
        }
    }

    // Slang may emit "layout(set=0, binding=N)" (no spaces)
    {
        size_t pos = 0;
        while ((pos = glsl.find("set=", pos)) != std::string::npos) {
            size_t end = glsl.find(',', pos);
            if (end != std::string::npos) {
                glsl.erase(pos, end - pos + 1);
                while (pos < glsl.size() && glsl[pos] == ' ') {
                    glsl.erase(pos, 1);
                }
            } else {
                size_t numEnd = pos + 4;
                while (numEnd < glsl.size() && (glsl[numEnd] >= '0' && glsl[numEnd] <= '9')) {
                    numEnd++;
                }
                glsl.erase(pos, numEnd - pos);
            }
        }
    }

    // Fix "layout(, binding" -> "layout(binding"
    replaceAll(glsl, "layout(, ", "layout(");
    replaceAll(glsl, "layout( , ", "layout(");

    // Vulkan push_constant -> std140 uniform block
    replaceAll(glsl, "layout(push_constant)", "layout(std140)");

    // Vulkan separate texture/sampler -> GL combined sampler2D
    // "uniform texture2D name;" -> "uniform sampler2D name;"
    replaceAll(glsl, "uniform texture2D ", "uniform sampler2D ");
    replaceAll(glsl, "uniform texture3D ", "uniform sampler3D ");
    replaceAll(glsl, "uniform textureCube ", "uniform samplerCube ");
    replaceAll(glsl, "uniform texture2DArray ", "uniform sampler2DArray ");

    // Remove standalone sampler declarations: "uniform sampler name_0;"
    // These are Vulkan-only separate sampler objects
    {
        size_t pos = 0;
        while ((pos = glsl.find("uniform sampler ", pos)) != std::string::npos) {
            // Check it's not "uniform sampler2D" etc
            size_t afterSampler = pos + 16; // length of "uniform sampler "
            if (afterSampler < glsl.size() && glsl[afterSampler] != '2' && glsl[afterSampler] != '3' &&
                glsl[afterSampler] != 'C') {
                size_t lineEnd = glsl.find('\n', pos);
                if (lineEnd == std::string::npos) {
                    lineEnd = glsl.size();
                }
                // Also remove any preceding layout line
                size_t lineStart = pos;
                if (lineStart > 0 && glsl[lineStart - 1] == '\n') {
                    size_t prevLine = glsl.rfind('\n', lineStart - 2);
                    if (prevLine != std::string::npos) {
                        std::string prev = glsl.substr(prevLine + 1, lineStart - prevLine - 1);
                        if (prev.find("layout(") != std::string::npos) {
                            lineStart = prevLine + 1;
                        }
                    }
                }
                glsl.erase(lineStart, lineEnd - lineStart + 1);
            } else {
                pos = afterSampler;
            }
        }
    }

    // Fix Vulkan combined sampler construction: sampler2D(tex, samp) -> tex
    // "texture(sampler2D(texName,sampName), uv)" -> "texture(texName, uv)"
    {
        size_t pos = 0;
        while ((pos = glsl.find("sampler2D(", pos)) != std::string::npos) {
            // Check this is not a declaration "uniform sampler2D"
            if (pos > 0 && glsl[pos - 1] != ' ' && glsl[pos - 1] != '(') {
                pos += 10;
                continue;
            }
            size_t start = pos;
            size_t openParen = pos + 9; // position of '('
            // Find the comma separating texture from sampler
            size_t comma = glsl.find(',', openParen + 1);
            if (comma == std::string::npos) {
                pos += 10;
                continue;
            }
            // Find the closing paren
            size_t closeParen = glsl.find(')', comma + 1);
            if (closeParen == std::string::npos) {
                pos += 10;
                continue;
            }

            // Extract texture name
            std::string texName = glsl.substr(openParen + 1, comma - openParen - 1);
            // Trim whitespace
            while (!texName.empty() && texName.front() == ' ') {
                texName.erase(0, 1);
            }
            while (!texName.empty() && texName.back() == ' ') {
                texName.pop_back();
            }

            // Replace "sampler2D(tex,samp)" with just "tex"
            glsl.replace(start, closeParen - start + 1, texName);
            pos = start + texName.length();
        }
    }

    // Ensure #version is present
    if (glsl.find("#version") == std::string::npos) {
        glsl = "#version 450 core\n" + glsl;
    }

    return glsl;
}

GLShaderModule::GLShaderModule(const ShaderBlob& blob, const ShaderCreateInfo& info)
    : m_blob(blob),
      m_reflection(info.Reflection),
      m_stage(info.Stage),
      m_entryPoint(info.EntryPoint)
{
    m_program = glCreateProgram();

    std::string rawStr(reinterpret_cast<const char*>(blob.Data.data()), blob.Data.size());
    while (!rawStr.empty() && (rawStr.back() == '\0' || static_cast<unsigned char>(rawStr.back()) > 127)) {
        rawStr.pop_back();
    }
    std::string fixedSource = FixupVulkanGLSLForOpenGL(rawStr.c_str());
    const char* source = fixedSource.c_str();

    if (info.Stage == ShaderStage::Vertex) {
        if (!CompileShader(GL_VERTEX_SHADER, source, m_vertShader)) {
            return;
        }
        glAttachShader(m_program, m_vertShader);
    } else if (info.Stage == ShaderStage::Fragment) {
        if (!CompileShader(GL_FRAGMENT_SHADER, source, m_fragShader)) {
            return;
        }
        glAttachShader(m_program, m_fragShader);
    } else if (info.Stage == ShaderStage::Compute) {
        if (!CompileShader(GL_COMPUTE_SHADER, source, m_compShader)) {
            return;
        }
        glAttachShader(m_program, m_compShader);
    }

    m_valid = LinkProgram();
}

Ref<GLShaderModule> GLShaderModule::Create(const ShaderBlob& blob, const ShaderCreateInfo& info)
{
    return std::make_shared<GLShaderModule>(blob, info);
}

GLShaderModule::~GLShaderModule()
{
    if (m_vertShader) {
        glDeleteShader(m_vertShader);
    }
    if (m_fragShader) {
        glDeleteShader(m_fragShader);
    }
    if (m_compShader) {
        glDeleteShader(m_compShader);
    }
    if (m_program) {
        glDeleteProgram(m_program);
    }
}

bool GLShaderModule::CompileShader(GLenum type, const char* source, GLuint& outShader)
{
    outShader = glCreateShader(type);
    glShaderSource(outShader, 1, &source, nullptr);
    glCompileShader(outShader);

    GLint success = 0;
    glGetShaderiv(outShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLength = 0;
        glGetShaderiv(outShader, GL_INFO_LOG_LENGTH, &logLength);
        if (logLength > 0) {
            std::string log(logLength, '\0');
            glGetShaderInfoLog(outShader, logLength, nullptr, log.data());
            LogMessage(LogLevel::Error, std::string("[GLShaderModule] Compile error: ") + log);
        }
        LogMessage(LogLevel::Error,
                   std::string("[GLShaderModule] === Full GLSL source ===\n") + source + "\n=== End ===");
        glDeleteShader(outShader);
        outShader = 0;
        return false;
    }
    return true;
}

bool GLShaderModule::LinkProgram()
{
    glLinkProgram(m_program);

    GLint success = 0;
    glGetProgramiv(m_program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLength = 0;
        glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &logLength);
        if (logLength > 0) {
            std::string log(logLength, '\0');
            glGetProgramInfoLog(m_program, logLength, nullptr, log.data());
            LogMessage(LogLevel::Error, std::string("[GLShaderModule] Link error: ") + log);
        }
        return false;
    }
    return true;
}
} // namespace luna::RHI
