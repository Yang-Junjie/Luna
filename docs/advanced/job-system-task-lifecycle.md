# JobSystem 任务生命周期

> **提示 (Note):**
> 这篇文章专门解释“一个任务从 `submit()` 到完成到底经历了什么”。如果你还没建立目录级概念，建议先读 [job-system-source-map.md](./job-system-source-map.md)。

## 一个任务并不是直接交给 `TaskHandle`

Luna 的任务生命周期可以先抽象成一句话:

```text
用户 lambda -> ManagedTaskBase 子类 -> enkiTS 任务对象 -> TaskCompletionState -> TaskHandle
```

这里有两个经常被混淆的层次:

1. 真正被 enkiTS 执行的是 `FunctionTask` / `ParallelFunctionTask` / `PinnedFunctionTask`。
2. 上层代码拿到的是 `TaskHandle`，它观察的是 `TaskCompletionState`。

所以 `TaskHandle` 从来不是“任务本身”，而是“任务完成态的共享引用”。

## 第一步: `submit()` 先决定任务壳类型

`TaskSystem::submit()` 的第一件事不是立即调度，而是根据 `TaskSubmitDesc::target` 选择具体壳对象:

- `TaskTarget::Worker` -> `FunctionTask`
- `TaskTarget::MainThread` / `TaskTarget::IO` -> `PinnedFunctionTask`

`submitParallel()` 则总是创建 `ParallelFunctionTask`，并强制把目标改成 `TaskTarget::Worker`。这背后的原因很直接:

- 并行分片依赖 `enki::ITaskSet` 的区间切分语义。
- pinned task 天然是固定在线程号上执行的一次性工作，不适合再做范围并行。

这一步产物是一个 `std::shared_ptr<detail::ManagedTaskBase>`，后面所有逻辑都围绕这个共享对象展开。

## 第二步: `submitManagedTask()` 建立完成中继

整个 JobSystem 的核心入口其实是 [TaskSystem.cpp](../../Luna/JobSystem/TaskSystem.cpp) 里的 `TaskSystem::submitManagedTask()`。

它做的事情可以拆成六步:

1. 校验调度器是否已初始化。
2. 若目标是 `Worker`，校验当前线程是否允许提交 worker task。
3. 把依赖 `TaskHandle` 转成内部的 `TaskCompletionState` 列表。
4. 为当前任务创建一个新的 `TaskCompletionState`。
5. 调用 `trackManagedTask()`，把任务壳和 completion state 一起保活。
6. 根据是否有依赖，选择“立即调度”还是“挂到 `PendingLaunch` 里等待依赖”。

关键点在第四步。这里创建的 `TaskCompletionState` 并不是一个空壳，而是会通过 `bind()` 和真正任务对象的 `enki::ICompletable` 连接起来:

```text
TaskCompletionState depends on task_state->completable()
```

这等价于说:

“当前这个 completion state 不自己做工作，它只负责在底层任务完成后被 enkiTS 标记为完成。”

## `TaskCompletionState` 为什么继承 `enki::ICompletable`

这层设计决定了整个依赖系统的样子。

`TaskCompletionState` 内部保存了:

- `m_finished`
- `m_submit_failed`
- `m_pending_launches`
- 一个 enkiTS `Dependency`

它的职责有两块:

### 职责一: 对外暴露状态

`TaskHandle::isComplete()`、`status()`、`hasFailed()` 最终都只是读 `TaskCompletionState` 里的原子状态。

所以 `TaskHandle` 查询状态的开销很低，也不需要知道底层任务是哪一种。

### 职责二: 作为后继任务的依赖源

当另一个任务依赖当前任务时，依赖的并不是 `FunctionTask` 本体，而是当前任务的 `TaskCompletionState`。

这会带来两个好处:

1. 后继任务只关心“前驱是否完成/失败”，不关心前驱的具体执行形式。
2. 可以把真正任务对象与依赖图中的完成节点拆开，避免上层 API 直接暴露 enkiTS 原生对象。

## 没有依赖时，任务如何被立即调度

如果 `submitManagedTask()` 发现依赖列表为空，它会直接执行:

```text
task_state->schedule(*this)
```

然后根据任务类型走向不同分支:

- `FunctionTask::schedule()` / `ParallelFunctionTask::schedule()` -> `TaskSystem::submitTask()`
- `PinnedFunctionTask::schedule()` -> `TaskSystem::submitMainThreadJob()` 或 `submitIOJob()`

最终真正调用 enkiTS 的地方只有三个:

- `m_scheduler->AddTaskSetToPipe(task)`
- `m_scheduler->AddPinnedTask(task)`
- `m_scheduler->WaitforTask(...)` / `WaitforAll()` 这种消费端入口

也就是说，Luna 的封装主要负责“什么时候可以调度”和“如何把状态暴露给上层”，而不自己实现底层工作窃取或分片调度。

## 有依赖时，任务为什么不会立刻进入调度器

当你调用:

```cpp
auto c = task_system.submit(fn_c, {a, b});
```

或者:

```cpp
auto c = a.then(task_system, fn_c);
```

`submitManagedTask()` 不会直接把 `c` 扔进 enkiTS，而是创建一个 `PendingLaunch`。

`PendingLaunch` 里保存了:

- 目标 `TaskSystem*`
- 待调度的 `ManagedTaskBase`
- 该任务自己的 `TaskCompletionState`
- 剩余未完成依赖数 `m_remaining_dependencies`
- 是否已有依赖失败 `m_dependency_failed`
- 是否已经真正 schedule 过 `m_scheduled`

然后它会被挂到每个依赖的 `TaskCompletionState::m_pending_launches` 上。

这等于构造了一种“完成后回调注册表”。

## 依赖完成后，谁来触发 continuation

答案是 `TaskCompletionState::OnDependenciesComplete()`。

名字看起来有点绕，但它的语义是:

- 当前 `TaskCompletionState` 依赖的底层任务完成了
- enkiTS 因此回调这个 completion state

在这个函数里，Luna 先做两件事:

1. 把 `m_finished` 设为 `true`
2. 取出所有挂在自己身上的 `m_pending_launches`

然后再逐个调用:

```text
launch->dependencyResolved(false)
```

如果当前任务不是正常完成，而是在提交阶段就失败了，则会改走 `failSubmission()`，它会把 `submit_failed` 设为真，并以 `dependencyResolved(true)` 的形式把失败继续向后传播。

所以 continuation 的推进不是在用户任务函数里做的，也不是在 `TaskHandle::then()` 里同步完成的，而是在“前驱任务被标记完成”的那一刻，由 completion state 统一推进。

## `PendingLaunch::dependencyResolved()` 是 DAG 推进的闸门

这个函数做的事情很像一个原子倒计时闸门:

1. 如果本次通知说明某个依赖失败，先记下 `m_dependency_failed = true`。
2. 原子递减 `m_remaining_dependencies`。
3. 只有最后一个到达的依赖，才有资格继续往下调度。
4. 若任何依赖失败，则直接让当前任务的 completion state 进入 failed。
5. 若全部依赖成功，则真正调用 `m_task_state->schedule(*m_task_system)`。

从控制流上看，它把“依赖图中的节点 ready”翻译成了“现在可以把真实任务丢进 enkiTS 了”。

这套设计的一个优点是:

- 依赖等待发生在图的边界，而不是某个 worker 线程内部的阻塞等待。

换句话说，Luna 没有用“任务里 `wait()` 前驱”的方式来串任务，而是用“等前驱完成时再投递后驱”的方式做 continuation。这对任务系统来说通常更健康，因为它避免了线程池里大量工作线程被互相等待卡住。

## `waitForTask()` 为什么要特殊对待 `TaskCompletionState`

这段实现是整个封装里最值得注意的细节之一。

如果传进 `waitForTask()` 的对象其实是 `TaskCompletionState`，Luna 不会直接调用:

```cpp
m_scheduler->WaitforTask(task);
```

而是循环:

```text
while (!completion_state->isFinished()) {
    m_scheduler->WaitforTask(nullptr);
}
```

这么做的原因是 `TaskCompletionState` 只是完成中继，不是一个真正排进调度器执行的任务体。对它调用原生 `WaitforTask(completion_state)` 并不能自然表达“把这个 relay 对应的前置任务链跑完”。因此这里改成了更保守的策略:

- 反复让调度器帮忙执行可运行任务
- 直到这个 completion state 观察到完成

这也解释了为什么 `TaskHandle::wait()` 最终只是把 `m_state.get()` 传回 `TaskSystem::waitForTask()`。

## `whenAll()` 其实只是构造了一个空 barrier 任务

`whenAll()` 的实现比名字看起来朴素得多:

- 0 个依赖 -> 提交一个空 lambda
- 1 个依赖 -> 直接返回那个 handle
- 多个依赖 -> 提交一个空 lambda，并把所有依赖挂上去

所以 `whenAll()` 本身不是特殊调度原语，它只是借用了现有的“依赖全部完成后再启动任务”的机制，把一个空任务当作 barrier 节点。

这也意味着:

- `whenAll()` 的返回值依然是普通 `TaskHandle`
- 它后面同样可以继续 `then(...)`

## 失败是如何传播的

当前 JobSystem 里的失败语义比较明确，但也比较克制:

1. 如果调度器没初始化、线程未注册、IO 线程数量不够等，`submit...()` 会记录错误并返回失败。
2. 如果一个带依赖的任务发现某个前驱已经失败，它自己不会再 schedule，而是直接进入 failed。
3. `TaskHandle::status()` 只区分 `Pending`、`Completed`、`Failed`，不保存更细粒度错误码。

这里的“失败”主要指:

- 任务提交失败
- 依赖推进失败

它不是异常系统。用户 lambda 内如果抛异常，这层封装并没有统一兜底逻辑。

## 为什么任务对象必须被显式保活

`trackManagedTask()` / `reapCompletedManagedTasks()` 解决的是生命周期问题，不是调度问题。

`TaskHandle` 只保存 completion state，不保存 `FunctionTask` 这类真实任务对象。因此 `TaskSystem` 必须另外存一份:

```cpp
std::vector<detail::ManagedTaskRecord> m_managed_tasks;
```

每个 `ManagedTaskRecord` 同时持有:

- `task_state`
- `completion_state`

直到 `completion_state->isFinished()` 才会在 `reapCompletedManagedTasks()` 里被擦除。

少了这一步，最直接的问题就是: 任务可能还没执行完，保存 lambda 的对象就已经被析构了。

## 用一个完整例子串起来

以 `samples/TaskSystem/TaskSystemSmoke.cpp` 里的依赖链为例:

```cpp
auto dependency_root = task_system.submit(...);
auto dependency_mid = dependency_root.then(task_system, ...);
auto dependency_tail = dependency_mid.then(task_system, ..., {.target = luna::TaskTarget::MainThread});
dependency_tail.wait(task_system);
```

实际发生的是:

1. `dependency_root` 创建 `FunctionTask + TaskCompletionState`，无依赖，立即 schedule。
2. `dependency_mid` 创建新的任务壳和新的 completion state，但不立即 schedule，而是创建 `PendingLaunch` 挂到 `dependency_root` 上。
3. `dependency_tail` 再次创建一个 `PendingLaunch`，挂到 `dependency_mid` 上。
4. `dependency_root` 执行完成后，自己的 `TaskCompletionState::OnDependenciesComplete()` 触发 `dependency_mid` 的 `PendingLaunch`。
5. `dependency_mid` 被 schedule、执行、完成，再触发 `dependency_tail`。
6. `dependency_tail` 因为目标是 `MainThread`，最终会被作为 pinned task 跑在主线程上。
7. `dependency_tail.wait(task_system)` 在主线程持续泵调度器，直到尾节点 completion state 变成 finished。

整个过程中，没有任何一个任务在任务体内部阻塞等待前驱。

## 最后用一句话概括

Luna 的任务生命周期不是“提交一个任务，然后得到一个 future”，而是:

“提交一个真实任务体，同时创建一个完成态节点；依赖图围绕完成态推进，真实任务只在节点 ready 时才进入 enkiTS 调度器。”

理解了这一点，`TaskHandle`、`PendingLaunch`、`whenAll()` 和 `waitForTask()` 的实现都会顺起来。
