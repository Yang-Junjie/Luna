#include "Impls/WebGPU/WGPUAdapter.h"
#include "Impls/WebGPU/WGPUInstance.h"
#include "Impls/WebGPU/WGPUQueue.h"
#include "Logging.h"
#include "ShaderCompiler.h"

namespace luna::RHI {
WGPUInstance::~WGPUInstance()
{
    if (m_instance) {
        wgpuInstanceRelease(m_instance);
    }
}

BackendType WGPUInstance::GetType() const
{
    return BackendType::WebGPU;
}

bool WGPUInstance::Initialize(const InstanceCreateInfo& createInfo)
{
    m_createInfo = createInfo;

    WGPUInstanceDescriptor desc = {};
    m_instance = wgpuCreateInstance(&desc);
    if (!m_instance) {
        LogMessage(LogLevel::Error, "Failed to create WebGPU instance");
        return false;
    }
    return true;
}

std::vector<Ref<Adapter>> WGPUInstance::EnumerateAdapters()
{
    auto self = std::dynamic_pointer_cast<WGPUInstance>(shared_from_this());

    struct AdapterResult {
        ::WGPUAdapter adapter = nullptr;
        bool done = false;
    };

    AdapterResult result;

    WGPURequestAdapterOptions options = {};
    options.powerPreference = WGPUPowerPreference_HighPerformance;

    wgpuInstanceRequestAdapter(
        m_instance,
        &options,
        [](WGPURequestAdapterStatus status, ::WGPUAdapter adapter, const char* message, void* userdata) {
            auto* res = static_cast<AdapterResult*>(userdata);
            if (status == WGPURequestAdapterStatus_Success) {
                res->adapter = adapter;
            } else {
                LogMessage(LogLevel::Error,
                           std::string("WebGPU adapter request failed: ") + (message ? message : "unknown"));
            }
            res->done = true;
        },
        &result);

    // Dawn processes callbacks synchronously on the main thread
    if (!result.adapter) {
        return {};
    }

    return {std::make_shared<WGPUAdapter>(self, result.adapter)};
}

bool WGPUInstance::IsFeatureEnabled(InstanceFeature feature) const
{
    for (auto& f : m_createInfo.enabledFeatures) {
        if (f == feature) {
            return true;
        }
    }
    return false;
}

Ref<Surface> WGPUInstance::CreateSurface(const NativeWindowHandle& windowHandle)
{
    if (!windowHandle.IsValid()) {
        throw std::runtime_error("Invalid NativeWindowHandle for WebGPU surface");
    }

    return std::make_shared<WGPUSurfaceImpl>(windowHandle);
}

Ref<ShaderCompiler> WGPUInstance::CreateShaderCompiler()
{
    return ShaderCompiler::Create(BackendType::WebGPU);
}
} // namespace luna::RHI
