#include "Core/Log.h"
#include "JobSystem/ResourceLoadQueue.h"
#include "JobSystem/TaskSystem.h"

#include <atomic>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace {

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
    if (!task_system.initialize({.external_thread_count = 1})) {
        return fail("initialize() returned false");
    }

    luna::TaskHandle invalid_handle;
    if (invalid_handle.status() != luna::TaskStatus::Invalid) {
        return fail("default-constructed task handle did not report Invalid status");
    }

    std::atomic<uint32_t> worker_counter = 0;
    auto worker_task = task_system.submit([&]() {
        worker_counter.fetch_add(1, std::memory_order_relaxed);
    });
    worker_task.wait(task_system);
    if (worker_counter.load(std::memory_order_acquire) != 1) {
        return fail("worker task handle did not execute");
    }
    if (worker_task.status() != luna::TaskStatus::Completed || worker_task.hasFailed()) {
        return fail("worker task handle did not report Completed status");
    }

    std::atomic<uint32_t> parallel_counter = 0;
    auto parallel_task = task_system.submitParallel(
        [&](enki::TaskSetPartition range, uint32_t) {
            for (uint32_t index = range.start; index < range.end; ++index) {
                (void)index;
                parallel_counter.fetch_add(1, std::memory_order_relaxed);
            }
        },
        {.set_size = 256, .min_range = 32});
    parallel_task.wait(task_system);
    if (parallel_counter.load(std::memory_order_acquire) != 256) {
        return fail("parallel task handle did not process the full range");
    }

    std::atomic<bool> main_thread_executed = false;
    std::atomic<uint32_t> main_thread_number = std::numeric_limits<uint32_t>::max();
    auto main_thread_task = task_system.submit(
        [&]() {
            main_thread_executed.store(true, std::memory_order_release);
            main_thread_number.store(task_system.getScheduler().GetThreadNum(), std::memory_order_release);
        },
        {.target = luna::TaskTarget::MainThread});
    main_thread_task.wait(task_system);
    if (!main_thread_executed.load(std::memory_order_acquire)) {
        return fail("main-thread task handle did not execute");
    }
    if (main_thread_number.load(std::memory_order_acquire) != 0) {
        return fail("main-thread task handle executed on the wrong thread");
    }

    std::atomic<bool> io_thread_executed = false;
    std::atomic<uint32_t> io_thread_number = std::numeric_limits<uint32_t>::max();
    auto io_thread_task = task_system.submit(
        [&]() {
            io_thread_executed.store(true, std::memory_order_release);
            io_thread_number.store(task_system.getScheduler().GetThreadNum(), std::memory_order_release);
        },
        {.target = luna::TaskTarget::IO});
    io_thread_task.wait(task_system);
    if (!io_thread_executed.load(std::memory_order_acquire)) {
        return fail("IO task handle did not execute");
    }
    if (io_thread_number.load(std::memory_order_acquire) != task_system.getFirstIOThreadNumber()) {
        return fail("IO task handle executed on the wrong thread");
    }

    std::atomic<uint32_t> dependency_stage = 0;
    auto dependency_root = task_system.submit([&]() {
        dependency_stage.store(1, std::memory_order_release);
    });
    auto dependency_mid = dependency_root.then(task_system, [&]() {
        if (dependency_stage.load(std::memory_order_acquire) != 1) {
            dependency_stage.store(99, std::memory_order_release);
            return;
        }
        dependency_stage.store(2, std::memory_order_release);
    });
    auto dependency_tail = dependency_mid.then(task_system,
                                               [&]() {
                                                   if (dependency_stage.load(std::memory_order_acquire) == 2) {
                                                       dependency_stage.store(3, std::memory_order_release);
                                                   }
                                               },
                                               {.target = luna::TaskTarget::MainThread});
    dependency_tail.wait(task_system);
    if (dependency_stage.load(std::memory_order_acquire) != 3) {
        return fail("dependency chain did not execute in order");
    }

    std::atomic<uint32_t> fan_in_counter = 0;
    auto fan_in_a = task_system.submit([&]() {
        fan_in_counter.fetch_add(1, std::memory_order_relaxed);
    });
    auto fan_in_b = task_system.submit([&]() {
        fan_in_counter.fetch_add(1, std::memory_order_relaxed);
    });
    auto fan_in_done = task_system.whenAll({fan_in_a, fan_in_b}).then(task_system, [&]() {
        fan_in_counter.fetch_add(10, std::memory_order_relaxed);
    });
    fan_in_done.wait(task_system);
    if (fan_in_counter.load(std::memory_order_acquire) != 12) {
        return fail("whenAll barrier did not wait for both dependencies");
    }

    std::atomic<uint32_t> external_counter = 0;
    std::thread external_thread([&]() {
        auto registration = task_system.registerExternalThread();
        if (!registration.isRegistered()) {
            return;
        }

        auto external_task = task_system.submit([&]() {
            external_counter.fetch_add(1, std::memory_order_relaxed);
        });
        external_task.wait(task_system);
    });
    external_thread.join();
    if (external_counter.load(std::memory_order_acquire) != 1) {
        return fail("registered external thread could not submit and wait for a worker task");
    }

    luna::ResourceLoadQueue resource_queue(task_system);

    auto loaded_string = resource_queue.submitLoad([]() {
        return std::string("luna-resource");
    });
    loaded_string.wait(task_system);
    if (!loaded_string.isReady()) {
        return fail("resource load handle never completed");
    }

    std::optional<std::string> loaded_value = loaded_string.take();
    if (!loaded_value.has_value() || *loaded_value != "luna-resource") {
        return fail("resource load handle did not preserve the loaded payload");
    }

    std::atomic<int> committed_value = 0;
    std::atomic<uint32_t> commit_thread = std::numeric_limits<uint32_t>::max();
    auto committed_task = resource_queue.submitLoadWithCommit(
        []() {
            return 42;
        },
        [&](int value) {
            committed_value.store(value, std::memory_order_release);
            commit_thread.store(task_system.getScheduler().GetThreadNum(), std::memory_order_release);
        });
    committed_task.wait(task_system);
    if (committed_value.load(std::memory_order_acquire) != 42) {
        return fail("resource queue commit callback did not receive the loaded value");
    }
    if (commit_thread.load(std::memory_order_acquire) != 0) {
        return fail("resource queue commit callback did not execute on the main thread");
    }

    task_system.shutdown();
    luna::Logger::shutdown();
    return EXIT_SUCCESS;
}
