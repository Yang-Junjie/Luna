#include "Impls/OpenGL/GLTimelineSemaphore.h"

namespace luna::RHI {
GLTimelineSemaphore::GLTimelineSemaphore(uint64_t initialValue)
    : m_currentValue(initialValue)
{}

GLTimelineSemaphore::~GLTimelineSemaphore()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [val, fence] : m_fences) {
        if (fence) {
            glDeleteSync(fence);
        }
    }
}

void GLTimelineSemaphore::Signal(uint64_t value)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    m_fences[value] = fence;
    m_currentValue = value;
}

bool GLTimelineSemaphore::Wait(uint64_t value, uint64_t timeoutNs)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_fences.find(value);
    if (it == m_fences.end() || !it->second) {
        return true;
    }

    GLenum result = GL_UNSIGNALED;
    while (result != GL_ALREADY_SIGNALED && result != GL_CONDITION_SATISFIED) {
        result = glClientWaitSync(it->second, GL_SYNC_FLUSH_COMMANDS_BIT, timeoutNs);
        if (result == GL_WAIT_FAILED) {
            break;
        }
    }

    glDeleteSync(it->second);
    it->second = nullptr;

    // Clean up all fences up to this value
    for (auto jt = m_fences.begin(); jt != it;) {
        if (jt->second) {
            glDeleteSync(jt->second);
        }
        jt = m_fences.erase(jt);
    }
    m_fences.erase(it);
    return result != GL_WAIT_FAILED;
}
} // namespace luna::RHI
