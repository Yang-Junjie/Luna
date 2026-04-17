#ifndef LUNA_RHI_MTL_INSTANCE_H
#define LUNA_RHI_MTL_INSTANCE_H

#ifdef __APPLE__

#include "Instance.h"

namespace luna::RHI {
class LUNA_RHI_API MTLInstance : public Instance {
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
} // namespace luna::RHI

#endif // __APPLE__
#endif
