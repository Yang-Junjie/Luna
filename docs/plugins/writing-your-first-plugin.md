# 编写你的第一个 Luna 插件

## 1. 先明确当前适用范围

本文档讨论的是“按 Luna 当前代码状态写一个可工作的插件”。

这意味着:

- 插件以源码形式存在于工作区
- 你通过 Bundle 启用它
- 你运行 `python Tools/luna/sync.py` 生成中间文件
- 你再通过 CMake 编译整个宿主

当前最适合写的插件类型是:

- 编辑器 Panel 插件
- 编辑器 Command 插件
- 提供 Layer / Overlay 的插件
- 运行时 Layer 插件

仓库里当前已经有两个可直接参考的示例插件:

- `Plugins/builtin/luna.example.hello`
- `Plugins/builtin/luna.example.imgui_demo`

如果你想看 runtime host 下的最小插件，也可以直接打开:

- `Plugins/builtin/luna.runtime.core`

当前不建议你一开始就写:

- 远程下载插件
- 二进制插件
- RenderGraph 深度注入插件
- Asset importer / project template 的复杂插件

因为这些扩展点还没有完整落地。

## 2. 当前推荐的最小目标

如果你是第一次给 Luna 写插件，建议目标定成:

> 给编辑器增加一个新 Panel，或者给编辑器增加一个 Command

这是当前代码里最稳定、最容易成功、最能体现插件系统价值的路径。

## 3. 你需要准备什么

至少需要:

- 一个插件目录
- 一个 `luna.plugin.toml`
- 一个 `CMakeLists.txt`
- 一个注册函数
- 至少一个具体贡献对象，例如 Panel 或 Layer
- 一个启用了该插件的 Bundle
- 一个支持 `tomllib` 的 Python 版本，建议 Python 3.11 或更新版本

## 4. 当前最小目录模板

建议新插件先按这个结构起步:

```text
Plugins/
└─ builtin/
   └─ luna.example.hello/
      ├─ luna.plugin.toml
      ├─ CMakeLists.txt
      └─ src/
         ├─ HelloPlugin.cpp
         ├─ HelloPanel.h
         └─ HelloPanel.cpp
```

如果你想做外部插件，也可以放到:

```text
Plugins/external/luna.example.hello
```

当前 `sync.py` 会同时扫描 `builtin` 和 `external`。

如果你想先看一个真实而不是文档里的模板，可以直接打开:

- `Plugins/builtin/luna.example.hello`
- `Plugins/builtin/luna.example.imgui_demo`

## 5. 第一步: 写 `luna.plugin.toml`

一个最小编辑器插件 manifest 可以写成这样:

```toml
id = "luna.example.hello"
name = "Hello Plugin"
version = "0.1.0"
sdk = "0.1"
kind = "editor"
cmake_target = "LunaExampleHelloPlugin"
entry = "luna_register_luna_example_hello"
hosts = ["editor"]

[dependencies]
```

注意:

- `luna.example.hello` 现在已经是仓库里的真实示例插件 id
- 如果你在写自己的新插件，应当复制这个模板并改成你自己的唯一 id

### 5.1 字段解释

- `id`
  - 插件唯一标识
  - 在整个工作区里必须唯一

- `name`
  - 给人看的名字
  - 当前主要是元数据

- `version`
  - 插件版本
  - 当前写入 lock file

- `sdk`
  - 目标 SDK 版本
  - 当前还没有严格版本校验

- `kind`
  - 插件类别
  - 当前更多是信息字段

- `cmake_target`
  - 插件的 CMake target 名称
  - `sync.py` 会把它写进 `PluginList.cmake`

- `entry`
  - 插件注册函数名
  - `sync.py` 会把它写进 `ResolvedPlugins.cpp`

- `hosts`
  - 允许该插件被哪些宿主使用
  - 当前 `sync.py` 会根据 Bundle 的 `host` 做校验

- `[dependencies]`
  - 依赖的其他插件
  - 当前 key 会参与依赖拓扑排序，value 只要求是字符串

## 6. 第二步: 写插件的 `CMakeLists.txt`

如果这是一个给当前编辑器宿主使用的插件，最小 CMake 可以写成这样:

```cmake
add_library(
    LunaExampleHelloPlugin
    STATIC
    luna.plugin.toml
    src/HelloPlugin.cpp
    src/HelloPanel.h
    src/HelloPanel.cpp
)

target_include_directories(
    LunaExampleHelloPlugin
    PUBLIC
        ${PROJECT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(
    LunaExampleHelloPlugin
    PUBLIC
        LunaEditorFramework
)
```

### 6.1 为什么编辑器插件要链接 `LunaEditorFramework`

因为当前编辑器插件最常用到的接口都在这里:

- `EditorPanel`
- `EditorRegistry`
- `EditorShellLayer` 所依赖的扩展协议

如果你只是写一个纯 Layer 插件，也通常仍然会链接 `LunaEditorFramework`，因为当前生成链路是挂在编辑器宿主上的。

如果你写的是纯 runtime 插件，并且不依赖 `EditorRegistry` / `EditorPanel` 这些编辑器接口，那么直接链接 `LunaCore` 就够了。当前 `Plugins/builtin/luna.runtime.core` 就是这个模式。

## 7. 第三步: 写一个 Panel

先定义一个最小 Panel:

```cpp
#pragma once

#include "Editor/EditorPanel.h"

namespace luna::example {

class HelloPanel final : public luna::editor::EditorPanel {
public:
    void onImGuiRender() override;
};

} // namespace luna::example
```

然后实现它:

```cpp
#include "HelloPanel.h"

#include "imgui.h"

namespace luna::example {

void HelloPanel::onImGuiRender()
{
    ImGui::TextUnformatted("Hello from plugin.");
}

} // namespace luna::example
```

### 7.1 `EditorPanel` 的最小要求

当前 `EditorPanel` 很简单:

- 可以覆写 `onAttach()`
- 可以覆写 `onDetach()`
- 必须实现 `onImGuiRender()`

如果你的 Panel 没有复杂状态，这个接口已经够用。

## 8. 第四步: 写插件注册函数

这是当前最重要的一步。

你的插件必须提供一个和 manifest 里 `entry` 对应的函数，例如:

```cpp
#include "HelloPanel.h"

#include "Editor/EditorRegistry.h"
#include "Plugin/PluginRegistry.h"

extern "C" void luna_register_luna_example_hello(luna::PluginRegistry& registry)
{
    if (!registry.hasEditorRegistry()) {
        return;
    }

    auto& editor_registry = registry.editor();
    editor_registry.addPanel<luna::example::HelloPanel>(
        "luna.example.hello.panel",
        "Hello",
        true);
}
```

### 8.1 为什么要用 `extern "C"`

当前 `sync.py` 只是把入口名按字符串写进生成文件。

使用 `extern "C"` 的好处是:

- 避免 C++ 名字改编
- 入口名和 manifest 保持一一对应
- 生成文件里更容易直接声明和调用

### 8.2 为什么不要依赖静态自动注册

当前 Luna 的方向是:

- 插件注册顺序显式
- 入口函数显式
- 生成文件显式

这比靠静态全局对象构造自动注册更稳、更容易调试。

## 9. 第五步: 如果你要注册 Layer

如果插件想注册一个普通 Layer，可以这样写:

```cpp
registry.addLayer("luna.example.runtime_layer", [] {
    return std::make_unique<MyLayer>();
});
```

如果想注册 Overlay:

```cpp
registry.addOverlay("luna.example.overlay", [] {
    return std::make_unique<MyOverlayLayer>();
});
```

### 9.1 当前 Layer 贡献的装配方式

`EditorApp` 会在插件注册完成后遍历 `plugin_registry.layers()`，然后:

- 如果是 `overlay`，调用 `pushOverlay()`
- 否则调用 `pushLayer()`

因此，插件不需要也不应该自己直接操纵宿主的主循环。

## 10. 第六步: 把插件加入 Bundle

你的插件不会自动生效。

你必须把它加到某个 Bundle 的 `[plugins].enabled` 里。

例如修改:

```text
Bundles/EditorDefault/luna.bundle.toml
```

让它变成:

```toml
[plugins]
enabled = [
  "luna.editor.core",
  "luna.example.hello",
]
```

如果你写的是 runtime 插件，就把它加入 runtime Bundle，例如:

```text
Bundles/RuntimeDefault/luna.bundle.toml
```

## 11. 第七步: 运行 `sync.py`

在项目根目录执行:

```powershell
python Tools\luna\sync.py --project-root . --bundle Bundles/EditorDefault/luna.bundle.toml --generated-dir Plugins/Generated/editor --lock-file luna.editor.lock
```

这一步当前必须手工执行。

如果你改了 Bundle 或任意插件 manifest，就要重新执行一次 `sync.py`，这样才会刷新:

- `Plugins/Generated/editor/PluginList.cmake`
- `Plugins/Generated/editor/ResolvedPlugins.h`
- `Plugins/Generated/editor/ResolvedPlugins.cpp`
- `luna.editor.lock`

执行完成后，当前会生成:

- `Plugins/Generated/editor/PluginList.cmake`
- `Plugins/Generated/editor/ResolvedPlugins.h`
- `Plugins/Generated/editor/ResolvedPlugins.cpp`
- `luna.editor.lock`

如果你在同步 runtime Bundle，对应命令是:

```powershell
python Tools\luna\sync.py --project-root . --bundle Bundles/RuntimeDefault/luna.bundle.toml --generated-dir Plugins/Generated/runtime --lock-file luna.runtime.lock
```

### 11.1 如果你不运行 `sync.py` 会怎样

如果你改了 Bundle 或 manifest，但没有重新运行 `sync.py`，那么:

- 新插件不会被加入 `PluginList.cmake`
- 新入口不会被写进 `ResolvedPlugins.cpp`
- CMake 和运行时都看不到你的新插件

这是当前最常见的错误之一。

## 12. 第八步: 重新配置并编译

当前默认建议流程:

```powershell
python Tools\luna\sync.py --project-root . --bundle Bundles/EditorDefault/luna.bundle.toml --generated-dir Plugins/Generated/editor --lock-file luna.editor.lock
cmake -S . -B build
cmake --build build --config Debug --target LunaEditorApp
```

如果你换了别的 Bundle，就把 `--bundle` 改成对应路径:

```powershell
python Tools\luna\sync.py --project-root . --bundle Bundles/YourBundle/luna.bundle.toml --generated-dir Plugins/Generated/editor --lock-file luna.editor.lock
cmake -S . -B build
cmake --build build --config Debug --target LunaEditorApp
```

如果你要编 runtime 宿主，则使用:

```powershell
python Tools\luna\sync.py --project-root . --bundle Bundles/RuntimeDefault/luna.bundle.toml --generated-dir Plugins/Generated/runtime --lock-file luna.runtime.lock
cmake -S . -B build
cmake --build build --config Debug --target LunaRuntimeApp
```

## 13. 第九步: 验证插件是否生效

如果你写的是 Panel 插件，最直接的验证方式是:

- 启动编辑器
- 查看菜单栏里的 `Panels`
- 查看你的 Panel 是否出现

如果你写的是 Command 插件:

- 查看菜单栏里的 `Commands`
- 点击后观察效果是否触发

如果你写的是 Layer:

- 看 Layer 的更新逻辑、渲染逻辑或日志是否生效

## 14. 一个完整的最小示例

下面这个插件只做一件事: 给编辑器增加一个 `Hello` 面板。

### `luna.plugin.toml`

```toml
id = "luna.example.hello"
name = "Hello Plugin"
version = "0.1.0"
sdk = "0.1"
kind = "editor"
cmake_target = "LunaExampleHelloPlugin"
entry = "luna_register_luna_example_hello"
hosts = ["editor"]

[dependencies]
```

### `HelloPanel.h`

```cpp
#pragma once

#include "Editor/EditorPanel.h"

namespace luna::example {

class HelloPanel final : public luna::editor::EditorPanel {
public:
    void onImGuiRender() override;
};

} // namespace luna::example
```

### `HelloPanel.cpp`

```cpp
#include "HelloPanel.h"

#include "imgui.h"

namespace luna::example {

void HelloPanel::onImGuiRender()
{
    ImGui::TextUnformatted("Hello from luna.example.hello");
}

} // namespace luna::example
```

### `HelloPlugin.cpp`

```cpp
#include "HelloPanel.h"

#include "Editor/EditorRegistry.h"
#include "Plugin/PluginRegistry.h"

extern "C" void luna_register_luna_example_hello(luna::PluginRegistry& registry)
{
    if (!registry.hasEditorRegistry()) {
        return;
    }

    registry.editor().addPanel<luna::example::HelloPanel>(
        "luna.example.hello.panel",
        "Hello",
        true);
}
```

## 15. 当前支持的插件写法模式

### 15.1 只注册 Panel

最简单、最推荐的入门方式。

### 15.2 只注册 Command

适合做快捷功能或动作入口。

### 15.3 注册 Layer

适合做大粒度运行时逻辑，例如:

- 相机控制
- 调试绘制
- 大型工具层

### 15.4 Panel + Command + Layer 混合

当前 builtin 插件 `luna.editor.core` 就是这种组合。

## 16. 当前最常见的错误

### 16.1 `id` 重复

如果两个插件用了相同 `id`，`sync.py` 会失败。

### 16.2 `entry` 和实际函数名不一致

manifest 里写的是一个名字，代码里实现的是另一个名字，编译或链接阶段就会出问题。

### 16.3 忘记把插件加入 Bundle

插件目录存在，并不代表插件会被启用。

### 16.4 改了 Bundle 但没重新运行 `sync.py`

这是最常见的问题。

### 16.5 编辑器插件没有链接 `LunaEditorFramework`

如果你的插件需要 `EditorRegistry`、`EditorPanel`，就应该链接 `LunaEditorFramework`。

### 16.6 `hosts = ["editor"]` 写错

当前 Bundle 默认 host 是 `editor`，如果插件 host 不匹配，`sync.py` 会直接失败。

## 17. 当前不建议你做的事

当前不建议你在新插件里做下面这些事:

- 直接改 `EditorApp` 来实现插件逻辑
- 依赖静态全局对象做自动注册
- 手工编辑 `Plugins/Generated/ResolvedPlugins.cpp`
- 手工编辑 `Plugins/Generated/PluginList.cmake`
- 把具体插件代码继续塞回 `Editor/`

## 18. 当前一句话建议

如果你现在要写一个 Luna 插件，最稳妥的路线是:

> 先写一个 editor host 下的 Panel 或 Command 插件，把 manifest、CMake、注册函数、Bundle、`sync.py` 这条最小链路跑通，再继续往更复杂的能力扩展。

如果你想先对照真实代码，再回来看文档，优先看:

- `Plugins/builtin/luna.example.hello`
- `Plugins/builtin/luna.example.imgui_demo`
- `Plugins/builtin/luna.editor.core`
- `Plugins/builtin/luna.runtime.core`
