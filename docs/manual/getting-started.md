# 第三部分: 快速入门

## 安装与配置

### 环境要求

根据 `CMakeLists.txt` 和本地构建缓存，Luna 的基础要求如下:

| 项目 | 要求 |
| --- | --- |
| CMake | 3.16 或更高 |
| C++ 编译器 | 支持 C++20 |
| 图形 API | Vulkan SDK |
| 平台库 | GLFW |
| GUI | ImGui docking 分支 |
| 日志 | spdlog |
| 资源解析 | fastgltf、tinyobjloader、stb、tinyddsloader |
| Shader 工具 | glslang、SPIRV-Cross |

当前仓库在本机快照中已经验证过以下一组构建环境:

| 组件 | 观测值 |
| --- | --- |
| 生成器 | Ninja |
| C++ 编译器 | Clang++ |
| Vulkan SDK | 1.4.328.1 |
| 平台 | Windows |

### 获取源码

```powershell
git clone <your-repo-url> Luna
cd Luna
git submodule update --init --recursive
```

### 配置与构建

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

生成的可执行文件是:

```text
build/LunaApp.exe
```

### 运行

```powershell
.\build\LunaApp.exe
```

首次运行时，你应该看到:

- 一个 GLFW 创建的原生窗口
- Vulkan 初始化的渲染上下文
- ImGui DockSpace
- 一个名为 `Renderer` 的面板
- 鼠标右键 + `WASD/QE/Shift` 控制的自由摄像机

> **提示 (Note):**
> 当前默认场景没有真正的网格绘制。你看到的核心可见结果是清屏颜色和 ImGui 面板，这是符合源码现状的。

## 你的第一个 Luna 实体

对于当前仓库，最简单的“Hello World”不是写一个完整渲染 pass，而是写一个自己的 `Layer`，并让它在 ImGui 面板中输出内容。

### 示例 1: 最小自定义 Layer

```cpp
#include "Core/Layer.h"
#include "imgui.h"

class HelloLayer final : public luna::Layer {
public:
    HelloLayer() : Layer("HelloLayer") {}

    void onImGuiRender() override {
        ImGui::Begin("Hello");
        ImGui::TextUnformatted("Hello from Luna.");
        ImGui::End();
    }
};
```

### 示例 2: 在应用中挂载 Layer

```cpp
#include "Core/Application.h"
#include <memory>

class HelloApp final : public luna::Application {
public:
    HelloApp()
        : Application(luna::ApplicationSpecification{
              .m_name = "Hello Luna",
              .m_window_width = 1280,
              .m_window_height = 720,
          }) {}

protected:
    void onInit() override {
        pushLayer(std::make_unique<HelloLayer>());
    }
};

namespace luna {
Application* createApplication(int, char**) {
    return new HelloApp();
}
}
```

### 这个示例为什么有效

因为 `Application::run()` 已经帮你做了下面这些事:

1. 驱动窗口消息循环
2. 计算 `Timestep`
3. 调用每个 Layer 的 `onUpdate()`
4. 开启 ImGui 帧
5. 调用每个 Layer 的 `onImGuiRender()`
6. 驱动渲染器执行 RenderGraph

## 第一个渲染扩展点: 新增 RenderPass

当前 `VulkanRenderer::rebuildRenderGraph()` 已经是最好的示范。你可以仿照它添加新 pass:

```cpp
luna::val::RenderGraphBuilder builder;
builder
    .AddRenderPass("clear", std::make_unique<MyClearPass>())
    .AddRenderPass("imgui", std::make_unique<luna::val::ImGuiRenderPass>("scene_color"))
    .SetOutputName("scene_color");

auto graph = builder.Build();
```

关键点不是“如何手写 Vulkan 命令”，而是“如何声明附件、资源依赖与输出关系”。

如果你准备做一个真正独立的应用，而不是只在默认编辑器里试验一个 pass，下一步应继续阅读:

- [使用现有框架与 RenderGraph 构建应用](./building-an-application-with-rendergraph.md)

## 常见启动问题

| 现象 | 原因 | 解决方式 |
| --- | --- | --- |
| CMake 找不到 Vulkan | 未安装 Vulkan SDK 或环境变量未配置 | 安装 Vulkan SDK 并重新打开终端 |
| 子模块目录为空 | 未拉取递归子模块 | 执行 `git submodule update --init --recursive` |
| 程序闪退 | Vulkan validation layer 或设备初始化失败 | 先检查 `logs/luna.log` |
| 窗口创建失败 | GLFW 初始化失败或平台环境异常 | 检查图形驱动与系统权限 |
| 启动后黑屏但无 UI | ImGui 初始化失败或 RenderGraph 构建失败 | 查看日志中的 `ImGui` / `VAL` 输出 |
