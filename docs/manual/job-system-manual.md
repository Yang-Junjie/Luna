# JobSystem 使用手册

> **提示 (Note):**
> 本文档描述的是当前 `Luna/JobSystem` 的实际封装，而不是 enkiTS 原始接口的完整说明。上层代码通常只需要使用 `luna::TaskSystem`、`luna::TaskHandle` 和 `luna::ResourceLoadQueue`。

## 适用场景

`JobSystem` 用于把 CPU 侧工作拆到不同执行目标上:

| 目标 | 类型 | 典型用途 |
| --- | --- | --- |
| `TaskTarget::Worker` | 普通 worker task | CPU 并行计算、数据预处理、轻量后台逻辑 |
| `TaskTarget::MainThread` | pinned main-thread task | 渲染资源 commit、需要主线程上下文的回写 |
| `TaskTarget::IO` | pinned IO task | 文件读取、资源解码前置步骤、外部阻塞 IO |

它适合做:

- 资源加载管线: `IO load -> MainThread commit`
- 帧内 fan-out/fan-in: 多个 worker task 完成后再继续
- 链式 continuation: `task.then(...)`
- 外部工具线程接入 job system

它暂时不负责:

- task 取消
- 超时等待
- 自动异常捕获
- 带返回值的通用 future/promise
- 复杂静态任务图编辑器

## 头文件

```cpp
#include "JobSystem/TaskSystem.h"
#include "JobSystem/ResourceLoadQueue.h"
```

## 初始化与关闭

`TaskSystem` 必须先初始化，再提交任务。`Application` 内部已经持有并初始化了一个 `TaskSystem`，普通上层代码优先通过 `Application::get().getTaskSystem()` 访问它。

```cpp
luna::TaskSystem task_system;

if (!task_system.initialize()) {
    // 初始化失败，停止继续使用 task_system
}

// submit / wait / resource load ...

task_system.shutdown();
```

### 配置项

```cpp
luna::TaskSystemConfig config;
config.worker_thread_count = 0;    // 0 表示根据硬件线程数自动计算
config.io_thread_count = 1;        // 默认保留 1 个 IO pinned thread
config.external_thread_count = 0;  // 默认不允许额外外部线程注册

task_system.initialize(config);
```

`worker_thread_count == 0` 时，系统会尽量为主线程、IO 线程和 external slot 预留位置，然后把剩余硬件线程分配给 worker。若硬件线程很少，仍至少创建 1 个 worker。

## 线程边界

`TaskSystem` 明确区分“已注册 enkiTS 线程”和普通外部线程。以下操作只能在已注册线程上使用:

- `submit()` 提交 worker task
- `submitParallel()` 提交并行 worker task
- `TaskHandle::wait()`
- `TaskSystem::waitForAll()`
- `TaskSystem::runPinnedTasksForCurrentThread()`

已注册线程包括:

- 初始化 `TaskSystem` 的应用主线程
- enkiTS 创建的 worker 线程
- `TaskSystem` 内部创建的 IO 线程
- 通过 `registerExternalThread()` 注册过的外部线程

如果普通 `std::thread` 想提交 worker task 或等待 task，初始化时必须预留 external slot，并在该线程里创建 RAII scope:

```cpp
luna::TaskSystem task_system;
task_system.initialize({.external_thread_count = 1});

std::thread worker([&]() {
    auto registration = task_system.registerExternalThread();
    if (!registration.isRegistered()) {
        return;
    }

    auto task = task_system.submit([]() {
        // 这里运行在 worker task 上
    });

    task.wait(task_system);
});

worker.join();
```

`ExternalThreadScope` 析构时会自动反注册，不要手工调用 enkiTS 的 register/deregister API。

## 提交普通 Worker Task

```cpp
auto task = task_system.submit([]() {
    // CPU 工作
});

task.wait(task_system);
```

`TaskHandle` 可以查询状态:

```cpp
if (task.status() == luna::TaskStatus::Completed) {
    // 已完成
}

if (task.hasFailed()) {
    // 提交或依赖链推进失败
}
```

状态枚举:

| 状态 | 含义 |
| --- | --- |
| `Invalid` | 默认构造或提交失败返回的空 handle |
| `Pending` | 已提交或等待依赖完成 |
| `Completed` | 成功完成 |
| `Failed` | 提交失败或依赖链失败 |

> **警告 (Warning):**
> 当前 task 函数内部不要向外抛异常。`TaskSystem` 不做异常捕获，异常会按 C++ 线程/任务执行环境的规则传播或终止进程。

## 提交并行 Worker Task

并行 task 使用 enkiTS 的 `TaskSetPartition`。`set_size` 是总工作量，`min_range` 控制每个切片的最小范围。

```cpp
auto parallel_task = task_system.submitParallel(
    [&](enki::TaskSetPartition range, uint32_t thread_number) {
        for (uint32_t index = range.start; index < range.end; ++index) {
            // 处理 index
        }
    },
    {.set_size = 1024, .min_range = 64});

parallel_task.wait(task_system);
```

并行 task 总是强制运行在 `TaskTarget::Worker`。如果传入其他 target，系统会记录警告并改回 worker。

## 提交 MainThread Task

主线程 task 是 pinned task，`threadNum == 0`。它适合做必须在主线程执行的 commit 工作。

```cpp
auto commit_task = task_system.submit(
    [&]() {
        // 主线程回写，例如提交渲染资源句柄、更新主线程状态
    },
    {.target = luna::TaskTarget::MainThread});

commit_task.wait(task_system);
```

主线程 task 的执行依赖主线程泵 task。`Application::run()` 每帧会调用 `m_task_system.waitForAll()`，等待过程中会运行当前线程的 pinned task。若你写独立测试或自管循环，需要自己调用 `waitForAll()`、`TaskHandle::wait()` 或 `runPinnedTasksForCurrentThread()`。

## 提交 IO Task

IO task 是 pinned 到内部 IO 线程的 task。默认配置有 1 个 IO 线程。

```cpp
auto io_task = task_system.submit(
    []() {
        // 文件读取或阻塞 IO
    },
    {.target = luna::TaskTarget::IO});

io_task.wait(task_system);
```

如果 `io_thread_count == 0`，提交 IO task 会被视为显式使用错误: 系统会记录错误并触发断言，而不是静默丢弃任务。

## 依赖与 Continuation

### `then()`

`then()` 表示“当前 handle 完成后再执行下一个任务”:

```cpp
auto load = task_system.submit([]() {
    // stage 1
});

auto process = load.then(task_system, []() {
    // stage 2, 一定在 load 完成之后启动
});

auto commit = process.then(
    task_system,
    []() {
        // stage 3, 主线程执行
    },
    {.target = luna::TaskTarget::MainThread});

commit.wait(task_system);
```

`thenParallel()` 用于在某个 task 完成后启动并行 worker task:

```cpp
auto prepare = task_system.submit([]() {
    // 准备数据
});

auto parallel = prepare.thenParallel(
    task_system,
    [](enki::TaskSetPartition range, uint32_t) {
        for (uint32_t i = range.start; i < range.end; ++i) {
            // 并行处理
        }
    },
    {.set_size = 512, .min_range = 32});
```

### `whenAll()`

`whenAll()` 用于 fan-in:

```cpp
auto a = task_system.submit([]() {});
auto b = task_system.submit([]() {});

auto done = task_system.whenAll({a, b}).then(task_system, []() {
    // a 和 b 都完成后执行
});

done.wait(task_system);
```

> **实现说明 (Implementation Note):**
> 当前封装使用 enkiTS 原生 `Dependency` 作为 completion relay，再由 relay 触发 continuation。这样避免了在 worker task 内阻塞等待前置任务，但仍保留了上层动态 `then()` 的使用方式。

## 资源加载队列

`ResourceLoadQueue` 是一个针对资源加载的薄封装，默认执行模式是:

```text
IO load task -> MainThread commit task
```

### 只加载，稍后取值

```cpp
luna::ResourceLoadQueue queue(task_system);

auto handle = queue.submitLoad([]() {
    return std::string("resource payload");
});

handle.wait(task_system);

if (auto value = handle.take()) {
    // value 是 std::optional<std::string>
}
```

`take()` 会移动并清空内部资源；之后再次 `take()` 会返回 `std::nullopt`。

如果只需要只读访问，可以使用 `withValue()`:

```cpp
handle.withValue([](const std::string& value) {
    // 只读访问 value
});
```

`withValue()` 不会在内部互斥锁持有期间执行用户回调。

### 加载后提交主线程 Commit

```cpp
auto task = queue.submitLoadWithCommit(
    []() {
        // IO 线程: 读取或解码资源
        return 42;
    },
    [&](int value) {
        // 主线程: 把资源提交到主线程拥有的数据结构
    });

task.wait(task_system);
```

可以通过 `ResourceLoadQueueDesc` 覆盖 load/commit 的目标、优先级或并行参数:

```cpp
luna::ResourceLoadQueueDesc desc;
desc.load_task.target = luna::TaskTarget::IO;
desc.commit_task.target = luna::TaskTarget::MainThread;

auto task = queue.submitLoadWithCommit(load_function, commit_function, {}, desc);
```

## 失败语义

当前封装对错误使用采取“记录日志 + 断言 + 返回失败/空 handle”的策略。

常见错误:

| 错误 | 结果 |
| --- | --- |
| 未初始化就提交任务 | 记录错误并断言 |
| 从未注册外部线程提交 worker task | 记录错误并断言 |
| 未配置 IO 线程却提交 IO task | 记录错误并断言 |
| 未预留 external slot 却注册外部线程 | 记录错误并断言 |
| 等待空 task 指针 | 记录错误并断言 |

`TaskHandle::hasFailed()` 主要用于识别提交失败或依赖链推进失败。它不是异常捕获机制。

## 最佳实践

- 每帧不要滥用 `waitForAll()` 作为同步锤子；优先等待明确的 `TaskHandle` 或通过 `then()/whenAll()` 串接。
- 不要在 task 内访问非线程安全的渲染器状态；需要主线程回写时使用 `TaskTarget::MainThread`。
- IO task 适合阻塞 IO，不适合长时间 CPU 密集计算；CPU 密集工作用 worker task。
- 并行 task 的 `min_range` 不宜过小，否则调度开销会吞掉收益。
- task 捕获引用时必须保证被捕获对象活得比 task 更久；资源生命周期不明确时使用 `shared_ptr` 或把数据 move 进 lambda。
- 外部线程必须用 `ExternalThreadScope` 注册后再调用 `submit/wait`，并确保 `TaskSystem` 生命周期覆盖该 scope。

## 可运行参考

完整 smoke test 位于:

```text
samples/TaskSystem/TaskSystemSmoke.cpp
```

它覆盖了:

- worker task
- parallel task
- main-thread task
- IO task
- `then()` 链式依赖
- `whenAll()` fan-in
- external thread 注册
- `ResourceLoadQueue` load/commit
