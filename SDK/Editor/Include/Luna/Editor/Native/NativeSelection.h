#pragma once

#include "Luna/Editor/Native/NativeTypes.h"

namespace luna::editor::native {

class Selection final {
public:
    constexpr Selection() noexcept = default;

    explicit constexpr Selection(const LunaEditorSelectionApi* api) noexcept
        : api_(api)
    {}

    [[nodiscard]] bool available() const noexcept
    {
        return api_ != nullptr;
    }

    [[nodiscard]] uint64_t selectedEntityId() const noexcept
    {
        if (api_ != nullptr && api_->selected_entity_id != nullptr) {
            return api_->selected_entity_id(api_->api_user_data);
        }
        return 0u;
    }

    void selectEntity(uint64_t entity_id) const noexcept
    {
        if (api_ != nullptr && api_->select_entity != nullptr) {
            api_->select_entity(api_->api_user_data, entity_id);
        }
    }

    void clear() const noexcept
    {
        if (api_ != nullptr && api_->clear_selection != nullptr) {
            api_->clear_selection(api_->api_user_data);
        }
    }

    [[nodiscard]] const LunaEditorSelectionApi* native() const noexcept
    {
        return api_;
    }

private:
    const LunaEditorSelectionApi* api_{};
};

} // namespace luna::editor::native
