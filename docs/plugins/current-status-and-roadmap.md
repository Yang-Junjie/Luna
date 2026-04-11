# Luna 插件系统当前状态与后续路线

## 1. 本文档的目的

本文档不再解释“插件系统应该如何设计”，而是明确回答下面这些现实问题:

- 当前到底已经实现了什么
- 当前哪些东西只是骨架
- 当前明确缺了什么
- 下一步最合理的工程推进顺序是什么

如果你准备继续开发这套系统，这份文档比抽象愿景更重要。

## 2. 当前已经真正工作的部分

下面这些内容不是设计稿，而是当前仓库里已经打通的链路。

### 2.1 本地 Bundle 驱动插件选择

当前已有:

- `Bundles/EditorDefault/luna.bundle.toml`
- `Bundles/RuntimeDefault/luna.bundle.toml`

它们分别决定编辑器宿主和运行时宿主启用哪些插件。

当前默认 Bundle 已经接入:

- `luna.editor.core`
- `luna.example.hello`
- `luna.example.imgui_demo`

当前默认 runtime Bundle 已经接入:

- `luna.runtime.core`

### 2.2 本地插件 manifest 扫描

`Tools/luna/sync.py` 当前会扫描:

- `Plugins/builtin`
- `Plugins/external`

中的所有 `luna.plugin.toml`。

### 2.3 依赖拓扑排序

当前已经支持:

- 读取 `dependencies` 的 key
- 递归解析依赖
- 检测循环依赖

### 2.4 host 兼容校验

当前已经支持:

- Bundle 声明 `host`
- 插件声明 `hosts`
- `sync.py` 校验两者是否兼容

### 2.5 生成中间文件

当前已经稳定生成:

- `Plugins/Generated/editor/PluginList.cmake`
- `Plugins/Generated/editor/ResolvedPlugins.h`
- `Plugins/Generated/editor/ResolvedPlugins.cpp`
- `Plugins/Generated/runtime/PluginList.cmake`
- `Plugins/Generated/runtime/ResolvedPlugins.h`
- `Plugins/Generated/runtime/ResolvedPlugins.cpp`
- `luna.editor.lock`
- `luna.runtime.lock`

### 2.6 宿主显式注册插件

当前 `EditorApp` 已经通过:

- `registerResolvedPlugins(plugin_registry)`

来装配插件贡献。

当前 `RuntimeApp` 也已经通过同样的显式注册流程装配 runtime plugins。

### 2.7 运行时注册表骨架

当前已经有:

- `ServiceRegistry`
- `PluginRegistry`
- `EditorRegistry`

### 2.8 编辑器扩展点的最小链路

当前编辑器插件已经能贡献:

- Layer
- Overlay
- Panel
- Command

### 2.9 多个真正工作的 builtin plugins

当前已有:

- `Plugins/builtin/luna.editor.core`
- `Plugins/builtin/luna.example.hello`
- `Plugins/builtin/luna.example.imgui_demo`
- `Plugins/builtin/luna.runtime.core`

其中:

- `luna.editor.core` 是默认编辑器基础能力插件
- `luna.example.hello` 是最小 Panel 示例插件
- `luna.example.imgui_demo` 是 Dear ImGui Demo 面板示例插件
- `luna.runtime.core` 是最小 runtime Layer 示例插件

## 3. 当前只是“保留位”或“半实现”的部分

这些字段或机制已经出现了，但还没有完全长成最终形态。

### 3.1 `sdk`

当前:

- Bundle 里有 `sdk`
- 插件里有 `sdk`
- host 对应的 lock file 会记录 `sdk`

但当前还没有真正做:

- Bundle SDK 与插件 SDK 的兼容校验
- 升级策略
- 破坏性变更检查

### 3.2 `kind`

当前:

- manifest 里有 `kind`
- host 对应的 lock file 会记录它

但当前还没有真正根据 `kind` 分流不同解析或装配逻辑。

### 3.3 `dependencies` 的版本字符串

当前:

- `dependencies` 的 value 必须是字符串

但当前:

- 不做 semver 求解
- 不做版本区间匹配
- 不做冲突裁决

### 3.4 `ServiceRegistry`

当前它已经存在，但还没有在默认编辑器里注册太多真实 service。

因此目前它更像“基础设施预留位”，而不是“已经承载复杂共享对象的完整容器”。

## 4. 当前明确还没有实现的部分

下面这些能力当前没有，不要误以为已经支持。

### 4.1 远程插件下载

当前 `sync.py` 不会:

- clone GitHub 仓库
- fetch 更新
- checkout 指定 revision

它只扫描当前工作区里已经存在的本地目录。

### 4.2 完整 lock file

当前 lock file 还没有:

- Git URL
- revision
- 校验信息
- 来源类型

它们现在更像“本地解析结果摘要”。

### 4.3 `optional_dependencies`

当前没有实现可选依赖逻辑。

### 4.4 Render 插件注册表

当前没有:

- `RenderRegistry`
- `RenderFeature`
- Render phase 注入

### 4.5 Asset 插件注册表

当前没有:

- `AssetRegistry`
- importer 扩展点
- 虚拟文件系统挂载注册

### 4.6 Inspector / Menu / Toolbar 扩展点

当前只有:

- Panel
- Command

还没有更细粒度的编辑器扩展协议。

### 4.7 二进制插件

当前完全没有这条链路。

### 4.8 热重载

当前完全没有，也不应该优先做。

## 5. 当前系统的真实边界

理解这条边界非常重要。

### 5.1 现在已经是插件系统

因为它已经具备:

- 插件元数据
- 插件选择
- 插件构建图接入
- 插件注册入口
- 插件运行时贡献

### 5.2 但它还不是完整生态系统

因为它还缺:

- 下载
- 安装
- 升级
- 版本求解
- 多宿主大规模验证
- 更多注册表

所以最准确的说法是:

> Luna 当前已经有一个可工作的插件系统骨架，但还没有形成完整插件生态。

## 6. 当前最值得保持的设计选择

下面这些方向，当前看是正确的，后面最好继续坚持。

### 6.1 显式注册优先

`ResolvedPlugins.cpp` 明确列出所有插件入口，这比静态自动注册更透明。

### 6.2 插件是单独目标

插件作为独立 CMake target 参与构建，这是非常重要的边界。

### 6.3 宿主框架与插件实现分离

- `Editor/` 是宿主框架
- `Plugins/...` 是具体插件

### 6.4 工具层优先用 Python

`sync.py` 这种“读配置、扫目录、生成文件”的工具，用 Python 明显比 C++ 更高效。

## 7. 当前最明显的风险点

### 7.1 元数据字段看起来很多，但真正生效的不多

如果继续加字段却不让它们真正参与逻辑，manifest 会越来越像“看起来很完整”的假接口。

### 7.2 `Editor/` 可能再次长回“大杂烩”

如果后面继续把面板、工具、工作流逻辑直接写回 `Editor/`，那插件系统的边界会很快失效。

### 7.3 `sync.py` 可能变成无结构大脚本

当前脚本还小，后面如果继续扩功能，最好拆成:

- manifest parsing
- dependency resolve
- file generation
- validation

而不是无限往一个文件里堆。

## 8. 下一阶段最合理的推进顺序

如果按“工程收益最大、复杂度最可控”的顺序推进，我建议这样做。

### Phase 1: 做强当前最小链路

建议优先补:

- `optional_dependencies`
- 更严格的 `sdk` 校验
- 更严格的 `kind` / `host` 校验
- 更明确的错误输出
- `sync.py` 的 `validate` / `resolve` / `sync` 子命令拆分

原因:

- 这些都基于当前已经存在的最小骨架
- 收益高，风险低

### Phase 2: 新增几个真正的 builtin plugin

建议优先拆:

- `luna.viewport`
- `luna.asset.browser`
- `luna.scene.editor`

原因:

- 只有插件数量变多，依赖解析、Bundle、注册表这套体系的价值才会真正体现出来

### Phase 3: 扩大 EditorRegistry

建议逐步加入:

- Menu item
- Toolbar item
- Inspector provider
- Dock layout contribution

### Phase 4: 扩展更多注册表

后续再考虑:

- `RenderRegistry`
- `AssetRegistry`
- `ProjectTemplateRegistry`

### Phase 5: 远程插件同步

只有当前本地插件体系稳定后，才值得继续做:

- Git 下载
- revision 锁定
- 更完整的 lock file

## 9. 当前不建议优先做的方向

下面这些方向当前都不应该排在前面:

- 二进制插件
- 插件热重载
- GUI 插件商店
- 复杂版本求解器
- 跨平台发布工具链

因为这些方向会把复杂度迅速拉高，但对当前“把宿主和插件边界做实”帮助有限。

## 10. 当前一句话判断

如果你问“Luna 现在有插件系统吗”，答案是:

> 有，而且最小链路已经打通。

如果你问“Luna 现在有成熟插件生态吗”，答案是:

> 还没有，当前阶段更准确的定位是一个可靠的插件系统骨架。
