# HomePanel 上位机项目 —— 分层稳定性改造

## 总体思路

这个项目的核心难点不是单纯连 WiFi，而是让 STM32H743 + AP6181 在 TouchGFX、FreeRTOS、lwIP、多个 ESP 下位机 TCP 连接同时存在时稳定运行。改造按自底向上的层次展开：SDIO 总线 → AP6181 固件/参数 → lwIP 网络栈 → 下位机协议 → UI 数据同步。每层问题独立诊断、独立修复，最终形成一套可解释的工程化方案。

---

## 一、WiFi Bring-Up 状态机

AP6181 的初始化被拆分为 24 个显式状态：

```
RESET → SDIO 枚举 → CCCR 探测 → Fn1 使能 → 4-bit 总线切换
→ CMD53 烟雾测试 → ALP 时钟请求 → Backplane 窗口 → HT 时钟
→ Fn2 使能 → 中断路径配置 → 固件下载 → NVRAM 下载
→ ARM 核心释放 → 固件启动等待 → Shared Memory 探测
→ Console 探测 → Mailbox 探测 → 就绪
```

每个状态都记录关键寄存器（OCR、RCA、CCCR rev/sdrev、IORDY、BICTRL、CMD53 回读字、backplane 窗口字、chip clock CSR、host/fn interrupt mask），任何一步失败都能精确定位到是 SDIO 层、固件层还是 bringup 时序问题，而不是一个"初始化失败"黑盒。

---

## 二、SDIO 时钟 —— 从 480KHz 到 25.7MHz

这是整个项目最关键的改进。最初 SDIO 运行时钟仅 **480 KHz**（SDMMCCLK=360MHz, ClockDiv=250），远低于 AP6181 的能力上限。

### 诊断过程

| 尝试 | 配置 | 结果 |
|---|---|---|
| 初始状态 | 480 KHz, ClockDiv=250 | 能跑但极慢，每个帧 ≈30ms |
| 第一次提频 | 12 MHz, ClockDiv=10 | hwtag 错乱，IOCTL 返回全零 — 信号完整性问题 |
| 退回低位 | 恢复 480 KHz | 稳定但不解决问题 |
| 对照野火官方 | 发现官方有专用 `host_platform_enable_high_speed_sdio()` | 我们缺少这个阶段 |

### 解决方案

参照野火 H743Pro 官方示例，在固件启动后、SDPCM 流量开始前，增加独立的高速模式切换函数，逐步验证：

```
480KHz → 18MHz (Div=10) → 22.5MHz (Div=8) → 25.7MHz (Div=7)
```

最终稳定在 **25.7MHz**（SDMMCCLK=360MHz, ClockDiv=7），与野火官方 25MHz 持平，SDIO 吞吐量提升约 **53 倍**。每个 WiFi 帧的 SDIO 传输时间从 30ms 降至 <1ms，为后续多节点并发提供了基本保障。

### 频率计算

```
SDIO_CK = SDMMCCLK / (2 × ClockDiv)
SDMMCCLK = HSE(25MHz) / PLL2M(5) × PLL2N(144) / PLL2Q(2) = 360MHz
```

---

## 三、TCP/lwIP 参数调优

原始配置沿用了野火官方示例的宽松参数（TCP_SND_BUF=6×MSS, TCP_WND 默认 4×MSS），但多节点场景下暴露出问题：

### 发现的问题

实测中，3 节点在线时 TX 队列会逐步增长：`txq=0 → 2 → 7`，接近 link down 前可达 17。但 heap/pbuf 未耗尽，说明瓶颈不在内存，而在 TCP 窗口与 SDIO 实际吞吐的匹配。

### 调整策略

| 参数 | 调整路径 | 最终值 | 原因 |
|---|---|---|---|
| TCP_WND | 2→1 MSS (往复) | 1×MSS=1460 | 减小单节点允许在途数据量，避免 TX 队列堆积 |
| TCP_SND_BUF | 2→1 MSS | 1×MSS=1460 | 与 TCP_WND 匹配，减少 lwIP 内部缓冲 |
| TCP_SNDLOWAT | MSS → MSS/2 | 730 | 更快触发应用层发送回调 |
| TCP_SNDQUEUELOWAT | 2 → 1 | 1 | 降低排队阈值 |
| LWIP_DISABLE_TCP_SANITY_CHECKS | — | 1 | 允许小窗口配置 |

这些收紧措施的核心思路是：让 TCP 层的"承诺能力"不超出 SDIO/AP6181 的实际交付能力，避免数据在 lwIP 内部堆积。

---

## 四、SDIO RX Drain 与 TX/RX 调度

AP6181 通过 SDPCM 帧收发数据，SDIO 中断驱动。多节点场景下 RX 和 TX 争抢 SDIO 总线。

### 发现的问题

- 原始 RX drain 每次处理 24 帧（主循环）+ 16 帧（额外轮次），生成了大量 TCP ACK 反压 TX
- TX 函数中还尝试"借 RX drain 回收 credit"，导致 SDPCM credit 被意外覆盖
- osDelay(0ms) 造成 WiFi 任务死循环，AP6181 无暇处理 Beacon → **link down**

### 调整策略

| 参数 | 调整路径 | 最终值 |
|---|---|---|
| RX_DRAIN_BURST_MAIN | 24 → 8 → 4 | 4 |
| RX_DRAIN_BURST_EXTRA | 16 → 8 → 4 | 4 |
| RX_DRAIN_EXTRA_ROUNDS | 2 → 6 → 3 → 2 | 2 |
| 主循环顺序 | RX 优先 | **TX 优先**（先 Service() 再 DrainRX()） |
| TX-pending 延迟 | 无 | TX 有数据时 osDelay(1ms)，否则正常等待 |
| Credit 恢复 | drain 8 帧回收 credit | **完全移除**，return BUSY 让上层重试 |

TX 优先排序的效果最明显：第一个控制帧延迟大幅下降，因为不再被积压的 RX 帧阻塞。

### SDPCM 空帧处理

AP6181 有时产生 `irq=0x40 len=0 chk=0 raw=00...` 的空中断帧，早期代码将其误判为 hwtag mismatch。实际这是 AP6181 固件的正常行为，单独处理避免误触发错误状态。

---

## 五、AP6181 固件与 NVRAM

### 固件版本

当前使用的固件编译于 **2015 年 10 月，版本 5.90.230.15**，而同期 BCM43438/43364 使用的是 v7.45.98.5。两个大版本的差距意味着可能缺少稳定性修复（尤其是 Beacon 丢失处理、省电模式交互、SDIO flow control 方面）。

### NVRAM 配置

NVRAM 中 `devid=0x4343`（BCM43438 的 ID）而非 43362 的 ID，`ccode=0`（全球通用），且缺少 CLM blob 的下载路径。固件 Brand 字段为空，意味着缺少 feature flag 内嵌 —— 对比 43438 固件的完整 feature set（`pool, p2p, pno, pktfilter, keepalive, aoe, lpc, swdiv, clm_min` 等），当前固件的功能完整度存疑。

### AP6181 参数控制

- `WLC_SET_PM(PM_OFF)` 关闭省电模式，避免低功耗导致的延迟抖动和意外断连
- `event_msgs` 设置确保能接收 auth/link/disassoc 等关键事件
- join 前恢复最小控制链，避免残留参数干扰
- 不强制改 txglom、apsta、country 等不确定参数

---

## 六、Link Down 与根因定位

### 问题现象

3 节点在线时频繁 link down（reason=1），日志显示并非发生在数据高峰期（txq=0, clients=0 时也会掉），排除了 SDIO 拥塞和数据量过大两种假设。

### 排除过程

| 假设 | 测试 | 结论 |
|---|---|---|
| SDIO 速度不够 | 480KHz → 25.7MHz | 改善吞吐但 link down 依然 |
| 天线不良 | 更换天线 | 无变化 |
| AP/RF 环境 | 校园网 AP → 手机热点 | 依然掉 |
| 固件版本问题 | 分析版本差异 | 可能相关但无法验证 |
| TCP 参数不当 | 多轮收紧/放宽 | 改善延迟但 link down 依然 |

### 最终发现

通过 ping 测试发现，上位机 DHCP 获取 IP `10.20.250.232`，笔记本电脑在 `10.20.138.x`——**不同子网**。校园网多 AP 环境下，不同设备可能被分配不同子网：

- **同子网** → 二层直通，延迟低，一切正常
- **跨子网** → 需网关转发，高延迟 + TCP 重传 + 可能被防火墙丢弃

这解释了"间歇性连通"的根本原因，也解释了为什么"之前一直用校园网测试也能通"——恰好落在同一子网时正常，跨子网时异常。

**link down 的根因很可能是校园网跨子网转发导致 AP6181 Beacon 超时，而非代码缺陷。**

### 验证方向

使用专用路由器，所有设备在同一个 `192.168.x.0/24` 子网下测试，排除校园网子网隔离变量。

---

## 七、下位机协议与重连

下位机侧（ESP AT 固件）主要修复 TCP 状态机：

- CIPSTART 未返回明确 OK 时不再假设 TCP 已经连上
- 移除"CIPSTART 不确定就 probe CIPSEND"的逻辑，避免 TCP 状态混乱
- TCP 重试加入退避和 jitter
- 3 节点首次连接和状态上传按 node id 错峰
- 控制确认弱化 ACK/ERR 依赖，主要靠 telemetry 回传确认

这部分确保三个下位机能独立、可靠地完成 TCP 上线，避免单个下位机的连接抖动影响其他节点。

---

## 八、TouchGFX UI 与数据解耦

UI 层与网络层严格解耦：

- TouchGFX 不直接访问 WiFi/lwIP 线程数据结构
- 网络层通过事件队列推送 telemetry/在线状态到 Model
- 在线状态独立于 telemetry 渲染，避免短暂数据缺失导致 UI 闪烁
- USB 控制加入 pending guard：用户点击后 UI 先乐观更新，等 telemetry 确认后锁定，避免旧数据覆盖新操作

---

## 九、技术收获总结

| 层次 | 改造内容 | 效果 |
|---|---|---|
| SDIO 总线 | 480KHz → 25.7MHz 高频切换 | 吞吐量提升 53× |
| TCP 参数 | 窗口/缓冲收紧匹配实际吞吐 | 队列堆积缓解 |
| RX/TX 调度 | TX 优先 + 削峰 + credit 简化 | 首帧延迟显著降低 |
| SDPCM | 空帧处理 + irq 解析 | 不再误判 hwtag 错误 |
| 链路恢复 | rebind + 状态机回退 | 异常时自动恢复 |
| 下位机协议 | TCP 状态机修复 + 错峰上线 | 节点独立可靠 |
| UI | 事件驱动解耦 + 乐观更新 | 不阻塞网络线程 |
| 根因定位 | 逐层排除到校园网子网隔离 | 问题可解释、可复现 |

整个项目的工程收获不在于"修好了一个 bug"，而在于系统性建立了从 SDIO 总线到 UI 层的完整诊断能力和稳定性保障体系。
