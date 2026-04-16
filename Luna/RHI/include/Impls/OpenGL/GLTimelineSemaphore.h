#ifndef CACAO_GLTIMELINESEMAPHORE_H
#define CACAO_GLTIMELINESEMAPHORE_H
#include "GLCommon.h"
#include "Synchronization.h"

#include <map>
#include <mutex>

namespace Cacao {
class CACAO_API GLTimelineSemaphore final : public TimelineSemaphore {
public:
    GLTimelineSemaphore(uint64_t initialValue = 0);
    ~GLTimelineSemaphore() override;

    void Signal(uint64_t value) override;
    bool Wait(uint64_t value, uint64_t timeoutNs = UINT64_MAX) override;

    uint64_t GetValue() const override
    {
        return m_currentValue;
    }

private:
    uint64_t m_currentValue;
    std::map<uint64_t, GLsync> m_fences;
    mutable std::mutex m_mutex;
};
} // namespace Cacao

#endif
