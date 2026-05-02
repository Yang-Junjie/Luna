#include "Platform/Common/FileDialogs.h"

#include "Core/Application.h"
#include "Core/Log.h"
#include "Platform/Common/NativeWindowHandle.h"

#if !defined(WIN32_LEAN_AND_MEAN)
#    define WIN32_LEAN_AND_MEAN
#endif
#if !defined(NOMINMAX)
#    define NOMINMAX
#endif

#include <Windows.h>
#include <commdlg.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <array>
#include <cwchar>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace luna::FileDialogs {
namespace {

constexpr DWORD kMaxDialogPathLength = 32'768;
constexpr size_t kMaxDialogFilterLength = 4'096;

class ScopedComApartment {
public:
    ScopedComApartment()
        : m_result(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)),
          m_needs_uninitialize(SUCCEEDED(m_result))
    {}

    ScopedComApartment(const ScopedComApartment&) = delete;
    ScopedComApartment& operator=(const ScopedComApartment&) = delete;
    ScopedComApartment(ScopedComApartment&&) = delete;
    ScopedComApartment& operator=(ScopedComApartment&&) = delete;

    ~ScopedComApartment()
    {
        if (m_needs_uninitialize) {
            CoUninitialize();
        }
    }

    [[nodiscard]] bool initialized() const noexcept
    {
        return SUCCEEDED(m_result);
    }

    [[nodiscard]] HRESULT result() const noexcept
    {
        return m_result;
    }

private:
    HRESULT m_result{E_FAIL};
    bool m_needs_uninitialize{false};
};

struct CoTaskMemDeleter {
    void operator()(wchar_t* value) const noexcept
    {
        if (value != nullptr) {
            CoTaskMemFree(value);
        }
    }
};

using CoTaskMemString = std::unique_ptr<wchar_t, CoTaskMemDeleter>;

[[nodiscard]] HWND ownerWindow()
{
    return static_cast<HWND>(createNativeWindowHandle(Application::get().getWindow()).hWnd);
}

[[nodiscard]] std::wstring pathString(const std::string& path)
{
    return path.empty() ? std::wstring{} : std::filesystem::path(path).wstring();
}

[[nodiscard]] std::wstring dialogFilter(const char* filter)
{
    if (filter == nullptr || filter[0] == '\0') {
        return {};
    }

    std::wstring result;
    for (size_t index = 0; index + 1 < kMaxDialogFilterLength; ++index) {
        const char current = filter[index];
        result.push_back(static_cast<wchar_t>(static_cast<unsigned char>(current)));
        if (current == '\0' && filter[index + 1] == '\0') {
            result.push_back(L'\0');
            return result;
        }
    }

    LUNA_PLATFORM_WARN("Windows file dialog filter is not double-null terminated");
    result.push_back(L'\0');
    return result;
}

[[nodiscard]] std::wstring defaultExtension(const std::wstring& filter)
{
    if (filter.empty()) {
        return {};
    }

    const wchar_t* pattern = filter.c_str() + std::wcslen(filter.c_str()) + 1;
    if (*pattern == L'\0') {
        return {};
    }

    std::wstring extension(pattern);
    if (const size_t separator = extension.find(L';'); separator != std::wstring::npos) {
        extension.erase(separator);
    }
    if (extension.starts_with(L"*.")) {
        extension.erase(0, 2);
    } else if (extension.starts_with(L".")) {
        extension.erase(0, 1);
    }
    if (extension.find(L'*') != std::wstring::npos || extension.find(L'?') != std::wstring::npos) {
        return {};
    }
    return extension;
}

void logCommonDialogError(std::string_view operation)
{
    const DWORD error = CommDlgExtendedError();
    if (error != 0) {
        LUNA_PLATFORM_WARN("Windows {} dialog failed with common dialog error 0x{:x}", operation, error);
    }
}

void logHResultError(std::string_view operation, HRESULT result)
{
    if (FAILED(result)) {
        LUNA_PLATFORM_WARN("Windows {} dialog failed with HRESULT 0x{:x}",
                           operation,
                           static_cast<unsigned long>(result));
    }
}

} // namespace

std::filesystem::path openFile(const char* filter, const std::string& defaultPath)
{
    std::array<wchar_t, kMaxDialogPathLength> selected_file{};
    const std::wstring filter_text = dialogFilter(filter);
    const std::wstring initial_directory = pathString(defaultPath);

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = ownerWindow();
    dialog.lpstrFile = selected_file.data();
    dialog.nMaxFile = static_cast<DWORD>(selected_file.size());
    dialog.lpstrInitialDir = initial_directory.empty() ? nullptr : initial_directory.c_str();
    dialog.lpstrFilter = filter_text.empty() ? nullptr : filter_text.c_str();
    dialog.nFilterIndex = 1;
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&dialog) == TRUE) {
        return std::filesystem::path(selected_file.data());
    }

    logCommonDialogError("open file");
    return {};
}

std::filesystem::path saveFile(const char* filter, const std::string& defaultPath)
{
    std::array<wchar_t, kMaxDialogPathLength> selected_file{};
    const std::wstring filter_text = dialogFilter(filter);
    const std::wstring initial_directory = pathString(defaultPath);
    const std::wstring extension = defaultExtension(filter_text);

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = ownerWindow();
    dialog.lpstrFile = selected_file.data();
    dialog.nMaxFile = static_cast<DWORD>(selected_file.size());
    dialog.lpstrInitialDir = initial_directory.empty() ? nullptr : initial_directory.c_str();
    dialog.lpstrFilter = filter_text.empty() ? nullptr : filter_text.c_str();
    dialog.nFilterIndex = 1;
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    dialog.lpstrDefExt = extension.empty() ? nullptr : extension.c_str();

    if (GetSaveFileNameW(&dialog) == TRUE) {
        return std::filesystem::path(selected_file.data());
    }

    logCommonDialogError("save file");
    return {};
}

std::filesystem::path selectDirectory(const std::string& defaultPath)
{
    const ScopedComApartment com_apartment;
    if (!com_apartment.initialized()) {
        logHResultError("select directory COM initialization", com_apartment.result());
        return {};
    }

    Microsoft::WRL::ComPtr<IFileDialog> file_dialog;
    HRESULT result =
        CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(file_dialog.GetAddressOf()));
    if (FAILED(result)) {
        logHResultError("select directory creation", result);
        return {};
    }

    DWORD options = 0;
    result = file_dialog->GetOptions(&options);
    if (FAILED(result)) {
        logHResultError("select directory options query", result);
        return {};
    }

    result = file_dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    if (FAILED(result)) {
        logHResultError("select directory options update", result);
        return {};
    }

    if (!defaultPath.empty()) {
        Microsoft::WRL::ComPtr<IShellItem> initial_folder;
        const std::wstring initial_directory = pathString(defaultPath);
        result = SHCreateItemFromParsingName(
            initial_directory.c_str(), nullptr, IID_PPV_ARGS(initial_folder.GetAddressOf()));
        if (SUCCEEDED(result)) {
            file_dialog->SetFolder(initial_folder.Get());
        }
    }

    result = file_dialog->Show(ownerWindow());
    if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        return {};
    }
    if (FAILED(result)) {
        logHResultError("select directory show", result);
        return {};
    }

    Microsoft::WRL::ComPtr<IShellItem> selected_item;
    result = file_dialog->GetResult(selected_item.GetAddressOf());
    if (FAILED(result)) {
        logHResultError("select directory result query", result);
        return {};
    }

    wchar_t* raw_path = nullptr;
    result = selected_item->GetDisplayName(SIGDN_FILESYSPATH, &raw_path);
    CoTaskMemString selected_path(raw_path);
    if (FAILED(result) || selected_path == nullptr) {
        logHResultError("select directory path query", result);
        return {};
    }

    return std::filesystem::path(selected_path.get());
}

} // namespace luna::FileDialogs
