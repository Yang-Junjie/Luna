#pragma once

#include "JobSystem/TaskHandle.h"
#include "TaskScheduler.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace luna {

class TaskSystem;

namespace detail {

struct PendingLaunch;

struct ManagedTaskBase {
    virtual ~ManagedTaskBase() = default;

    virtual enki::ICompletable* completable() = 0;
    virtual const enki::ICompletable* completable() const = 0;
    virtual bool schedule(TaskSystem& task_system) = 0;
};

struct TaskCompletionState final : enki::ICompletable {
    explicit TaskCompletionState(TaskTarget target_in);

    void bind(TaskSystem& task_system, const enki::ICompletable* dependency);
    void addPendingLaunch(const std::shared_ptr<PendingLaunch>& launch);
    void failSubmission();
    bool isFinished() const;
    bool submitFailed() const;
    TaskTarget target() const;

protected:
    // when all dependencies are complete, this will be automatically called by task system
    void OnDependenciesComplete(enki::TaskScheduler* scheduler, uint32_t thread_number) override;

private:
    TaskSystem* m_task_system = nullptr;
    enki::Dependency m_dependency;
    std::mutex m_pending_launches_mutex;
    std::vector<std::shared_ptr<PendingLaunch>> m_pending_launches;
    std::atomic<bool> m_finished = true;
    std::atomic<bool> m_submit_failed = false;
    TaskTarget m_target = TaskTarget::Worker;
};

struct PendingLaunch {
    PendingLaunch(TaskSystem& task_system,
                  std::shared_ptr<ManagedTaskBase> task_state,
                  std::shared_ptr<TaskCompletionState> completion_state,
                  uint32_t dependency_count);

    void dependencyResolved(bool dependency_failed);

private:
    TaskSystem* m_task_system = nullptr;
    std::shared_ptr<ManagedTaskBase> m_task_state;
    std::shared_ptr<TaskCompletionState> m_completion_state;
    std::atomic<uint32_t> m_remaining_dependencies = 0;
    std::atomic<bool> m_dependency_failed = false;
    std::atomic<bool> m_scheduled = false;
};

struct ManagedTaskRecord {
    std::shared_ptr<ManagedTaskBase> task_state;
    std::shared_ptr<TaskCompletionState> completion_state;
};

} // namespace detail
} // namespace luna
