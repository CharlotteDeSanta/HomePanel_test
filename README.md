# HomePanel Test

基于 `STM32H743` 的家庭控制面板实验工程。

当前仓库已经完成了外部 `SDRAM`、`LTDC` 与 `800x480 RGB` 屏幕的基础打通，并在此基础上整理出一套相对稳定的 `BSP + CubeMX` 协作结构，作为后续移植 `TouchGFX` 和实现毕业设计界面的开发基线。

## 项目状态

当前版本已经验证通过的内容：

- `FMC + SDRAM` 初始化完成，基础读写测试通过
- `LTDC` 已经可以正常从外部显存读取 framebuffer
- 屏幕可以稳定显示测试图案
- `CubeMX .ioc` 已同步到当前可运行配置
- 板级配置已迁移到 `BSP/`，降低了后续重新生成代码时的覆盖风险

上电后的当前行为：

1. 初始化系统时钟、板级 GPIO、外部 SDRAM 和 LTDC
2. 对 SDRAM 做基础读写测试
3. 在 LCD 上显示测试彩条
4. 点亮板载 RGB LED 白灯，表示初始化完成

## 工程结构

- `BSP/`
  板级支持包。当前项目中真正生效的显示、SDRAM、板级 GPIO 与时钟相关实现都在这里。
- `Core/`
  STM32CubeMX 生成的基础工程文件与应用入口。
- `Drivers/`
  STM32 HAL 与 CMSIS 驱动。
- `cmake/`
  CMake 工具链和 STM32CubeMX 生成的构建支持文件。
- `Docs/`
  开发过程中使用的原理图、手册、屏幕资料等本地参考文档。
- `HomePanel_test.ioc`
  STM32CubeMX 工程配置文件。

## BSP 设计说明

当前工程采用了“`CubeMX 负责生成基础框架，BSP 负责板级实现`”的组织方式。

主要板级模块：

- `BSP/Inc/bsp_board.h` / `BSP/Src/bsp_board.c`
  负责系统时钟封装、LED、背光、LCD 复位、MPU 等板级初始化。
- `BSP/Inc/bsp_sdram.h` / `BSP/Src/bsp_sdram.c`
  负责 SDRAM 配置、初始化序列、GPIO 映射与读写测试。
- `BSP/Inc/bsp_display.h` / `BSP/Src/bsp_display.c`
  负责 LTDC 配置、framebuffer 地址、测试图填充等显示相关功能。
- `BSP/Inc/bsp_config.h`
  集中管理分辨率、framebuffer 地址、像素格式等板级常量。

这样做的目的，是把最容易被 CubeMX 重新生成覆盖的内容从 `Core/Src/gpio.c`、`Core/Src/fmc.c`、`Core/Src/ltdc.c` 中抽离出来，方便后续继续添加 `FreeRTOS`、`DMA2D`、`CRC`、`TouchGFX` 等功能。

## 当前显示基线

当前工程使用的核心显示参数如下：

- 分辨率：`800 x 480`
- 显存地址：`0xD0000000`
- Layer 像素格式：`ARGB8888`
- LTDC 像素时钟：约 `27 MHz`
- 外部 SDRAM：`BANK2 / 32-bit / 9 column / 13 row`

这些参数已经同步到了当前 `.ioc` 文件中，后续可继续以 CubeMX 作为外设和中间件配置入口。

## 构建方式

本项目当前使用：

- `CMake`
- `Ninja`
- `gcc-arm-none-eabi`

Debug 构建：

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

Release 构建：

```powershell
cmake --preset Release
cmake --build --preset Release
```

构建输出默认位于 `build/` 目录，该目录已经被 `.gitignore` 忽略。

## 使用 CubeMX 的约定

后续继续在 CubeMX 中添加功能时，建议遵循以下原则：

- `CRC`、`DMA2D`、`FreeRTOS`、`TouchGFX` 等新增外设或中间件，优先在 CubeMX 中启用
- 板级相关逻辑继续保留在 `BSP/`
- 生成代码后，优先检查 `.ioc`、编译结果和 BSP 接口是否仍然一致
- 不建议再把显示、SDRAM、背光、复位等板级细节重新写回生成文件作为主实现

## 后续计划

- 在当前硬件基线上移植 `TouchGFX`
- 引入 `DMA2D`、`CRC` 等图形相关外设支持
- 接入触摸功能
- 在示例工程基础上实现毕业设计界面与业务逻辑

## 仓库说明

这是我的毕业设计项目，请不要严肃阅读（纰漏很多）。毕业答辩通过后，我会把所有硬件设计公开。
