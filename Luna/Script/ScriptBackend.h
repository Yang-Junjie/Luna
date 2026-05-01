#pragma once

#include "ScriptRuntime.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace luna {

struct ScriptBackendDescriptor {
    std::string name;
    std::string display_name;
    std::string language;
    std::vector<std::string> supported_extensions;
    bool built_in{false};
    std::filesystem::path plugin_path;
};

class IScriptBackend {
public:
    virtual ~IScriptBackend() = default;

    virtual const ScriptBackendDescriptor& descriptor() const noexcept = 0;
    virtual std::unique_ptr<IScriptRuntime> createRuntime() const = 0;
};

} // namespace luna
