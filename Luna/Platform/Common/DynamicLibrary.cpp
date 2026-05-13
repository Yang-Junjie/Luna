#include "Platform/Common/DynamicLibrary.h"

#include "Core/Log.h"

#include <string>
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
#endif

} // namespace

namespace luna {

std::shared_ptr<DynamicLibrary> DynamicLibrary::load(const std::filesystem::path& path)
{
#if defined(_WIN32)
    const HMODULE module = ::LoadLibraryW(path.c_str());
    if (module == nullptr) {
        const DWORD error = ::GetLastError();
        LUNA_CORE_ERROR("Failed to load dynamic library '{}': {}", path.string(), formatWindowsError(error));
        return {};
    }

    return std::shared_ptr<DynamicLibrary>(
        new DynamicLibrary(reinterpret_cast<void*>(module), path.lexically_normal()));
#else
    dlerror();
    void* module = ::dlopen(path.string().c_str(), RTLD_NOW | RTLD_LOCAL);
    if (module == nullptr) {
        const char* error = dlerror();
        LUNA_CORE_ERROR("Failed to load dynamic library '{}': {}",
                        path.string(),
                        error != nullptr ? error : "unknown error");
        return {};
    }

    return std::shared_ptr<DynamicLibrary>(new DynamicLibrary(module, path.lexically_normal()));
#endif
}

DynamicLibrary::~DynamicLibrary()
{
    if (m_module == nullptr) {
        return;
    }

#if defined(_WIN32)
    ::FreeLibrary(reinterpret_cast<HMODULE>(m_module));
#else
    ::dlclose(m_module);
#endif
}

void* DynamicLibrary::findSymbol(const char* name) const
{
    if (m_module == nullptr || name == nullptr) {
        return nullptr;
    }

#if defined(_WIN32)
    return reinterpret_cast<void*>(::GetProcAddress(reinterpret_cast<HMODULE>(m_module), name));
#else
    dlerror();
    void* symbol = ::dlsym(m_module, name);
    const char* error = dlerror();
    return error == nullptr ? symbol : nullptr;
#endif
}

const std::filesystem::path& DynamicLibrary::path() const noexcept
{
    return m_path;
}

DynamicLibrary::DynamicLibrary(void* module, std::filesystem::path path)
    : m_module(module),
      m_path(std::move(path))
{}

} // namespace luna
