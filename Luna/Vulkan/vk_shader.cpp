#include "vk_shader.h"

#include <cassert>

#include <limits>
#include <spirv_cross.hpp>

namespace luna {
namespace {
uint32_t getArrayElementCount(const spirv_cross::SPIRType& type)
{
    if (type.array.empty()) {
        return 1;
    }

    uint64_t count = 1;
    for (size_t index = 0; index < type.array.size(); ++index) {
        if (!type.array_size_literal[index] || type.array[index] == 0) {
            return 0;
        }

        count *= type.array[index];
        if (count > std::numeric_limits<uint32_t>::max()) {
            return 0;
        }
    }

    return static_cast<uint32_t>(count);
}

ShaderDataType mapMemberType(const spirv_cross::SPIRType& type)
{
    if (type.basetype == spirv_cross::SPIRType::Struct) {
        return ShaderDataType::Struct;
    }

    if (type.columns > 1) {
        if (type.columns == 3 && type.vecsize == 3) {
            return ShaderDataType::Mat3;
        }
        return ShaderDataType::Mat4;
    }

    switch (type.basetype) {
        case spirv_cross::SPIRType::Boolean:
            return ShaderDataType::Bool;
        case spirv_cross::SPIRType::Int:
        case spirv_cross::SPIRType::UInt:
        case spirv_cross::SPIRType::Int64:
        case spirv_cross::SPIRType::UInt64:
            switch (type.vecsize) {
                case 1:
                    return ShaderDataType::Int;
                case 2:
                    return ShaderDataType::Int2;
                case 3:
                    return ShaderDataType::Int3;
                default:
                    return ShaderDataType::Int4;
            }
        case spirv_cross::SPIRType::Half:
        case spirv_cross::SPIRType::Float:
        case spirv_cross::SPIRType::Double:
        default:
            switch (type.vecsize) {
                case 1:
                    return ShaderDataType::Float;
                case 2:
                    return ShaderDataType::Float2;
                case 3:
                    return ShaderDataType::Float3;
                default:
                    return ShaderDataType::Float4;
            }
    }
}

void reflectBufferMembers(const spirv_cross::Compiler& compiler,
                          const spirv_cross::SPIRType& blockType,
                          std::vector<ShaderMember>& members)
{
    members.reserve(blockType.member_types.size());

    for (uint32_t memberIndex = 0; memberIndex < blockType.member_types.size(); ++memberIndex) {
        const auto& memberType = compiler.get_type(blockType.member_types[memberIndex]);

        ShaderMember member{};
        member.name = compiler.get_member_name(blockType.self, memberIndex);
        member.offset = compiler.type_struct_member_offset(blockType, memberIndex);
        member.size = static_cast<uint32_t>(compiler.get_declared_struct_member_size(blockType, memberIndex));
        member.type = mapMemberType(memberType);
        member.arrayElements = getArrayElementCount(memberType);
        members.push_back(std::move(member));
    }
}

void reflectResources(const spirv_cross::Compiler& compiler,
                      const spirv_cross::SmallVector<spirv_cross::Resource>& resources,
                      ResourceType resourceType,
                      Shader::ReflectionMap& reflectionMap)
{
    for (const auto& resource : resources) {
        ShaderReflectionData data{};
        data.name = resource.name.empty() ? compiler.get_fallback_name(resource.id) : resource.name;
        data.type = resourceType;
        data.binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
        data.count = getArrayElementCount(compiler.get_type(resource.type_id));
        data.size = 0;

        const uint32_t setIndex = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
        if (resourceType == ResourceType::UniformBuffer || resourceType == ResourceType::StorageBuffer) {
            const auto& blockType = compiler.get_type(resource.base_type_id);
            data.size = static_cast<uint32_t>(compiler.get_declared_struct_size(blockType));
            reflectBufferMembers(compiler, blockType, data.members);
        }

        reflectionMap[setIndex].push_back(std::move(data));
    }
}
} // namespace

VulkanShader::VulkanShader(const std::vector<uint32_t>& spvCode, ShaderType type)
    : m_type(type)
{
    assert(!spvCode.empty());

    spirv_cross::Compiler compiler(spvCode);
    const spirv_cross::ShaderResources resources = compiler.get_shader_resources();

    reflectResources(compiler, resources.uniform_buffers, ResourceType::UniformBuffer, m_reflectionMap);
    reflectResources(compiler, resources.storage_buffers, ResourceType::StorageBuffer, m_reflectionMap);
    reflectResources(compiler, resources.sampled_images, ResourceType::CombinedImageSampler, m_reflectionMap);
    reflectResources(compiler, resources.separate_images, ResourceType::SampledImage, m_reflectionMap);
    reflectResources(compiler, resources.separate_samplers, ResourceType::Sampler, m_reflectionMap);
    reflectResources(compiler, resources.storage_images, ResourceType::StorageImage, m_reflectionMap);
    reflectResources(compiler, resources.subpass_inputs, ResourceType::InputAttachment, m_reflectionMap);
}

ShaderType VulkanShader::getType() const
{
    return m_type;
}
} // namespace luna
