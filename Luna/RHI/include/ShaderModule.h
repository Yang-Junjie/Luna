#ifndef CACAO_CACAOSHADERMODULE_H
#define CACAO_CACAOSHADERMODULE_H
#include <map>
#include "DescriptorSetLayout.h"
namespace Cacao
{
    struct ShaderBlob
    {
        std::vector<uint8_t> Data;
        size_t Hash = 0;
    };
    using ShaderDefines = std::map<std::string, std::string>;
    struct ShaderCreateInfo
    {
        std::string SourcePath;
        std::string EntryPoint = "main";
        ShaderStage Stage = ShaderStage::Vertex;
        ShaderDefines Defines;
        std::string Profile;
    };

    struct ShaderResourceBinding
    {
        uint32_t Set = 0;
        uint32_t Binding = 0;
        DescriptorType Type = DescriptorType::UniformBuffer;

        uint32_t Count = 1;
        ShaderStage StageFlags = ShaderStage::All;
        std::string Name;
    };

    struct ShaderReflectionData
    {
        std::vector<ShaderResourceBinding> ResourceBindings;
        uint32_t PushConstantSize = 0;
        ShaderStage PushConstantStages = ShaderStage::None;
    };

    class CACAO_API ShaderModule : public std::enable_shared_from_this<ShaderModule>
    {
    public:
        virtual ~ShaderModule() = default;
        virtual const std::string& GetEntryPoint() const = 0;
        virtual ShaderStage GetStage() const = 0;
        virtual const ShaderBlob& GetBlob() const = 0;
        virtual std::span<const uint8_t> GetBytecode() const { return {GetBlob().Data.data(), GetBlob().Data.size()}; }
        virtual const ShaderReflectionData& GetReflection() const
        {
            static ShaderReflectionData empty;
            return empty;
        }
    };
}
#endif
