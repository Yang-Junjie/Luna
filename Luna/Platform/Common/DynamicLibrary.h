#pragma once

#include <filesystem>
#include <memory>

namespace luna {

class DynamicLibrary final {
public:
    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;
    DynamicLibrary(DynamicLibrary&&) = delete;
    DynamicLibrary& operator=(DynamicLibrary&&) = delete;

    [[nodiscard]] static std::shared_ptr<DynamicLibrary> load(const std::filesystem::path& path);

    ~DynamicLibrary();

    [[nodiscard]] void* findSymbol(const char* name) const;
    [[nodiscard]] const std::filesystem::path& path() const noexcept;

private:
    DynamicLibrary(void* module, std::filesystem::path path);

    void* m_module{nullptr};
    std::filesystem::path m_path;
};

} // namespace luna
