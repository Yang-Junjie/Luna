#include "ShaderCompiler.h"
#include <iostream>
#include <fstream>
#include "Device.h"
#include "Instance.h"

namespace Cacao
{
#if !CACAO_HAS_SLANG
    namespace
    {
        [[noreturn]] void ThrowSlangUnavailable()
        {
            throw std::runtime_error(
                "Cacao ShaderCompiler requires Slang headers/libraries, but Slang was not found in this build.");
        }
    }

    ShaderCompiler::ShaderCompiler()
    {
    }

    ShaderCompiler::~ShaderCompiler()
    {
    }

    Ref<ShaderCompiler> ShaderCompiler::Create(BackendType)
    {
        ThrowSlangUnavailable();
    }

    void ShaderCompiler::Initialize(BackendType)
    {
        ThrowSlangUnavailable();
    }

    std::shared_ptr<ShaderModule> ShaderCompiler::CompileOrLoad(const Ref<Device>&, const ShaderCreateInfo&)
    {
        ThrowSlangUnavailable();
    }

    std::shared_ptr<ShaderModule> ShaderCompiler::CompileLibrary(
        const Ref<Device>&, const std::string&, const std::vector<std::string>&)
    {
        ThrowSlangUnavailable();
    }

    void ShaderCompiler::SetCacheDirectory(const std::filesystem::path& path)
    {
        m_cacheDir = path;
    }

    void ShaderCompiler::PruneCache()
    {
        if (!std::filesystem::exists(m_cacheDir))
        {
            return;
        }

        for (const auto& entry : std::filesystem::directory_iterator(m_cacheDir))
        {
            std::filesystem::remove(entry.path());
        }
    }

    size_t ShaderCompiler::CalculateHash(const ShaderCreateInfo& info) const
    {
        std::string combined = info.SourcePath + "|" + info.EntryPoint + "|" + info.Profile;
        combined += "|" + std::to_string(static_cast<int>(info.Stage));
        combined += "|" + std::to_string(static_cast<int>(m_targetBackend));
        return std::hash<std::string>()(combined);
    }

    ShaderBlob ShaderCompiler::ConvertBlob(slang::IBlob*)
    {
        return {};
    }

    SlangStage ShaderCompiler::ConvertShaderStageToSlang(ShaderStage)
    {
        return 0;
    }

    ShaderReflectionData ShaderCompiler::ExtractReflection(slang::IComponentType*, ShaderStage)
    {
        return {};
    }

    bool ShaderCompiler::ValidateShaderAgainstLayout(
        const ShaderReflectionData& reflection,
        const DescriptorSetLayoutCreateInfo& layout,
        std::string& outError)
    {
        for (auto& shaderBinding : reflection.ResourceBindings)
        {
            bool found = false;
            for (auto& layoutBinding : layout.Bindings)
            {
                if (layoutBinding.Binding == shaderBinding.Binding && shaderBinding.Set == 0)
                {
                    if (layoutBinding.Type != shaderBinding.Type)
                    {
                        outError = "Binding " + std::to_string(shaderBinding.Binding) + " '" + shaderBinding.Name +
                            "': shader expects " + std::to_string(static_cast<int>(shaderBinding.Type)) +
                            " but layout has " + std::to_string(static_cast<int>(layoutBinding.Type));
                        return false;
                    }
                    found = true;
                    break;
                }
            }
            if (!found && shaderBinding.Set == 0)
            {
                outError = "Shader binding " + std::to_string(shaderBinding.Binding) + " '" + shaderBinding.Name +
                    "' not found in layout";
                return false;
            }
        }
        return true;
    }

    std::vector<DescriptorSetLayoutCreateInfo> ShaderCompiler::CreateLayoutsFromReflection(
        const ShaderReflectionData& reflection)
    {
        std::map<uint32_t, DescriptorSetLayoutCreateInfo> setMap;
        for (auto& rb : reflection.ResourceBindings)
        {
            auto& layout = setMap[rb.Set];
            DescriptorSetLayoutBinding b{};
            b.Binding = rb.Binding;
            b.Type = rb.Type;
            b.Count = rb.Count;
            b.StageFlags = rb.StageFlags;
            layout.Bindings.push_back(b);
        }

        std::vector<DescriptorSetLayoutCreateInfo> result;
        for (auto& [_, info] : setMap)
        {
            result.push_back(std::move(info));
        }
        return result;
    }

    std::vector<DescriptorSetLayoutCreateInfo> ShaderCompiler::CreateLayoutsFromReflection(
        const std::vector<ShaderReflectionData>& reflections)
    {
        std::map<uint32_t, std::map<uint32_t, DescriptorSetLayoutBinding>> merged;
        for (auto& refl : reflections)
        {
            for (auto& rb : refl.ResourceBindings)
            {
                auto& existing = merged[rb.Set][rb.Binding];
                if (existing.Count == 0)
                {
                    existing.Binding = rb.Binding;
                    existing.Type = rb.Type;
                    existing.Count = rb.Count;
                    existing.StageFlags = rb.StageFlags;
                }
                else
                {
                    existing.StageFlags = static_cast<ShaderStage>(
                        static_cast<uint32_t>(existing.StageFlags) | static_cast<uint32_t>(rb.StageFlags));
                }
            }
        }

        std::vector<DescriptorSetLayoutCreateInfo> result;
        for (auto& [_, bindings] : merged)
        {
            DescriptorSetLayoutCreateInfo info{};
            for (auto& [__, b] : bindings)
            {
                info.Bindings.push_back(b);
            }
            result.push_back(std::move(info));
        }
        return result;
    }
#else
    ShaderCompiler::ShaderCompiler()
    {
    }

    ShaderCompiler::~ShaderCompiler()
    {
    }

    Ref<ShaderCompiler> ShaderCompiler::Create(BackendType backend)
    {
        auto compiler = CreateRef<ShaderCompiler>();
        compiler->Initialize(backend);
        return compiler;
    }

    void ShaderCompiler::Initialize(BackendType backend)
    {
        m_targetBackend = backend;
        if (SLANG_FAILED(createGlobalSession(m_globalSession.writeRef())))
        {
            throw std::runtime_error("Failed to create Slang global session");
        }
        SessionDesc sessionDesc = {};
        TargetDesc targetDesc = {};
        switch (backend)
        {
        case BackendType::Vulkan:
            targetDesc.format = SLANG_SPIRV;
            targetDesc.profile = m_globalSession->findProfile("spirv_1_5");
            break;
        case BackendType::DirectX12:
            targetDesc.format = SLANG_DXIL;
            targetDesc.profile = m_globalSession->findProfile("sm_6_6");
            break;
        case BackendType::OpenGL:
        case BackendType::OpenGLES:
            targetDesc.format = SLANG_GLSL;
            targetDesc.profile = m_globalSession->findProfile("glsl_450");
            break;
        case BackendType::DirectX11:
            targetDesc.format = SLANG_DXBC;
            targetDesc.profile = m_globalSession->findProfile("sm_5_0");
            break;
        case BackendType::Metal:
            targetDesc.format = SLANG_METAL;
            targetDesc.profile = m_globalSession->findProfile("metal");
            break;
        case BackendType::WebGPU:
            targetDesc.format = SLANG_WGSL;
            break;
        default:
            throw std::runtime_error("Unsupported CacaoType in CacaoShaderCompiler");
        }
        sessionDesc.targetCount = 1;
        sessionDesc.targets = &targetDesc;
        if (SLANG_FAILED(m_globalSession->createSession(sessionDesc, m_session.writeRef())))
        {
            throw std::runtime_error("Failed to create Slang session");
        }
    }

    std::shared_ptr<ShaderModule> ShaderCompiler::CompileOrLoad(
        const Ref<Device>& device, const ShaderCreateInfo& info)
    {
        size_t hash = CalculateHash(info);
        std::filesystem::path cachePath = m_cacheDir / (std::to_string(hash) + ".bin");
        ShaderBlob blob;
        if (std::filesystem::exists(cachePath))
        {
            std::ifstream ifs(cachePath, std::ios::binary | std::ios::ate);
            if (!ifs)
            {
                throw std::runtime_error("Failed to open shader cache file: " + cachePath.string());
            }
            std::streamsize size = ifs.tellg();
            ifs.seekg(0, std::ios::beg);
            blob.Data.resize(size);
            if (!ifs.read(reinterpret_cast<char*>(blob.Data.data()), size))
            {
                throw std::runtime_error("Failed to read shader cache file: " + cachePath.string());
            }
            blob.Hash = std::hash<std::string_view>()(
                std::string_view(reinterpret_cast<const char*>(blob.Data.data()), blob.Data.size())
            );
            return device->CreateShaderModule(blob, info);
        }
        Slang::ComPtr<slang::IBlob> diagnosticsBlob;
        Slang::ComPtr<slang::IModule> slangModule;
        slangModule = m_session->loadModule(info.SourcePath.c_str(), diagnosticsBlob.writeRef());
        if (diagnosticsBlob && diagnosticsBlob->getBufferSize() > 0)
        {
            std::cerr << "Shader load diagnostics:\n"
                << static_cast<const char*>(diagnosticsBlob->getBufferPointer()) << std::endl;
        }
        if (!slangModule)
        {
            std::cerr << "Failed to load shader module: " << info.SourcePath << std::endl;
            return nullptr;
        }
        Slang::ComPtr<slang::IEntryPoint> entryPoint;
        SlangStage slangStage = ConvertShaderStageToSlang(info.Stage);
        SlangResult result = slangModule->findEntryPointByName(info.EntryPoint.c_str(), entryPoint.writeRef());
        if (SLANG_FAILED(result) || !entryPoint)
        {
            std::cerr << "Failed to find entry point: " << info.EntryPoint << std::endl;
            return nullptr;
        }
        std::vector<slang::IComponentType*> components;
        components.push_back(slangModule);
        components.push_back(entryPoint);
        Slang::ComPtr<slang::IComponentType> composedProgram;
        result = m_session->createCompositeComponentType(
            components.data(),
            static_cast<SlangInt>(components.size()),
            composedProgram.writeRef(),
            diagnosticsBlob.writeRef());
        if (diagnosticsBlob && diagnosticsBlob->getBufferSize() > 0)
        {
            std::cerr << "Composition diagnostics:\n"
                << static_cast<const char*>(diagnosticsBlob->getBufferPointer()) << std::endl;
        }
        if (SLANG_FAILED(result) || !composedProgram)
        {
            std::cerr << "Failed to create composite program" << std::endl;
            return nullptr;
        }
        Slang::ComPtr<slang::IComponentType> linkedProgram;
        result = composedProgram->link(linkedProgram.writeRef(), diagnosticsBlob.writeRef());
        if (diagnosticsBlob && diagnosticsBlob->getBufferSize() > 0)
        {
            std::cerr << "Link diagnostics:\n"
                << static_cast<const char*>(diagnosticsBlob->getBufferPointer()) << std::endl;
        }
        if (SLANG_FAILED(result) || !linkedProgram)
        {
            std::cerr << "Failed to link program" << std::endl;
            return nullptr;
        }
        Slang::ComPtr<slang::IBlob> codeBlob;
        result = linkedProgram->getEntryPointCode(
            0,
            0,
            codeBlob.writeRef(),
            diagnosticsBlob.writeRef());
        if (diagnosticsBlob && diagnosticsBlob->getBufferSize() > 0)
        {
            std::cerr << "Code generation diagnostics:\n"
                << static_cast<const char*>(diagnosticsBlob->getBufferPointer()) << std::endl;
        }
        if (SLANG_FAILED(result) || !codeBlob)
        {
            std::cerr << "Failed to get entry point code" << std::endl;
            return nullptr;
        }
        blob = ConvertBlob(codeBlob.get());
        if (!blob.Data.empty())
        {
            if (!std::filesystem::exists(m_cacheDir))
            {
                std::filesystem::create_directories(m_cacheDir);
            }
            std::ofstream ofs(cachePath, std::ios::binary);
            if (ofs)
            {
                ofs.write(reinterpret_cast<const char*>(blob.Data.data()),
                          static_cast<std::streamsize>(blob.Data.size()));
                ofs.close();
            }
        }
        return device->CreateShaderModule(blob, info);
    }

    std::shared_ptr<ShaderModule> ShaderCompiler::CompileLibrary(
        const Ref<Device>& device,
        const std::string& sourcePath,
        const std::vector<std::string>& entryPoints)
    {
        Slang::ComPtr<slang::IBlob> diagnosticsBlob;
        auto slangModule = m_session->loadModule(sourcePath.c_str(), diagnosticsBlob.writeRef());
        if (diagnosticsBlob && diagnosticsBlob->getBufferSize() > 0)
            std::cerr << "Library load: " << static_cast<const char*>(diagnosticsBlob->getBufferPointer()) << std::endl;
        if (!slangModule) { std::cerr << "Failed to load module: " << sourcePath << std::endl; return nullptr; }

        std::vector<Slang::ComPtr<slang::IEntryPoint>> eps(entryPoints.size());
        std::vector<slang::IComponentType*> components;
        components.push_back(slangModule);
        for (size_t i = 0; i < entryPoints.size(); i++)
        {
            auto r = slangModule->findEntryPointByName(entryPoints[i].c_str(), eps[i].writeRef());
            if (SLANG_FAILED(r) || !eps[i]) { std::cerr << "EP not found: " << entryPoints[i] << std::endl; return nullptr; }
            components.push_back(eps[i]);
        }

        Slang::ComPtr<slang::IComponentType> composed;
        auto r = m_session->createCompositeComponentType(components.data(), components.size(), composed.writeRef(), diagnosticsBlob.writeRef());
        if (SLANG_FAILED(r) || !composed) { std::cerr << "Failed to compose library" << std::endl; return nullptr; }

        Slang::ComPtr<slang::IComponentType> linked;
        r = composed->link(linked.writeRef(), diagnosticsBlob.writeRef());
        if (diagnosticsBlob && diagnosticsBlob->getBufferSize() > 0)
            std::cerr << "Library link: " << static_cast<const char*>(diagnosticsBlob->getBufferPointer()) << std::endl;
        if (SLANG_FAILED(r) || !linked) { std::cerr << "Failed to link library" << std::endl; return nullptr; }

        Slang::ComPtr<slang::IBlob> codeBlob;
        r = linked->getTargetCode(0, codeBlob.writeRef(), diagnosticsBlob.writeRef());
        if (diagnosticsBlob && diagnosticsBlob->getBufferSize() > 0)
            std::cerr << "Library codegen: " << static_cast<const char*>(diagnosticsBlob->getBufferPointer()) << std::endl;
        if (SLANG_FAILED(r) || !codeBlob)
        {
            std::cerr << "getTargetCode failed (0x" << std::hex << r << std::dec << "), trying getEntryPointCode..." << std::endl;
            // Fallback: get code for first entry point (produces a library in DXIL mode for RT shaders)
            r = linked->getEntryPointCode(0, 0, codeBlob.writeRef(), diagnosticsBlob.writeRef());
            if (diagnosticsBlob && diagnosticsBlob->getBufferSize() > 0)
                std::cerr << "EP codegen: " << static_cast<const char*>(diagnosticsBlob->getBufferPointer()) << std::endl;
            if (SLANG_FAILED(r) || !codeBlob) { std::cerr << "Failed to get library code" << std::endl; return nullptr; }
        }

        ShaderBlob blob = ConvertBlob(codeBlob.get());
        ShaderCreateInfo info;
        info.SourcePath = sourcePath;
        info.EntryPoint = entryPoints[0];
        info.Stage = ShaderStage::RayGen;
        return device->CreateShaderModule(blob, info);
    }

    void ShaderCompiler::SetCacheDirectory(const std::filesystem::path& path)
    {
        m_cacheDir = path;
        if (!std::filesystem::exists(m_cacheDir))
        {
            std::filesystem::create_directories(m_cacheDir);
        }
    }

    void ShaderCompiler::PruneCache()
    {
        if (std::filesystem::exists(m_cacheDir))
        {
            for (const auto& entry : std::filesystem::directory_iterator(m_cacheDir))
            {
                std::filesystem::remove(entry.path());
            }
        }
    }

    size_t ShaderCompiler::CalculateHash(const ShaderCreateInfo& info) const
    {
        std::string combined = info.SourcePath + "|" + info.EntryPoint + "|" + info.Profile;
        combined += "|" + std::to_string(static_cast<int>(info.Stage));
        combined += "|" + std::to_string(static_cast<int>(m_targetBackend));
        if (std::filesystem::exists(info.SourcePath))
        {
            const auto lastWrite = std::filesystem::last_write_time(info.SourcePath).time_since_epoch().count();
            const auto fileSize = std::filesystem::file_size(info.SourcePath);
            combined += "|mtime=" + std::to_string(lastWrite);
            combined += "|size=" + std::to_string(fileSize);
        }
        for (const auto& define : info.Defines)
        {
            combined += "|" + define.first + "=" + define.second;
        }
        return std::hash<std::string>()(combined);
    }

    ShaderBlob ShaderCompiler::ConvertBlob(IBlob* blob)
    {
        ShaderBlob result;
        if (!blob)
        {
            return result;
        }
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(blob->getBufferPointer());
        size_t size = blob->getBufferSize();
        if (!ptr || size == 0)
        {
            return result;
        }
        result.Data.assign(ptr, ptr + size);
        result.Hash = std::hash<std::string_view>()(
            std::string_view(reinterpret_cast<const char*>(ptr), size)
        );
        return result;
    }

    SlangStage ShaderCompiler::ConvertShaderStageToSlang(ShaderStage stage)
    {
        switch (stage)
        {
        case ShaderStage::Vertex:
            return SLANG_STAGE_VERTEX;
        case ShaderStage::Fragment:
            return SLANG_STAGE_FRAGMENT;
        case ShaderStage::Compute:
            return SLANG_STAGE_COMPUTE;
        case ShaderStage::Geometry:
            return SLANG_STAGE_GEOMETRY;
        case ShaderStage::TessellationControl:
            return SLANG_STAGE_HULL;
        case ShaderStage::TessellationEvaluation:
            return SLANG_STAGE_DOMAIN;
        case ShaderStage::RayGen:
            return SLANG_STAGE_RAY_GENERATION;
        case ShaderStage::RayMiss:
            return SLANG_STAGE_MISS;
        case ShaderStage::RayClosestHit:
            return SLANG_STAGE_CLOSEST_HIT;
        case ShaderStage::RayAnyHit:
            return SLANG_STAGE_ANY_HIT;
        case ShaderStage::RayIntersection:
            return SLANG_STAGE_INTERSECTION;
        case ShaderStage::Callable:
            return SLANG_STAGE_CALLABLE;
        case ShaderStage::Mesh:
            return SLANG_STAGE_MESH;
        case ShaderStage::Task:
            return SLANG_STAGE_AMPLIFICATION;
        default:
            return SLANG_STAGE_NONE;
        }
    }

    ShaderReflectionData ShaderCompiler::ExtractReflection(slang::IComponentType* linkedProgram, ShaderStage stage)
    {
        ShaderReflectionData data;
        auto* layout = linkedProgram->getLayout();
        if (!layout) return data;

        uint32_t paramCount = layout->getParameterCount();
        for (uint32_t i = 0; i < paramCount; i++)
        {
            auto* param = layout->getParameterByIndex(i);
            if (!param) continue;

            auto* typeLayout = param->getTypeLayout();
            if (!typeLayout) continue;

            ShaderResourceBinding binding;
            binding.Name = param->getName() ? param->getName() : "";
            binding.Set = param->getBindingSpace();
            binding.Binding = param->getBindingIndex();
            binding.StageFlags = stage;

            auto kind = typeLayout->getKind();
            switch (kind)
            {
            case slang::TypeReflection::Kind::ConstantBuffer:
                binding.Type = DescriptorType::UniformBuffer;
                break;
            case slang::TypeReflection::Kind::Resource:
            {
                auto shape = typeLayout->getType()->getResourceShape();
                if (shape & SLANG_STRUCTURED_BUFFER)
                    binding.Type = DescriptorType::StorageBuffer;
                else if (shape & SLANG_TEXTURE_BUFFER)
                    binding.Type = DescriptorType::StorageBuffer;
                else
                    binding.Type = DescriptorType::SampledImage;
                break;
            }
            case slang::TypeReflection::Kind::SamplerState:
                binding.Type = DescriptorType::Sampler;
                break;
            default:
                binding.Type = DescriptorType::UniformBuffer;
                break;
            }

            data.ResourceBindings.push_back(binding);
        }
        return data;
    }

    bool ShaderCompiler::ValidateShaderAgainstLayout(
        const ShaderReflectionData& reflection,
        const DescriptorSetLayoutCreateInfo& layout,
        std::string& outError)
    {
        for (auto& shaderBinding : reflection.ResourceBindings)
        {
            bool found = false;
            for (auto& layoutBinding : layout.Bindings)
            {
                if (layoutBinding.Binding == shaderBinding.Binding &&
                    shaderBinding.Set == 0)
                {
                    if (layoutBinding.Type != shaderBinding.Type)
                    {
                        outError = "Binding " + std::to_string(shaderBinding.Binding) +
                            " '" + shaderBinding.Name + "': shader expects " +
                            std::to_string(static_cast<int>(shaderBinding.Type)) +
                            " but layout has " +
                            std::to_string(static_cast<int>(layoutBinding.Type));
                        return false;
                    }
                    found = true;
                    break;
                }
            }
            if (!found && shaderBinding.Set == 0)
            {
                outError = "Shader binding " + std::to_string(shaderBinding.Binding) +
                    " '" + shaderBinding.Name + "' not found in layout";
                return false;
            }
        }
        return true;
    }

    std::vector<DescriptorSetLayoutCreateInfo> ShaderCompiler::CreateLayoutsFromReflection(
        const ShaderReflectionData& reflection)
    {
        std::map<uint32_t, DescriptorSetLayoutCreateInfo> setMap;
        for (auto& rb : reflection.ResourceBindings)
        {
            auto& layout = setMap[rb.Set];
            DescriptorSetLayoutBinding b{};
            b.Binding = rb.Binding;
            b.Type = rb.Type;
            b.Count = rb.Count;
            b.StageFlags = rb.StageFlags;
            layout.Bindings.push_back(b);
        }
        std::vector<DescriptorSetLayoutCreateInfo> result;
        for (auto& [_, info] : setMap)
            result.push_back(std::move(info));
        return result;
    }

    std::vector<DescriptorSetLayoutCreateInfo> ShaderCompiler::CreateLayoutsFromReflection(
        const std::vector<ShaderReflectionData>& reflections)
    {
        std::map<uint32_t, std::map<uint32_t, DescriptorSetLayoutBinding>> merged;
        for (auto& refl : reflections)
        {
            for (auto& rb : refl.ResourceBindings)
            {
                auto& existing = merged[rb.Set][rb.Binding];
                if (existing.Count == 0)
                {
                    existing.Binding = rb.Binding;
                    existing.Type = rb.Type;
                    existing.Count = rb.Count;
                    existing.StageFlags = rb.StageFlags;
                }
                else
                {
                    existing.StageFlags = static_cast<ShaderStage>(
                        static_cast<uint32_t>(existing.StageFlags) | static_cast<uint32_t>(rb.StageFlags));
                }
            }
        }
        std::vector<DescriptorSetLayoutCreateInfo> result;
        for (auto& [_, bindings] : merged)
        {
            DescriptorSetLayoutCreateInfo info{};
            for (auto& [__, b] : bindings)
                info.Bindings.push_back(b);
            result.push_back(std::move(info));
        }
        return result;
    }
#endif
}
