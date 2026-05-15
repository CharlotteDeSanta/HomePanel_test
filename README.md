# HomePanel_test

`HomePanel_test` 是智能家居系统的上位机工程，运行在 `STM32H743`，负责：

- 图形界面与触摸交互（TouchGFX）
- AP6181 WiFi 入网（SDIO）
- TCP Socket 服务端
- 与多个下位机节点的协议通信与状态展示


---

## 1. 当前进度（与代码一致）

当前已经跑通并在持续联调的能力：

- 主界面（Main Screen）可显示三房间温湿度、风速、USB 状态。
- WiFi 配网界面已改造完成：
  - SSID 滚轮列表
  - SSID / Password 文本输入
  - 内嵌键盘输入与同步
- AP6181 驱动已接入，支持扫描、入网、DHCP。
- 手工接入 lwIP + sockets（未依赖 CubeMX 的 lwIP 开关）。
- 上位机 TCP 服务端已跑通（端口 `5000`）。
- 自定义协议收发已跑通：
  - 下位机上报 `TELEMETRY`
  - 上位机下发 `CONTROL`
  - `ACK/ERR` 回应
- 主界面在线状态联动已实现：
  - WiFi 在线状态：`Offline / Online`
  - 节点在线状态：`kitchenStat / livingStat / bedroomStat`

---

## 2. 系统架构

### 2.1 核心层次

- UI 层：`TouchGFX/gui/...`
  - 显示、交互、动画、输入。
- 模型层：`TouchGFX/gui/src/model/Model.cpp`
  - 每秒轮询网络状态与节点数据并刷新 UI。
- 协议/数据层：`Core/Src/app_home_protocol.c`、`Core/Src/app_home_data.c`
  - 协议编解码、节点数据缓存、在线状态缓存。
- 网络层：`Core/Src/app_wifi_lwip.c`
  - lwIP netif、DHCP、TCP server、协议帧处理与控制下发队列。
- WiFi 设备层：`Core/Src/app_wifi.c`
  - AP6181 bring-up、扫描、入网、以太帧收发。

### 2.2 上下行链路

- 下位机 -> 上位机：`TELEMETRY` 上报（温湿度、模式、风速、输出位）。
- 上位机 -> 下位机：`CONTROL` 下发（目标温度、模式、风速、USB 标志位）。
- 双向：`ACK/ERR`。

---

## 3. 协议定义（当前版本）

协议实现位于：

- `Core/Inc/app_home_protocol.h`
- `Core/Src/app_home_protocol.c`

帧格式：

```text
SOF0 SOF1 VER NODE CMD SEQ_L SEQ_H LEN_L LEN_H PAYLOAD CRC_L CRC_H
```

- `SOF0/SOF1` = `0xAA 0x55`
- `VER` = `0x01`
- `CRC16`（Modbus 风格）计算范围：`VER` 到 `PAYLOAD` 末尾

命令字：

- `0x01 HELLO`
- `0x02 TELEMETRY`
- `0x03 CONTROL`
- `0x04 STATUS`
- `0x05 HEARTBEAT`
- `0x06 ACK`
- `0x07 ERR`

关键 payload：

- `TELEMETRY`（8 字节）
  - `temp_x10` `int16 LE`
  - `hum_x10` `uint16 LE`
  - `mode` `uint8`
  - `fan` `uint8`
  - `online` `uint8`
  - `usb_flags` `uint8`
- `CONTROL`（5 字节）
  - `target_temp_x10` `int16 LE`
  - `mode` `uint8`
  - `fan` `uint8`
  - `usb_flags` `uint8`

---

## 4. UI 与状态映射

### 4.1 WiFi 在线状态

主界面的 `wifistatText` 由 wildcard buffer 驱动：

- 网络在线：`Online`
- 网络离线：`Offline`

在线判定来自 `APP_WiFi_LwIP_IsNetworkOnline()`。

### 4.2 节点在线状态

主界面三个 RadioButton 仅做显示，不允许用户点击：

- `kitchenStat`
- `livingStat`
- `bedroomStat`

当收到对应节点有效通信后置为在线，超时后自动离线。

### 4.3 USB 状态

USB 状态已按房间独立：

- UI 的每个房间卡片独立维护 USB flags
- 下位机回传状态后会同步到对应房间
- 模型层包含 pending 策略，减少“刚下发就被旧状态覆盖”

---

## 5. 构建说明

### 5.1 推荐方式（CMake Presets）

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

Release：

```powershell
cmake --preset Release
cmake --build --preset Release
```

### 5.2 VS Code / cube-cmake

仓库也兼容STM32Cube插件使用的 `cube-cmake --build ...` 工作流。

---

## 6. 联调建议（当前工程经验）

1. 先让下位机入网，再启动上位机，成功率更高。
2. 先验证上行（TELEMETRY），再压测下发（CONTROL）。
3. 快速连点控制时，优先看串口是否出现发送风暴或重连风暴。
4. 若出现异常，优先保留两端串口日志做时间对齐分析。

---

## 7. 关键文件索引

- WiFi 驱动主流程：`Core/Src/app_wifi.c`
- lwIP + socket server：`Core/Src/app_wifi_lwip.c`
- 协议编解码：`Core/Src/app_home_protocol.c`
- 节点数据缓存：`Core/Src/app_home_data.c`
- 主界面联动：`TouchGFX/gui/src/main_screen/MainView.cpp`
- 模型轮询与状态同步：`TouchGFX/gui/src/model/Model.cpp`

---

## 8. 对应下位机仓库

下位机工程：`https://github.com/CharlotteDeSanta/HomeNode`

已支持编译期节点角色选择（厨房/起居室/卧室），详见下位机仓库 README。
