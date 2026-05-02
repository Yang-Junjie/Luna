#include "Platform/Common/NativeWindowHandle.h"

#include "Core/Log.h"

#if defined(_WIN32)
#    define GLFW_EXPOSE_NATIVE_WIN32
#    if !defined(WIN32_LEAN_AND_MEAN)
#        define WIN32_LEAN_AND_MEAN
#    endif
#    if !defined(NOMINMAX)
#        define NOMINMAX
#    endif
#    include <Windows.h>
#elif defined(__linux__) && !defined(__ANDROID__)
#    if defined(USE_WAYLAND)
#        define GLFW_EXPOSE_NATIVE_WAYLAND
#    elif defined(USE_XLIB)
#        define GLFW_EXPOSE_NATIVE_X11
#    endif
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace luna {

const char* nativeWindowPlatformName() noexcept
{
#if defined(_WIN32)
    return "Win32";
#elif defined(__ANDROID__)
    return "Android";
#elif defined(__linux__) && defined(USE_WAYLAND)
    return "Wayland";
#elif defined(__linux__) && defined(USE_XLIB)
    return "X11";
#elif defined(__linux__) && defined(USE_XCB)
    return "XCB";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

RHI::NativeWindowHandle createNativeWindowHandle(const Window& window)
{
    RHI::NativeWindowHandle handle{};
    auto* native_window = static_cast<GLFWwindow*>(window.getNativeWindow());
    if (native_window == nullptr) {
        return handle;
    }

#if defined(_WIN32)
    handle.hWnd = glfwGetWin32Window(native_window);
    handle.hInst = GetModuleHandleW(nullptr);
#elif defined(__linux__) && !defined(__ANDROID__)
#    if defined(USE_WAYLAND)
    handle.wlDisplay = glfwGetWaylandDisplay();
    handle.wlSurface = glfwGetWaylandWindow(native_window);
#    elif defined(USE_XLIB)
    handle.x11Display = glfwGetX11Display();
    handle.x11Window = static_cast<std::uintptr_t>(glfwGetX11Window(native_window));
#    elif defined(USE_XCB)
    LUNA_PLATFORM_ERROR("GLFW does not expose an XCB native window handle; build Luna with USE_XLIB or USE_WAYLAND");
#    else
    LUNA_PLATFORM_ERROR("No Linux native window backend selected for GLFW");
#    endif
#elif defined(__ANDROID__)
    LUNA_PLATFORM_ERROR("Android native window handle conversion is not implemented for GLFW windows");
#endif

    return handle;
}

} // namespace luna
