#include "Platform/Common/NativeWindowHandle.h"

#define GLFW_EXPOSE_NATIVE_COCOA

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

namespace luna {

const char* nativeWindowPlatformName() noexcept
{
    return "Cocoa";
}

RHI::NativeWindowHandle createNativeWindowHandle(const Window& window)
{
    RHI::NativeWindowHandle handle{};
    auto* native_window = static_cast<GLFWwindow*>(window.getNativeWindow());
    if (native_window == nullptr) {
        return handle;
    }

    NSWindow* cocoa_window = glfwGetCocoaWindow(native_window);
    if (cocoa_window == nil) {
        return handle;
    }

    NSView* content_view = [cocoa_window contentView];
    if (content_view == nil) {
        return handle;
    }

    [content_view setWantsLayer:YES];

    CAMetalLayer* metal_layer = [CAMetalLayer layer];
    metal_layer.contentsScale = [cocoa_window backingScaleFactor];
    content_view.layer = metal_layer;
    handle.metalLayer = (__bridge void*) metal_layer;
    return handle;
}

} // namespace luna
