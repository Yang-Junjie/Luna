#include "Core/Log.h"
#include "Core/TaskSystem.h"

#include <atomic>
#include <cstdlib>
#include <limits>

namespace {

struct CountingTask final : enki::ITaskSet {
    explicit CountingTask(std::atomic<uint32_t>& counter_ref, uint32_t item_count)
        : enki::ITaskSet(item_count)
        , counter(counter_ref)
    {
    }

    void ExecuteRange(enki::TaskSetPartition range, uint32_t) override
    {
        for (uint32_t index = range.start; index < range.end; ++index) {
            (void)index;
            counter.fetch_add(1, std::memory_order_relaxed);
        }
    }

    std::atomic<uint32_t>& counter;
};

struct RecordedPinnedTask : enki::IPinnedTask {
    explicit RecordedPinnedTask(luna::TaskSystem& system_ref, std::atomic<bool>& executed_ref, std::atomic<uint32_t>& thread_ref)
        : system(system_ref)
        , executed(executed_ref)
        , thread_number(thread_ref)
    {
    }

    void Execute() override
    {
        executed.store(true, std::memory_order_release);
        thread_number.store(system.getScheduler().GetThreadNum(), std::memory_order_release);
    }

    luna::TaskSystem& system;
    std::atomic<bool>& executed;
    std::atomic<uint32_t>& thread_number;
};

struct FanOutTask final : enki::ITaskSet {
    FanOutTask(luna::TaskSystem& system_ref, enki::IPinnedTask& main_task_ref, enki::IPinnedTask& io_task_ref)
        : system(system_ref)
        , main_task(main_task_ref)
        , io_task(io_task_ref)
    {
        m_SetSize = 1;
    }

    void ExecuteRange(enki::TaskSetPartition range, uint32_t) override
    {
        if (range.start != 0) {
            return;
        }

        system.submitMainThreadJob(&main_task);
        system.submitIOJob(&io_task);
    }

    luna::TaskSystem& system;
    enki::IPinnedTask& main_task;
    enki::IPinnedTask& io_task;
};

int fail(const char* message)
{
    LUNA_CORE_ERROR("TaskSystem smoke test failed: {}", message);
    luna::Logger::shutdown();
    return EXIT_FAILURE;
}

} // namespace

int main()
{
    luna::Logger::init("logs/task-system-smoke.log", luna::Logger::Level::Trace);

    luna::TaskSystem task_system;
    if (!task_system.initialize()) {
        return fail("initialize() returned false");
    }

    std::atomic<uint32_t> work_counter = 0;
    CountingTask counting_task(work_counter, 256);
    task_system.submitTask(&counting_task);
    task_system.waitForTask(&counting_task);

    if (work_counter.load(std::memory_order_acquire) != 256) {
        return fail("counting task did not process the full range");
    }

    std::atomic<bool> main_thread_executed = false;
    std::atomic<uint32_t> main_thread_number = std::numeric_limits<uint32_t>::max();
    RecordedPinnedTask main_thread_task(task_system, main_thread_executed, main_thread_number);
    task_system.submitMainThreadJob(&main_thread_task);
    task_system.waitForTask(&main_thread_task);

    if (!main_thread_executed.load(std::memory_order_acquire)) {
        return fail("main-thread pinned task did not execute");
    }
    if (main_thread_number.load(std::memory_order_acquire) != 0) {
        return fail("main-thread pinned task executed on the wrong thread");
    }

    std::atomic<bool> io_thread_executed = false;
    std::atomic<uint32_t> io_thread_number = std::numeric_limits<uint32_t>::max();
    RecordedPinnedTask io_thread_task(task_system, io_thread_executed, io_thread_number);
    task_system.submitIOJob(&io_thread_task);
    task_system.waitForTask(&io_thread_task);

    if (!io_thread_executed.load(std::memory_order_acquire)) {
        return fail("IO pinned task did not execute");
    }
    if (io_thread_number.load(std::memory_order_acquire) != task_system.getFirstIOThreadNumber()) {
        return fail("IO pinned task executed on the wrong thread");
    }

    std::atomic<bool> fanout_main_executed = false;
    std::atomic<uint32_t> fanout_main_thread_number = std::numeric_limits<uint32_t>::max();
    RecordedPinnedTask fanout_main_task(task_system, fanout_main_executed, fanout_main_thread_number);

    std::atomic<bool> fanout_io_executed = false;
    std::atomic<uint32_t> fanout_io_thread_number = std::numeric_limits<uint32_t>::max();
    RecordedPinnedTask fanout_io_task(task_system, fanout_io_executed, fanout_io_thread_number);

    FanOutTask fanout_task(task_system, fanout_main_task, fanout_io_task);
    task_system.submitTask(&fanout_task);
    task_system.waitForAll();

    if (!fanout_main_executed.load(std::memory_order_acquire)) {
        return fail("worker task failed to dispatch the main-thread callback");
    }
    if (!fanout_io_executed.load(std::memory_order_acquire)) {
        return fail("worker task failed to dispatch the IO callback");
    }
    if (fanout_main_thread_number.load(std::memory_order_acquire) != 0) {
        return fail("worker-dispatched main-thread callback executed on the wrong thread");
    }
    if (fanout_io_thread_number.load(std::memory_order_acquire) != task_system.getFirstIOThreadNumber()) {
        return fail("worker-dispatched IO callback executed on the wrong thread");
    }

    task_system.shutdown();
    luna::Logger::shutdown();
    return EXIT_SUCCESS;
}
