#ifndef CACAO_CACAOSHADERCOMPILER_H
#define CACAO_CACAOSHADERCOMPILER_H
#include "ShaderModule.h"

#if __has_include(<slang/slang.h>) && __has_include("slang-com-ptr.h")
#define CACAO_HAS_SLANG 1
#include <slang/slang.h>
#include "slang-com-ptr.h"
#else
#define CACAO_HAS_SLANG 0
namespace slang
{
    class IComponentType;
    class IBlob;
    class IGlobalSession;
    class ISession;
}
namespace Slang
{
    template <typename T>
    class ComPtr
    {
    public:
        void** writeRef() { return nullptr; }
    };
}
using SlangStage = int;
#endif

namespace Cacao
{
    enum class BackendType;
    struct ShaderCreateInfo;
    class ShaderModule;
    class Device;
    class CACAO_API ShaderCompiler
    {
    public:
        ShaderCompiler();
        ~ShaderCompiler();
        static Ref<ShaderCompiler> Create(BackendType backend);
        void Initialize(BackendType backend);
        std::shared_ptr<ShaderModule> CompileOrLoad(
            const Ref<Device>& device,
            const ShaderCreateInfo& info);

        /// Compile an entire shader file as a DXIL/SPIRV library with multiple entry points.
        /// Used for ray tracing shaders that must be compiled as a single library.
        std::shared_ptr<ShaderModule> CompileLibrary(
            const Ref<Device>& device,
            const std::string& sourcePath,
            const std::vector<std::string>& entryPoints);

        void SetCacheDirectory(const std::filesystem::path& path);
        void PruneCache();
        ShaderReflectionData ExtractReflection(slang::IComponentType* linkedProgram, ShaderStage stage);

        static bool ValidateShaderAgainstLayout(
            const ShaderReflectionData& reflection,
            const DescriptorSetLayoutCreateInfo& layout,
            std::string& outError);

        static std::vector<DescriptorSetLayoutCreateInfo> CreateLayoutsFromReflection(
            const ShaderReflectionData& reflection);

        static std::vector<DescriptorSetLayoutCreateInfo> CreateLayoutsFromReflection(
            const std::vector<ShaderReflectionData>& reflections);

    private:
        size_t CalculateHash(const ShaderCreateInfo& info) const;
        ShaderBlob ConvertBlob(slang::IBlob* blob);
        SlangStage ConvertShaderStageToSlang(ShaderStage stage);
        BackendType m_targetBackend;
        std::filesystem::path m_cacheDir = "shader_cache";
#if CACAO_HAS_SLANG
        Slang::ComPtr<slang::IGlobalSession> m_globalSession;
        Slang::ComPtr<slang::ISession> m_session;
#endif
    };
}
#endif 
