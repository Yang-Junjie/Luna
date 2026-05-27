# Luna

Luna 是一个 C++20/CMake 项目，包含核心引擎库、RHI、编辑器、运行时、CLI、脚本插件和示例工程。使用 CacaoRHI。

该项目已经被屎山淹没，暂时停止维护。

本文重点说明如何从源码构建。

## 目录结构

- `Luna/`：核心引擎、RHI、渲染、场景、资产、脚本系统。
- `Editor/`：`LunaEditor` 编辑器可执行程序。
- `Runtime/`：`LunaRuntime` 运行时可执行程序。
- `CLI/`：`LunaCLI` 和 `LunaAuthoringHost`。
- `Plugins/`：脚本插件和编辑器插件。
- `SDK/`：编辑器 Native Plugin SDK 和模板。
- `SampleProject/`：示例工程。
- `Tests/`：C++/CLI/SDK 合同测试。
- `third_party/`：第三方依赖，多数通过 Git submodule 管理。

## 构建依赖

必需：

- CMake 3.16 或更新版本。
- 支持 C++20 的编译器，例如 MSVC、Clang 或 GCC。
- Git submodule。
- Slang SDK，并且 CMake 能找到 `slang` 包。

按后端需要：

- Vulkan：需要 Vulkan SDK。项目默认启用 `LUNA_RHI_ENABLE_VULKAN=ON`。
- Windows：默认同时启用 D3D11 和 D3D12。
- macOS：默认启用 Metal，但 Vulkan 仍默认开启；如果没有 Vulkan SDK，配置时请关闭 Vulkan。
- Linux：默认使用 Vulkan；窗口系统通过 `LUNA_LINUX_WINDOW_SYSTEM=X11|Wayland` 选择，默认 `X11`。

可选：

- Ninja：推荐用于单配置构建。
- Node.js：如果存在，CTest 会注册 TypeScript CLI smoke tests。

## 获取源码

新克隆仓库时建议直接拉取 submodule：

```powershell
git clone --recursive <repo-url> Luna
cd Luna
```

如果已经克隆过仓库：

```powershell
git submodule update --init --recursive
```

## 配置构建

本项目只在windows上编译成功，其他平台未测试。

### Windows + Ninja

如果 Slang 安装在 `C:\slang`，并且包含 `C:\slang\cmake\slangConfig.cmake`：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -Dslang_DIR=C:/slang/cmake
```

也可以使用环境变量：

```powershell
$env:SLANG_DIR = "C:/slang/cmake"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
```

Windows 默认会启用 Vulkan、D3D11、D3D12。如果没有安装 Vulkan SDK，可以关闭 Vulkan：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -Dslang_DIR=C:/slang/cmake -DLUNA_RHI_ENABLE_VULKAN=OFF
```

### Windows + Visual Studio

```powershell
cmake -S . -B build-vs -G "Visual Studio 17 2022" -A x64 -Dslang_DIR=C:/slang/cmake
cmake --build build-vs --config Debug
```

### Linux

默认使用 Vulkan + X11：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -Dslang_DIR=/path/to/slang/cmake
cmake --build build
```

使用 Wayland：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -Dslang_DIR=/path/to/slang/cmake \
  -DLUNA_LINUX_WINDOW_SYSTEM=Wayland
```

### macOS

如果只构建 Metal 后端，可以关闭 Vulkan：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -Dslang_DIR=/path/to/slang/cmake \
  -DLUNA_RHI_ENABLE_VULKAN=OFF
cmake --build build
```

## 编译

编译全部默认目标：

```powershell
cmake --build build --config Debug
```

只编译某个目标：

```powershell
cmake --build build --target LunaEditor --config Debug
cmake --build build --target LunaRuntime --config Debug
cmake --build build --target LunaCLI --config Debug
cmake --build build --target LunaAuthoringHost --config Debug
```

Ninja 是单配置生成器，`--config Debug` 可以保留但通常不会影响结果。Visual Studio 是多配置生成器，运行和测试时需要使用对应的 `Debug`、`Release` 等配置。

## 常用 CMake 选项

- `BUILD_TESTING=ON|OFF`：是否启用 CTest。
- `LUNA_BUILD_TESTS=ON|OFF`：是否构建 `Tests/` 下的 Luna 测试，默认跟随 `BUILD_TESTING`。
- `LUNA_RHI_ENABLE_VULKAN=ON|OFF`：构建 Vulkan 后端，默认 `ON`。
- `LUNA_RHI_ENABLE_D3D11=ON|OFF`：构建 D3D11 后端，仅 Windows 默认 `ON`。
- `LUNA_RHI_ENABLE_D3D12=ON|OFF`：构建 D3D12 后端，仅 Windows 默认 `ON`。
- `LUNA_RHI_ENABLE_METAL=ON|OFF`：构建 Metal 后端，仅 Apple 平台默认 `ON`。
- `LUNA_RHI_ENABLE_OPENGL=ON|OFF`：构建桌面 OpenGL 后端，默认 `OFF`。
- `LUNA_RHI_ENABLE_WEBGPU=ON|OFF`：构建 WebGPU 后端，默认 `OFF`，需要预先提供 Dawn/WebGPU CMake target。
- `LUNA_LINUX_WINDOW_SYSTEM=X11|Wayland`：Linux 原生窗口系统，默认 `X11`。
- `slang_DIR=<path>` 或环境变量 `SLANG_DIR`：Slang CMake package 路径。

至少需要启用一个 RHI 后端。

## 运行

Ninja/单配置构建后的常见路径：

```powershell
.\build\Editor\LunaEditor.exe --backend d3d12
.\build\Runtime\LunaRuntime.exe --backend d3d12 --project "SampleProject\Sample Project.lunaproj"
.\build\CLI\LunaCLI.exe --help
```

Visual Studio/多配置构建后的路径通常带配置目录：

```powershell
.\build-vs\Editor\Debug\LunaEditor.exe --backend d3d12
.\build-vs\Runtime\Debug\LunaRuntime.exe --backend d3d12 --project "SampleProject\Sample Project.lunaproj"
.\build-vs\CLI\Debug\LunaCLI.exe --help
```

`--backend` 支持的常用值包括：

- `auto`
- `vulkan` 或 `vk`
- `d3d12`、`dx12` 或 `directx12`
- `d3d11`、`dx11` 或 `directx11`
- `metal` 或 `mtl`
- `opengl` 或 `gl`
- `webgpu` 或 `wgpu`

`LunaRuntime` 使用 `--project <path-to-.lunaproj>` 加载工程。示例工程入口是：

```text
SampleProject\Sample Project.lunaproj
```

## 测试

先确保配置时开启测试：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DLUNA_BUILD_TESTS=ON -Dslang_DIR=C:/slang/cmake
cmake --build build --config Debug
```

运行全部测试：

```powershell
ctest --test-dir build --output-on-failure -C Debug
```

只运行快速测试：

```powershell
ctest --test-dir build -L quick --output-on-failure -C Debug
```

如果安装了 Node.js，构建系统会额外注册 CLI TypeScript smoke tests；部分带 `ai` 标签的测试可能还需要本地 `.env` 或模型服务配置。

## 安装 Editor SDK

`SDK/Editor` 提供可安装的 `LunaEditorSDK` CMake package：

```powershell
cmake --install build --config Debug --component LunaEditorSDK --prefix install/LunaEditorSDK
```

安装后可以用模板构建一个 Native Editor Plugin：

```powershell
cmake -S install/LunaEditorSDK/share/LunaEditorSDK/Templates/NativePlugin -B build-native-plugin -DCMAKE_PREFIX_PATH=install/LunaEditorSDK
cmake --build build-native-plugin --config Debug
```

## 常见问题

`find_package(slang REQUIRED)` 失败：

- 确认已安装 Slang SDK。
- 通过 `-Dslang_DIR=<slang-cmake-package-dir>` 或 `SLANG_DIR` 指向包含 `slangConfig.cmake` 的目录。

`find_package(Vulkan REQUIRED)` 失败：

- 安装 Vulkan SDK，或在已有其他可用后端时添加 `-DLUNA_RHI_ENABLE_VULKAN=OFF`。

Linux 配置 GLFW/Vulkan surface 失败：

- 明确设置 `-DLUNA_LINUX_WINDOW_SYSTEM=X11` 或 `-DLUNA_LINUX_WINDOW_SYSTEM=Wayland`。

运行时提示请求的 RHI 后端没有编译进当前构建：

- 检查对应 `LUNA_RHI_ENABLE_*` 选项是否为 `ON`，然后重新配置并构建。
