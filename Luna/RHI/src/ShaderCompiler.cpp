#include "Device.h"
#include "Instance.h"
#include "Logging.h"
#include "ShaderCompiler.h"

#include <cctype>

#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>
#include <string_view>
#include <vector>

namespace luna::RHI {
namespace {
#if LUNA_RHI_HAS_SLANG
void PrintDiagnostics(const char* label, slang::IBlob* diagnosticsBlob)
{
    if (!diagnosticsBlob || diagnosticsBlob->getBufferSize() == 0) {
        return;
    }

    std::string message(label);
    message += ":\n";
    message += static_cast<const char*>(diagnosticsBlob->getBufferPointer());
    LogMessage(LogLevel::Warn, message);
}

std::string ReadTextFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open shader source file: " + path.string());
    }

    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::string BuildModuleName(const std::filesystem::path& path)
{
    std::string moduleName = path.stem().string();
    if (moduleName.empty()) {
        moduleName = "shader";
    }

    for (char& ch : moduleName) {
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') {
            ch = '_';
        }
    }

    const auto canonicalPath = std::filesystem::absolute(path).lexically_normal().string();
    moduleName += "_";
    moduleName += std::to_string(std::hash<std::string>()(canonicalPath));
    return moduleName;
}

std::string ComposeSourceWithDefines(const std::string& source, const ShaderDefines& defines)
{
    if (defines.empty()) {
        return source;
    }

    std::ostringstream stream;
    for (const auto& [name, value] : defines) {
        stream << "#define " << name;
        if (!value.empty()) {
            stream << " " << value;
        }
        stream << "\n";
    }
    stream << "\n" << source;
    return stream.str();
}

void InjectBackendDefines(BackendType backend, ShaderDefines& defines)
{
    defines.insert_or_assign("LUNA_SHADER_SLANG", "1");

    switch (backend) {
        case BackendType::Vulkan:
            defines.insert_or_assign("LUNA_BACKEND_VULKAN", "1");
            break;
        case BackendType::DirectX12:
            defines.insert_or_assign("LUNA_BACKEND_D3D12", "1");
            break;
        case BackendType::DirectX11:
            defines.insert_or_assign("LUNA_BACKEND_D3D11", "1");
            break;
        case BackendType::OpenGL:
            defines.insert_or_assign("LUNA_BACKEND_OPENGL", "1");
            break;
        case BackendType::OpenGLES:
            defines.insert_or_assign("LUNA_BACKEND_OPENGLES", "1");
            break;
        case BackendType::Metal:
            defines.insert_or_assign("LUNA_BACKEND_METAL", "1");
            break;
        case BackendType::WebGPU:
            defines.insert_or_assign("LUNA_BACKEND_WEBGPU", "1");
            break;
        default:
            break;
    }
}

slang::TargetDesc MakeTargetDesc(BackendType backend, slang::IGlobalSession* globalSession)
{
    slang::TargetDesc targetDesc = {};

    switch (backend) {
        case BackendType::Vulkan:
            targetDesc.format = SLANG_SPIRV;
            targetDesc.profile = globalSession->findProfile("spirv_1_5");
            break;
        case BackendType::DirectX12:
            targetDesc.format = SLANG_DXIL;
            targetDesc.profile = globalSession->findProfile("sm_6_6");
            break;
        case BackendType::DirectX11:
            targetDesc.format = SLANG_DXBC;
            targetDesc.profile = globalSession->findProfile("sm_5_0");
            break;
        case BackendType::OpenGL:
        case BackendType::OpenGLES:
            targetDesc.format = SLANG_GLSL;
            targetDesc.profile = globalSession->findProfile("glsl_450");
            break;
        case BackendType::Metal:
            targetDesc.format = SLANG_METAL;
            targetDesc.profile = globalSession->findProfile("metal");
            break;
        case BackendType::WebGPU:
            targetDesc.format = SLANG_WGSL;
            break;
        default:
            throw std::runtime_error("Unsupported BackendType in ShaderCompiler");
    }

    return targetDesc;
}
#endif
} // namespace

#if !LUNA_RHI_HAS_SLANG
namespace {
[[noreturn]] void ThrowSlangUnavailable()
{
    throw std::runtime_error(
        "Luna RHI ShaderCompiler requires Slang headers/libraries, but Slang was not found in this build.");
}
} // namespace

ShaderCompiler::ShaderCompiler() {}

ShaderCompiler::~ShaderCompiler() {}

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

std::shared_ptr<ShaderModule>
    ShaderCompiler::CompileLibrary(const Ref<Device>&, const std::string&, const std::vector<std::string>&)
{
    ThrowSlangUnavailable();
}

void ShaderCompiler::SetCacheDirectory(const std::filesystem::path& path)
{
    m_cacheDir = path;
}

void ShaderCompiler::PruneCache()
{
    if (!std::filesystem::exists(m_cacheDir)) {
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(m_cacheDir)) {
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

bool ShaderCompiler::ValidateShaderAgainstLayout(const ShaderReflectionData& reflection,
                                                 const DescriptorSetLayoutCreateInfo& layout,
                                                 std::string& outError)
{
    for (auto& shaderBinding : reflection.ResourceBindings) {
        bool found = false;
        for (auto& layoutBinding : layout.Bindings) {
            if (layoutBinding.Binding == shaderBinding.Binding && shaderBinding.Set == 0) {
                if (layoutBinding.Type != shaderBinding.Type) {
                    outError = "Binding " + std::to_string(shaderBinding.Binding) + " '" + shaderBinding.Name +
                               "': shader expects " + std::to_string(static_cast<int>(shaderBinding.Type)) +
                               " but layout has " + std::to_string(static_cast<int>(layoutBinding.Type));
                    return false;
                }
                found = true;
                break;
            }
        }
        if (!found && shaderBinding.Set == 0) {
            outError = "Shader binding " + std::to_string(shaderBinding.Binding) + " '" + shaderBinding.Name +
                       "' not found in layout";
            return false;
        }
    }
    return true;
}

std::vector<DescriptorSetLayoutCreateInfo>
    ShaderCompiler::CreateLayoutsFromReflection(const ShaderReflectionData& reflection)
{
    std::map<uint32_t, DescriptorSetLayoutCreateInfo> setMap;
    for (auto& rb : reflection.ResourceBindings) {
        auto& layout = setMap[rb.Set];
        DescriptorSetLayoutBinding b{};
        b.Binding = rb.Binding;
        b.Type = rb.Type;
        b.Count = rb.Count;
        b.StageFlags = rb.StageFlags;
        layout.Bindings.push_back(b);
    }

    std::vector<DescriptorSetLayoutCreateInfo> result;
    for (auto& [_, info] : setMap) {
        result.push_back(std::move(info));
    }
    return result;
}

std::vector<DescriptorSetLayoutCreateInfo>
    ShaderCompiler::CreateLayoutsFromReflection(const std::vector<ShaderReflectionData>& reflections)
{
    std::map<uint32_t, std::map<uint32_t, DescriptorSetLayoutBinding>> merged;
    for (auto& refl : reflections) {
        for (auto& rb : refl.ResourceBindings) {
            auto& existing = merged[rb.Set][rb.Binding];
            if (existing.Count == 0) {
                existing.Binding = rb.Binding;
                existing.Type = rb.Type;
                existing.Count = rb.Count;
                existing.StageFlags = rb.StageFlags;
            } else {
                existing.StageFlags = static_cast<ShaderStage>(static_cast<uint32_t>(existing.StageFlags) |
                                                               static_cast<uint32_t>(rb.StageFlags));
            }
        }
    }

    std::vector<DescriptorSetLayoutCreateInfo> result;
    for (auto& [_, bindings] : merged) {
        DescriptorSetLayoutCreateInfo info{};
        for (auto& [__, b] : bindings) {
            info.Bindings.push_back(b);
        }
        result.push_back(std::move(info));
    }
    return result;
}
#else
ShaderCompiler::ShaderCompiler() {}

ShaderCompiler::~ShaderCompiler() {}

Ref<ShaderCompiler> ShaderCompiler::Create(BackendType backend)
{
    auto compiler = CreateRef<ShaderCompiler>();
    compiler->Initialize(backend);
    return compiler;
}

void ShaderCompiler::Initialize(BackendType backend)
{
    m_targetBackend = backend;

    if (SLANG_FAILED(createGlobalSession(m_globalSession.writeRef()))) {
        throw std::runtime_error("Failed to create Slang global session");
    }

    slang::TargetDesc targetDesc = MakeTargetDesc(backend, m_globalSession.get());
    slang::SessionDesc sessionDesc = {};
    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;
    sessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;

    if (SLANG_FAILED(m_globalSession->createSession(sessionDesc, m_session.writeRef()))) {
        throw std::runtime_error("Failed to create Slang session");
    }
}

std::shared_ptr<ShaderModule> ShaderCompiler::CompileOrLoad(const Ref<Device>& device, const ShaderCreateInfo& info)
{
    ShaderCreateInfo effectiveInfo = info;
    InjectBackendDefines(m_targetBackend, effectiveInfo.Defines);

    size_t hash = CalculateHash(effectiveInfo);
    std::filesystem::path cachePath = m_cacheDir / (std::to_string(hash) + ".bin");
    ShaderBlob blob;

    if (std::filesystem::exists(cachePath)) {
        std::ifstream ifs(cachePath, std::ios::binary | std::ios::ate);
        if (!ifs) {
            throw std::runtime_error("Failed to open shader cache file: " + cachePath.string());
        }

        std::streamsize size = ifs.tellg();
        ifs.seekg(0, std::ios::beg);
        blob.Data.resize(static_cast<size_t>(size));
        if (!ifs.read(reinterpret_cast<char*>(blob.Data.data()), size)) {
            throw std::runtime_error("Failed to read shader cache file: " + cachePath.string());
        }

        blob.Hash = std::hash<std::string_view>()(
            std::string_view(reinterpret_cast<const char*>(blob.Data.data()), blob.Data.size()));
        return device->CreateShaderModule(blob, effectiveInfo);
    }

    const std::filesystem::path sourcePath = std::filesystem::absolute(effectiveInfo.SourcePath).lexically_normal();
    const std::string sourceText = ComposeSourceWithDefines(ReadTextFile(sourcePath), effectiveInfo.Defines);
    const std::string moduleName = BuildModuleName(sourcePath);

    Slang::ComPtr<slang::IBlob> diagnosticsBlob;
    Slang::ComPtr<slang::IModule> slangModule;
    slangModule = m_session->loadModuleFromSourceString(
        moduleName.c_str(), sourcePath.string().c_str(), sourceText.c_str(), diagnosticsBlob.writeRef());
    PrintDiagnostics("Shader load diagnostics", diagnosticsBlob.get());
    if (!slangModule) {
        LogMessage(LogLevel::Error, "Failed to load shader module: " + sourcePath.string());
        return nullptr;
    }

    Slang::ComPtr<slang::IEntryPoint> entryPoint;
    const SlangResult findResult =
        slangModule->findEntryPointByName(effectiveInfo.EntryPoint.c_str(), entryPoint.writeRef());
    if (SLANG_FAILED(findResult) || !entryPoint) {
        LogMessage(LogLevel::Error, "Failed to find entry point: " + effectiveInfo.EntryPoint);
        return nullptr;
    }

    std::vector<slang::IComponentType*> components;
    components.push_back(slangModule);
    components.push_back(entryPoint);

    Slang::ComPtr<slang::IComponentType> composedProgram;
    SlangResult result = m_session->createCompositeComponentType(components.data(),
                                                                 static_cast<SlangInt>(components.size()),
                                                                 composedProgram.writeRef(),
                                                                 diagnosticsBlob.writeRef());
    PrintDiagnostics("Composition diagnostics", diagnosticsBlob.get());
    if (SLANG_FAILED(result) || !composedProgram) {
        LogMessage(LogLevel::Error, "Failed to create composite program");
        return nullptr;
    }

    Slang::ComPtr<slang::IComponentType> linkedProgram;
    result = composedProgram->link(linkedProgram.writeRef(), diagnosticsBlob.writeRef());
    PrintDiagnostics("Link diagnostics", diagnosticsBlob.get());
    if (SLANG_FAILED(result) || !linkedProgram) {
        LogMessage(LogLevel::Error, "Failed to link program");
        return nullptr;
    }

    Slang::ComPtr<slang::IBlob> codeBlob;
    result = linkedProgram->getEntryPointCode(0, 0, codeBlob.writeRef(), diagnosticsBlob.writeRef());
    PrintDiagnostics("Code generation diagnostics", diagnosticsBlob.get());
    if (SLANG_FAILED(result) || !codeBlob) {
        LogMessage(LogLevel::Error, "Failed to get entry point code");
        return nullptr;
    }

    blob = ConvertBlob(codeBlob.get());
    if (blob.Data.empty()) {
        return nullptr;
    }

    if (!std::filesystem::exists(m_cacheDir)) {
        std::filesystem::create_directories(m_cacheDir);
    }

    std::ofstream ofs(cachePath, std::ios::binary);
    if (ofs) {
        ofs.write(reinterpret_cast<const char*>(blob.Data.data()), static_cast<std::streamsize>(blob.Data.size()));
    }

    return device->CreateShaderModule(blob, effectiveInfo);
}

std::shared_ptr<ShaderModule> ShaderCompiler::CompileLibrary(const Ref<Device>& device,
                                                             const std::string& sourcePath,
                                                             const std::vector<std::string>& entryPoints)
{
    const std::filesystem::path absolutePath = std::filesystem::absolute(sourcePath).lexically_normal();
    const std::string moduleName = BuildModuleName(absolutePath);
    ShaderDefines defines;
    InjectBackendDefines(m_targetBackend, defines);
    const std::string sourceText = ComposeSourceWithDefines(ReadTextFile(absolutePath), defines);

    Slang::ComPtr<slang::IBlob> diagnosticsBlob;
    auto slangModule = m_session->loadModuleFromSourceString(
        moduleName.c_str(), absolutePath.string().c_str(), sourceText.c_str(), diagnosticsBlob.writeRef());
    PrintDiagnostics("Library load diagnostics", diagnosticsBlob.get());
    if (!slangModule) {
        LogMessage(LogLevel::Error, "Failed to load module: " + absolutePath.string());
        return nullptr;
    }

    std::vector<Slang::ComPtr<slang::IEntryPoint>> eps(entryPoints.size());
    std::vector<slang::IComponentType*> components;
    components.push_back(slangModule);
    for (size_t i = 0; i < entryPoints.size(); i++) {
        auto result = slangModule->findEntryPointByName(entryPoints[i].c_str(), eps[i].writeRef());
        if (SLANG_FAILED(result) || !eps[i]) {
            LogMessage(LogLevel::Error, "EP not found: " + entryPoints[i]);
            return nullptr;
        }
        components.push_back(eps[i]);
    }

    Slang::ComPtr<slang::IComponentType> composed;
    auto result = m_session->createCompositeComponentType(
        components.data(), static_cast<SlangInt>(components.size()), composed.writeRef(), diagnosticsBlob.writeRef());
    PrintDiagnostics("Library composition diagnostics", diagnosticsBlob.get());
    if (SLANG_FAILED(result) || !composed) {
        LogMessage(LogLevel::Error, "Failed to compose library");
        return nullptr;
    }

    Slang::ComPtr<slang::IComponentType> linked;
    result = composed->link(linked.writeRef(), diagnosticsBlob.writeRef());
    PrintDiagnostics("Library link diagnostics", diagnosticsBlob.get());
    if (SLANG_FAILED(result) || !linked) {
        LogMessage(LogLevel::Error, "Failed to link library");
        return nullptr;
    }

    Slang::ComPtr<slang::IBlob> codeBlob;
    result = linked->getTargetCode(0, codeBlob.writeRef(), diagnosticsBlob.writeRef());
    PrintDiagnostics("Library codegen diagnostics", diagnosticsBlob.get());
    if (SLANG_FAILED(result) || !codeBlob) {
        result = linked->getEntryPointCode(0, 0, codeBlob.writeRef(), diagnosticsBlob.writeRef());
        PrintDiagnostics("Library fallback codegen diagnostics", diagnosticsBlob.get());
        if (SLANG_FAILED(result) || !codeBlob) {
            LogMessage(LogLevel::Error, "Failed to get library code");
            return nullptr;
        }
    }

    ShaderBlob blob = ConvertBlob(codeBlob.get());
    if (blob.Data.empty()) {
        return nullptr;
    }

    ShaderCreateInfo info;
    info.SourcePath = absolutePath.string();
    info.EntryPoint = entryPoints.empty() ? "" : entryPoints.front();
    info.Stage = ShaderStage::RayGen;
    return device->CreateShaderModule(blob, info);
}

void ShaderCompiler::SetCacheDirectory(const std::filesystem::path& path)
{
    m_cacheDir = path;
    if (!std::filesystem::exists(m_cacheDir)) {
        std::filesystem::create_directories(m_cacheDir);
    }
}

void ShaderCompiler::PruneCache()
{
    if (!std::filesystem::exists(m_cacheDir)) {
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(m_cacheDir)) {
        std::filesystem::remove(entry.path());
    }
}

size_t ShaderCompiler::CalculateHash(const ShaderCreateInfo& info) const
{
    std::string combined = info.SourcePath + "|" + info.EntryPoint + "|" + info.Profile;
    combined += "|" + std::to_string(static_cast<int>(info.Stage));
    combined += "|" + std::to_string(static_cast<int>(m_targetBackend));
    if (std::filesystem::exists(info.SourcePath)) {
        const auto lastWrite = std::filesystem::last_write_time(info.SourcePath).time_since_epoch().count();
        const auto fileSize = std::filesystem::file_size(info.SourcePath);
        combined += "|mtime=" + std::to_string(lastWrite);
        combined += "|size=" + std::to_string(fileSize);
    }
    for (const auto& define : info.Defines) {
        combined += "|" + define.first + "=" + define.second;
    }
    return std::hash<std::string>()(combined);
}

ShaderBlob ShaderCompiler::ConvertBlob(slang::IBlob* blob)
{
    ShaderBlob result;
    if (!blob) {
        return result;
    }

    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(blob->getBufferPointer());
    const size_t size = blob->getBufferSize();
    if (!ptr || size == 0) {
        return result;
    }

    result.Data.assign(ptr, ptr + size);
    result.Hash = std::hash<std::string_view>()(std::string_view(reinterpret_cast<const char*>(ptr), size));
    return result;
}

SlangStage ShaderCompiler::ConvertShaderStageToSlang(ShaderStage stage)
{
    switch (stage) {
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
    if (!layout) {
        return data;
    }

    uint32_t paramCount = layout->getParameterCount();
    for (uint32_t i = 0; i < paramCount; i++) {
        auto* param = layout->getParameterByIndex(i);
        if (!param) {
            continue;
        }

        auto* typeLayout = param->getTypeLayout();
        if (!typeLayout) {
            continue;
        }

        ShaderResourceBinding binding;
        binding.Name = param->getName() ? param->getName() : "";
        binding.Set = param->getBindingSpace();
        binding.Binding = param->getBindingIndex();
        binding.StageFlags = stage;

        auto kind = typeLayout->getKind();
        switch (kind) {
            case slang::TypeReflection::Kind::ConstantBuffer:
                binding.Type = DescriptorType::UniformBuffer;
                break;
            case slang::TypeReflection::Kind::Resource: {
                auto shape = typeLayout->getType()->getResourceShape();
                if (shape & SLANG_STRUCTURED_BUFFER) {
                    binding.Type = DescriptorType::StorageBuffer;
                } else if (shape & SLANG_TEXTURE_BUFFER) {
                    binding.Type = DescriptorType::StorageBuffer;
                } else {
                    binding.Type = DescriptorType::SampledImage;
                }
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

bool ShaderCompiler::ValidateShaderAgainstLayout(const ShaderReflectionData& reflection,
                                                 const DescriptorSetLayoutCreateInfo& layout,
                                                 std::string& outError)
{
    for (auto& shaderBinding : reflection.ResourceBindings) {
        bool found = false;
        for (auto& layoutBinding : layout.Bindings) {
            if (layoutBinding.Binding == shaderBinding.Binding && shaderBinding.Set == 0) {
                if (layoutBinding.Type != shaderBinding.Type) {
                    outError = "Binding " + std::to_string(shaderBinding.Binding) + " '" + shaderBinding.Name +
                               "': shader expects " + std::to_string(static_cast<int>(shaderBinding.Type)) +
                               " but layout has " + std::to_string(static_cast<int>(layoutBinding.Type));
                    return false;
                }
                found = true;
                break;
            }
        }
        if (!found && shaderBinding.Set == 0) {
            outError = "Shader binding " + std::to_string(shaderBinding.Binding) + " '" + shaderBinding.Name +
                       "' not found in layout";
            return false;
        }
    }
    return true;
}

std::vector<DescriptorSetLayoutCreateInfo>
    ShaderCompiler::CreateLayoutsFromReflection(const ShaderReflectionData& reflection)
{
    std::map<uint32_t, DescriptorSetLayoutCreateInfo> setMap;
    for (auto& rb : reflection.ResourceBindings) {
        auto& layout = setMap[rb.Set];
        DescriptorSetLayoutBinding b{};
        b.Binding = rb.Binding;
        b.Type = rb.Type;
        b.Count = rb.Count;
        b.StageFlags = rb.StageFlags;
        layout.Bindings.push_back(b);
    }
    std::vector<DescriptorSetLayoutCreateInfo> result;
    for (auto& [_, info] : setMap) {
        result.push_back(std::move(info));
    }
    return result;
}

std::vector<DescriptorSetLayoutCreateInfo>
    ShaderCompiler::CreateLayoutsFromReflection(const std::vector<ShaderReflectionData>& reflections)
{
    std::map<uint32_t, std::map<uint32_t, DescriptorSetLayoutBinding>> merged;
    for (auto& refl : reflections) {
        for (auto& rb : refl.ResourceBindings) {
            auto& existing = merged[rb.Set][rb.Binding];
            if (existing.Count == 0) {
                existing.Binding = rb.Binding;
                existing.Type = rb.Type;
                existing.Count = rb.Count;
                existing.StageFlags = rb.StageFlags;
            } else {
                existing.StageFlags = static_cast<ShaderStage>(static_cast<uint32_t>(existing.StageFlags) |
                                                               static_cast<uint32_t>(rb.StageFlags));
            }
        }
    }
    std::vector<DescriptorSetLayoutCreateInfo> result;
    for (auto& [_, bindings] : merged) {
        DescriptorSetLayoutCreateInfo info{};
        for (auto& [__, b] : bindings) {
            info.Bindings.push_back(b);
        }
        result.push_back(std::move(info));
    }
    return result;
}
#endif
} // namespace luna::RHI
