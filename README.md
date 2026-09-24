# treelike_ui

`treelike_ui` 的 surface、树和基础字体渲染是一个不依赖窗口系统的 C11 framebuffer
UI 核心。嵌入式显示端和桌面模拟端共享同一套像素格式、dirty region、surface 与树形
控件绘制代码；Windows 可选的 GDI outline path backend 是编译期平台扩展；
[`yunqinglt/sdl_player`](https://github.com/yunqinglt/sdl_player) 作为开发期 SDL2
预览器接入，但不会成为库的运行时依赖。

## 工程树

```text
treelike_ui/
├─ CMakeLists.txt                 根选项、测试、安装与 SDL 联调入口
├─ CMakePresets.json             Windows/Linux、RGB565/RGB888 开发预设
├─ assets/fonts/                 带独立授权的桌面字体测试资源
├─ cmake/                        find_package 配置模板
├─ player_conf.h                 pixel_t、颜色与 framebuffer 公共配置
├─ ui/
│  ├─ CMakeLists.txt             treelike_ui::treelike_ui 静态库
│  ├─ ui_surface.*               framebuffer、stride、blit、dirty region
│  ├─ ui_drawer.*                UiBuffer 树、dirty 传播与渲染调度
│  ├─ buffer_font_render.*        点阵与可缩放笔画字体渲染服务
│  ├─ ui_object_raw.*            直接绘制回调、临近分组与调试控件
│  └─ experimental/              未进入稳定 target 的对象模型草稿
├─ tests/
│  ├─ CMakeLists.txt
│  ├─ fixtures/                  字体格式与 240×320 RGB565 图片 fixture
│  └─ ui_core_tests.c            不链接 SDL 的核心单元测试
└─ sdl_player/                   固定版本的 Git submodule，桌面预览与冒烟测试
```

`treelike_ui` 始终显式构建为静态库，避免父工程的 `BUILD_SHARED_LIBS` 意外改变
嵌入式链接模型。像素格式宏通过 target 的 PUBLIC usage requirements 传播，保证
UI、测试与 SDL 端使用同一个 `pixel_t` ABI。

`ui_drawer` 只负责共享 framebuffer 上的 Buffer 树和 dirty/render 生命周期；
`ui_object_raw` 则承载直接操作 `UiBuffer` 的低级回调，以及供 SDL 演示使用的临近分组
构建器。高级对象或业务控件可以复用 `UiDrawCallback` 契约，但应自行拥有 context，
不应把这些 raw 示例当成 retained `UiObject` 的基类。

`buffer_font_render` 是与对象模型解耦的绘制服务，内置 1-bpp 5×7 点阵字体和基于几何
线段、按目标字号重新光栅化的 scalable stroke/vector font。后者不是 TrueType outline
引擎。两种引擎都直接输出 `pixel_t`，可把 `buffer_font_draw` 绑定到
`UiControl.draw`；渲染器本身不是 `UiObject` 派生类，也不依赖 SDL。

平台字体可以使用可审阅的 ASCII `TLFNT1` 文件；Windows 启用可选 GDI TrueType backend
时，也可把真实 TTF outline 字体作为另一种 path 来源。二者与内置 stroke font 采用
不同来源和后端，不能把 TTF 当作 `TLFNT1` 解析。`buffer_font_load()` 把
`font_type_t.path` 绑定到调用方提供的
`UiFontStorage`；`font_type_t.addr` 保留给后续 SPI Flash 端口，当前只带 addr 的来源会
返回 unsupported，平台实现不会直接解引用该地址。

根工程在 Windows 下启用 TrueType backend 时，会使用
`assets/fonts/fantasque_sans_mono/FantasqueSansMono-Regular.ttf`，并只把它复制到 SDL
demo 的 build tree 用于预览和冒烟测试；字体不会进入安装包。仓库只收录测试实际使用的
Regular TTF，其 SIL Open Font License 1.1 位于同目录的 `OFL.txt`，不受本项目 GPLv2
代码许可证覆盖。

## 最快的开发—测试循环

首次获取源码时连同预览器一起检出：

```sh
git clone --recurse-submodules git@github.com:yunqinglt/treelike_ui.git
cd treelike_ui
```

已有 checkout 补齐子模块：

```sh
git submodule update --init --recursive
```

Windows（本机有 Visual Studio 2026 时）：

```powershell
cmake --preset windows-vs2026
cmake --build --preset windows-vs2026
ctest --preset windows-vs2026
cmake --build --preset windows-vs2026 --target treelike-ui-smoke
cmake --build --preset windows-vs2026 --target treelike-ui-run
```

Visual Studio 2022 使用同名的 `windows-vs2022` 预设。Linux 使用：

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
cmake --build --preset linux-debug --target treelike-ui-smoke
cmake --build --preset linux-debug --target treelike-ui-run
```

第一次 configure 后，日常循环只需重新 build，然后运行 CTest 或
`treelike-ui-smoke`；后者使用 SDL dummy driver 跑 10 帧，不打开窗口。
`treelike-ui-run` 会打开 `sdl_player` 的 phase 3 UI-tree 字体预览；`SPACE` 切换画面，
`ESC` 退出。配置日志应显示：

```text
treelike_ui: existing target
```

这表示播放器正在链接当前工作树中的 UI，而不是 FetchContent 中固定的旧提交。
需要把新控件接入可视 demo 时，在 `sdl_player/app/demo.c` 的 phase 3 组合它；纯算法、
裁剪和 dirty-region 行为应优先补入 `tests/ui_core_tests.c`。

RGB888 使用独立构建目录，避免与 RGB565 cache 混用：

```powershell
cmake --preset windows-vs2026-rgb888
cmake --build --preset windows-vs2026-rgb888
ctest --preset windows-vs2026-rgb888
```

CMake 优先查找系统、vcpkg 或 MSYS2 的 SDL2；找不到时由播放器下载 SDL 2.30.11。
离线开发可在被 Git 忽略的 `CMakeUserPresets.json` 中设置 `SDL2_DIR`，或在命令行传入
`-DSDL2_DIR=... -DSDL_PLAYER_FETCH_SDL2=OFF`。

## 仅构建 UI 核心

SDL 联调默认关闭，所以普通 CMake 消费者不会下载或构建 SDL：

```sh
cmake -S . -B out/build/core \
  -DBUILD_TESTING=ON \
  -DTREELIKE_UI_BUILD_SDL_PLAYER=OFF
cmake --build out/build/core
ctest --test-dir out/build/core --output-on-failure
```

Visual Studio 等多配置生成器在 build/test 命令中另加 `--config Debug` 或 `-C Debug`。
`TREELIKE_UI_BUILD_TESTS` 仍作为兼容开关保留；新顶层构建也支持标准的
`BUILD_TESTING=OFF`。

## 作为 CMake 子项目使用

```cmake
add_subdirectory(path/to/treelike_ui)
target_link_libraries(my_app PRIVATE treelike_ui::treelike_ui)
```

公共头文件沿用原路径：

```c
#include "player_conf.h"
#include "ui/ui_surface.h"
#include "ui/ui_drawer.h"
#include "ui/buffer_font_render.h" /* 点阵与 stroke/vector 字体服务 */
#include "ui/ui_object_raw.h" /* 仅在使用低级 raw 控件时需要 */
```

兼容旧构建的 `ui_core` alias 只在源码树中保留；新代码应链接带 namespace 的 target。

## 安装后使用

```sh
cmake --install out/build/core --prefix out/install
```

Visual Studio 多配置构建应指定刚才构建的配置：

```powershell
cmake --install out/build/core --config Debug --prefix out/install
```

消费端：

```cmake
find_package(treelike_ui CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE treelike_ui::treelike_ui)
```

通过 `CMAKE_PREFIX_PATH` 或 `treelike_ui_DIR` 指向安装前缀。安装内容只包含稳定头文件、
静态库和 CMake package；`ui/experimental/` 不安装，也不参与当前构建和测试。

## 主要选项

| 选项 | 默认值 | 作用 |
| --- | --- | --- |
| `TREELIKE_UI_PIXEL_FORMAT` | `RGB565` | 选择 `RGB565` 或 `RGB888` |
| `TREELIKE_UI_BUILD_TESTS` | 顶层跟随 `BUILD_TESTING` | 构建 UI core tests |
| `TREELIKE_UI_BUILD_SDL_PLAYER` | `OFF` | 加入 SDL 预览与无头测试 |
| `TREELIKE_UI_ENABLE_TRUETYPE` | Windows 为 `ON`，其他平台为 `OFF` | 启用 Win32/GDI outline path backend |
| `TREELIKE_UI_SDL_PLAYER_SOURCE_DIR` | `sdl_player/` | 改用另一个本地播放器 checkout |
| `TREELIKE_UI_INSTALL` | 顶层为 `ON` | 生成 install/export 规则 |

`player_conf.h` 目前仍保留播放器时期的日志/FPS 宏，以维持现有调用方源码兼容。
