#include "VkShader.h"

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
                          const spirv_cross::SPIRType& block_type,
                          std::vector<ShaderMember>& members)
{
    members.reserve(block_type.member_types.size());

    for (uint32_t member_index = 0; member_index < block_type.member_types.size(); ++member_index) {
        const auto& member_type = compiler.get_type(block_type.member_types[member_index]);

        ShaderMember member{};
        member.m_name = compiler.get_member_name(block_type.self, member_index);
        member.m_offset = compiler.type_struct_member_offset(block_type, member_index);
        member.m_size = static_cast<uint32_t>(compiler.get_declared_struct_member_size(block_type, member_index));
        member.m_type = mapMemberType(member_type);
        member.m_array_elements = getArrayElementCount(member_type);
        members.push_back(std::move(member));
    }
}

void reflectResources(const spirv_cross::Compiler& compiler,
                      const spirv_cross::SmallVector<spirv_cross::Resource>& resources,
                      ResourceType resource_type,
                      Shader::ReflectionMap& reflection_map)
{
    for (const auto& resource : resources) {
        ShaderReflectionData data{};
        data.m_name = resource.name.empty() ? compiler.get_fallback_name(resource.id) : resource.name;
        data.m_type = resource_type;
        data.m_binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
        data.m_count = getArrayElementCount(compiler.get_type(resource.type_id));
        data.m_size = 0;

        const uint32_t set_index = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
        if (resource_type == ResourceType::UniformBuffer || resource_type == ResourceType::StorageBuffer) {
            const auto& block_type = compiler.get_type(resource.base_type_id);
            data.m_size = static_cast<uint32_t>(compiler.get_declared_struct_size(block_type));
            reflectBufferMembers(compiler, block_type, data.m_members);
        }

        reflection_map[set_index].push_back(std::move(data));
    }
}
} // namespace

VulkanShader::VulkanShader(const std::vector<uint32_t>& spv_code, ShaderType type)
    : m_type(type)
{
    assert(!spv_code.empty());

    spirv_cross::Compiler compiler(spv_code);
    const spirv_cross::ShaderResources resources = compiler.get_shader_resources();

    reflectResources(compiler, resources.uniform_buffers, ResourceType::UniformBuffer, m_reflection_map);
    reflectResources(compiler, resources.storage_buffers, ResourceType::StorageBuffer, m_reflection_map);
    reflectResources(compiler, resources.sampled_images, ResourceType::CombinedImageSampler, m_reflection_map);
    reflectResources(compiler, resources.separate_images, ResourceType::SampledImage, m_reflection_map);
    reflectResources(compiler, resources.separate_samplers, ResourceType::Sampler, m_reflection_map);
    reflectResources(compiler, resources.storage_images, ResourceType::StorageImage, m_reflection_map);
    reflectResources(compiler, resources.subpass_inputs, ResourceType::InputAttachment, m_reflection_map);
}

ShaderType VulkanShader::getType() const
{
    return m_type;
}
} // namespace luna

