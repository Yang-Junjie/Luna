#ifndef CACAO_MTL_INSTANCE_H
#define CACAO_MTL_INSTANCE_H

#ifdef __APPLE__

#include "Instance.h"

namespace Cacao {
class CACAO_API MTLInstance : public Instance {
private:
    InstanceCreateInfo m_createInfo;

public:
    ~MTLInstance() override = default;

    [[nodiscard]] BackendType GetType() const override;
    bool Initialize(const InstanceCreateInfo& createInfo) override;
    std::vector<Ref<Adapter>> EnumerateAdapters() override;
    bool IsFeatureEnabled(InstanceFeature feature) const override;
    Ref<Surface> CreateSurface(const NativeWindowHandle& windowHandle) override;
    Ref<ShaderCompiler> CreateShaderCompiler() override;
};
} // namespace Cacao

#endif // __APPLE__
#endif
