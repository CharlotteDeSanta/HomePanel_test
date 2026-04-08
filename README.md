# HomePanel Test

基于 `STM32H743` 的家庭控制面板实验工程，当前已经完成显示、外部资源、TouchGFX 和 GT911 触摸的基础适配，可以作为后续界面与业务功能开发的工作底座。

## 当前状态

当前代码已验证通过的内容：

- `800x480 RGB` 屏幕可稳定显示完整 TouchGFX 界面
- `SDRAM + LTDC` 双帧缓冲工作正常
- 外部 `QSPI Flash` 图片资源可正常读取和显示
- `GT911` 触摸已接通，TouchGFX 可正常响应点击
- 主界面帧率稳定，重动画场景也已做过一轮性能优化

当前结论：

- 外部资源方案按 **单片 `W25Q256`** 工作，不是双闪
- 原生 `STM32Cube` VS Code 调试/下载在当前环境已验证可用
- 自定义 `J-Link` 配置仍保留，作为显式、可复现的备用方案

## 硬件适配摘要

### 显示

- 分辨率：`800 x 480`
- 像素格式：`ARGB8888`
- 主帧缓冲地址：`0xD0000000`
- 第二帧缓冲地址：`0xD0177000`
- 帧缓冲位于外部 `SDRAM`

相关定义见：

- [bsp_config.h](/C:/Users/20953/Documents/MCUProjects/HomePanel_test/BSP/Inc/bsp_config.h)
- [TouchGFXHAL.cpp](/C:/Users/20953/Documents/MCUProjects/HomePanel_test/TouchGFX/target/TouchGFXHAL.cpp)

### SDRAM

- 基地址：`0xD0000000`
- 容量：`64 MB`

### QSPI 外部资源

- 基地址：`0x90000000`
- 容量：`32 MB`
- 当前按 **单片 `W25Q256`** 配置
- TouchGFX 图片资源通过 `memory-mapped` 模式读取

相关实现见：

- [bsp_qspi_flash.c](/C:/Users/20953/Documents/MCUProjects/HomePanel_test/BSP/Src/bsp_qspi_flash.c)
- [STM32H743XX_FLASH.ld](/C:/Users/20953/Documents/MCUProjects/HomePanel_test/STM32H743XX_FLASH.ld)

### GT911 触摸

- 芯片：`GT911`
- 当前探测地址：`0xBA`
- 软件 I2C 引脚：
  - `PH4` -> `SCL`
  - `PH5` -> `SDA`
- 控制引脚：
  - `PG7` -> `RST`
  - `PG3` -> `INT`

相关实现见：

- [bsp_touch_gt911.h](/C:/Users/20953/Documents/MCUProjects/HomePanel_test/BSP/Inc/bsp_touch_gt911.h)
- [bsp_touch_gt911.c](/C:/Users/20953/Documents/MCUProjects/HomePanel_test/BSP/Src/bsp_touch_gt911.c)
- [STM32TouchController.cpp](/C:/Users/20953/Documents/MCUProjects/HomePanel_test/TouchGFX/target/STM32TouchController.cpp)

## 工程结构

- `BSP/`
  - 板级驱动与适配层，包含显示、SDRAM、QSPI、触摸等实现
- `Core/`
  - CubeMX 生成的基础工程与应用入口
- `TouchGFX/`
  - TouchGFX 界面代码、生成代码与目标层对接
- `Docs/`
  - 项目开发中使用的手册、数据手册和参考资料
- `Examples/`
  - 参考例程，包括 QSPI 和触摸相关工程
- `tools/`
  - 调试辅助脚本和 J-Link 相关配置

## 关键实现说明

### 双帧缓冲

当前显示链路使用 SDRAM 双帧缓冲，LTDC 在合适的行事件时机切换地址，避免了早期的闪烁和撕裂问题。

核心文件：

- [TouchGFXHAL.cpp](/C:/Users/20953/Documents/MCUProjects/HomePanel_test/TouchGFX/target/TouchGFXHAL.cpp)

### QSPI 稳定配置

当前稳定版本的 QSPI 关键点：

- 单片 `W25Q256`
- `memory-mapped` 使用四线高速读取
- 已验证当前参数可以兼顾稳定性和动画性能

核心文件：

- [bsp_qspi_flash.c](/C:/Users/20953/Documents/MCUProjects/HomePanel_test/BSP/Src/bsp_qspi_flash.c)

### 触摸适配

GT911 当前采用软件 I2C + 轮询方案，已经加入了轻量轮询节流，减少空闲时对 UI 帧率的影响。

## 构建方式

项目当前使用：

- `CMake`
- `Ninja`
- `gcc-arm-none-eabi`

配置与构建：

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

Release 构建：

```powershell
cmake --preset Release
cmake --build --preset Release
```

预设定义见：

- [CMakePresets.json](/C:/Users/20953/Documents/MCUProjects/HomePanel_test/CMakePresets.json)

## 调试与下载

### 原生 STM32Cube 配置

当前环境下，VS Code 中的原生调试配置已经验证可用：

- `STM32Cube: Launch JLink GDB Server`

配置位置：

- [launch.json](/C:/Users/20953/Documents/MCUProjects/HomePanel_test/.vscode/launch.json)

### 自定义 J-Link 备用配置

仓库中仍保留了自定义 `J-Link` 配置，用于以下场景：

- 新环境首次排障
- 需要显式指定外部 QSPI loader
- 原生下载行为不清晰时做对照验证

相关文件：

- [launch.json](/C:/Users/20953/Documents/MCUProjects/HomePanel_test/.vscode/launch.json)
- [tools/jlink/README.md](/C:/Users/20953/Documents/MCUProjects/HomePanel_test/tools/jlink/README.md)

## 内存占用查看

`CMake Tools` 的 `Build Analyzer` 对这种 MCU 工程的 `FLASH / EXTFLASH / RAM` 归类不够准确，项目里额外提供了一个更可靠的统计脚本。

VS Code 任务：

- `Memory Report (Debug)`

相关文件：

- [tasks.json](/C:/Users/20953/Documents/MCUProjects/HomePanel_test/.vscode/tasks.json)
- [report-memory-usage.ps1](/C:/Users/20953/Documents/MCUProjects/HomePanel_test/tools/report-memory-usage.ps1)

## 参考资料

开发中主要参考这些本地资料：

- `Docs/` 中的芯片、屏幕、触摸相关手册
- `Examples/` 中的 QSPI 和 GT911 参考工程

## 后续建议

比较适合继续推进的方向：

- 完善触摸交互细节
- 增加多点触控或手势支持
- 接入实际传感器/联网数据
- 继续打磨界面动画和业务逻辑

## 备注

这个仓库是毕业设计过程中的实验工程，当前更重视“可运行、可验证、可继续开发”的工程状态，而不是把所有实现都打磨成最终产品形态。
