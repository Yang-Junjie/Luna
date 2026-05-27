#pragma once

#include "Luna/Editor/Native/NativeTypes.h"

namespace luna::editor::native {

class RuntimeViewport final {
public:
    constexpr RuntimeViewport() noexcept = default;

    explicit constexpr RuntimeViewport(const LunaEditorRuntimeViewportApi* api) noexcept
        : api_(api)
    {}

    [[nodiscard]] bool available() const noexcept
    {
        return api_ != nullptr;
    }

    [[nodiscard]] bool enabled() const noexcept
    {
        return api_ != nullptr && api_->is_runtime_viewport_enabled != nullptr &&
               api_->is_runtime_viewport_enabled(api_->api_user_data) != 0;
    }

    [[nodiscard]] bool requested() const noexcept
    {
        return api_ != nullptr && api_->is_runtime_viewport_requested != nullptr &&
               api_->is_runtime_viewport_requested(api_->api_user_data) != 0;
    }

    void setRequested(bool enabled) const noexcept
    {
        if (api_ != nullptr && api_->set_runtime_viewport_requested != nullptr) {
            api_->set_runtime_viewport_requested(api_->api_user_data, enabled ? 1 : 0);
        }
    }

    [[nodiscard]] size_t entityCount() const noexcept
    {
        if (api_ != nullptr && api_->runtime_entity_count != nullptr) {
            return api_->runtime_entity_count(api_->api_user_data);
        }
        return 0u;
    }

    [[nodiscard]] const LunaEditorRuntimeViewportApi* native() const noexcept
    {
        return api_;
    }

private:
    const LunaEditorRuntimeViewportApi* api_{};
};

} // namespace luna::editor::native
