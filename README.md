# treelike_ui

`treelike_ui` 是从 `yunqinglt/stm32_in_c` 的 SDL framebuffer 实验中拆出的、
不依赖 SDL 的 C11 UI 核心。仓库保留原有 `ui/...` include 形状，现有嵌入式端和
桌面模拟端可以继续使用同一套 framebuffer、dirty region 和树形控件接口。

## 内容边界

- `player_conf.h`：`pixel_t`、RGB565/RGB888 选择、颜色和当前兼容配置。
- `ui/ui_surface.*`：framebuffer 所有权、stride、blit 与 dirty region。
- `ui/ui_drawer.*`：共享 surface 的树形 buffer、控件绘制与拓扑调试。
- `ui/experimental/`：尚未纳入稳定目标的块池和对象模型实验。
- `tests/ui_core_tests.c`：不链接 SDL 的核心单元测试。

`player_conf.h` 目前仍保留播放器时期的日志/FPS 宏，以维持已有调用方的源码兼容；
UI target 对外公开像素格式宏，避免不同调用方编译出不一致的 `pixel_t` ABI。

## 独立构建

```sh
git clone git@github.com:yunqinglt/treelike_ui.git
cd treelike_ui
cmake -S . -B out/build -DTREELIKE_UI_BUILD_TESTS=ON
cmake --build out/build
ctest --test-dir out/build --output-on-failure
```

Visual Studio 等多配置生成器在 build/test 命令中另加 `--config Debug`。

像素格式默认是 RGB565；可在配置时改为 RGB888：

```sh
cmake -S . -B out/build -DTREELIKE_UI_PIXEL_FORMAT=RGB888
```

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
```

兼容旧构建的 `ui_core` alias 暂时保留；新代码应链接
`treelike_ui::treelike_ui`。
