# FarmAuto 基础库能力审计

本文件记录 FarmAuto 成品规划前必须确认的两个基础库能力边界。

审计对象：

- `/Users/tyg/dir/claude_dir/Esp32Base`
- `/Users/tyg/dir/codex_dir/Stc8hBase`

本文件只做规划依据，不修改基础库、不修改 `old_prj/`、`pcb/` 或 `_pending_delete/`。

## 总体判断

FarmAuto 应该复用两个基础库，但不能把它们当成业务框架。

- ESP32 主控使用 `Esp32Base` 做联网、Web、认证、OTA、时间、文件系统和诊断底座。
- STC8H 分站使用 `Stc8hBase` 做 GPIO、UART/RS485、PWM、ADC、Timer、WDT、IAP/EEPROM 和小工具底座。
- FarmAuto 自己负责设备模型、RS485 业务协议、计划调度、FRAM 数据模型、分站闭环控制、保护逻辑、Web 业务页面和验收测试。

## Esp32Base 能承担的主控能力

建议主控优先按 `ESP32BASE_PROFILE_FULL` 规划，并启用 RTC、App Events 和必要的业务 Web route。

依据：

- FarmAuto 是长期现场设备，Web OTA 对维护价值高。
- 需要 WiFi/AP 配网、Web 管理、RTC/NTP 统一时间、LittleFS、系统诊断和应用业务事件。
- 当前目标就是成品形态，不适合先做一个低能力 profile 再返工。

需要同时接受的约束：

- 4MB ESP32 使用 FULL profile 时必须做固件体积验收，单 OTA app slot 约 1.5MB。
- `Esp32BaseWeb` 是 Arduino `WebServer` 同步模型，不支持 WebSocket、SPA、多用户权限或 HTTPS。
- Web route 默认容量是 24，FarmAuto 页面/API 要合并设计，不能无限拆小接口。
- Web Basic Auth 只防误操作，不是强安全边界。
- NVS Config 适合小配置，blob 最大 256 字节，不适合设备表、计划表、执行记录。
- App Events 是近期关键业务事件窗口，不是完整业务历史或传感器采样历史。
- RTC 驱动在构建期选择 DS3231 或 PCF8563，不做运行时自动识别。

FarmAuto 主控可直接复用：

- WiFi STA/AP 配网和安全启动保护。
- Web Basic Auth、业务页面/API 注册、统一 UI baseline。
- Web OTA、命令行 OTA 和 OTA 回滚/mark-valid 支撑。
- `Esp32BaseTime` 统一时间入口，RTC/NTP/uptime 来源融合。
- `Esp32BaseFs` 的 LittleFS 文件读写、固定文件、偏移读写能力。
- `Esp32BaseFileLog` 系统诊断日志。
- `Esp32BaseAppEventLog` 低频业务事件记录。
- `Esp32BaseAppConfig` 少量系统级参数页面。

FarmAuto 主控必须自研：

- RS485 主站轮询、地址扫描、命令/响应、重试、离线判定和冲突记录。
- 分站设备表、显示顺序、地址绑定、启停状态、替换和归档逻辑。
- 自动下料计划、自动门控计划、暂停自动执行和手动执行。
- FRAM 数据结构、提交/恢复策略、关键状态落盘边界。
- 温湿度 SHT30 读取、记录和页面展示。
- 主控按钮和 LED 行为。
- 门控/下料业务记录、温湿度历史、统计数据和分页查询。
- FarmAuto 首页、自动、手动、记录、设置等业务 Web 页面。

## Stc8hBase 能承担的分站能力

`STC8H8K64U-45I-LQFP48` 已经是 `Stc8hBase` 的正式目标芯片，可以作为分站基础库。

FarmAuto 分站可直接复用：

- 显式芯片配置和板级 `board_config.h` / `board_pins.h` 接入模式。
- UART1/2/3 轮询初始化、收发。
- `drv_rs485_uart` 半双工 UART 包装，通过 `BOARD_RS485_TX_ENABLE()` / `BOARD_RS485_RX_ENABLE()` 控制 DE/RE。
- GPIO 输入输出、上拉和数字输入使能辅助。
- Timer 1ms tick、Timer0 free-run、Timer start/stop/interrupt 基础能力。
- PWM A/B 组基础输出；同组共享周期和 prescaler。
- ADC 采样，H8K64U 板级默认 12-bit。
- WDT 启用、喂狗和复位标志。
- EEPROM/IAP 基础读写擦除能力，但 H8K64U 参数区必须单独确认。
- CRC16、checksum、ring buffer、soft timer、简单滤波。
- H8K64U OTA/IAP 参考能力；本项目当前不把分站 OTA 纳入成品闭环。

需要注意的限制：

- `drv_rs485_uart` 只是方向控制和 UART 透传，不包含 FarmAuto 协议。
- `drv_ec11` 面向人手旋钮编码器，不能直接等同于电机高速 AB 相编码器闭环。
- H8K64U 默认板级配置中 `STC8H_EEPROM_SIZE=0`，不能假设有 H1K08 那种 4KB EEPROM。
- H8K64U OTA 依赖生产烧录时的 code/EEPROM split，若 split 不允许 IAP 覆盖应用区，则不能做应用 OTA。
- UART2/UART3、RS485 DE/RE 时序、多从机冲突和总线恢复需要 FarmAuto 目标硬件实测。

FarmAuto 分站必须自研：

- FarmAuto RS485 从站协议解析和响应。
- 拨码地址读取、地址 0 保留处理和地址变化策略。
- AT8236 电机驱动封装，包括方向、PWM、刹车/停止安全态。
- 电机编码器 AB 相计数，明确 X1/X2/X4 口径和最大脉冲频率。
- 下料模式状态机：目标脉冲、运行、停止、完成、异常。
- 门控模式状态机：开门、关门、停止、位置、校准、异常。
- INA240A2 ADC 采样、滤波、过流/堵转判断和保护阈值。
- 本地闭环保护：目标到达、过流、堵转、超时、通信中断后的安全处理。
- 分站参数接收、运行期缓存和必要的最小持久化。
- 分站自检、故障码、状态上报和看门狗喂狗策略。

## 对 FarmAuto 成品的直接影响

1. 主控不是实时控制器，只下发目标和策略；分站必须本地闭环完成动作。
2. 主控权威保存业务配置、计划、设备表和历史；分站只保存运行安全必需的最小参数。
3. Web 正式实现应是轻量多页面，不做复杂前端框架。
4. 业务历史不要塞进 App Events；App Events 只记录计划跳过、保护触发、维护动作这类关键解释事件。
5. 分站电机编码器驱动要作为 FarmAuto 关键工程任务，不应套用 EC11 旋钮驱动。
6. 分站 OTA 不纳入当前成品闭环，协议和状态机不依赖远程升级能力。

## 仍需工程核对

- 主控 PCB 上 ESP32 Flash 容量和 RTC 型号，用于确定 profile、分区表和 RTC 驱动。
- 主控 FRAM 型号、容量、I2C 地址和与 SHT30/RTC 的总线关系。
- 新从站 PCB 上 AT8236、编码器、INA240A2、拨码、RS485 DE/RE 的实际引脚。
- H8K64U 生产烧录 code/EEPROM split，决定片内参数区策略；分站 OTA 仅作为基础库能力记录，不纳入成品闭环。
- RS485 默认波特率、轮询周期、重试次数和离线判定阈值。
- 电机编码器最大脉冲频率和 STC8H 捕获方式。
