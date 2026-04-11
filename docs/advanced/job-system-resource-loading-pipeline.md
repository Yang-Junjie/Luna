# JobSystem 里的资源加载流水线

> **提示 (Note):**
> `ResourceLoadQueue` 只有一个头文件，但它非常能体现 Luna 对 JobSystem 的使用哲学: 不把任务系统直接暴露成“到处都能塞 lambda 的工具箱”，而是先用一层小而明确的封装，把典型业务流固定下来。

## 它不是一个真正的“队列线程池”

先澄清一个容易误解的点: `ResourceLoadQueue` 并不拥有自己的线程、队列或后台循环。

它只有一个成员:

```cpp
TaskSystem& m_task_system;
```

所以它本质上是:

- 对 `TaskSystem` 的模板语法糖
- 对“加载”和“提交”两个阶段的语义命名
- 对资源临时结果的一层线程安全保存

换句话说，它叫 Queue，但源码里没有常驻消费者线程，也没有独立调度器。

## 默认工作流就是 `IO -> MainThread`

`ResourceLoadQueueDesc` 给出了这个类型的默认意图:

| 字段 | 默认值 | 含义 |
| --- | --- | --- |
| `load_task.target` | `TaskTarget::IO` | 加载阶段默认跑在 IO 线程 |
| `commit_task.target` | `TaskTarget::MainThread` | 提交阶段默认回到主线程 |

这其实已经把项目最常见的资源流程写死成一个推荐路径:

```text
文件读取 / 解码 / CPU 侧准备 -> 主线程回写引擎状态或渲染资源
```

如果你只看接口设计，这个类型最重要的不是“能做很多事”，而是“默认就把正确的线程边界写出来了”。

## `ResourceLoadState<T>` 为什么不用 `optional<T>`

内部状态类型是:

```cpp
template <typename Resource> struct ResourceLoadState
```

它没有直接保存 `std::optional<Resource>`，而是保存:

```cpp
std::shared_ptr<Resource> resource;
mutable std::mutex mutex;
```

这么做主要有三个考虑。

### 第一，兼容 move-only 资源

如果 `Resource` 是 move-only 类型，直接在多个 API 间搬运 `optional<Resource>` 会更容易引入额外要求。用 `shared_ptr<Resource>` 先把结果放到堆上，可以把“状态共享”和“最终转移所有权”分成两个步骤。

### 第二，`withValue()` 可以安全只读访问

`withValue()` 的实现先在锁里复制一份 `shared_ptr`，再解锁执行回调。这样做的好处是:

- 用户回调不会在持锁状态下运行
- 即使另一条线程随后调用 `take()`，当前回调也仍握有资源对象的共享引用

这是一个很重要的工程细节，因为它避免了“在锁里执行未知用户代码”这种常见设计错误。

### 第三，`take()` 可以天然表达一次性消费

`take()` 的语义是:

1. 锁住状态
2. 若无资源则返回 `std::nullopt`
3. 把 `shared_ptr<Resource>` 移出内部状态
4. 清空内部保存
5. 返回 `Resource` 的值语义副本

这意味着同一个加载结果可以:

- 在提交前先通过 `withValue()` 看一眼
- 最终只被 `take()` 真正消费一次

## `submitLoad()` 做的事情很少，但刚好够用

`submitLoad()` 是整个封装的第一阶段。它的模板返回值直接从 `load_function` 的返回类型推导:

```cpp
-> ResourceLoadHandle<std::invoke_result_t<std::decay_t<LoadFunction>>>
```

也就是说，只要你的加载函数长这样:

```cpp
[]() { return MyResource(...); }
```

返回的 handle 类型就会自动变成:

```cpp
ResourceLoadHandle<MyResource>
```

真正提交给 `TaskSystem` 的 lambda 只做一件事:

1. 调用 `load()`
2. 把结果包成 `std::shared_ptr<Resource>`
3. 在锁保护下写入 `ResourceLoadState<Resource>::resource`

所以 `submitLoad()` 不是“帮你管理资源生命周期的复杂系统”，它只是把“异步得到一个值”变成了“异步填充一个共享状态 + 暴露一个 handle”。

## `ResourceLoadHandle<T>` 其实是“两段式句柄”

`ResourceLoadHandle<T>` 内部同时持有:

- 一个 `TaskHandle`
- 一个 `std::shared_ptr<ResourceLoadState<T>>`

这意味着它对外暴露了两类能力:

### 第一类: 任务维度

- `isValid()`
- `isReady()`
- `wait()`
- `task()`

这些接口回答的问题是“加载任务完成了吗”。

### 第二类: 资源维度

- `hasValue()`
- `withValue()`
- `take()`

这些接口回答的问题是“加载结果现在有没有值、能否只读访问、能否拿走所有权”。

把这两层拆开很有价值，因为任务完成与资源是否已被消费不是一回事。

## `submitLoadWithCommit()` 才是这个类型最像“流水线”的地方

如果说 `submitLoad()` 只是异步得到一个资源值，那么 `submitLoadWithCommit()` 才真正把两阶段管线串起来。

它的实现顺序是:

1. 先调用 `submitLoad(...)`，得到 `load_handle`
2. 再拿 `load_handle.task()` 去做一次 `then(...)`
3. continuation 里从 `state->take()` 取出资源
4. 若取值成功，就执行用户提供的 `commit(resource)`

把它翻译成更直白的控制流就是:

```text
先在后台把资源准备出来
-> 等准备完成
-> 把资源移动到 commit 阶段
-> 在主线程或指定目标线程做最终接线
```

默认情况下，这正好对应引擎里最经典的资源流程:

```text
IO 线程读取文件 -> 主线程创建/回写 GPU 或引擎对象
```

## 为什么 commit 阶段默认要回主线程

从纯任务系统角度看，commit 不一定非要在主线程。但从引擎上下文看，很多最终回写动作都有明显线程归属:

- 更新主线程拥有的资源表
- 修改渲染器状态
- 创建依赖窗口或图形上下文的对象
- 将加载结果接入游戏/编辑器主状态

因此 `ResourceLoadQueueDesc` 默认把 `commit_task.target` 设成 `TaskTarget::MainThread`，实际上是在帮上层代码守住线程边界。

这也是它和“单纯返回一个 `future<T>`”最大的区别: 它不只关心值的到达，还关心值应该在哪个线程落地。

## 依赖是如何接进资源管线的

`submitLoad()` 和 `submitLoadWithCommit()` 都接受依赖列表。这意味着资源加载本身也可以成为大任务图的一部分。

例如:

```cpp
auto decode_ready = task_system.submit(...);
auto texture_task = queue.submitLoadWithCommit(load_texture, commit_texture, {decode_ready});
```

语义就是:

```text
先等 decode_ready
-> 再启动 load_texture
-> 加载完成后回主线程 commit_texture
```

也就是说，`ResourceLoadQueue` 没有发明新的依赖系统，它只是完整继承了 `TaskSystem` 那套 `TaskHandle` / `PendingLaunch` 机制。

## `withValue()` 与 `take()` 的差别要分清

这两个接口都能“看到资源”，但语义完全不同。

### `withValue()`

- 不消费资源
- 适合只读观察
- 回调执行时不持锁
- 若资源不存在，返回 `false`

### `take()`

- 消费资源
- 适合转移所有权
- 调用成功后内部状态会被清空
- 之后再次 `take()` 只会得到 `std::nullopt`

这对写调用方代码很重要。只要 commit 阶段用的是 `take()`，你就应该把它当成“资源已经被管线下游拿走了”。

## sample 里这个类型是怎么被验证的

[samples/TaskSystem/TaskSystemSmoke.cpp](../../samples/TaskSystem/TaskSystemSmoke.cpp) 里有两段非常典型的验证。

第一段验证 `submitLoad()`:

- 提交一个返回 `std::string` 的加载任务
- `wait()` 之后调用 `take()`
- 检查拿到的值是否是 `"luna-resource"`

第二段验证 `submitLoadWithCommit()`:

- 加载阶段返回整数 `42`
- commit 阶段把值写入原子变量
- 同时记录 `GetThreadNum()`
- 最终确认 commit 在线程号 `0` 执行

这两段 sample 其实把 `ResourceLoadQueue` 的核心语义都测到了:

- 它确实能跨线程传递一个加载结果
- 它确实能把 commit 固定到主线程

## 它的边界在哪里

`ResourceLoadQueue` 很实用，但边界也很清楚。它当前不负责:

- 取消加载
- 限流或批量调度策略
- 自动缓存同一路径资源
- 错误对象的结构化传播
- 多阶段资源状态机

所以更准确地说，它是“资源任务流水线模板”，不是完整资源系统。

## 最后用一句话概括

`ResourceLoadQueue` 的价值不在于复杂，而在于它把项目里最常见的一条异步资源路径写成了默认答案:

“先在后台把值生产出来，再在正确的线程上把值消费掉。”

如果你想理解 Luna 是怎样把通用 JobSystem 往具体引擎业务上收束的，这个头文件是最直接的例子。
