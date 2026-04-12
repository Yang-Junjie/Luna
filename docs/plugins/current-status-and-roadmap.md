# Luna 插件系统当前状态与后续路线

## 1. 这份文档回答什么问题

这份文档不讨论“理想中的插件系统应该有多强”，而是回答下面这些现实问题:

- 当前到底哪些能力已经稳定可用
- 哪些字段或机制只是保留位
- 哪些扩展点明确还没有做
- 下一阶段最值得投入的方向是什么

## 2. 当前成熟度判断

如果要给当前插件系统一个准确定位，最合适的说法是:

> Luna 已经有一个可靠的源码插件系统骨架，但还没有形成完整插件生态。

这个判断里有两个关键词:

- `可靠`
- `骨架`

也就是说:

- 最小链路已经打通，确实能工作
- 但远程来源、版本治理、更多扩展点仍然缺失

## 3. 当前能力成熟度矩阵

| 能力域 | 当前状态 | 说明 |
| --- | --- | --- |
| Bundle 选择插件 | 稳定可用 | 已用于 `editor` / `runtime` profile |
| manifest 扫描与校验 | 稳定可用 | `sync.py` 已处理缺失、重复、host 不匹配等情况 |
| 构建接入 | 稳定可用 | `PluginList.cmake` 已进入宿主 target |
| 启动时注册 | 稳定可用 | `ResolvedPlugins.cpp` -> `registerResolvedPlugins()` |
| Layer / Overlay 扩展 | 稳定可用 | 已被 runtime / editor shell 真实使用 |
| Panel / Command 扩展 | 稳定可用 | 已被 `luna.editor.core` 和示例插件真实使用 |
| ImGui 请求 | 稳定可用 | `luna.editor.shell`、`luna.imgui` 都可触发 |
| `ServiceRegistry` 共享服务 | 初级可用 | 基础容器已经有，但默认服务还不多 |
| `sdk` / `kind` 元数据治理 | 半实现 | 字段存在，但还没有强校验与策略 |
| 版本求解 | 未实现 | 依赖 value 目前只是字符串 |
| 远程插件同步 | 未实现 | 当前只扫本地目录 |
| RenderGraph 插件协议 | 未实现 | 还没有 render registry / render contribution |
| 二进制插件 / 热重载 | 未实现 | 当前完全没有这条链路 |

## 4. 当前已经真正落地的部分

### 4.1 单宿主 + 多 Bundle 组合

当前已经稳定落地的不是“双宿主”，而是:

- 单一宿主 `LunaApp`
- 多套 Bundle 组合

默认组合如下:

| Bundle | 当前启用插件 |
| --- | --- |
| `EditorDefault` | `luna.editor.shell`、`luna.editor.core`、`luna.example.hello`、`luna.example.imgui_demo` |
| `RuntimeDefault` | `luna.runtime.core` |

### 4.2 插件已经进入真实构建图

这不是文档层的概念，而是构建层的现实:

- `sync.py` 生成 `PluginList.cmake`
- `App/CMakeLists.txt` include 这个生成文件
- `LunaAppHost` 链接选中的插件 target

这意味着插件已经不是“源码目录里的一堆普通文件”，而是真正参与宿主装配的模块。

### 4.3 默认 builtin plugins 已经证明链路有效

当前仓库里，下面这些插件都已经是“真正在工作”的插件，而不是演示用伪代码:

| 插件 | 作用 |
| --- | --- |
| `luna.editor.shell` | 请求 ImGui，并把 `EditorShellLayer` 作为 overlay 注入宿主 |
| `luna.editor.core` | 注册默认编辑器相机控制、Renderer 面板与 Reset Camera 命令 |
| `luna.example.hello` | 注册 Hello 示例面板 |
| `luna.example.imgui_demo` | 注册 Dear ImGui Demo 面板 |
| `luna.runtime.core` | 注册 runtime clear color 动画层 |
| `luna.imgui` | 提供独立的 ImGui 请求插件，当前不在默认 editor bundle 中 |

## 5. 当前“半实现”的部分

下面这些机制已经出现了，但还没有形成完整治理能力。

### 5.1 `sdk`

当前现状:

- Bundle 有 `sdk`
- 插件 manifest 有 `sdk`
- lock file 会记录 `sdk`

当前缺失:

- Bundle 与插件的兼容性判断
- 升级策略
- break-change 协议

### 5.2 `kind`

当前现状:

- 每个插件都有 `kind`
- lock file 会记录它

当前缺失:

- 基于 `kind` 的装配分流
- 更强的语义约束

### 5.3 `dependencies` 的版本值

当前现状:

- 依赖 key 已参与拓扑排序
- 依赖 value 必须是字符串

当前缺失:

- semver 求解
- 区间匹配
- 冲突裁决

### 5.4 `ServiceRegistry`

当前它已经是可用基础设施，但还没有真正承载一组丰富的共享上下文。

所以当前最准确的评价是:

- 基础设计正确
- 使用规模还很小

## 6. 当前明确没有做的部分

### 6.1 远程插件来源

`sync.py` 当前不会:

- clone Git 仓库
- checkout revision
- fetch 更新
- 缓存来源

它只处理当前工作区里已经存在的本地插件目录。

### 6.2 完整 lock file

当前 `luna.lock` 更像“解析结果摘要”，还不是完整锁文件体系。

它缺少:

- 来源 URL
- revision
- 哈希或校验信息
- 来源类型

### 6.3 更细粒度的 editor 扩展点

当前 editor 侧只有两类正式贡献:

- Panel
- Command

还没有:

- menu item registry
- toolbar item registry
- inspector provider
- dock layout contribution

### 6.4 RenderGraph 方向的正式插件协议

当前没有:

- render registry
- render feature registry
- render graph contribution
- render pass injection protocol

这意味着当前插件系统虽然能访问 renderer 状态，但仍然不能被定义为“正式支持渲染主路径扩展”。

### 6.5 二进制插件与热重载

当前完全没有:

- 动态装载协议
- 卸载协议
- ABI 稳定层
- 运行时重载

这不是缺一两个接口就能补出来的能力，而是一整条独立工程路线。

## 7. 当前最关键的现实边界

### 7.1 `Samples/Model` 能做出来，不等于插件系统已经支持它

这是当前最容易被误解的点。

仓库中已经有:

- `RenderGraphBuilder`
- 自定义 `RenderPass`
- `Samples/Model`

但这条能力链当前是通过:

- 自定义 `Application`
- 在 renderer 初始化前提供 `m_render_graph_builder`

来接入的。

插件系统当前则是:

- renderer 初始化完成后
- 才进入插件注册阶段

因此当前最准确的结论是:

| 问题 | 当前答案 |
| --- | --- |
| 插件能访问 renderer 基本状态吗 | 能 |
| 插件能改 clear color / 相机吗 | 能 |
| 插件能像 `Samples/Model` 那样正式替换 RenderGraph 吗 | 不能，当前没有正式协议 |

### 7.2 `Editor/` 不应该重新长回“大而全”

插件系统真正有价值的前提是:

- `Editor/` 保持 framework 身份
- 具体功能继续迁移到 `Plugins/`

如果后面继续把大量具体工具逻辑直接写回 `Editor/`，那整个插件系统的价值会被迅速稀释。

## 8. 当前最值得坚持的设计选择

### 8.1 显式注册优先

`ResolvedPlugins.cpp` 显式列出所有入口，这比静态全局自动注册更透明、更容易调试。

### 8.2 插件作为独立 target

插件不是宿主里的普通源码文件，而是独立构建目标。  
这是后续做依赖治理、模块化边界和 profile 组合的前提。

### 8.3 Bundle 属于宿主装配层

插件组合属于宿主层决策。  
因此 `PluginList.cmake` 由 `App/CMakeLists.txt` 消费，是当前正确的分层结果。

### 8.4 工具链优先用 Python

`sync.py` / `build.py` 这种配置解析、目录扫描、文件生成工作，用 Python 比用 C++ 更高效也更易维护。  
这个方向不应该倒退。

## 9. 下一阶段最合理的推进顺序

### Phase 1: 把当前最小链路做“更可靠”

优先项:

- 更严格的 `sdk` 校验
- 更清晰的诊断输出
- 对 `kind` 与 `host` 做更明确约束
- 继续整理 `sync.py` 的内部结构

原因:

- 风险低
- 立刻提升可维护性
- 不会破坏当前已工作的插件链路

### Phase 2: 增加更多真实插件

建议方向:

- `luna.viewport`
- `luna.asset.browser`
- `luna.scene.editor`
- `luna.inspector`

原因:

- 插件数量一多，当前这套 Bundle / registry / dependency 设计的价值才会真正体现出来

### Phase 3: 扩大 `EditorRegistry`

建议新增:

- menu item
- toolbar item
- inspector provider
- dock contribution

原因:

- 这是 editor 方向最自然的下一步
- 相比渲染协议，复杂度更可控

### Phase 4: 让 `ServiceRegistry` 真正承载共享上下文

建议逐步接入:

- ProjectContext
- SceneContext
- SelectionService
- AssetDatabase

原因:

- 这会直接决定后续插件之间的协作质量

### Phase 5: 设计正式的渲染扩展协议

建议前提:

- 先明确宿主到底开放哪些渲染扩展点
- 再定义独立 registry
- 最后再考虑如何插件化

也就是说，不建议直接把 RenderGraph 细节硬塞进现在的 `PluginRegistry`。

## 10. 当前不建议优先做的方向

下面这些方向现在都不应该排在前面:

- 二进制插件
- 插件热重载
- GUI 插件市场
- 复杂版本求解器
- 跨仓库分发体系

原因不是它们永远不重要，而是:

- 当前边界和扩展协议还不够成熟
- 先做这些会显著放大复杂度
- 对把宿主/插件结构做稳帮助不大

## 11. 一句话结论

如果你现在问“Luna 的插件系统到了哪一步”，最准确的回答是:

> 它已经可靠地进入了构建、装配和运行时注册链路，足以继续承载 editor/runtime 方向的功能扩展；但正式的渲染扩展协议、资产扩展协议和完整生态治理还没有开始。
