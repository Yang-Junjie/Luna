#include "Impls/OpenGL/GLSynchronization.h"

namespace luna::RHI {
GLSynchronization::GLSynchronization(uint32_t maxFramesInFlight)
    : m_maxFramesInFlight(maxFramesInFlight)
{
    m_fences.resize(maxFramesInFlight, nullptr);
}

Ref<GLSynchronization> GLSynchronization::Create(uint32_t maxFramesInFlight)
{
    return std::make_shared<GLSynchronization>(maxFramesInFlight);
}

GLSynchronization::~GLSynchronization()
{
    for (auto& fence : m_fences) {
        if (fence) {
            glDeleteSync(fence);
        }
    }
}

void GLSynchronization::WaitForFrame(uint32_t frameIndex) const
{
    if (frameIndex >= m_maxFramesInFlight) {
        return;
    }
    auto& fence = m_fences[frameIndex];
    if (!fence) {
        return;
    }

    GLenum result = GL_UNSIGNALED;
    while (result != GL_ALREADY_SIGNALED && result != GL_CONDITION_SATISFIED) {
        result = glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 1'000'000);
    }

    glDeleteSync(fence);
    fence = nullptr;
}

void GLSynchronization::ResetFrameFence(uint32_t frameIndex) const
{
    if (frameIndex >= m_maxFramesInFlight) {
        return;
    }
    auto& fence = m_fences[frameIndex];
    if (fence) {
        glDeleteSync(fence);
        fence = nullptr;
    }
}

uint32_t GLSynchronization::AcquireNextImageIndex(const Ref<Swapchain>&, uint32_t) const
{
    return 0;
}

void GLSynchronization::SignalFrame(uint32_t frameIndex)
{
    if (frameIndex >= m_maxFramesInFlight) {
        return;
    }
    auto& fence = m_fences[frameIndex];
    if (fence) {
        glDeleteSync(fence);
    }
    fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

uint32_t GLSynchronization::GetMaxFramesInFlight() const
{
    return m_maxFramesInFlight;
}

void GLSynchronization::WaitIdle() const
{
    glFinish();
}
} // namespace luna::RHI
