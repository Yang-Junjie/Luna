#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace luna::render {

struct RenderGraphBuildInfo {
    uint32_t m_framebuffer_width{0};
    uint32_t m_framebuffer_height{0};
    bool m_include_imgui{false};
};

enum class RenderGraphPassKind {
    Scene,
    ClearColor,
    ImGui,
};

struct RenderGraphPassSpec {
    std::string m_name;
    RenderGraphPassKind m_kind{RenderGraphPassKind::Scene};
};

class RenderGraphDefinition {
public:
    RenderGraphDefinition& addScenePass(std::string name = "scene")
    {
        m_passes.push_back(RenderGraphPassSpec{
            .m_name = std::move(name),
            .m_kind = RenderGraphPassKind::Scene,
        });
        return *this;
    }

    RenderGraphDefinition& addClearColorPass(std::string name = "scene_clear")
    {
        m_passes.push_back(RenderGraphPassSpec{
            .m_name = std::move(name),
            .m_kind = RenderGraphPassKind::ClearColor,
        });
        return *this;
    }

    RenderGraphDefinition& addImGuiPass(std::string name = "imgui")
    {
        m_passes.push_back(RenderGraphPassSpec{
            .m_name = std::move(name),
            .m_kind = RenderGraphPassKind::ImGui,
        });
        return *this;
    }

    const std::vector<RenderGraphPassSpec>& passes() const
    {
        return m_passes;
    }

private:
    std::vector<RenderGraphPassSpec> m_passes;
};

using RenderGraphFactory = std::function<RenderGraphDefinition(const RenderGraphBuildInfo&)>;

} // namespace luna::render
