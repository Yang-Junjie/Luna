#ifndef CACAO_GLSYNCHRONIZATION_H
#define CACAO_GLSYNCHRONIZATION_H
#include "Synchronization.h"
#include "GLCommon.h"
#include <vector>

namespace Cacao
{
    class CACAO_API GLSynchronization final : public Synchronization
    {
    public:
        GLSynchronization(uint32_t maxFramesInFlight);
        static Ref<GLSynchronization> Create(uint32_t maxFramesInFlight);
        ~GLSynchronization() override;

        void WaitForFrame(uint32_t frameIndex) const override;
        void ResetFrameFence(uint32_t frameIndex) const override;
        uint32_t AcquireNextImageIndex(const Ref<Swapchain>& swapchain, uint32_t frameIndex) const override;
        uint32_t GetMaxFramesInFlight() const override;
        void WaitIdle() const override;

        void SignalFrame(uint32_t frameIndex);

    private:
        uint32_t m_maxFramesInFlight;
        mutable std::vector<GLsync> m_fences;
    };
}

#endif
