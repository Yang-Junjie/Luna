#ifndef CACAO_GLINSTANCE_H
#define CACAO_GLINSTANCE_H
#include "GLCommon.h"

#include <Instance.h>

namespace Cacao {
class CACAO_API GLInstance : public Instance {
public:
    [[nodiscard]] BackendType GetType() const override;
    bool Initialize(const InstanceCreateInfo& createInfo) override;
    std::vector<Ref<Adapter>> EnumerateAdapters() override;
    bool IsFeatureEnabled(InstanceFeature feature) const override;
    Ref<Surface> CreateSurface(const NativeWindowHandle& windowHandle) override;
    Ref<ShaderCompiler> CreateShaderCompiler() override;

private:
    InstanceCreateInfo m_createInfo;
    int m_glMajor = 0;
    int m_glMinor = 0;
    std::string m_renderer;
    std::string m_vendor;
};
} // namespace Cacao

#endif
