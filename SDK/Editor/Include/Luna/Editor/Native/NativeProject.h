#pragma once

#include "Luna/Editor/Native/NativeTypes.h"

namespace luna::editor::native {

class Project final {
public:
    constexpr Project() noexcept = default;
    explicit constexpr Project(const LunaEditorProjectApi* api) noexcept
        : api_(api)
    {
    }

    [[nodiscard]] bool hasProjectLoaded() const noexcept
    {
        return api_ != nullptr && api_->has_project_loaded != nullptr &&
               api_->has_project_loaded(api_->api_user_data) != 0;
    }

    [[nodiscard]] bool info(LunaEditorProjectInfo* out_info) const noexcept
    {
        return api_ != nullptr && api_->project_info != nullptr && out_info != nullptr &&
               api_->project_info(api_->api_user_data, out_info) != 0;
    }

    [[nodiscard]] bool rootPath(char* out_path, size_t out_path_size) const noexcept
    {
        return api_ != nullptr && api_->project_root_path != nullptr && out_path != nullptr &&
               api_->project_root_path(api_->api_user_data, out_path, out_path_size) != 0;
    }

    [[nodiscard]] bool save() const noexcept
    {
        return api_ != nullptr && api_->save_project != nullptr && api_->save_project(api_->api_user_data) != 0;
    }

    [[nodiscard]] const LunaEditorProjectApi* native() const noexcept
    {
        return api_;
    }

private:
    const LunaEditorProjectApi* api_{};
};

} // namespace luna::editor::native
