#pragma once

#include <cstddef>

namespace luna::editor {

class RuntimeViewportService {
public:
    virtual ~RuntimeViewportService() = default;

    virtual bool isRuntimeViewportEnabled() const noexcept = 0;
    virtual bool isRuntimeViewportRequested() const noexcept = 0;
    virtual void setRuntimeViewportRequested(bool enabled) = 0;
    virtual size_t runtimeEntityCount() const noexcept = 0;
};

} // namespace luna::editor
