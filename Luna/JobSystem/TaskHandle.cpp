#include "JobSystem/TaskHandle.h"
#include "JobSystem/TaskHandleState.h"
#include "JobSystem/TaskSystem.h"

#include <utility>
#include <vector>

namespace luna {

TaskHandle::TaskHandle(std::shared_ptr<detail::TaskCompletionState> state)
    : m_state(std::move(state))
{}

bool TaskHandle::isValid() const
{
    return static_cast<bool>(m_state);
}

bool TaskHandle::isComplete() const
{
    return !m_state || m_state->GetIsComplete();
}

bool TaskHandle::hasFailed() const
{
    return m_state && m_state->submitFailed();
}

TaskStatus TaskHandle::status() const
{
    if (!m_state) {
        return TaskStatus::Invalid;
    }

    if (!m_state->GetIsComplete()) {
        return TaskStatus::Pending;
    }

    if (m_state->submitFailed()) {
        return TaskStatus::Failed;
    }

    return TaskStatus::Completed;
}

void TaskHandle::wait(TaskSystem& task_system) const
{
    if (m_state) {
        task_system.waitForTask(m_state.get());
    }
}

TaskHandle TaskHandle::then(TaskSystem& task_system, std::function<void()> function, TaskSubmitDesc desc) const
{
    if (!isValid()) {
        return task_system.submit(std::move(function), desc);
    }

    return task_system.submit(std::move(function), std::vector<TaskHandle>{*this}, desc);
}

TaskHandle TaskHandle::thenParallel(TaskSystem& task_system,
                                    std::function<void(enki::TaskSetPartition, uint32_t)> function,
                                    TaskSubmitDesc desc) const
{
    if (!isValid()) {
        return task_system.submitParallel(std::move(function), desc);
    }

    return task_system.submitParallel(std::move(function), std::vector<TaskHandle>{*this}, desc);
}

} // namespace luna
