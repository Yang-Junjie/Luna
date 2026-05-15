#pragma once

#include "Luna/Editor/Native/NativeTypes.h"

namespace luna::editor::native {

class Log final {
public:
    constexpr Log() noexcept = default;
    explicit constexpr Log(const LunaEditorLogApi* api) noexcept
        : api_(api)
    {
    }

    [[nodiscard]] bool available() const noexcept
    {
        return api_ != nullptr && api_->log != nullptr;
    }

    void write(LunaEditorLogLevel level, const char* message) const noexcept
    {
        if (available()) {
            api_->log(api_->api_user_data, level, message != nullptr ? message : "");
        }
    }

    void trace(const char* message) const noexcept
    {
        write(LunaEditorLogLevel_Trace, message);
    }

    void info(const char* message) const noexcept
    {
        write(LunaEditorLogLevel_Info, message);
    }

    void warn(const char* message) const noexcept
    {
        write(LunaEditorLogLevel_Warn, message);
    }

    void error(const char* message) const noexcept
    {
        write(LunaEditorLogLevel_Error, message);
    }

private:
    const LunaEditorLogApi* api_{};
};

} // namespace luna::editor::native
