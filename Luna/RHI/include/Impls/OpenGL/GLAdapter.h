#ifndef CACAO_GLADAPTER_H
#define CACAO_GLADAPTER_H
#include "Adapter.h"
#include "GLCommon.h"

namespace Cacao
{
    class CACAO_API GLAdapter final : public Adapter
    {
    public:
        GLAdapter(const std::string& renderer, const std::string& vendor,
                  int glMajor, int glMinor);
        static Ref<GLAdapter> Create(const std::string& renderer, const std::string& vendor,
                                     int glMajor, int glMinor);

        AdapterProperties GetProperties() const override;
        AdapterType GetAdapterType() const override;
        bool IsFeatureSupported(DeviceFeature feature) const override;
        DeviceLimits QueryLimits() const override;
        Ref<Device> CreateDevice(const DeviceCreateInfo& info) override;
        uint32_t FindQueueFamilyIndex(QueueType type) const override;

    private:
        std::string m_renderer;
        std::string m_vendor;
        int m_glMajor;
        int m_glMinor;
    };
}

#endif
