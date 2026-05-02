#pragma once

#include "Core/Window.h"
#include "Instance.h"

namespace luna {

[[nodiscard]] RHI::NativeWindowHandle createNativeWindowHandle(const Window& window);
[[nodiscard]] const char* nativeWindowPlatformName() noexcept;

} // namespace luna
