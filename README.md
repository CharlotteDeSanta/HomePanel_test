# HomePanel Test

基于 `STM32H743` 的家庭面板实验工程，当前已经完成：

- `FMC + SDRAM` 初始化与读写验证
- `LTDC` 驱动 `800x480 RGB` 屏幕
- 屏幕正常显示测试彩条
- 为后续移植 `TouchGFX` 提供稳定的底层基线

## 当前状态

当前工程上电后会：

1. 初始化 GPIO、SDRAM、LTDC
2. 对 SDRAM 做基础读写测试
3. 在 LCD 上显示从左到右的竖条色块
4. 点亮板载 RGB LED 为白色，表示初始化成功

这意味着当前仓库已经具备继续移植 `TouchGFX` 示例工程的基础。

## 目录说明

- `Core/`：应用代码
- `Drivers/`：STM32 HAL/CMSIS 驱动
- `cmake/`：STM32CubeMX 生成的 CMake 支持文件
- `Docs/`：本地调试时参考的硬件资料
- `HomePanel_test.ioc`：STM32CubeMX 工程文件

## 构建方式

本项目使用 `CMake + Ninja + gcc-arm-none-eabi`。

配置 Debug：

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

配置 Release：

```powershell
cmake --preset Release
cmake --build --preset Release
```

构建产物默认位于 `build/`，该目录已加入 `.gitignore`。

## 后续计划

- 移植 `TouchGFX` 示例工程
- 在示例基础上开发毕业设计界面与业务逻辑
- 接入触摸与交互功能

## 说明

- `Examples/` 已被 `.gitignore` 忽略，不会上传到仓库
- 当前仓库保留了 `Docs/` 目录作为本地参考资料

如果后续准备做公开仓库，且不希望上传板卡原理图、规格书、屏幕手册等资料，可以把 `Docs/` 也加入 `.gitignore`
