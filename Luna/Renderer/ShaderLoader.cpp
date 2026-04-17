#include "Renderer/ShaderLoader.h"

#include <cstring>

#include <fstream>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <iterator>

namespace luna::rhi {
namespace {

class GlslangProcess {
public:
    GlslangProcess()
    {
        glslang::InitializeProcess();
    }

    ~GlslangProcess()
    {
        glslang::FinalizeProcess();
    }
};

void ensureGlslangInitialized()
{
    static GlslangProcess process;
    (void) process;
}

EShLanguage toNativeShaderStage(ShaderType type)
{
    switch (type) {
        case ShaderType::VERTEX:
            return EShLangVertex;
        case ShaderType::TESS_CONTROL:
            return EShLangTessControl;
        case ShaderType::TESS_EVALUATION:
            return EShLangTessEvaluation;
        case ShaderType::GEOMETRY:
            return EShLangGeometry;
        case ShaderType::FRAGMENT:
            return EShLangFragment;
        case ShaderType::COMPUTE:
            return EShLangCompute;
        case ShaderType::RAY_GEN:
            return EShLangRayGen;
        case ShaderType::INTERSECT:
            return EShLangIntersect;
        case ShaderType::ANY_HIT:
            return EShLangAnyHit;
        case ShaderType::CLOSEST_HIT:
            return EShLangClosestHit;
        case ShaderType::MISS:
            return EShLangMiss;
        case ShaderType::CALLABLE:
            return EShLangCallable;
        case ShaderType::TASK_NV:
            return EShLangTaskNV;
        case ShaderType::MESH_NV:
            return EShLangMeshNV;
        default:
            return EShLangVertex;
    }
}

glslang::EShSource toNativeShaderSource(ShaderLanguage language)
{
    switch (language) {
        case ShaderLanguage::GLSL:
            return glslang::EShSourceGlsl;
        case ShaderLanguage::HLSL:
            return glslang::EShSourceHlsl;
        default:
            return glslang::EShSourceGlsl;
    }
}

luna::RHI::ShaderStage toRHIShaderStage(ShaderType type)
{
    using ShaderStage = luna::RHI::ShaderStage;

    switch (type) {
        case ShaderType::VERTEX:
            return ShaderStage::Vertex;
        case ShaderType::TESS_CONTROL:
            return ShaderStage::TessellationControl;
        case ShaderType::TESS_EVALUATION:
            return ShaderStage::TessellationEvaluation;
        case ShaderType::GEOMETRY:
            return ShaderStage::Geometry;
        case ShaderType::FRAGMENT:
            return ShaderStage::Fragment;
        case ShaderType::COMPUTE:
            return ShaderStage::Compute;
        case ShaderType::RAY_GEN:
            return ShaderStage::RayGen;
        case ShaderType::INTERSECT:
            return ShaderStage::RayIntersection;
        case ShaderType::ANY_HIT:
            return ShaderStage::RayAnyHit;
        case ShaderType::CLOSEST_HIT:
            return ShaderStage::RayClosestHit;
        case ShaderType::MISS:
            return ShaderStage::RayMiss;
        case ShaderType::CALLABLE:
            return ShaderStage::Callable;
        case ShaderType::TASK_NV:
            return ShaderStage::Task;
        case ShaderType::MESH_NV:
            return ShaderStage::Mesh;
        default:
            return ShaderStage::None;
    }
}

} // namespace

ShaderData ShaderLoader::LoadFromBinaryFile(const std::string& filepath)
{
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return {};
    }

    const std::vector<char> binary_data{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    if (binary_data.empty() || (binary_data.size() % sizeof(uint32_t)) != 0) {
        return {};
    }

    std::vector<uint32_t> bytecode(binary_data.size() / sizeof(uint32_t));
    std::memcpy(bytecode.data(), binary_data.data(), binary_data.size());
    return LoadFromBinary(std::move(bytecode));
}

ShaderData ShaderLoader::LoadFromSourceFile(const std::string& filepath, ShaderType type, ShaderLanguage language)
{
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return {};
    }

    const std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    return LoadFromSource(source, type, language);
}

ShaderData ShaderLoader::LoadFromBinary(std::vector<uint32_t> bytecode)
{
    return ShaderData{
        .Bytecode = std::move(bytecode),
        .Stage = luna::RHI::ShaderStage::None,
    };
}

ShaderData ShaderLoader::LoadFromSource(const std::string& code, ShaderType type, ShaderLanguage language)
{
    if (code.empty()) {
        return {};
    }

    ensureGlslangInitialized();

    const auto native_stage = toNativeShaderStage(type);
    glslang::TShader shader(native_stage);
    const char* source = code.c_str();
    shader.setStrings(&source, 1);
    shader.setEntryPoint("main");
    shader.setSourceEntryPoint("main");
    shader.setEnvInput(toNativeShaderSource(language), native_stage, glslang::EShClientVulkan, 460);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_6);

    const auto messages = static_cast<EShMessages>(EShMsgDefault | EShMsgSpvRules | EShMsgVulkanRules);
    if (!shader.parse(GetDefaultResources(), 460, false, messages)) {
        return {};
    }

    glslang::TProgram program;
    program.addShader(&shader);
    if (!program.link(messages)) {
        return {};
    }

    glslang::TIntermediate* intermediate = program.getIntermediate(native_stage);
    if (intermediate == nullptr) {
        return {};
    }

    std::vector<uint32_t> bytecode;
    glslang::SpvOptions options;
    options.generateDebugInfo = false;
    options.disableOptimizer = true;
    options.optimizeSize = false;
    glslang::GlslangToSpv(*intermediate, bytecode, nullptr, &options);

    return ShaderData{
        .Bytecode = std::move(bytecode),
        .Stage = toRHIShaderStage(type),
    };
}

} // namespace luna::rhi
