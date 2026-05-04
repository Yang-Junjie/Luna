#include "ScriptPluginManager.h"

#include "Core/Log.h"
#include "Project/ProjectManager.h"
#include "Scene/Components/ScriptComponent.h"
#include "ScriptHostBridge.h"
#include "ScriptPluginApi.h"
#include "ScriptPluginDiscovery.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#if defined(_WIN32)
#    if !defined(WIN32_LEAN_AND_MEAN)
#        define WIN32_LEAN_AND_MEAN
#    endif
#    if !defined(NOMINMAX)
#        define NOMINMAX
#    endif
#    include <Windows.h>
#else
#    include <dlfcn.h>
#endif

namespace {

using BackendMap = std::unordered_map<std::string, std::unique_ptr<luna::IScriptBackend>>;

std::string normalizeBackendKey(std::string_view value)
{
    std::string normalized(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return normalized;
}

const char* scopeToString(luna::ScriptPluginScope scope)
{
    switch (scope) {
        case luna::ScriptPluginScope::Engine:
            return "engine";
        case luna::ScriptPluginScope::Project:
            return "project";
        default:
            return "unknown";
    }
}

const luna::ScriptBackendDescriptor* findBackendInMap(const BackendMap& backends, std::string_view backend_name)
{
    const auto it = backends.find(normalizeBackendKey(backend_name));
    return it != backends.end() ? &it->second->descriptor() : nullptr;
}

std::unique_ptr<luna::IScriptRuntime> createRuntimeFromMap(const BackendMap& backends, std::string_view backend_name)
{
    const auto it = backends.find(normalizeBackendKey(backend_name));
    if (it == backends.end()) {
        return {};
    }

    return it->second->createRuntime();
}

std::vector<luna::ScriptPropertySchema> getPropertySchemaFromMap(const BackendMap& backends,
                                                                 std::string_view backend_name,
                                                                 const luna::ScriptSchemaRequest& request)
{
    const auto it = backends.find(normalizeBackendKey(backend_name));
    if (it == backends.end()) {
        return {};
    }

    return it->second->getPropertySchema(request);
}

LunaScriptPropertyType toApiPropertyType(luna::ScriptPropertyType type)
{
    switch (type) {
        case luna::ScriptPropertyType::Bool:
            return LunaScriptPropertyType_Bool;
        case luna::ScriptPropertyType::Int:
            return LunaScriptPropertyType_Int;
        case luna::ScriptPropertyType::Float:
            return LunaScriptPropertyType_Float;
        case luna::ScriptPropertyType::String:
            return LunaScriptPropertyType_String;
        case luna::ScriptPropertyType::Vec3:
            return LunaScriptPropertyType_Vec3;
        case luna::ScriptPropertyType::Entity:
            return LunaScriptPropertyType_Entity;
        case luna::ScriptPropertyType::Asset:
            return LunaScriptPropertyType_Asset;
        default:
            return LunaScriptPropertyType_Float;
    }
}

LunaScriptVec3 toApiVec3(const glm::vec3& value)
{
    return LunaScriptVec3{value.x, value.y, value.z};
}

LunaScriptPropertyValueDesc toApiPropertyValueDesc(const luna::ScriptProperty& property, size_t property_index)
{
    LunaScriptPropertyValueDesc desc{};
    desc.name = property.name.c_str();
    desc.type = toApiPropertyType(property.type);
    desc.bool_value = property.boolValue ? 1 : 0;
    desc.int_value = property.intValue;
    desc.float_value = property.floatValue;
    desc.string_value = property.stringValue.c_str();
    desc.vec3_value = toApiVec3(property.vec3Value);
    desc.entity_value = static_cast<uint64_t>(property.entityValue);
    desc.asset_value = static_cast<uint64_t>(property.assetValue);
    desc.property_index = property_index;
    return desc;
}

void logHostMessage(void*, LunaScriptHostLogLevel level, const char* message)
{
    const char* text = message != nullptr ? message : "";

    switch (level) {
        case LunaScriptHostLogLevel_Trace:
            LUNA_CORE_TRACE("[ScriptHost] {}", text);
            break;
        case LunaScriptHostLogLevel_Info:
            LUNA_CORE_INFO("[ScriptHost] {}", text);
            break;
        case LunaScriptHostLogLevel_Warn:
            LUNA_CORE_WARN("[ScriptHost] {}", text);
            break;
        case LunaScriptHostLogLevel_Error:
            LUNA_CORE_ERROR("[ScriptHost] {}", text);
            break;
        default:
            LUNA_CORE_INFO("[ScriptHost] {}", text);
            break;
    }
}

bool equalsIgnoreCase(std::string_view lhs, std::string_view rhs)
{
    return lhs.size() == rhs.size() &&
           std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), [](char a, char b) {
               return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
           });
}

const luna::ScriptPluginCandidate* findUniqueCandidateByBackend(
    const std::vector<luna::ScriptPluginCandidate>& candidates, std::string_view backend_name)
{
    const std::string normalized_backend = normalizeBackendKey(backend_name);
    const luna::ScriptPluginCandidate* resolved = nullptr;

    for (const auto& candidate : candidates) {
        if (normalizeBackendKey(candidate.Manifest.BackendName) != normalized_backend) {
            continue;
        }

        if (resolved != nullptr) {
            return nullptr;
        }

        resolved = &candidate;
    }

    return resolved;
}

bool registerBackendIntoMap(BackendMap& backends, std::unique_ptr<luna::IScriptBackend> backend)
{
    if (!backend) {
        return false;
    }

    const luna::ScriptBackendDescriptor& descriptor = backend->descriptor();
    if (descriptor.name.empty()) {
        LUNA_CORE_WARN("Rejected script backend registration because the backend name is empty");
        return false;
    }

    const std::string key = normalizeBackendKey(descriptor.name);
    if (backends.contains(key)) {
        LUNA_CORE_WARN("Rejected duplicate script backend registration for '{}'", descriptor.name);
        return false;
    }

    LUNA_CORE_INFO("Registered script backend '{}' ({})",
                   descriptor.name,
                   descriptor.built_in ? "built-in" : "plugin");
    backends.emplace(key, std::move(backend));
    return true;
}

#if defined(_WIN32)
std::string formatWindowsError(DWORD error)
{
    LPSTR buffer = nullptr;
    const DWORD length = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                            FORMAT_MESSAGE_IGNORE_INSERTS,
                                        nullptr,
                                        error,
                                        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                        reinterpret_cast<LPSTR>(&buffer),
                                        0,
                                        nullptr);

    std::string message = (length > 0 && buffer != nullptr) ? std::string(buffer, length) : "unknown error";
    if (buffer != nullptr) {
        LocalFree(buffer);
    }

    while (!message.empty() &&
           (message.back() == '\r' || message.back() == '\n' || message.back() == ' ' || message.back() == '.')) {
        message.pop_back();
    }

    return message;
}

class DynamicLibraryHandle {
public:
    DynamicLibraryHandle(const DynamicLibraryHandle&) = delete;
    DynamicLibraryHandle& operator=(const DynamicLibraryHandle&) = delete;
    DynamicLibraryHandle(DynamicLibraryHandle&&) = delete;
    DynamicLibraryHandle& operator=(DynamicLibraryHandle&&) = delete;

    static std::shared_ptr<DynamicLibraryHandle> load(const std::filesystem::path& path)
    {
        const HMODULE module = ::LoadLibraryW(path.c_str());
        if (module == nullptr) {
            const DWORD error = ::GetLastError();
            LUNA_CORE_ERROR("Failed to load script plugin library '{}': {}", path.string(), formatWindowsError(error));
            return {};
        }

        return std::shared_ptr<DynamicLibraryHandle>(new DynamicLibraryHandle(module, path));
    }

    ~DynamicLibraryHandle()
    {
        if (m_module != nullptr) {
            ::FreeLibrary(m_module);
        }
    }

    [[nodiscard]] void* findSymbol(const char* name) const
    {
        if (m_module == nullptr || name == nullptr) {
            return nullptr;
        }

        return reinterpret_cast<void*>(::GetProcAddress(m_module, name));
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept
    {
        return m_path;
    }

private:
    DynamicLibraryHandle(HMODULE module, std::filesystem::path path)
        : m_module(module),
          m_path(std::move(path))
    {}

private:
    HMODULE m_module{nullptr};
    std::filesystem::path m_path;
};
#else
class DynamicLibraryHandle {
public:
    DynamicLibraryHandle(const DynamicLibraryHandle&) = delete;
    DynamicLibraryHandle& operator=(const DynamicLibraryHandle&) = delete;
    DynamicLibraryHandle(DynamicLibraryHandle&&) = delete;
    DynamicLibraryHandle& operator=(DynamicLibraryHandle&&) = delete;

    static std::shared_ptr<DynamicLibraryHandle> load(const std::filesystem::path& path)
    {
        dlerror();
        void* module = ::dlopen(path.string().c_str(), RTLD_NOW | RTLD_LOCAL);
        if (module == nullptr) {
            const char* error = dlerror();
            LUNA_CORE_ERROR("Failed to load script plugin library '{}': {}",
                            path.string(),
                            error != nullptr ? error : "unknown error");
            return {};
        }

        return std::shared_ptr<DynamicLibraryHandle>(new DynamicLibraryHandle(module, path));
    }

    ~DynamicLibraryHandle()
    {
        if (m_module != nullptr) {
            ::dlclose(m_module);
        }
    }

    [[nodiscard]] void* findSymbol(const char* name) const
    {
        if (m_module == nullptr || name == nullptr) {
            return nullptr;
        }

        dlerror();
        void* symbol = ::dlsym(m_module, name);
        const char* error = dlerror();
        return error == nullptr ? symbol : nullptr;
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept
    {
        return m_path;
    }

private:
    DynamicLibraryHandle(void* module, std::filesystem::path path)
        : m_module(module),
          m_path(std::move(path))
    {}

private:
    void* m_module{nullptr};
    std::filesystem::path m_path;
};
#endif

class PluginScriptRuntime final : public luna::IScriptRuntime {
public:
    PluginScriptRuntime(std::string runtime_name,
                        LunaScriptRuntimeApi runtime_api,
                        std::shared_ptr<DynamicLibraryHandle> plugin_library)
        : m_runtime_name(std::move(runtime_name)),
          m_runtime_api(runtime_api),
          m_plugin_library(std::move(plugin_library))
    {}

    ~PluginScriptRuntime() override
    {
        shutdown();
        if (m_runtime_api.destroy_runtime != nullptr && m_runtime_api.runtime_user_data != nullptr) {
            m_runtime_api.destroy_runtime(m_runtime_api.runtime_user_data);
            m_runtime_api.runtime_user_data = nullptr;
        }
    }

    const char* name() const noexcept override
    {
        return m_runtime_name.c_str();
    }

    bool initialize() override
    {
        if (m_initialized) {
            return true;
        }

        if (m_runtime_api.initialize != nullptr && m_runtime_api.initialize(m_runtime_api.runtime_user_data) == 0) {
            return false;
        }

        m_initialized = true;
        return true;
    }

    void shutdown() override
    {
        if (!m_initialized) {
            return;
        }

        if (m_runtime_api.shutdown != nullptr) {
            m_runtime_api.shutdown(m_runtime_api.runtime_user_data);
        }

        m_initialized = false;
    }

    void onRuntimeStart(luna::Scene& scene) override
    {
        if (m_runtime_api.on_runtime_start != nullptr) {
            m_runtime_api.on_runtime_start(m_runtime_api.runtime_user_data, &scene);
        }
    }

    void onRuntimeStop(luna::Scene& scene) override
    {
        if (m_runtime_api.on_runtime_stop != nullptr) {
            m_runtime_api.on_runtime_stop(m_runtime_api.runtime_user_data, &scene);
        }
    }

    void onUpdate(luna::Scene& scene, luna::Timestep timestep) override
    {
        if (m_runtime_api.on_update != nullptr) {
            m_runtime_api.on_update(m_runtime_api.runtime_user_data, &scene, timestep.getSeconds());
        }
    }

    void setScriptProperty(luna::Scene& scene,
                           luna::UUID entity_id,
                           luna::UUID script_id,
                           const luna::ScriptProperty& property,
                           size_t property_index) override
    {
        if (m_runtime_api.set_script_property == nullptr) {
            return;
        }

        const LunaScriptPropertyValueDesc desc = toApiPropertyValueDesc(property, property_index);
        m_runtime_api.set_script_property(m_runtime_api.runtime_user_data,
                                          &scene,
                                          static_cast<uint64_t>(entity_id),
                                          static_cast<uint64_t>(script_id),
                                          &desc);
    }

private:
    std::string m_runtime_name;
    LunaScriptRuntimeApi m_runtime_api{};
    std::shared_ptr<DynamicLibraryHandle> m_plugin_library;
    bool m_initialized{false};
};

class RuntimeApiOwner final {
public:
    explicit RuntimeApiOwner(LunaScriptRuntimeApi runtime_api) noexcept
        : m_runtime_api(runtime_api)
    {}

    RuntimeApiOwner(const RuntimeApiOwner&) = delete;
    RuntimeApiOwner& operator=(const RuntimeApiOwner&) = delete;

    ~RuntimeApiOwner()
    {
        reset();
    }

    [[nodiscard]] LunaScriptRuntimeApi release() noexcept
    {
        LunaScriptRuntimeApi runtime_api = m_runtime_api;
        m_runtime_api = {};
        return runtime_api;
    }

private:
    void reset() noexcept
    {
        if (m_runtime_api.destroy_runtime != nullptr && m_runtime_api.runtime_user_data != nullptr) {
            m_runtime_api.destroy_runtime(m_runtime_api.runtime_user_data);
            m_runtime_api.runtime_user_data = nullptr;
        }
    }

private:
    LunaScriptRuntimeApi m_runtime_api{};
};

class PluginScriptBackend final : public luna::IScriptBackend {
public:
    PluginScriptBackend(luna::ScriptPluginCandidate candidate,
                        LunaScriptBackendApi backend_api,
                        std::shared_ptr<DynamicLibraryHandle> plugin_library)
        : m_candidate(std::move(candidate)),
          m_backend_api(backend_api),
          m_plugin_library(std::move(plugin_library))
    {
        m_descriptor.name = m_backend_api.backend_name != nullptr ? m_backend_api.backend_name
                                                                  : m_candidate.Manifest.BackendName;
        m_descriptor.display_name =
            m_backend_api.display_name != nullptr ? m_backend_api.display_name : m_candidate.Manifest.DisplayName;
        m_descriptor.language =
            m_backend_api.language_name != nullptr ? m_backend_api.language_name : m_candidate.Manifest.Language;
        m_descriptor.built_in = false;
        m_descriptor.plugin_path = m_candidate.PluginRootPath;

        if (m_backend_api.supported_extensions != nullptr) {
            m_descriptor.supported_extensions.reserve(m_backend_api.supported_extension_count);
            for (size_t extension_index = 0; extension_index < m_backend_api.supported_extension_count;
                 ++extension_index) {
                const char* extension = m_backend_api.supported_extensions[extension_index];
                if (extension != nullptr && extension[0] != '\0') {
                    m_descriptor.supported_extensions.emplace_back(extension);
                }
            }
        }
    }

    const luna::ScriptBackendDescriptor& descriptor() const noexcept override
    {
        return m_descriptor;
    }

    std::unique_ptr<luna::IScriptRuntime> createRuntime() const override
    {
        if (m_backend_api.create_runtime == nullptr) {
            LUNA_CORE_ERROR("Script backend '{}' does not expose a runtime factory", m_descriptor.name);
            return {};
        }

        LunaScriptRuntimeApi runtime_api{};
        runtime_api.struct_size = sizeof(LunaScriptRuntimeApi);
        runtime_api.api_version = LUNA_SCRIPT_RUNTIME_API_VERSION;
        const int create_result = m_backend_api.create_runtime(m_backend_api.backend_user_data, &runtime_api);
        RuntimeApiOwner runtime_api_owner(runtime_api);

        if (create_result == 0) {
            LUNA_CORE_ERROR("Script backend '{}' failed to create a runtime instance", m_descriptor.name);
            return {};
        }

        if (runtime_api.api_version != LUNA_SCRIPT_RUNTIME_API_VERSION) {
            LUNA_CORE_ERROR("Script backend '{}' returned runtime API version {} but the engine expects {}",
                            m_descriptor.name,
                            runtime_api.api_version,
                            static_cast<uint32_t>(LUNA_SCRIPT_RUNTIME_API_VERSION));
            return {};
        }

        if (runtime_api.destroy_runtime == nullptr) {
            LUNA_CORE_ERROR("Script backend '{}' returned a runtime without destroy_runtime", m_descriptor.name);
            return {};
        }

        return std::make_unique<PluginScriptRuntime>(m_descriptor.name, runtime_api_owner.release(), m_plugin_library);
    }

    std::vector<luna::ScriptPropertySchema> getPropertySchema(const luna::ScriptSchemaRequest& request) const override
    {
        if (m_backend_api.enumerate_property_schema == nullptr) {
            return {};
        }

        LunaScriptSchemaRequest api_request{};
        api_request.asset_name = request.assetName.c_str();
        api_request.type_name = request.typeName.c_str();
        api_request.language = request.language.c_str();
        api_request.source = request.source.c_str();

        std::vector<luna::ScriptPropertySchema> schemas;
        if (m_backend_api.enumerate_property_schema(
                m_backend_api.backend_user_data, &api_request, &schemas, &enumeratePropertySchema) == 0) {
            LUNA_CORE_WARN("Script backend '{}' failed to enumerate property schema for '{}'",
                           m_descriptor.name,
                           request.assetName);
            return {};
        }

        return schemas;
    }

private:
    static int enumeratePropertySchema(void* user_data, const LunaScriptPropertySchemaDesc* property_schema)
    {
        if (user_data == nullptr || property_schema == nullptr || property_schema->name == nullptr ||
            property_schema->name[0] == '\0') {
            return 1;
        }

        auto& schemas = *static_cast<std::vector<luna::ScriptPropertySchema>*>(user_data);
        luna::ScriptPropertySchema schema{};
        schema.name = property_schema->name;
        schema.displayName = property_schema->display_name != nullptr ? property_schema->display_name : schema.name;
        schema.description = property_schema->description != nullptr ? property_schema->description : std::string{};
        schema.type = toPropertyType(property_schema->type);
        schema.defaultValue.name = schema.name;
        schema.defaultValue.type = schema.type;
        applyDefaultValue(schema.defaultValue, *property_schema);
        schemas.push_back(std::move(schema));
        return 1;
    }

    static luna::ScriptPropertyType toPropertyType(LunaScriptPropertyType type)
    {
        switch (type) {
            case LunaScriptPropertyType_Bool:
                return luna::ScriptPropertyType::Bool;
            case LunaScriptPropertyType_Int:
                return luna::ScriptPropertyType::Int;
            case LunaScriptPropertyType_Float:
                return luna::ScriptPropertyType::Float;
            case LunaScriptPropertyType_String:
                return luna::ScriptPropertyType::String;
            case LunaScriptPropertyType_Vec3:
                return luna::ScriptPropertyType::Vec3;
            case LunaScriptPropertyType_Entity:
                return luna::ScriptPropertyType::Entity;
            case LunaScriptPropertyType_Asset:
                return luna::ScriptPropertyType::Asset;
            default:
                return luna::ScriptPropertyType::Float;
        }
    }

    static void applyDefaultValue(luna::ScriptProperty& property, const LunaScriptPropertySchemaDesc& schema)
    {
        switch (property.type) {
            case luna::ScriptPropertyType::Bool:
                property.boolValue = schema.default_bool_value != 0;
                break;
            case luna::ScriptPropertyType::Int:
                property.intValue = schema.default_int_value;
                break;
            case luna::ScriptPropertyType::Float:
                property.floatValue = schema.default_float_value;
                break;
            case luna::ScriptPropertyType::String:
                property.stringValue = schema.default_string_value != nullptr ? schema.default_string_value : "";
                break;
            case luna::ScriptPropertyType::Vec3:
                property.vec3Value = {schema.default_vec3_value.x,
                                      schema.default_vec3_value.y,
                                      schema.default_vec3_value.z};
                break;
            case luna::ScriptPropertyType::Entity:
                property.entityValue = luna::UUID(schema.default_entity_value);
                break;
            case luna::ScriptPropertyType::Asset:
                property.assetValue = luna::AssetHandle(schema.default_asset_value);
                break;
        }
    }

    luna::ScriptPluginCandidate m_candidate;
    luna::ScriptBackendDescriptor m_descriptor;
    LunaScriptBackendApi m_backend_api{};
    std::shared_ptr<DynamicLibraryHandle> m_plugin_library;
};

} // namespace

namespace luna {

ScriptPluginManager& ScriptPluginManager::instance()
{
    static ScriptPluginManager manager;
    return manager;
}

ScriptPluginManager::ScriptPluginManager()
{
    m_host_api.struct_size = sizeof(LunaScriptHostApi);
    m_host_api.api_version = LUNA_SCRIPT_HOST_API_VERSION;
    m_host_api.user_data = nullptr;
    m_host_api.log = &logHostMessage;
    initializeScriptHostApiBridge(m_host_api);

    registerBuiltinBackends();
    refreshDiscoveredPlugins();
}

const ScriptBackendDescriptor* ScriptPluginManager::findBackend(std::string_view backend_name) const
{
    if (const ScriptBackendDescriptor* plugin_backend = findBackendInMap(m_plugin_backends, backend_name);
        plugin_backend != nullptr) {
        return plugin_backend;
    }

    return findBackendInMap(m_builtin_backends, backend_name);
}

std::vector<ScriptBackendDescriptor> ScriptPluginManager::getBackends() const
{
    std::vector<ScriptBackendDescriptor> backends;
    backends.reserve(m_builtin_backends.size() + m_plugin_backends.size());

    for (const auto& [key, backend] : m_plugin_backends) {
        (void) key;
        backends.push_back(backend->descriptor());
    }

    for (const auto& [key, backend] : m_builtin_backends) {
        if (m_plugin_backends.contains(key)) {
            continue;
        }

        backends.push_back(backend->descriptor());
    }

    std::sort(backends.begin(), backends.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.name < rhs.name;
    });
    return backends;
}

void ScriptPluginManager::refreshDiscoveredPlugins(std::optional<std::filesystem::path> project_root_path)
{
    ScriptPluginDiscovery discovery;
    ScriptPluginDiscovery::Options options{};
    options.EnginePluginsRoot = std::filesystem::path(LUNA_PROJECT_ROOT) / "Plugins";
    if (project_root_path.has_value()) {
        options.ProjectPluginsRoot = project_root_path.value() / "Plugins";
    }

    m_discovered_plugins = discovery.discover(options);
}

const std::vector<ScriptPluginCandidate>& ScriptPluginManager::getDiscoveredPlugins() const noexcept
{
    return m_discovered_plugins;
}

const ScriptPluginCandidate* ScriptPluginManager::findDiscoveredPlugin(std::string_view plugin_id) const
{
    const std::string normalized_plugin_id = normalizeBackendKey(plugin_id);
    for (const ScriptPluginCandidate& candidate : m_discovered_plugins) {
        if (normalizeBackendKey(candidate.Manifest.PluginId) == normalized_plugin_id) {
            return &candidate;
        }
    }

    return nullptr;
}

std::vector<ScriptPropertySchema> ScriptPluginManager::getPropertySchema(std::string_view backend_name,
                                                                          const ScriptSchemaRequest& request) const
{
    if (std::vector<ScriptPropertySchema> schemas = getPropertySchemaFromMap(m_plugin_backends, backend_name, request);
        !schemas.empty()) {
        return schemas;
    }

    return getPropertySchemaFromMap(m_builtin_backends, backend_name, request);
}

std::vector<ScriptPropertySchema> ScriptPluginManager::getPropertySchemaForProject(
    const ProjectInfo* project_info,
    const ScriptSchemaRequest& request)
{
    const auto project_root_path = ProjectManager::instance().getProjectRootPath();
    refreshDiscoveredPlugins(project_root_path);

    const ScriptPluginSelectionResult selection = resolveProjectSelection(project_info);
    if (!selection.isResolved()) {
        if (!selection.StatusMessage.empty()) {
            LUNA_CORE_ERROR("{}", selection.StatusMessage);
        }
        return {};
    }

    if (selection.Candidate != nullptr && !ensurePluginLoaded(selection.Candidate)) {
        return {};
    }

    return getPropertySchema(selection.BackendName, request);
}

std::unique_ptr<IScriptRuntime> ScriptPluginManager::createRuntime(std::string_view backend_name) const
{
    if (std::unique_ptr<IScriptRuntime> runtime = createRuntimeFromMap(m_plugin_backends, backend_name)) {
        return runtime;
    }

    return createRuntimeFromMap(m_builtin_backends, backend_name);
}

std::unique_ptr<IScriptRuntime> ScriptPluginManager::createRuntimeForProject(const ProjectInfo* project_info)
{
    const auto project_root_path = ProjectManager::instance().getProjectRootPath();
    refreshDiscoveredPlugins(project_root_path);

    const ScriptPluginSelectionResult selection = resolveAndLoadProjectSelection(project_info);
    if (selection.State == ScriptPluginSelectionState::BackendAmbiguous) {
        LUNA_CORE_ERROR("{}", selection.StatusMessage);
        for (const auto& candidate : m_discovered_plugins) {
            LUNA_CORE_ERROR("  candidate: '{}' backend='{}' scope='{}'",
                            candidate.Manifest.PluginId,
                            candidate.Manifest.BackendName,
                            scopeToString(candidate.Scope));
        }
        return {};
    }

    if (!selection.isResolved()) {
        if (!selection.StatusMessage.empty()) {
            LUNA_CORE_ERROR("{}", selection.StatusMessage);
        }
        return {};
    }

    if (std::unique_ptr<IScriptRuntime> runtime = createRuntime(selection.BackendName)) {
        return runtime;
    }

    if (selection.Candidate != nullptr) {
        LUNA_CORE_ERROR("Script plugin '{}' resolved to backend '{}', but that backend is not registered",
                        selection.Candidate->Manifest.PluginId,
                        selection.BackendName);
        return {};
    }

    LUNA_CORE_ERROR("Configured script backend '{}' is unavailable", selection.BackendName);
    return {};
}

ScriptPluginSelectionResult ScriptPluginManager::resolveAndLoadProjectSelection(const ProjectInfo* project_info)
{
    ScriptPluginSelectionResult selection = resolveProjectSelection(project_info);
    if (!selection.isResolved()) {
        return selection;
    }

    if (selection.Candidate != nullptr && !ensurePluginLoaded(selection.Candidate)) {
        selection.State = ScriptPluginSelectionState::BackendNotFound;
        selection.StatusMessage = "Script plugin '" + selection.Candidate->Manifest.PluginId +
                                  "' failed to load its backend '" + selection.BackendName + "'.";
        return selection;
    }

    if (selection.Candidate == nullptr) {
        unloadActivePlugin();
    }

    return selection;
}

ScriptPluginSelectionResult ScriptPluginManager::resolveProjectSelection(const ProjectInfo* project_info) const
{
    ScriptPluginSelectionResult selection{};
    selection.ExplicitSelection = project_info != nullptr &&
                                  (!project_info->Scripting.SelectedPluginId.empty() ||
                                   !project_info->Scripting.SelectedBackendName.empty());

    if (project_info == nullptr) {
        selection.State = ScriptPluginSelectionState::NoProject;
        selection.StatusMessage = "No project is loaded.";
        return selection;
    }

    if (m_discovered_plugins.empty()) {
        selection.State = ScriptPluginSelectionState::NoPluginsDiscovered;
        selection.StatusMessage = "No script plugins were discovered.";
        return selection;
    }

    const std::string& configured_plugin_id = project_info->Scripting.SelectedPluginId;
    const std::string& configured_backend_name = project_info->Scripting.SelectedBackendName;

    if (configured_plugin_id.empty() && configured_backend_name.empty()) {
        selection.State = ScriptPluginSelectionState::MissingSelection;
        selection.StatusMessage =
            "No script plugin is selected for this project. Set 'Scripting.SelectedPluginId' before importing or "
            "running scripts.";
        return selection;
    }

    if (!configured_plugin_id.empty()) {
        selection.Candidate = findDiscoveredPlugin(configured_plugin_id);
        if (selection.Candidate == nullptr) {
            selection.State = ScriptPluginSelectionState::PluginNotFound;
            selection.StatusMessage = "Configured script plugin '" + configured_plugin_id +
                                     "' was not discovered in the current plugin directories.";
            return selection;
        }
    }

    if (selection.Candidate == nullptr && !configured_backend_name.empty()) {
        selection.Candidate = findUniqueCandidateByBackend(m_discovered_plugins, configured_backend_name);
        if (selection.Candidate != nullptr) {
            selection.AutoSelected = true;
        } else {
            const bool backend_exists = std::any_of(m_discovered_plugins.begin(),
                                                    m_discovered_plugins.end(),
                                                    [&](const ScriptPluginCandidate& candidate) {
                                                        return equalsIgnoreCase(candidate.Manifest.BackendName,
                                                                                configured_backend_name);
                                                    });
            selection.State = backend_exists ? ScriptPluginSelectionState::BackendAmbiguous
                                             : ScriptPluginSelectionState::BackendNotFound;
            selection.StatusMessage = backend_exists
                                          ? "Configured script backend '" + configured_backend_name +
                                                "' is provided by multiple plugins. Use SelectedPluginId instead."
                                          : "Configured script backend '" + configured_backend_name +
                                                "' was not discovered in the current plugin directories.";
            return selection;
        }
    }

    if (selection.Candidate == nullptr) {
        selection.State = ScriptPluginSelectionState::MissingSelection;
        selection.StatusMessage =
            "No script plugin is selected for this project. Set 'Scripting.SelectedPluginId' before importing or "
            "running scripts.";
        return selection;
    }

    if (selection.Candidate->Manifest.HostApiVersion != LUNA_SCRIPT_HOST_API_VERSION) {
        selection.State = ScriptPluginSelectionState::HostApiMismatch;
        selection.StatusMessage = "Script plugin '" + selection.Candidate->Manifest.PluginId +
                                  "' requires host API version " +
                                  std::to_string(selection.Candidate->Manifest.HostApiVersion) +
                                  " but the engine provides " +
                                  std::to_string(LUNA_SCRIPT_HOST_API_VERSION) + ".";
        return selection;
    }

    if (!configured_plugin_id.empty() && !configured_backend_name.empty() &&
        !equalsIgnoreCase(selection.Candidate->Manifest.BackendName, configured_backend_name)) {
        selection.State = ScriptPluginSelectionState::BackendMismatch;
        selection.StatusMessage = "Configured script plugin '" + configured_plugin_id +
                                  "' resolves to backend '" + selection.Candidate->Manifest.BackendName +
                                  "' but the project configuration declares backend '" + configured_backend_name +
                                  "'.";
        return selection;
    }

    selection.BackendName = selection.Candidate->Manifest.BackendName;
    selection.State = ScriptPluginSelectionState::Resolved;

    if (selection.AutoSelected) {
        if (!configured_backend_name.empty() && configured_plugin_id.empty()) {
            selection.StatusMessage = "Resolved script backend '" + configured_backend_name +
                                      "' to plugin '" + selection.Candidate->Manifest.PluginId + "'.";
        } else {
            selection.StatusMessage = "Automatically selected script plugin '" +
                                      selection.Candidate->Manifest.DisplayName + "'.";
        }
    } else {
        selection.StatusMessage = "Selected script plugin '" + selection.Candidate->Manifest.DisplayName + "'.";
    }

    return selection;
}

bool ScriptPluginManager::registerBackend(std::unique_ptr<IScriptBackend> backend)
{
    if (!backend) {
        return false;
    }

    BackendMap& target_backends = backend->descriptor().built_in ? m_builtin_backends : m_plugin_backends;
    return registerBackendIntoMap(target_backends, std::move(backend));
}

const LunaScriptHostApi& ScriptPluginManager::hostApi() const noexcept
{
    return m_host_api;
}

bool ScriptPluginManager::ensurePluginLoaded(const ScriptPluginCandidate* candidate)
{
    if (candidate == nullptr) {
        unloadActivePlugin();
        return true;
    }

    const std::string normalized_plugin_id = normalizeBackendKey(candidate->Manifest.PluginId);
    if (m_active_plugin_id == normalized_plugin_id && !m_plugin_backends.empty()) {
        return true;
    }

    unloadActivePlugin();

    const ScriptBackendDescriptor* builtin_backend = findBackendInMap(m_builtin_backends, candidate->Manifest.BackendName);
    if (candidate->Manifest.Entry.empty()) {
        if (builtin_backend != nullptr) {
            LUNA_CORE_WARN("Script plugin '{}' does not define an entry library yet; using built-in backend '{}'",
                           candidate->Manifest.PluginId,
                           candidate->Manifest.BackendName);
            return true;
        }

        LUNA_CORE_ERROR("Script plugin '{}' does not define 'Plugin.Entry'", candidate->Manifest.PluginId);
        return false;
    }

    if (!candidate->EntryExists) {
        if (builtin_backend != nullptr) {
            LUNA_CORE_WARN("Script plugin '{}' entry '{}' does not exist yet; using built-in backend '{}'",
                           candidate->Manifest.PluginId,
                           candidate->ResolvedEntryPath.string(),
                           candidate->Manifest.BackendName);
            return true;
        }

        LUNA_CORE_ERROR("Script plugin '{}' entry '{}' does not exist",
                        candidate->Manifest.PluginId,
                        candidate->ResolvedEntryPath.string());
        return false;
    }

    std::shared_ptr<DynamicLibraryHandle> plugin_library = DynamicLibraryHandle::load(candidate->ResolvedEntryPath);
    if (!plugin_library) {
        return false;
    }

    auto* create_plugin_fn =
        reinterpret_cast<LunaCreateScriptPluginFn>(plugin_library->findSymbol("LunaCreateScriptPlugin"));
    if (create_plugin_fn == nullptr) {
        LUNA_CORE_ERROR("Script plugin '{}' does not export LunaCreateScriptPlugin",
                        candidate->ResolvedEntryPath.string());
        return false;
    }

    LunaScriptPluginApi plugin_api{};
    plugin_api.struct_size = sizeof(LunaScriptPluginApi);
    plugin_api.api_version = LUNA_SCRIPT_PLUGIN_API_VERSION;
    if (create_plugin_fn(LUNA_SCRIPT_HOST_API_VERSION, &m_host_api, &plugin_api) == 0) {
        LUNA_CORE_ERROR("Script plugin '{}' failed to initialize its plugin API", candidate->Manifest.PluginId);
        return false;
    }

    if (plugin_api.api_version != LUNA_SCRIPT_PLUGIN_API_VERSION) {
        LUNA_CORE_ERROR("Script plugin '{}' returned plugin API version {} but the engine expects {}",
                        candidate->Manifest.PluginId,
                        plugin_api.api_version,
                        static_cast<uint32_t>(LUNA_SCRIPT_PLUGIN_API_VERSION));
        return false;
    }

    if (plugin_api.backends == nullptr || plugin_api.backend_count == 0) {
        LUNA_CORE_ERROR("Script plugin '{}' did not expose any script backends", candidate->Manifest.PluginId);
        return false;
    }

    BackendMap loaded_backends;
    loaded_backends.reserve(plugin_api.backend_count);

    for (size_t backend_index = 0; backend_index < plugin_api.backend_count; ++backend_index) {
        const LunaScriptBackendApi& backend_api = plugin_api.backends[backend_index];
        if (backend_api.api_version != LUNA_SCRIPT_BACKEND_API_VERSION) {
            LUNA_CORE_ERROR("Script plugin '{}' returned backend API version {} for backend #{} but the engine expects {}",
                            candidate->Manifest.PluginId,
                            backend_api.api_version,
                            backend_index,
                            static_cast<uint32_t>(LUNA_SCRIPT_BACKEND_API_VERSION));
            return false;
        }

        if (backend_api.backend_name == nullptr || backend_api.backend_name[0] == '\0') {
            LUNA_CORE_ERROR("Script plugin '{}' returned backend #{} with an empty backend_name",
                            candidate->Manifest.PluginId,
                            backend_index);
            return false;
        }

        if (backend_api.create_runtime == nullptr) {
            LUNA_CORE_ERROR("Script plugin '{}' returned backend '{}' without create_runtime",
                            candidate->Manifest.PluginId,
                            backend_api.backend_name);
            return false;
        }

        if (!registerBackendIntoMap(loaded_backends,
                                    std::make_unique<PluginScriptBackend>(*candidate, backend_api, plugin_library))) {
            LUNA_CORE_ERROR("Script plugin '{}' failed to register backend '{}'",
                            candidate->Manifest.PluginId,
                            backend_api.backend_name);
            return false;
        }
    }

    if (!loaded_backends.contains(normalizeBackendKey(candidate->Manifest.BackendName))) {
        LUNA_CORE_ERROR("Script plugin '{}' was loaded, but it does not expose the manifest backend '{}'",
                        candidate->Manifest.PluginId,
                        candidate->Manifest.BackendName);
        return false;
    }

    m_plugin_backends = std::move(loaded_backends);
    m_active_plugin_id = normalized_plugin_id;

    LUNA_CORE_INFO("Loaded script plugin '{}' from '{}'",
                   candidate->Manifest.PluginId,
                   candidate->ResolvedEntryPath.string());
    return true;
}

void ScriptPluginManager::unloadActivePlugin()
{
    if (m_active_plugin_id.empty() && m_plugin_backends.empty()) {
        return;
    }

    if (!m_active_plugin_id.empty()) {
        LUNA_CORE_INFO("Unloaded script plugin '{}'", m_active_plugin_id);
    }

    m_plugin_backends.clear();
    m_active_plugin_id.clear();
}

void ScriptPluginManager::registerBuiltinBackends()
{
    // Intentionally empty. Script languages are expected to be provided by plugins.
}

} // namespace luna
