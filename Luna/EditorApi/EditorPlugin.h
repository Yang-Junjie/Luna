#pragma once

#include <filesystem>
#include <string>

namespace luna::editor {

class Host;
class Ui;

struct PluginDescriptor {
    std::string id;
    std::string display_name;
    std::string version;
    std::filesystem::path root_path;
};

class Plugin {
public:
    virtual ~Plugin() = default;

    [[nodiscard]] virtual PluginDescriptor descriptor() const = 0;
    virtual bool onLoad(Host& host) = 0;

    virtual void onUnload(Host&) {}

    virtual void onUpdate(Host&, float) {}

    virtual void onDrawUi(Host&, Ui&) {}
};

} // namespace luna::editor
