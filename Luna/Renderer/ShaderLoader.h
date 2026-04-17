#pragma once

#include <cstdint>

#include <Core.h>
#include <string>
#include <vector>

namespace luna::rhi {

enum class ShaderType {
    VERTEX,
    TESS_CONTROL,
    TESS_EVALUATION,
    GEOMETRY,
    FRAGMENT,
    COMPUTE,
    RAY_GEN,
    INTERSECT,
    ANY_HIT,
    CLOSEST_HIT,
    MISS,
    CALLABLE,
    TASK_NV,
    MESH_NV,
};

enum class ShaderLanguage {
    GLSL,
    HLSL,
};

struct ShaderData {
    std::vector<uint32_t> Bytecode;
    luna::RHI::ShaderStage Stage{luna::RHI::ShaderStage::None};

    bool isValid() const
    {
        return !Bytecode.empty() && Stage != luna::RHI::ShaderStage::None;
    }
};

class ShaderLoader {
public:
    static ShaderData LoadFromSourceFile(const std::string& filepath, ShaderType type, ShaderLanguage language);
    static ShaderData LoadFromBinaryFile(const std::string& filepath);
    static ShaderData LoadFromBinary(std::vector<uint32_t> bytecode);
    static ShaderData LoadFromSource(const std::string& code, ShaderType type, ShaderLanguage language);
};

} // namespace luna::rhi
