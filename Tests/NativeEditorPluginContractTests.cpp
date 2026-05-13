#include "Core/Log.h"
#include "EditorApi/EditorNativePluginApi.h"
#include "Platform/Common/DynamicLibrary.h"
#include "Shell/EditorPluginDependencyResolver.h"
#include "Shell/EditorPluginManifest.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

constexpr const char* kExpectedPluginId = "luna.test.native";
constexpr const char* kCommandId = "luna.test.native.open";
constexpr const char* kWindowId = "luna.test.native.window";

class TempDirectory {
public:
    explicit TempDirectory(std::string_view name)
    {
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = std::filesystem::temp_directory_path() / ("Luna-" + std::string(name) + "-" + std::to_string(now));
        std::filesystem::create_directories(m_path);
    }

    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;

    ~TempDirectory()
    {
        std::error_code ec;
        std::filesystem::remove_all(m_path, ec);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept
    {
        return m_path;
    }

private:
    std::filesystem::path m_path;
};

class TestContext {
public:
    bool expect(bool condition, std::string_view message)
    {
        if (condition) {
            return true;
        }

        ++m_failures;
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }

    [[nodiscard]] int result() const noexcept
    {
        return m_failures == 0 ? 0 : 1;
    }

private:
    int m_failures{0};
};

void writeTextFile(const std::filesystem::path& path, std::string_view contents)
{
    std::filesystem::create_directories(path.parent_path());

    std::ofstream file(path, std::ios::binary);
    file << contents;
}

std::filesystem::path testPluginBinaryPath(std::string_view target_name)
{
#if defined(_WIN32)
    constexpr std::string_view extension = ".dll";
#elif defined(__APPLE__)
    constexpr std::string_view extension = ".dylib";
#else
    constexpr std::string_view extension = ".so";
#endif

    return std::filesystem::path(LUNA_TEST_EDITOR_PLUGIN_DIR) / (std::string(target_name) + std::string(extension));
}

std::string yamlQuotedPath(const std::filesystem::path& path)
{
    const std::string value = path.generic_string();
    std::string quoted;
    quoted.reserve(value.size() + 2);
    quoted.push_back('"');
    for (const char character : value) {
        if (character == '"' || character == '\\') {
            quoted.push_back('\\');
        }
        quoted.push_back(character);
    }
    quoted.push_back('"');
    return quoted;
}

void writeEditorPluginManifest(const std::filesystem::path& plugins_root,
                               std::string_view directory_name,
                               std::string_view plugin_id,
                               std::string_view display_name,
                               const std::filesystem::path& entry_path,
                               const std::vector<std::string_view>& dependencies)
{
    std::ostringstream manifest;
    manifest << "EditorPlugin:\n"
             << "  Id: " << plugin_id << "\n"
             << "  DisplayName: " << display_name << "\n"
             << "  Runtime: Native\n"
             << "  Version: 0.1.0\n"
             << "  Enabled: true\n"
             << "  Entry: " << yamlQuotedPath(entry_path) << "\n";
    if (!dependencies.empty()) {
        manifest << "  Dependencies:\n";
        for (const std::string_view dependency : dependencies) {
            manifest << "    - " << dependency << "\n";
        }
    }

    writeTextFile(plugins_root / std::string(directory_name) / "editor-plugin.yaml", manifest.str());
}

enum class NativeLoadStatus {
    Loaded,
    MissingLibrary,
    MissingSymbol,
    CreateFailed,
    PluginApiSizeMismatch,
    PluginApiVersionMismatch,
    EmptyPluginId,
    PluginIdMismatch,
    MissingCallbacks,
    OnLoadFailed,
};

const char* statusName(NativeLoadStatus status)
{
    switch (status) {
        case NativeLoadStatus::Loaded:
            return "Loaded";
        case NativeLoadStatus::MissingLibrary:
            return "MissingLibrary";
        case NativeLoadStatus::MissingSymbol:
            return "MissingSymbol";
        case NativeLoadStatus::CreateFailed:
            return "CreateFailed";
        case NativeLoadStatus::PluginApiSizeMismatch:
            return "PluginApiSizeMismatch";
        case NativeLoadStatus::PluginApiVersionMismatch:
            return "PluginApiVersionMismatch";
        case NativeLoadStatus::EmptyPluginId:
            return "EmptyPluginId";
        case NativeLoadStatus::PluginIdMismatch:
            return "PluginIdMismatch";
        case NativeLoadStatus::MissingCallbacks:
            return "MissingCallbacks";
        case NativeLoadStatus::OnLoadFailed:
            return "OnLoadFailed";
    }
    return "Unknown";
}

struct TestNativeHost {
    struct CommandRecord {
        std::string id;
        std::string owner_id;
        void* user_data{nullptr};
        int (*can_execute)(void*, const LunaEditorHostApi*){nullptr};
        int (*is_checked)(void*, const LunaEditorHostApi*){nullptr};
        void (*execute)(void*, const LunaEditorHostApi*){nullptr};
    };

    struct WindowRecord {
        std::string id;
        std::string owner_id;
        bool open{false};
        void* user_data{nullptr};
        void (*draw)(void*, const LunaEditorHostApi*){nullptr};
    };

    explicit TestNativeHost(std::string owner)
        : owner_id(std::move(owner))
    {
        api.struct_size = sizeof(LunaEditorHostApi);
        api.api_version = LUNA_EDITOR_HOST_API_VERSION;
        api.host_user_data = this;
        api.log = LunaEditorLogApi{
            .struct_size = sizeof(LunaEditorLogApi),
            .api_version = LUNA_EDITOR_LOG_API_VERSION,
            .api_user_data = this,
            .log = &log,
        };
        api.ui = LunaEditorUiApi{
            .struct_size = sizeof(LunaEditorUiApi),
            .api_version = LUNA_EDITOR_UI_API_VERSION,
            .api_user_data = this,
            .text = &text,
            .separator = &separator,
            .button = &button,
        };
        api.commands = LunaEditorCommandApi{
            .struct_size = sizeof(LunaEditorCommandApi),
            .api_version = LUNA_EDITOR_COMMAND_API_VERSION,
            .api_user_data = this,
            .register_command = &registerCommand,
            .unregister_command = &unregisterCommand,
            .execute_command = &executeCommand,
            .can_execute_command = &canExecuteCommand,
            .is_command_checked = &isCommandChecked,
        };
        api.windows = LunaEditorWindowApi{
            .struct_size = sizeof(LunaEditorWindowApi),
            .api_version = LUNA_EDITOR_WINDOW_API_VERSION,
            .api_user_data = this,
            .register_window = &registerWindow,
            .unregister_window = &unregisterWindow,
            .is_window_open = &isWindowOpen,
            .set_window_open = &setWindowOpen,
        };
    }

    void unregisterContributionsForOwner(std::string_view owner)
    {
        for (auto it = commands.begin(); it != commands.end();) {
            if (it->second.owner_id == owner) {
                it = commands.erase(it);
            } else {
                ++it;
            }
        }

        for (auto it = windows.begin(); it != windows.end();) {
            if (it->second.owner_id == owner) {
                it = windows.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool drawWindow(std::string_view id)
    {
        const auto it = windows.find(std::string(id));
        if (it == windows.end() || it->second.draw == nullptr) {
            return false;
        }
        it->second.draw(it->second.user_data, &api);
        return true;
    }

    std::string owner_id;
    LunaEditorHostApi api{};
    std::unordered_map<std::string, CommandRecord> commands;
    std::unordered_map<std::string, WindowRecord> windows;
    std::vector<std::string> logs;
    int text_count{0};
    int separator_count{0};
    int button_count{0};
    bool next_button_pressed{false};

private:
    static TestNativeHost* self(void* api_user_data)
    {
        return static_cast<TestNativeHost*>(api_user_data);
    }

    static std::string copyString(const char* value)
    {
        return value != nullptr ? std::string(value) : std::string{};
    }

    static void log(void* api_user_data, LunaEditorLogLevel, const char* message)
    {
        if (TestNativeHost* host = self(api_user_data)) {
            host->logs.push_back(copyString(message));
        }
    }

    static void text(void* api_user_data, const char*)
    {
        if (TestNativeHost* host = self(api_user_data)) {
            ++host->text_count;
        }
    }

    static void separator(void* api_user_data)
    {
        if (TestNativeHost* host = self(api_user_data)) {
            ++host->separator_count;
        }
    }

    static int button(void* api_user_data, const char*, const LunaEditorVec2*, uint32_t)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr) {
            return 0;
        }
        ++host->button_count;
        const bool pressed = host->next_button_pressed;
        host->next_button_pressed = false;
        return pressed ? 1 : 0;
    }

    static int registerCommand(void* api_user_data, const LunaEditorCommandDescriptor* descriptor)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || descriptor == nullptr || descriptor->struct_size < sizeof(LunaEditorCommandDescriptor) ||
            descriptor->api_version != LUNA_EDITOR_COMMAND_DESCRIPTOR_API_VERSION || descriptor->id == nullptr ||
            descriptor->id[0] == '\0' || descriptor->execute == nullptr) {
            return 0;
        }

        CommandRecord record{};
        record.id = descriptor->id;
        record.owner_id = host->owner_id;
        record.user_data = descriptor->command_user_data;
        record.can_execute = descriptor->can_execute;
        record.is_checked = descriptor->is_checked;
        record.execute = descriptor->execute;
        host->commands[record.id] = record;
        return 1;
    }

    static void unregisterCommand(void* api_user_data, const char* id)
    {
        if (TestNativeHost* host = self(api_user_data); host != nullptr && id != nullptr) {
            host->commands.erase(id);
        }
    }

    static int executeCommand(void* api_user_data, const char* id)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || id == nullptr) {
            return 0;
        }

        const auto it = host->commands.find(id);
        if (it == host->commands.end() || it->second.execute == nullptr) {
            return 0;
        }
        if (it->second.can_execute != nullptr && it->second.can_execute(it->second.user_data, &host->api) == 0) {
            return 0;
        }

        it->second.execute(it->second.user_data, &host->api);
        return 1;
    }

    static int canExecuteCommand(void* api_user_data, const char* id)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || id == nullptr) {
            return 0;
        }

        const auto it = host->commands.find(id);
        if (it == host->commands.end()) {
            return 0;
        }
        return it->second.can_execute == nullptr || it->second.can_execute(it->second.user_data, &host->api) != 0 ? 1
                                                                                                                  : 0;
    }

    static int isCommandChecked(void* api_user_data, const char* id)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || id == nullptr) {
            return 0;
        }

        const auto it = host->commands.find(id);
        if (it == host->commands.end() || it->second.is_checked == nullptr) {
            return 0;
        }
        return it->second.is_checked(it->second.user_data, &host->api) != 0 ? 1 : 0;
    }

    static int registerWindow(void* api_user_data, const LunaEditorWindowDescriptor* descriptor)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || descriptor == nullptr || descriptor->struct_size < sizeof(LunaEditorWindowDescriptor) ||
            descriptor->api_version != LUNA_EDITOR_WINDOW_DESCRIPTOR_API_VERSION || descriptor->id == nullptr ||
            descriptor->id[0] == '\0' || descriptor->title == nullptr || descriptor->draw == nullptr) {
            return 0;
        }

        WindowRecord record{};
        record.id = descriptor->id;
        record.owner_id = host->owner_id;
        record.open = descriptor->default_open != 0;
        record.user_data = descriptor->window_user_data;
        record.draw = descriptor->draw;
        host->windows[record.id] = record;
        return 1;
    }

    static void unregisterWindow(void* api_user_data, const char* id)
    {
        if (TestNativeHost* host = self(api_user_data); host != nullptr && id != nullptr) {
            host->windows.erase(id);
        }
    }

    static int isWindowOpen(void* api_user_data, const char* id)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || id == nullptr) {
            return 0;
        }

        const auto it = host->windows.find(id);
        return it != host->windows.end() && it->second.open ? 1 : 0;
    }

    static void setWindowOpen(void* api_user_data, const char* id, int open)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || id == nullptr) {
            return;
        }

        const auto it = host->windows.find(id);
        if (it != host->windows.end()) {
            it->second.open = open != 0;
        }
    }
};

struct NativePluginLoadResult {
    NativeLoadStatus status{NativeLoadStatus::MissingLibrary};
    std::shared_ptr<luna::DynamicLibrary> library;
    std::unique_ptr<TestNativeHost> host;
    LunaEditorPluginApi plugin_api{};
    bool loaded{false};

    void unload()
    {
        if (!loaded || host == nullptr) {
            return;
        }

        if (plugin_api.on_unload != nullptr) {
            plugin_api.on_unload(plugin_api.plugin_user_data, &host->api);
        }
        host->unregisterContributionsForOwner(host->owner_id);
        loaded = false;
    }
};

NativePluginLoadResult loadNativePluginForTest(const std::filesystem::path& path, std::string_view expected_plugin_id)
{
    NativePluginLoadResult result{};
    result.host = std::make_unique<TestNativeHost>(std::string(expected_plugin_id));

    result.library = luna::DynamicLibrary::load(path);
    if (!result.library) {
        result.status = NativeLoadStatus::MissingLibrary;
        return result;
    }

    auto* create_plugin_fn =
        reinterpret_cast<LunaCreateEditorPluginFn>(result.library->findSymbol(LUNA_EDITOR_CREATE_PLUGIN_SYMBOL));
    if (create_plugin_fn == nullptr) {
        result.status = NativeLoadStatus::MissingSymbol;
        return result;
    }

    result.plugin_api = LunaEditorPluginApi{
        .struct_size = sizeof(LunaEditorPluginApi),
        .api_version = LUNA_EDITOR_PLUGIN_API_VERSION,
    };
    if (create_plugin_fn(LUNA_EDITOR_HOST_API_VERSION, &result.host->api, &result.plugin_api) == 0) {
        result.status = NativeLoadStatus::CreateFailed;
        return result;
    }
    if (result.plugin_api.struct_size != sizeof(LunaEditorPluginApi)) {
        result.status = NativeLoadStatus::PluginApiSizeMismatch;
        return result;
    }
    if (result.plugin_api.api_version != LUNA_EDITOR_PLUGIN_API_VERSION) {
        result.status = NativeLoadStatus::PluginApiVersionMismatch;
        return result;
    }
    if (result.plugin_api.plugin_id == nullptr || result.plugin_api.plugin_id[0] == '\0') {
        result.status = NativeLoadStatus::EmptyPluginId;
        return result;
    }
    if (expected_plugin_id != result.plugin_api.plugin_id) {
        result.status = NativeLoadStatus::PluginIdMismatch;
        return result;
    }
    if (result.plugin_api.on_load == nullptr || result.plugin_api.on_unload == nullptr) {
        result.status = NativeLoadStatus::MissingCallbacks;
        return result;
    }
    if (result.plugin_api.on_load(result.plugin_api.plugin_user_data, &result.host->api) == 0) {
        result.host->unregisterContributionsForOwner(expected_plugin_id);
        result.status = NativeLoadStatus::OnLoadFailed;
        return result;
    }

    result.status = NativeLoadStatus::Loaded;
    result.loaded = true;
    return result;
}

bool expectStatus(TestContext& context,
                  const NativePluginLoadResult& result,
                  NativeLoadStatus expected,
                  std::string_view label)
{
    if (result.status == expected) {
        return true;
    }

    std::ostringstream message;
    message << label << ": expected " << statusName(expected) << ", got " << statusName(result.status);
    return context.expect(false, message.str());
}

template <typename Container> bool contains(const Container& values, std::string_view value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

const luna::editor::EditorPluginPackage* findPackage(const std::vector<luna::editor::EditorPluginPackage>& packages,
                                                     std::string_view id)
{
    const auto it = std::find_if(packages.begin(), packages.end(), [&](const auto& package) {
        return package.id == id;
    });
    return it != packages.end() ? &*it : nullptr;
}

void testSuccessfulNativePluginLoad(TestContext& context)
{
    NativePluginLoadResult result =
        loadNativePluginForTest(testPluginBinaryPath("LunaTestEditorPluginGood"), kExpectedPluginId);
    if (!expectStatus(context, result, NativeLoadStatus::Loaded, "good native editor plugin")) {
        return;
    }

    context.expect(result.plugin_api.api_version == LUNA_EDITOR_PLUGIN_API_VERSION,
                   "good native editor plugin should use v1 plugin API");
    context.expect(result.host->commands.size() == 1, "good native editor plugin should register one command");
    context.expect(result.host->windows.size() == 1, "good native editor plugin should register one window");
    context.expect(result.host->commands.contains(kCommandId), "registered native command id should match");
    context.expect(result.host->windows.contains(kWindowId), "registered native window id should match");
    context.expect(result.host->commands[kCommandId].owner_id == kExpectedPluginId,
                   "registered native command should be owner-tagged");
    context.expect(result.host->windows[kWindowId].owner_id == kExpectedPluginId,
                   "registered native window should be owner-tagged");

    context.expect(result.host->api.commands.can_execute_command(result.host->api.commands.api_user_data, kCommandId) ==
                       1,
                   "registered native command should be executable");
    context.expect(result.host->api.commands.is_command_checked(result.host->api.commands.api_user_data, kCommandId) ==
                       0,
                   "registered native command should start unchecked");
    context.expect(result.host->api.commands.execute_command(result.host->api.commands.api_user_data, kCommandId) == 1,
                   "registered native command should execute through host API");
    context.expect(result.host->api.windows.is_window_open(result.host->api.windows.api_user_data, kWindowId) == 1,
                   "native command should open registered window");
    context.expect(result.host->api.commands.is_command_checked(result.host->api.commands.api_user_data, kCommandId) ==
                       1,
                   "registered native command should reflect open window state");

    context.expect(result.host->drawWindow(kWindowId), "registered native window should draw through callback");
    context.expect(result.host->text_count > 0, "native window draw should use host UI text API");
    context.expect(result.host->button_count > 0, "native window draw should use host UI button API");

    result.host->api.windows.set_window_open(result.host->api.windows.api_user_data, kWindowId, 0);
    result.host->next_button_pressed = true;
    context.expect(result.host->drawWindow(kWindowId), "native window button draw should run");
    context.expect(result.host->api.windows.is_window_open(result.host->api.windows.api_user_data, kWindowId) == 1,
                   "native window button should execute registered command");

    result.unload();
    context.expect(result.host->commands.empty(), "native plugin unload should clean command contributions");
    context.expect(result.host->windows.empty(), "native plugin unload should clean window contributions");
}

void testNativePluginLoadFailures(TestContext& context)
{
    {
        NativePluginLoadResult result =
            loadNativePluginForTest(testPluginBinaryPath("LunaTestEditorPluginMissingSymbol"), kExpectedPluginId);
        expectStatus(context, result, NativeLoadStatus::MissingSymbol, "native editor plugin missing symbol");
    }
    {
        NativePluginLoadResult result =
            loadNativePluginForTest(testPluginBinaryPath("LunaTestEditorPluginApiMismatch"), kExpectedPluginId);
        expectStatus(context, result, NativeLoadStatus::PluginApiVersionMismatch, "native editor plugin API mismatch");
    }
    {
        NativePluginLoadResult result =
            loadNativePluginForTest(testPluginBinaryPath("LunaTestEditorPluginIdMismatch"), kExpectedPluginId);
        expectStatus(context, result, NativeLoadStatus::PluginIdMismatch, "native editor plugin id mismatch");
    }
    {
        NativePluginLoadResult result =
            loadNativePluginForTest(testPluginBinaryPath("LunaTestEditorPluginMissingCallbacks"), kExpectedPluginId);
        expectStatus(context, result, NativeLoadStatus::MissingCallbacks, "native editor plugin missing callbacks");
    }
    {
        NativePluginLoadResult result =
            loadNativePluginForTest(testPluginBinaryPath("LunaTestEditorPluginLoadFailure"), kExpectedPluginId);
        expectStatus(context, result, NativeLoadStatus::OnLoadFailed, "native editor plugin on_load failure");
        context.expect(result.host->commands.empty(),
                       "failed native editor plugin load should clean registered command contributions");
        context.expect(result.host->windows.empty(),
                       "failed native editor plugin load should clean registered window contributions");
    }
}

void testManifestAndDependencyContract(TestContext& context)
{
    TempDirectory temp("NativeEditorPluginManifest");
    const std::filesystem::path plugins_root = temp.path() / "EditorPlugins";
    const std::filesystem::path good_entry = testPluginBinaryPath("LunaTestEditorPluginGood");

    writeEditorPluginManifest(plugins_root, "Good", "luna.test.native-good", "Native Good", good_entry, {});
    writeEditorPluginManifest(plugins_root,
                              "Dependent",
                              "luna.test.native-dependent",
                              "Native Dependent",
                              good_entry,
                              {"luna.test.native-good", "luna.test.native-missing"});

    luna::editor::EditorPluginManifestLoader loader;
    const std::vector<luna::editor::EditorPluginPackage> packages = loader.loadPackagesFromRoot(plugins_root);
    context.expect(packages.size() == 2, "native editor plugin manifests should be discovered");

    const luna::editor::EditorPluginPackage* good = findPackage(packages, "luna.test.native-good");
    const luna::editor::EditorPluginPackage* dependent = findPackage(packages, "luna.test.native-dependent");
    context.expect(good != nullptr, "native good manifest should load");
    context.expect(dependent != nullptr, "native dependent manifest should load");

    if (good != nullptr) {
        context.expect(good->runtime == luna::editor::EditorPluginRuntime::Native,
                       "native manifest should parse Runtime=Native");
        context.expect(good->entry_exists, "native manifest should record existing entry");
        context.expect(good->resolved_entry_path == good_entry.lexically_normal(),
                       "native manifest should resolve absolute entry path");
    }

    if (dependent != nullptr) {
        std::unordered_set<std::string> loaded_ids{"luna.test.native-good"};
        const std::vector<std::string> missing = luna::editor::missingEditorPluginDependencies(*dependent, loaded_ids);
        context.expect(missing.size() == 1 && contains(missing, "luna.test.native-missing"),
                       "dependency resolver should report missing native editor plugin dependency");
        context.expect(!luna::editor::areEditorPluginDependenciesLoaded(*dependent, loaded_ids),
                       "dependency resolver should reject incomplete dependency set");

        loaded_ids.insert("luna.test.native-missing");
        context.expect(luna::editor::areEditorPluginDependenciesLoaded(*dependent, loaded_ids),
                       "dependency resolver should accept complete dependency set");
    }
}

} // namespace

int main()
{
    luna::Logger::init("", luna::Logger::Level::Warn);

    TestContext context;
    testSuccessfulNativePluginLoad(context);
    testNativePluginLoadFailures(context);
    testManifestAndDependencyContract(context);

    luna::Logger::shutdown();
    return context.result();
}
