#ifdef __APPLE__

#include "Impls/Metal/MTLInstance.h"
#include "Impls/Metal/MTLAdapter.h"
#include "Impls/Metal/MTLQueue.h"
#include "ShaderCompiler.h"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <iostream>

namespace luna::RHI
{
    BackendType MTLInstance::GetType() const { return BackendType::Metal; }

    bool MTLInstance::Initialize(const InstanceCreateInfo& createInfo)
    {
        m_createInfo = createInfo;
        return true;
    }

    std::vector<Ref<Adapter>> MTLInstance::EnumerateAdapters()
    {
        auto self = std::dynamic_pointer_cast<MTLInstance>(shared_from_this());
        std::vector<Ref<Adapter>> adapters;

        NSArray<id<MTLDevice>>* devices = MTLCopyAllDevices();
        if (devices.count == 0)
        {
            id<MTLDevice> defaultDevice = MTLCreateSystemDefaultDevice();
            if (defaultDevice)
                adapters.push_back(std::make_shared<MTLAdapter>(self, defaultDevice));
        }
        else
        {
            for (id<MTLDevice> device in devices)
                adapters.push_back(std::make_shared<MTLAdapter>(self, device));
        }
        return adapters;
    }

    bool MTLInstance::IsFeatureEnabled(InstanceFeature feature) const
    {
        for (auto& f : m_createInfo.enabledFeatures)
            if (f == feature) return true;
        return false;
    }

    Ref<Surface> MTLInstance::CreateSurface(const NativeWindowHandle& windowHandle)
    {
        if (!windowHandle.IsValid())
            throw std::runtime_error("Invalid NativeWindowHandle for Metal surface");
        return std::make_shared<MTLSurface>(windowHandle);
    }

    Ref<ShaderCompiler> MTLInstance::CreateShaderCompiler()
    {
        return ShaderCompiler::Create(BackendType::Metal);
    }
}

#endif // __APPLE__
