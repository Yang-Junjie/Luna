#pragma once

#include "Luna/Editor/Native/NativeAssets.h"
#include "Luna/Editor/Native/NativeCommands.h"
#include "Luna/Editor/Native/NativeLog.h"
#include "Luna/Editor/Native/NativeMenus.h"
#include "Luna/Editor/Native/NativePluginAssets.h"
#include "Luna/Editor/Native/NativeProject.h"
#include "Luna/Editor/Native/NativeRuntimeViewport.h"
#include "Luna/Editor/Native/NativeScene.h"
#include "Luna/Editor/Native/NativeSelection.h"
#include "Luna/Editor/Native/NativeUi.h"
#include "Luna/Editor/Native/NativeViewport.h"
#include "Luna/Editor/Native/NativeWindows.h"

namespace luna::editor::native {

class Host final {
public:
    constexpr Host() noexcept = default;

    explicit constexpr Host(const LunaEditorHostApi* api) noexcept
        : api_(api)
    {}

    [[nodiscard]] bool valid() const noexcept
    {
        return api_ != nullptr && api_->struct_size >= sizeof(LunaEditorHostApi) &&
               api_->api_version == LUNA_EDITOR_HOST_API_VERSION;
    }

    [[nodiscard]] Log log() const noexcept
    {
        return Log(api_ != nullptr ? &api_->log : nullptr);
    }

    [[nodiscard]] Ui ui() const noexcept
    {
        return Ui(api_ != nullptr ? &api_->ui : nullptr);
    }

    [[nodiscard]] Commands commands() const noexcept
    {
        return Commands(api_ != nullptr ? &api_->commands : nullptr);
    }

    [[nodiscard]] Windows windows() const noexcept
    {
        return Windows(api_ != nullptr ? &api_->windows : nullptr);
    }

    [[nodiscard]] Menus menus() const noexcept
    {
        return Menus(api_ != nullptr ? &api_->menus : nullptr);
    }

    [[nodiscard]] Project project() const noexcept
    {
        return Project(api_ != nullptr ? &api_->project : nullptr);
    }

    [[nodiscard]] Assets assets() const noexcept
    {
        return Assets(api_ != nullptr ? &api_->assets : nullptr);
    }

    [[nodiscard]] PluginAssets pluginAssets() const noexcept
    {
        return PluginAssets(api_ != nullptr ? &api_->plugin_assets : nullptr);
    }

    [[nodiscard]] Scene scene() const noexcept
    {
        return Scene(api_ != nullptr ? &api_->scene : nullptr);
    }

    [[nodiscard]] Selection selection() const noexcept
    {
        return Selection(api_ != nullptr ? &api_->selection : nullptr);
    }

    [[nodiscard]] Viewport viewport() const noexcept
    {
        return Viewport(api_ != nullptr ? &api_->viewport : nullptr);
    }

    [[nodiscard]] RuntimeViewport runtimeViewport() const noexcept
    {
        return RuntimeViewport(api_ != nullptr ? &api_->runtime_viewport : nullptr);
    }

    [[nodiscard]] const LunaEditorHostApi* native() const noexcept
    {
        return api_;
    }

private:
    const LunaEditorHostApi* api_{};
};

} // namespace luna::editor::native
