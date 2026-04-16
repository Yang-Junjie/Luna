#ifndef CACAO_WGPU_QUEUE_H
#define CACAO_WGPU_QUEUE_H

#include "WGPUCommon.h"
#include "Queue.h"
#include "Synchronization.h"
#include "Surface.h"

namespace Cacao
{
    class CACAO_API WGPUQueueImpl final : public Queue
    {
    private:
        ::WGPUQueue m_queue = nullptr;
        ::WGPUDevice m_device = nullptr;
    public:
        WGPUQueueImpl(::WGPUQueue queue, ::WGPUDevice device);
        ~WGPUQueueImpl() override = default;

        void Submit(const Ref<CommandBufferEncoder>& encoder) override;
        void Submit(std::span<const Ref<CommandBufferEncoder>> encoders) override;
        void Submit(const Ref<CommandBufferEncoder>& encoder,
                    const Ref<Synchronization>& sync, uint32_t frameIndex) override;
        void WaitIdle() override;

        ::WGPUQueue GetNativeQueue() const { return m_queue; }
    };

    class CACAO_API WGPUSynchronization : public Synchronization
    {
    private:
        uint32_t m_maxFramesInFlight;
    public:
        WGPUSynchronization(uint32_t maxFramesInFlight);
        ~WGPUSynchronization() override = default;

        void WaitForFrame(uint32_t frameIndex) override;
        void ResetFrameFence(uint32_t frameIndex) override;
        void WaitIdle() override;
    };

    class CACAO_API WGPUSurfaceImpl : public Surface
    {
    private:
        ::WGPUSurface m_surface = nullptr;
    public:
        WGPUSurfaceImpl(const NativeWindowHandle& handle);
        ~WGPUSurfaceImpl() override;

        SurfaceCapabilities GetCapabilities(const Ref<Adapter>& adapter) override;
        Ref<Queue> GetPresentQueue(const Ref<Device>& device) override;

        ::WGPUSurface GetNativeSurface() const { return m_surface; }
    };
}

#endif
