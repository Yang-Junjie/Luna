// Copyright(c) 2021, #Momo
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met :
//
// 1. Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and /or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "Renderer/ShaderLoader.h"
#include "third_party/SPIRV-Cross/spirv_cross.hpp"
#include "Vulkan/VulkanContext.h"

#include <cstdint>
#include <cstring>

#include <algorithm>
#include <fstream>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <iterator>
#include <numeric>
#include <vector>

namespace luna::val {
namespace {
struct GlslangProcess {
    GlslangProcess()
    {
        glslang::InitializeProcess();
    }

    ~GlslangProcess()
    {
        glslang::FinalizeProcess();
    }
};

void EnsureGlslangInitialized()
{
    static GlslangProcess process;
    (void) process;
}

EShLanguage ToNativeShaderStage(const ShaderType type)
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
            assert(false);
            return EShLangVertex;
    }
}

glslang::EShSource ToNativeShaderSource(const ShaderLanguage language)
{
    switch (language) {
        case ShaderLanguage::GLSL:
            return glslang::EShSourceGlsl;
        case ShaderLanguage::HLSL:
            return glslang::EShSourceHlsl;
        default:
            assert(false);
            return glslang::EShSourceGlsl;
    }
}

glslang::EShTargetClientVersion ToTargetClientVersion(const uint32_t apiVersion)
{
    const uint32_t major = VK_VERSION_MAJOR(apiVersion);
    const uint32_t minor = VK_VERSION_MINOR(apiVersion);

    if (major > 1 || minor >= 4) {
        return glslang::EShTargetVulkan_1_4;
    }
    if (minor >= 3) {
        return glslang::EShTargetVulkan_1_3;
    }
    if (minor >= 2) {
        return glslang::EShTargetVulkan_1_2;
    }
    if (minor >= 1) {
        return glslang::EShTargetVulkan_1_1;
    }
    return glslang::EShTargetVulkan_1_0;
}

glslang::EShTargetLanguageVersion ToTargetSpirvVersion(const uint32_t apiVersion)
{
    const uint32_t major = VK_VERSION_MAJOR(apiVersion);
    const uint32_t minor = VK_VERSION_MINOR(apiVersion);

    if (major > 1 || minor >= 3) {
        return glslang::EShTargetSpv_1_6;
    }
    if (minor >= 2) {
        return glslang::EShTargetSpv_1_5;
    }
    if (minor >= 1) {
        return glslang::EShTargetSpv_1_3;
    }
    return glslang::EShTargetSpv_1_0;
}

Format ToFloatFormat(const uint32_t width, const uint32_t vecsize)
{
    if (width == 16) {
        switch (vecsize) {
            case 1:
                return Format::R16_SFLOAT;
            case 2:
                return Format::R16G16_SFLOAT;
            case 3:
                return Format::R16G16B16_SFLOAT;
            case 4:
                return Format::R16G16B16A16_SFLOAT;
            default:
                break;
        }
    }
    if (width == 32) {
        switch (vecsize) {
            case 1:
                return Format::R32_SFLOAT;
            case 2:
                return Format::R32G32_SFLOAT;
            case 3:
                return Format::R32G32B32_SFLOAT;
            case 4:
                return Format::R32G32B32A32_SFLOAT;
            default:
                break;
        }
    }
    if (width == 64) {
        switch (vecsize) {
            case 1:
                return Format::R64_SFLOAT;
            case 2:
                return Format::R64G64_SFLOAT;
            case 3:
                return Format::R64G64B64_SFLOAT;
            case 4:
                return Format::R64G64B64A64_SFLOAT;
            default:
                break;
        }
    }

    return Format::UNDEFINED;
}

Format ToSignedFormat(const uint32_t width, const uint32_t vecsize)
{
    if (width == 8) {
        switch (vecsize) {
            case 1:
                return Format::R8_SINT;
            case 2:
                return Format::R8G8_SINT;
            case 3:
                return Format::R8G8B8_SINT;
            case 4:
                return Format::R8G8B8A8_SINT;
            default:
                break;
        }
    }
    if (width == 16) {
        switch (vecsize) {
            case 1:
                return Format::R16_SINT;
            case 2:
                return Format::R16G16_SINT;
            case 3:
                return Format::R16G16B16_SINT;
            case 4:
                return Format::R16G16B16A16_SINT;
            default:
                break;
        }
    }
    if (width == 32) {
        switch (vecsize) {
            case 1:
                return Format::R32_SINT;
            case 2:
                return Format::R32G32_SINT;
            case 3:
                return Format::R32G32B32_SINT;
            case 4:
                return Format::R32G32B32A32_SINT;
            default:
                break;
        }
    }
    if (width == 64) {
        switch (vecsize) {
            case 1:
                return Format::R64_SINT;
            case 2:
                return Format::R64G64_SINT;
            case 3:
                return Format::R64G64B64_SINT;
            case 4:
                return Format::R64G64B64A64_SINT;
            default:
                break;
        }
    }

    return Format::UNDEFINED;
}

Format ToUnsignedFormat(const uint32_t width, const uint32_t vecsize)
{
    if (width == 8) {
        switch (vecsize) {
            case 1:
                return Format::R8_UINT;
            case 2:
                return Format::R8G8_UINT;
            case 3:
                return Format::R8G8B8_UINT;
            case 4:
                return Format::R8G8B8A8_UINT;
            default:
                break;
        }
    }
    if (width == 16) {
        switch (vecsize) {
            case 1:
                return Format::R16_UINT;
            case 2:
                return Format::R16G16_UINT;
            case 3:
                return Format::R16G16B16_UINT;
            case 4:
                return Format::R16G16B16A16_UINT;
            default:
                break;
        }
    }
    if (width == 32) {
        switch (vecsize) {
            case 1:
                return Format::R32_UINT;
            case 2:
                return Format::R32G32_UINT;
            case 3:
                return Format::R32G32B32_UINT;
            case 4:
                return Format::R32G32B32A32_UINT;
            default:
                break;
        }
    }
    if (width == 64) {
        switch (vecsize) {
            case 1:
                return Format::R64_UINT;
            case 2:
                return Format::R64G64_UINT;
            case 3:
                return Format::R64G64B64_UINT;
            case 4:
                return Format::R64G64B64A64_UINT;
            default:
                break;
        }
    }

    return Format::UNDEFINED;
}

TypeSPIRV ToTypeSPIRV(const spirv_cross::SPIRType& type)
{
    Format format = Format::UNDEFINED;
    switch (type.basetype) {
        case spirv_cross::SPIRType::Half:
        case spirv_cross::SPIRType::Float:
        case spirv_cross::SPIRType::Double:
            format = ToFloatFormat(type.width, type.vecsize);
            break;
        case spirv_cross::SPIRType::SByte:
        case spirv_cross::SPIRType::Short:
        case spirv_cross::SPIRType::Int:
        case spirv_cross::SPIRType::Int64:
            format = ToSignedFormat(type.width, type.vecsize);
            break;
        case spirv_cross::SPIRType::UByte:
        case spirv_cross::SPIRType::UShort:
        case spirv_cross::SPIRType::UInt:
        case spirv_cross::SPIRType::UInt64:
            format = ToUnsignedFormat(type.width, type.vecsize);
            break;
        default:
            break;
    }

    assert(format != Format::UNDEFINED);

    int32_t byteSize = static_cast<int32_t>((type.width / 8u) * type.vecsize * std::max(type.columns, 1u));
    for (size_t i = 0; i < type.array.size(); ++i) {
        if (i < type.array_size_literal.size() && type.array_size_literal[i]) {
            byteSize *= static_cast<int32_t>(type.array[i]);
        }
    }

    return TypeSPIRV{format, static_cast<int32_t>(std::max(type.columns, 1u)), byteSize};
}

void AppendUniformLayout(std::vector<TypeSPIRV>& layout,
                         const spirv_cross::Compiler& compiler,
                         const spirv_cross::SPIRType& type)
{
    if (type.basetype == spirv_cross::SPIRType::Struct) {
        for (const auto memberTypeId : type.member_types) {
            AppendUniformLayout(layout, compiler, compiler.get_type(memberTypeId));
        }
        return;
    }

    switch (type.basetype) {
        case spirv_cross::SPIRType::Half:
        case spirv_cross::SPIRType::Float:
        case spirv_cross::SPIRType::Double:
        case spirv_cross::SPIRType::SByte:
        case spirv_cross::SPIRType::Short:
        case spirv_cross::SPIRType::Int:
        case spirv_cross::SPIRType::Int64:
        case spirv_cross::SPIRType::UByte:
        case spirv_cross::SPIRType::UShort:
        case spirv_cross::SPIRType::UInt:
        case spirv_cross::SPIRType::UInt64:
            layout.push_back(ToTypeSPIRV(type));
            break;
        default:
            break;
    }
}

uint32_t GetDescriptorCount(const spirv_cross::SPIRType& type)
{
    if (type.array.empty()) {
        return 1;
    }

    uint32_t count = 1;
    for (size_t i = 0; i < type.array.size(); ++i) {
        if (i < type.array_size_literal.size() && type.array_size_literal[i] && type.array[i] > 0) {
            count *= type.array[i];
        }
    }
    return count == 0 ? 1 : count;
}

uint32_t GetDescriptorSet(const spirv_cross::Compiler& compiler, const spirv_cross::Resource& resource)
{
    return compiler.has_decoration(resource.id, spv::DecorationDescriptorSet)
               ? compiler.get_decoration(resource.id, spv::DecorationDescriptorSet)
               : 0u;
}

uint32_t GetBinding(const spirv_cross::Compiler& compiler, const spirv_cross::Resource& resource)
{
    return compiler.has_decoration(resource.id, spv::DecorationBinding)
               ? compiler.get_decoration(resource.id, spv::DecorationBinding)
               : 0u;
}

ShaderType InferShaderType(const spirv_cross::Compiler& compiler)
{
    const auto entryPoints = compiler.get_entry_points_and_stages();
    if (entryPoints.empty()) {
        return ShaderType::COMPUTE;
    }

    switch (entryPoints.front().execution_model) {
        case spv::ExecutionModelVertex:
            return ShaderType::VERTEX;
        case spv::ExecutionModelTessellationControl:
            return ShaderType::TESS_CONTROL;
        case spv::ExecutionModelTessellationEvaluation:
            return ShaderType::TESS_EVALUATION;
        case spv::ExecutionModelGeometry:
            return ShaderType::GEOMETRY;
        case spv::ExecutionModelFragment:
            return ShaderType::FRAGMENT;
        case spv::ExecutionModelGLCompute:
            return ShaderType::COMPUTE;
        case spv::ExecutionModelRayGenerationKHR:
            return ShaderType::RAY_GEN;
        case spv::ExecutionModelIntersectionKHR:
            return ShaderType::INTERSECT;
        case spv::ExecutionModelAnyHitKHR:
            return ShaderType::ANY_HIT;
        case spv::ExecutionModelClosestHitKHR:
            return ShaderType::CLOSEST_HIT;
        case spv::ExecutionModelMissKHR:
            return ShaderType::MISS;
        case spv::ExecutionModelCallableKHR:
            return ShaderType::CALLABLE;
        case spv::ExecutionModelTaskNV:
            return ShaderType::TASK_NV;
        case spv::ExecutionModelMeshNV:
            return ShaderType::MESH_NV;
        default:
            return ShaderType::COMPUTE;
    }
}

void ReflectInputAttributes(ShaderData& result, const spirv_cross::Compiler& compiler)
{
    auto resources = compiler.get_shader_resources();
    auto inputs = resources.stage_inputs;
    std::sort(
        inputs.begin(), inputs.end(), [&compiler](const spirv_cross::Resource& lhs, const spirv_cross::Resource& rhs) {
            return compiler.get_decoration(lhs.id, spv::DecorationLocation) <
                   compiler.get_decoration(rhs.id, spv::DecorationLocation);
        });

    for (const auto& input : inputs) {
        if (!compiler.has_decoration(input.id, spv::DecorationLocation)) {
            continue;
        }

        result.InputAttributes.push_back(ToTypeSPIRV(compiler.get_type(input.base_type_id)));
    }
}

void AddUniformResource(ShaderData& result,
                        const spirv_cross::Compiler& compiler,
                        const spirv_cross::Resource& resource,
                        const UniformType type)
{
    const auto setIndex = GetDescriptorSet(compiler, resource);
    if (result.DescriptorSets.size() <= setIndex) {
        result.DescriptorSets.resize(setIndex + 1);
    }

    std::vector<TypeSPIRV> layout;
    if (type == UniformType::UNIFORM_BUFFER || type == UniformType::STORAGE_BUFFER ||
        type == UniformType::UNIFORM_BUFFER_DYNAMIC || type == UniformType::STORAGE_BUFFER_DYNAMIC) {
        AppendUniformLayout(layout, compiler, compiler.get_type(resource.base_type_id));
    }

    result.DescriptorSets[setIndex].push_back(Uniform{std::move(layout),
                                                      type,
                                                      GetBinding(compiler, resource),
                                                      GetDescriptorCount(compiler.get_type(resource.type_id))});
}

void ReflectDescriptorSets(ShaderData& result, const spirv_cross::Compiler& compiler)
{
    const auto resources = compiler.get_shader_resources();

    for (const auto& resource : resources.uniform_buffers) {
        AddUniformResource(result, compiler, resource, UniformType::UNIFORM_BUFFER);
    }
    for (const auto& resource : resources.storage_buffers) {
        AddUniformResource(result, compiler, resource, UniformType::STORAGE_BUFFER);
    }
    for (const auto& resource : resources.sampled_images) {
        AddUniformResource(result, compiler, resource, UniformType::COMBINED_IMAGE_SAMPLER);
    }
    for (const auto& resource : resources.separate_images) {
        AddUniformResource(result, compiler, resource, UniformType::SAMPLED_IMAGE);
    }
    for (const auto& resource : resources.separate_samplers) {
        AddUniformResource(result, compiler, resource, UniformType::SAMPLER);
    }
    for (const auto& resource : resources.storage_images) {
        AddUniformResource(result, compiler, resource, UniformType::STORAGE_IMAGE);
    }
    for (const auto& resource : resources.subpass_inputs) {
        AddUniformResource(result, compiler, resource, UniformType::INPUT_ATTACHMENT);
    }
    for (const auto& resource : resources.acceleration_structures) {
        AddUniformResource(result, compiler, resource, UniformType::ACCELERATION_STRUCTURE_KHR);
    }

    for (auto& descriptorSet : result.DescriptorSets) {
        std::sort(descriptorSet.begin(), descriptorSet.end(), [](const Uniform& lhs, const Uniform& rhs) {
            return lhs.Binding < rhs.Binding;
        });
    }

    if (result.DescriptorSets.empty()) {
        result.DescriptorSets.emplace_back();
    }
}

ShaderData ReflectShaderData(std::vector<uint32_t> bytecode, const ShaderType type)
{
    ShaderData result;
    result.Bytecode = std::move(bytecode);

    spirv_cross::Compiler compiler(result.Bytecode);
    if (type == ShaderType::VERTEX) {
        ReflectInputAttributes(result, compiler);
    }
    ReflectDescriptorSets(result, compiler);
    return result;
}
} // namespace

ShaderData ShaderLoader::LoadFromBinaryFile(const std::string& filepath)
{
    std::ifstream file(filepath, std::ios_base::binary);
    if (!file.is_open()) {
        return {};
    }

    const auto binaryData = std::vector<char>(std::istreambuf_iterator(file), std::istreambuf_iterator<char>());
    if (binaryData.empty() || (binaryData.size() % sizeof(uint32_t)) != 0) {
        return {};
    }

    std::vector<uint32_t> bytecode(binaryData.size() / sizeof(uint32_t));
    std::memcpy(bytecode.data(), binaryData.data(), binaryData.size());
    return ShaderLoader::LoadFromBinary(std::move(bytecode));
}

ShaderData
    ShaderLoader::LoadFromSourceFile(const std::string& filepath, const ShaderType type, const ShaderLanguage language)
{
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return {};
    }

    const std::string source{std::istreambuf_iterator(file), std::istreambuf_iterator<char>()};
    return ShaderLoader::LoadFromSource(source, type, language);
}

ShaderData ShaderLoader::LoadFromBinary(std::vector<uint32_t> bytecode)
{
    try {
        spirv_cross::Compiler compiler(bytecode);
        return ReflectShaderData(std::move(bytecode), InferShaderType(compiler));
    } catch (...) {
        return {};
    }
}

ShaderData ShaderLoader::LoadFromSource(const std::string& code, const ShaderType type, const ShaderLanguage language)
{
    if (code.empty()) {
        return {};
    }

    EnsureGlslangInitialized();

    const auto stage = ToNativeShaderStage(type);
    glslang::TShader shader(stage);
    const char* source = code.c_str();
    shader.setStrings(&source, 1);
    shader.setEntryPoint("main");
    shader.setSourceEntryPoint("main");

    const auto apiVersion = GetCurrentVulkanContext().GetAPIVersion();
    shader.setEnvInput(ToNativeShaderSource(language), stage, glslang::EShClientVulkan, 460);
    shader.setEnvClient(glslang::EShClientVulkan, ToTargetClientVersion(apiVersion));
    shader.setEnvTarget(glslang::EShTargetSpv, ToTargetSpirvVersion(apiVersion));

    const auto messages = static_cast<EShMessages>(EShMsgDefault | EShMsgSpvRules | EShMsgVulkanRules);
    if (!shader.parse(GetDefaultResources(), 460, false, messages)) {
        return {};
    }

    glslang::TProgram program;
    program.addShader(&shader);
    if (!program.link(messages)) {
        return {};
    }

    auto* intermediate = program.getIntermediate(stage);
    if (intermediate == nullptr) {
        return {};
    }

    std::vector<uint32_t> bytecode;
    glslang::SpvOptions options;
    options.generateDebugInfo = false;
    options.disableOptimizer = true;
    options.optimizeSize = false;
    glslang::GlslangToSpv(*intermediate, bytecode, nullptr, &options);

    try {
        return ReflectShaderData(std::move(bytecode), type);
    } catch (...) {
        return {};
    }
}
} // namespace luna::val
