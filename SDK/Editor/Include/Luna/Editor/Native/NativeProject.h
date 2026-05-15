#pragma once

#include "Luna/Editor/Native/NativeTypes.h"

#include <array>
#include <string>

namespace luna::editor::native {

class Project final {
public:
    constexpr Project() noexcept = default;
    explicit constexpr Project(const LunaEditorProjectApi* api) noexcept
        : api_(api)
    {
    }

    [[nodiscard]] bool available() const noexcept
    {
        return api_ != nullptr;
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

    [[nodiscard]] ProjectInfo info() const
    {
        ProjectInfo result{};
        if (api_ == nullptr || api_->project_info == nullptr) {
            return result;
        }

        std::array<char, 128> name{};
        std::array<char, 64> version{};
        std::array<char, 128> author{};
        std::array<char, 512> description{};
        std::array<char, 512> start_scene{};
        std::array<char, 512> assets_path{};
        std::array<char, 128> selected_script_plugin_id{};
        std::array<char, 128> selected_script_backend_name{};

        LunaEditorProjectInfo native_info{};
        native_info.struct_size = sizeof(LunaEditorProjectInfo);
        native_info.api_version = LUNA_EDITOR_PROJECT_INFO_API_VERSION;
        native_info.name = name.data();
        native_info.name_size = name.size();
        native_info.version = version.data();
        native_info.version_size = version.size();
        native_info.author = author.data();
        native_info.author_size = author.size();
        native_info.description = description.data();
        native_info.description_size = description.size();
        native_info.start_scene = start_scene.data();
        native_info.start_scene_size = start_scene.size();
        native_info.assets_path = assets_path.data();
        native_info.assets_path_size = assets_path.size();
        native_info.selected_script_plugin_id = selected_script_plugin_id.data();
        native_info.selected_script_plugin_id_size = selected_script_plugin_id.size();
        native_info.selected_script_backend_name = selected_script_backend_name.data();
        native_info.selected_script_backend_name_size = selected_script_backend_name.size();

        if (api_->project_info(api_->api_user_data, &native_info) == 0) {
            return result;
        }

        result.name = native_info.name != nullptr ? native_info.name : "";
        result.version = native_info.version != nullptr ? native_info.version : "";
        result.author = native_info.author != nullptr ? native_info.author : "";
        result.description = native_info.description != nullptr ? native_info.description : "";
        result.start_scene = native_info.start_scene != nullptr ? native_info.start_scene : "";
        result.assets_path = native_info.assets_path != nullptr ? native_info.assets_path : "";
        result.selected_script_plugin_id =
            native_info.selected_script_plugin_id != nullptr ? native_info.selected_script_plugin_id : "";
        result.selected_script_backend_name =
            native_info.selected_script_backend_name != nullptr ? native_info.selected_script_backend_name : "";
        return result;
    }

    [[nodiscard]] bool rootPath(char* out_path, size_t out_path_size) const noexcept
    {
        return api_ != nullptr && api_->project_root_path != nullptr && out_path != nullptr &&
               api_->project_root_path(api_->api_user_data, out_path, out_path_size) != 0;
    }

    [[nodiscard]] std::string rootPath() const
    {
        if (api_ == nullptr || api_->project_root_path == nullptr) {
            return {};
        }

        std::array<char, 1024> buffer{};
        if (api_->project_root_path(api_->api_user_data, buffer.data(), buffer.size()) == 0) {
            return {};
        }
        return buffer.data();
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
