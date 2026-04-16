#include "Impls/WebGPU/WGPUQueue.h"
#include "Impls/WebGPU/WGPUCommandBufferEncoder.h"
#include <vector>

namespace Cacao
{
    WGPUQueueImpl::WGPUQueueImpl(::WGPUQueue queue, ::WGPUDevice device)
        : m_queue(queue), m_device(device) {}

    void WGPUQueueImpl::Submit(const Ref<CommandBufferEncoder>& encoder)
    {
        auto* wgpuEnc = static_cast<WGPUCommandBufferEncoder*>(encoder.get());
        ::WGPUCommandBuffer cb = wgpuEnc->GetFinishedCommandBuffer();
        if (cb) {
            wgpuQueueSubmit(m_queue, 1, &cb);
        }
    }

    void WGPUQueueImpl::Submit(std::span<const Ref<CommandBufferEncoder>> encoders)
    {
        std::vector<::WGPUCommandBuffer> buffers;
        buffers.reserve(encoders.size());
        for (auto& enc : encoders)
        {
            auto* wgpuEnc = static_cast<WGPUCommandBufferEncoder*>(enc.get());
            ::WGPUCommandBuffer cb = wgpuEnc->GetFinishedCommandBuffer();
            if (cb) buffers.push_back(cb);
        }
        if (!buffers.empty()) {
            wgpuQueueSubmit(m_queue, static_cast<uint32_t>(buffers.size()), buffers.data());
        }
    }

    void WGPUQueueImpl::Submit(const Ref<CommandBufferEncoder>& encoder,
                               const Ref<Synchronization>& sync, uint32_t frameIndex)
    {
        Submit(encoder);
    }

    void WGPUQueueImpl::WaitIdle()
    {
        // WebGPU requires polling; use onSubmittedWorkDone with a blocking wait
        struct WaitData { bool done = false; };
        WaitData data;
        wgpuQueueOnSubmittedWorkDone(m_queue, [](WGPUQueueWorkDoneStatus status, void* userdata) {
            static_cast<WaitData*>(userdata)->done = true;
        }, &data);
        while (!data.done) {
            wgpuDeviceTick(m_device);
        }
    }

    WGPUSynchronization::WGPUSynchronization(uint32_t maxFramesInFlight)
        : m_maxFramesInFlight(maxFramesInFlight) {}

    void WGPUSynchronization::WaitForFrame(uint32_t frameIndex)
    {
        // WebGPU has no per-frame fences; synchronization is implicit via queue ordering
    }

    void WGPUSynchronization::ResetFrameFence(uint32_t frameIndex)
    {
        // No-op: WebGPU manages synchronization automatically
    }

    void WGPUSynchronization::WaitIdle()
    {
        // No-op: WaitIdle is handled at the queue level
    }

    WGPUSurfaceImpl::WGPUSurfaceImpl(const NativeWindowHandle& handle)
    {
#ifdef _WIN32
        WGPUSurfaceDescriptorFromWindowsHWND hwndDesc = {};
        hwndDesc.chain.sType = WGPUSType_SurfaceDescriptorFromWindowsHWND;
        hwndDesc.hinstance = handle.hInst;
        hwndDesc.hwnd = handle.hWnd;
        WGPUSurfaceDescriptor surfDesc = {};
        surfDesc.nextInChain = &hwndDesc.chain;
        // Surface creation requires a WGPUInstance; deferred to device/adapter creation
#endif
    }

    WGPUSurfaceImpl::~WGPUSurfaceImpl()
    {
        if (m_surface) {
            wgpuSurfaceRelease(m_surface);
            m_surface = nullptr;
        }
    }

    SurfaceCapabilities WGPUSurfaceImpl::GetCapabilities(const Ref<Adapter>& adapter)
    {
        SurfaceCapabilities caps;
        caps.minImageCount = 2;
        caps.maxImageCount = 3;
        caps.currentExtent = {800, 600};
        caps.currentTransform = SurfaceTransform::Identity;
        return caps;
    }

    Ref<Queue> WGPUSurfaceImpl::GetPresentQueue(const Ref<Device>& device)
    {
        return device->GetQueue(QueueType::Graphics, 0);
    }
}
