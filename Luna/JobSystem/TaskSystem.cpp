#include "JobSystem/TaskSystem.h"

#include "Core/Log.h"
#include "JobSystem/TaskHandleState.h"

#include <algorithm>
#include <cassert>
#include <system_error>

namespace luna {

namespace {

bool reportTaskSystemFailure(const char* message)
{
    if (Logger::isInitialized()) {
        LUNA_CORE_ERROR("{}", message);
    }

    assert(false && "TaskSystem misuse");
    return false;
}

} // namespace

namespace detail {

struct FunctionTask final : ManagedTaskBase, enki::ITaskSet {
    explicit FunctionTask(std::function<void()> function_in, const TaskSubmitDesc& desc)
        : function(std::move(function_in))
    {
        m_Priority = desc.priority;
        m_SetSize = 1;
        m_MinRange = 1;
    }

    enki::ICompletable* completable() override
    {
        return this;
    }

    const enki::ICompletable* completable() const override
    {
        return this;
    }

    bool schedule(TaskSystem& task_system) override
    {
        return task_system.submitTask(this);
    }

    void ExecuteRange(enki::TaskSetPartition range, uint32_t) override
    {
        if (range.start == 0 && function) {
            function();
        }
    }

    std::function<void()> function;
};

struct ParallelFunctionTask final : ManagedTaskBase, enki::ITaskSet {
    explicit ParallelFunctionTask(std::function<void(enki::TaskSetPartition, uint32_t)> function_in,
                                  const TaskSubmitDesc& desc)
        : function(std::move(function_in))
    {
        m_Priority = desc.priority;
        m_SetSize = std::max(desc.set_size, 1u);
        m_MinRange = std::max(desc.min_range, 1u);
    }

    enki::ICompletable* completable() override
    {
        return this;
    }

    const enki::ICompletable* completable() const override
    {
        return this;
    }

    bool schedule(TaskSystem& task_system) override
    {
        return task_system.submitTask(this);
    }

    void ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) override
    {
        if (function) {
            function(range, threadnum);
        }
    }

    std::function<void(enki::TaskSetPartition, uint32_t)> function;
};

struct PinnedFunctionTask final : ManagedTaskBase, enki::IPinnedTask {
    explicit PinnedFunctionTask(std::function<void()> function_in, const TaskSubmitDesc& desc)
        : function(std::move(function_in))
        , target(desc.target)
    {
        m_Priority = desc.priority;
    }

    enki::ICompletable* completable() override
    {
        return this;
    }

    const enki::ICompletable* completable() const override
    {
        return this;
    }

    bool schedule(TaskSystem& task_system) override
    {
        if (target == TaskTarget::MainThread) {
            return task_system.submitMainThreadJob(this);
        }

        return task_system.submitIOJob(this);
    }

    void Execute() override
    {
        if (function) {
            function();
        }
    }

    std::function<void()> function;
    TaskTarget target = TaskTarget::MainThread;
};

TaskCompletionState::TaskCompletionState(TaskTarget target_in)
    : m_target(target_in)
{
}

void TaskCompletionState::bind(TaskSystem& task_system, const enki::ICompletable* dependency)
{
    assert(dependency != nullptr);

    m_task_system = &task_system;
    m_submit_failed.store(false, std::memory_order_release);
    m_finished.store(false, std::memory_order_release);
    SetDependency(m_dependency, dependency);
}

void TaskCompletionState::addPendingLaunch(const std::shared_ptr<PendingLaunch>& launch)
{
    if (!launch) {
        return;
    }

    if (m_finished.load(std::memory_order_acquire)) {
        launch->dependencyResolved(m_submit_failed.load(std::memory_order_acquire));
        return;
    }

    std::lock_guard<std::mutex> lock(m_pending_launches_mutex);
    if (m_finished.load(std::memory_order_relaxed)) {
        launch->dependencyResolved(m_submit_failed.load(std::memory_order_relaxed));
        return;
    }

    m_pending_launches.push_back(launch);
}

void TaskCompletionState::failSubmission()
{
    m_submit_failed.store(true, std::memory_order_release);

    std::vector<std::shared_ptr<PendingLaunch>> pending_launches;
    {
        std::lock_guard<std::mutex> lock(m_pending_launches_mutex);
        m_finished.store(true, std::memory_order_release);
        pending_launches.swap(m_pending_launches);
    }

    for (const auto& launch : pending_launches) {
        if (launch) {
            launch->dependencyResolved(true);
        }
    }
}

bool TaskCompletionState::isFinished() const
{
    return m_finished.load(std::memory_order_acquire);
}

bool TaskCompletionState::submitFailed() const
{
    return m_submit_failed.load(std::memory_order_acquire);
}

TaskTarget TaskCompletionState::target() const
{
    return m_target;
}

void TaskCompletionState::OnDependenciesComplete(enki::TaskScheduler* scheduler, uint32_t thread_number)
{
    std::vector<std::shared_ptr<PendingLaunch>> pending_launches;
    {
        std::lock_guard<std::mutex> lock(m_pending_launches_mutex);
        m_finished.store(true, std::memory_order_release);
        pending_launches.swap(m_pending_launches);
    }

    enki::ICompletable::OnDependenciesComplete(scheduler, thread_number);

    for (const auto& launch : pending_launches) {
        if (launch) {
            launch->dependencyResolved(false);
        }
    }
}

PendingLaunch::PendingLaunch(TaskSystem& task_system,
                             std::shared_ptr<ManagedTaskBase> task_state,
                             std::shared_ptr<TaskCompletionState> completion_state,
                             uint32_t dependency_count)
    : m_task_system(&task_system)
    , m_task_state(std::move(task_state))
    , m_completion_state(std::move(completion_state))
    , m_remaining_dependencies(dependency_count)
{
}

void PendingLaunch::dependencyResolved(bool dependency_failed)
{
    if (dependency_failed) {
        m_dependency_failed.store(true, std::memory_order_release);
    }

    const uint32_t previous = m_remaining_dependencies.fetch_sub(1, std::memory_order_acq_rel);
    assert(previous > 0);

    if (previous != 1) {
        return;
    }

    if (m_scheduled.exchange(true, std::memory_order_acq_rel)) {
        return;
    }

    if (!m_task_system || !m_task_state || !m_completion_state) {
        if (m_completion_state) {
            m_completion_state->failSubmission();
        }
        return;
    }

    if (m_dependency_failed.load(std::memory_order_acquire)) {
        m_completion_state->failSubmission();
        return;
    }

    if (!m_task_state->schedule(*m_task_system)) {
        m_completion_state->failSubmission();
    }
}

uint32_t computeWorkerThreadCount(const TaskSystemConfig& config)
{
    if (config.worker_thread_count > 0) {
        return config.worker_thread_count;
    }

    const uint32_t hardware_threads = std::max(enki::GetNumHardwareThreads(), 1u);
    const uint32_t reserved_threads = 1 + config.io_thread_count + config.external_thread_count;

    if (hardware_threads > reserved_threads) {
        return hardware_threads - reserved_threads;
    }

    return 1;
}

} // namespace detail

TaskSystem::ExternalThreadScope::~ExternalThreadScope()
{
    reset();
}

TaskSystem::ExternalThreadScope::ExternalThreadScope(TaskSystem* task_system)
    : m_task_system(task_system)
{
}

TaskSystem::ExternalThreadScope::ExternalThreadScope(ExternalThreadScope&& other) noexcept
    : m_task_system(other.m_task_system)
{
    other.m_task_system = nullptr;
}

TaskSystem::ExternalThreadScope& TaskSystem::ExternalThreadScope::operator=(ExternalThreadScope&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    reset();
    m_task_system = other.m_task_system;
    other.m_task_system = nullptr;
    return *this;
}

bool TaskSystem::ExternalThreadScope::isRegistered() const
{
    return m_task_system != nullptr;
}

void TaskSystem::ExternalThreadScope::reset()
{
    if (!m_task_system) {
        return;
    }

    m_task_system->deregisterExternalThread();
    m_task_system = nullptr;
}

TaskSystem::~TaskSystem()
{
    shutdown();
}

bool TaskSystem::initialize(const TaskSystemConfig& config)
{
    if (m_initialized) {
        return true;
    }

    m_config = config;
    m_config.worker_thread_count = detail::computeWorkerThreadCount(config);
    m_next_io_thread_index.store(0, std::memory_order_relaxed);
    m_ready_io_threads = 0;
    m_io_startup_failed = false;

    m_scheduler = std::make_unique<enki::TaskScheduler>();

    enki::TaskSchedulerConfig scheduler_config;
    scheduler_config.numTaskThreadsToCreate = m_config.worker_thread_count;
    scheduler_config.numExternalTaskThreads = m_config.io_thread_count + m_config.external_thread_count;

    m_scheduler->Initialize(scheduler_config);

    try {
        for (uint32_t io_index = 0; io_index < m_config.io_thread_count; ++io_index) {
            const uint32_t thread_number = getFirstIOThreadNumber() + io_index;
            m_io_threads.emplace_back([this, thread_number]() {
                ioThreadMain(thread_number);
            });
        }
    } catch (const std::system_error& error) {
        if (Logger::isInitialized()) {
            LUNA_CORE_ERROR("Task system failed to create IO thread: {}", error.what());
        }
        shutdown();
        return false;
    }

    if (m_config.io_thread_count > 0) {
        std::unique_lock<std::mutex> lock(m_io_startup_mutex);
        m_io_startup_cv.wait(lock, [this]() {
            return m_io_startup_failed || m_ready_io_threads == m_config.io_thread_count;
        });

        if (m_io_startup_failed) {
            if (Logger::isInitialized()) {
                LUNA_CORE_ERROR("Task system failed to register one or more IO threads");
            }
            shutdown();
            return false;
        }
    }

    m_initialized = true;

    if (Logger::isInitialized()) {
        LUNA_CORE_INFO("Task system initialized with {} worker threads, {} IO threads, {} external slots ({} total registered threads)",
                       m_config.worker_thread_count,
                       m_config.io_thread_count,
                       m_config.external_thread_count,
                       m_scheduler->GetNumTaskThreads());
    }

    return true;
}

void TaskSystem::shutdown()
{
    if (m_scheduler == nullptr && m_io_threads.empty()) {
        return;
    }

    if (m_scheduler != nullptr) {
        m_scheduler->WaitforAllAndShutdown();
    }

    for (std::thread& io_thread : m_io_threads) {
        if (io_thread.joinable()) {
            io_thread.join();
        }
    }
    m_io_threads.clear();

    {
        std::lock_guard<std::mutex> lock(m_managed_tasks_mutex);
        m_managed_tasks.clear();
    }

    m_scheduler.reset();
    m_next_io_thread_index.store(0, std::memory_order_relaxed);
    m_ready_io_threads = 0;
    m_io_startup_failed = false;
    m_config.worker_thread_count = 0;
    m_config.io_thread_count = 0;
    m_initialized = false;

    if (Logger::isInitialized()) {
        LUNA_CORE_INFO("Task system shutdown");
    }
}

bool TaskSystem::isInitialized() const
{
    return m_initialized;
}

bool TaskSystem::submitTask(enki::ITaskSet* task)
{
    if (!m_scheduler) {
        return reportTaskSystemFailure("TaskSystem::submitTask called before TaskSystem::initialize()");
    }

    if (!task) {
        return reportTaskSystemFailure("TaskSystem::submitTask received a null task");
    }

    if (!requireSchedulerApiThread("TaskSystem::submitTask")) {
        return false;
    }

    m_scheduler->AddTaskSetToPipe(task);
    return true;
}

void TaskSystem::waitForAll()
{
    if (!m_scheduler) {
        reportTaskSystemFailure("TaskSystem::waitForAll called before TaskSystem::initialize()");
        return;
    }

    if (!requireSchedulerApiThread("TaskSystem::waitForAll")) {
        return;
    }

    m_scheduler->WaitforAll();
    reapCompletedManagedTasks();
}

void TaskSystem::waitForTask(const enki::ICompletable* task)
{
    if (!m_scheduler) {
        reportTaskSystemFailure("TaskSystem::waitForTask called before TaskSystem::initialize()");
        return;
    }

    if (!task) {
        reportTaskSystemFailure("TaskSystem::waitForTask received a null task");
        return;
    }

    if (!requireSchedulerApiThread("TaskSystem::waitForTask")) {
        return;
    }

    if (const auto* completion_state = dynamic_cast<const detail::TaskCompletionState*>(task)) {
        while (!completion_state->isFinished()) {
            m_scheduler->WaitforTask(nullptr);
        }

        reapCompletedManagedTasks();

        if (completion_state->submitFailed()) {
            reportTaskSystemFailure("Task wait observed a task that failed to submit");
        }
        return;
    }

    m_scheduler->WaitforTask(task);
    reapCompletedManagedTasks();
}

void TaskSystem::runPinnedTasksForCurrentThread()
{
    if (!m_scheduler) {
        reportTaskSystemFailure("TaskSystem::runPinnedTasksForCurrentThread called before TaskSystem::initialize()");
        return;
    }

    if (!requireSchedulerApiThread("TaskSystem::runPinnedTasksForCurrentThread")) {
        return;
    }

    m_scheduler->RunPinnedTasks();
    reapCompletedManagedTasks();
}

bool TaskSystem::submitIOJob(enki::IPinnedTask* task)
{
    if (!m_scheduler) {
        return reportTaskSystemFailure("TaskSystem::submitIOJob called before TaskSystem::initialize()");
    }

    if (!task) {
        return reportTaskSystemFailure("TaskSystem::submitIOJob received a null task");
    }

    if (m_config.io_thread_count == 0) {
        return reportTaskSystemFailure("TaskSystem::submitIOJob requires at least one configured IO thread");
    }

    const uint32_t io_index = m_next_io_thread_index.fetch_add(1, std::memory_order_relaxed) % m_config.io_thread_count;
    task->threadNum = getFirstIOThreadNumber() + io_index;
    m_scheduler->AddPinnedTask(task);
    return true;
}

bool TaskSystem::submitMainThreadJob(enki::IPinnedTask* task)
{
    if (!m_scheduler) {
        return reportTaskSystemFailure("TaskSystem::submitMainThreadJob called before TaskSystem::initialize()");
    }

    if (!task) {
        return reportTaskSystemFailure("TaskSystem::submitMainThreadJob received a null task");
    }

    task->threadNum = 0;
    m_scheduler->AddPinnedTask(task);
    return true;
}

TaskHandle TaskSystem::submit(std::function<void()> function,
                              TaskSubmitDesc desc,
                              std::initializer_list<TaskHandle> dependencies)
{
    return submit(std::move(function), std::vector<TaskHandle>(dependencies), desc);
}

TaskHandle TaskSystem::submit(std::function<void()> function,
                              const std::vector<TaskHandle>& dependencies,
                              TaskSubmitDesc desc)
{
    std::shared_ptr<detail::ManagedTaskBase> task_state;
    if (desc.target == TaskTarget::Worker) {
        task_state = std::make_shared<detail::FunctionTask>(std::move(function), desc);
    } else {
        task_state = std::make_shared<detail::PinnedFunctionTask>(std::move(function), desc);
    }

    return submitManagedTask(std::move(task_state), desc.target, dependencies);
}

TaskHandle TaskSystem::submitParallel(std::function<void(enki::TaskSetPartition, uint32_t)> function,
                                      TaskSubmitDesc desc,
                                      std::initializer_list<TaskHandle> dependencies)
{
    return submitParallel(std::move(function), std::vector<TaskHandle>(dependencies), desc);
}

TaskHandle TaskSystem::submitParallel(std::function<void(enki::TaskSetPartition, uint32_t)> function,
                                      const std::vector<TaskHandle>& dependencies,
                                      TaskSubmitDesc desc)
{
    if (desc.target != TaskTarget::Worker && Logger::isInitialized()) {
        LUNA_CORE_WARN("Parallel task submissions only support worker execution; forcing TaskTarget::Worker");
    }

    desc.target = TaskTarget::Worker;
    auto task_state = std::make_shared<detail::ParallelFunctionTask>(std::move(function), desc);
    return submitManagedTask(std::move(task_state), desc.target, dependencies);
}

TaskHandle TaskSystem::whenAll(std::initializer_list<TaskHandle> dependencies)
{
    return whenAll(std::vector<TaskHandle>(dependencies));
}

TaskHandle TaskSystem::whenAll(const std::vector<TaskHandle>& dependencies)
{
    if (dependencies.empty()) {
        return submit([]() {});
    }

    if (dependencies.size() == 1) {
        return dependencies.front();
    }

    return submit([]() {}, dependencies);
}

uint32_t TaskSystem::getWorkerThreadCount() const
{
    return m_config.worker_thread_count;
}

uint32_t TaskSystem::getIOThreadCount() const
{
    return m_config.io_thread_count;
}

uint32_t TaskSystem::getExternalThreadCount() const
{
    return m_config.external_thread_count;
}

uint32_t TaskSystem::getFirstIOThreadNumber() const
{
    return enki::TaskScheduler::GetNumFirstExternalTaskThread();
}

bool TaskSystem::isCurrentThreadRegistered() const
{
    return m_scheduler != nullptr && m_scheduler->GetThreadNum() != enki::NO_THREAD_NUM;
}

TaskSystem::ExternalThreadScope TaskSystem::registerExternalThread()
{
    if (!m_scheduler) {
        reportTaskSystemFailure("TaskSystem::registerExternalThread called before TaskSystem::initialize()");
        return {};
    }

    if (m_config.external_thread_count == 0) {
        reportTaskSystemFailure("TaskSystem::registerExternalThread requires TaskSystemConfig::external_thread_count > 0");
        return {};
    }

    if (isCurrentThreadRegistered()) {
        reportTaskSystemFailure("TaskSystem::registerExternalThread was called from a thread that is already registered with enkiTS");
        return {};
    }

    if (!m_scheduler->RegisterExternalTaskThread()) {
        reportTaskSystemFailure("TaskSystem::registerExternalThread failed because no external thread slots are available");
        return {};
    }

    return ExternalThreadScope(this);
}

enki::TaskScheduler& TaskSystem::getScheduler()
{
    return *m_scheduler;
}

const enki::TaskScheduler& TaskSystem::getScheduler() const
{
    return *m_scheduler;
}

std::vector<std::shared_ptr<detail::TaskCompletionState>> TaskSystem::collectDependencyStates(
    const std::vector<TaskHandle>& dependencies) const
{
    std::vector<std::shared_ptr<detail::TaskCompletionState>> states;
    states.reserve(dependencies.size());

    for (const TaskHandle& dependency : dependencies) {
        if (dependency.m_state) {
            states.push_back(dependency.m_state);
        }
    }

    return states;
}

TaskHandle TaskSystem::submitManagedTask(std::shared_ptr<detail::ManagedTaskBase> task_state,
                                         TaskTarget target,
                                         const std::vector<TaskHandle>& dependencies)
{
    if (!task_state) {
        reportTaskSystemFailure("TaskSystem::submitManagedTask received an empty task state");
        return {};
    }

    if (!m_scheduler) {
        reportTaskSystemFailure("Task submission requires TaskSystem::initialize() to succeed first");
        return {};
    }

    if (target == TaskTarget::Worker && !canSubmitWorkerTasksFromCurrentThread()) {
        reportTaskSystemFailure("Worker task submission requires the calling thread to be the application thread, an enkiTS worker, or a registered external task thread");
        return {};
    }

    const auto dependency_states = collectDependencyStates(dependencies);
    auto completion_state = std::make_shared<detail::TaskCompletionState>(target);
    completion_state->bind(*this, task_state->completable());

    reapCompletedManagedTasks();
    trackManagedTask(task_state, completion_state);

    if (dependency_states.empty()) {
        if (!task_state->schedule(*this)) {
            completion_state->failSubmission();
            return {};
        }

        return TaskHandle(std::move(completion_state));
    }

    auto pending_launch =
        std::make_shared<detail::PendingLaunch>(*this, task_state, completion_state, static_cast<uint32_t>(dependency_states.size()));

    for (const auto& dependency_state : dependency_states) {
        dependency_state->addPendingLaunch(pending_launch);
    }

    return TaskHandle(std::move(completion_state));
}

void TaskSystem::trackManagedTask(const std::shared_ptr<detail::ManagedTaskBase>& task_state,
                                  const std::shared_ptr<detail::TaskCompletionState>& completion_state)
{
    std::lock_guard<std::mutex> lock(m_managed_tasks_mutex);
    m_managed_tasks.push_back({task_state, completion_state});
}

void TaskSystem::reapCompletedManagedTasks()
{
    std::lock_guard<std::mutex> lock(m_managed_tasks_mutex);
    std::erase_if(m_managed_tasks, [](const detail::ManagedTaskRecord& record) {
        return !record.task_state || !record.completion_state || record.completion_state->isFinished();
    });
}

bool TaskSystem::canSubmitWorkerTasksFromCurrentThread() const
{
    return isCurrentThreadRegistered();
}

bool TaskSystem::requireSchedulerApiThread(const char* operation) const
{
    if (isCurrentThreadRegistered()) {
        return true;
    }

    if (Logger::isInitialized()) {
        LUNA_CORE_ERROR("{} requires the calling thread to be the application thread, an enkiTS worker, or a registered external task thread",
                        operation);
    }

    assert(false && "TaskSystem scheduler API called from an unregistered thread");
    return false;
}

void TaskSystem::deregisterExternalThread()
{
    if (!m_scheduler) {
        return;
    }

    const uint32_t thread_number = m_scheduler->GetThreadNum();
    const uint32_t first_external = getFirstIOThreadNumber();
    const uint32_t external_thread_limit =
        first_external + m_config.io_thread_count + m_config.external_thread_count;

    if (thread_number < first_external || thread_number >= external_thread_limit) {
        return;
    }

    m_scheduler->DeRegisterExternalTaskThread();
}

void TaskSystem::ioThreadMain(uint32_t thread_number)
{
    const bool registered = m_scheduler != nullptr && m_scheduler->RegisterExternalTaskThread(thread_number);

    {
        std::lock_guard<std::mutex> lock(m_io_startup_mutex);
        if (registered) {
            ++m_ready_io_threads;
        } else {
            m_io_startup_failed = true;
        }
    }
    m_io_startup_cv.notify_one();

    if (!registered || m_scheduler == nullptr) {
        return;
    }

    while (!m_scheduler->GetIsShutdownRequested()) {
        m_scheduler->WaitForNewPinnedTasks();

        if (m_scheduler->GetIsShutdownRequested()) {
            break;
        }

        m_scheduler->RunPinnedTasks();
    }

    m_scheduler->DeRegisterExternalTaskThread();
}

} // namespace luna
