# FarmAuto RS485 协议、数据模型与状态机设计草案

本文是 FarmAuto 成品工程设计草案，供主控会话审查。它细化 `docs/05-system-architecture.md` 中提出的协议、数据模型、FRAM、分站状态机和本地保护边界。

## 设计输入与约束

### 已确认决策

- 主控使用 ESP32 + `Esp32Base`，负责联网、Web、配置、调度、记录、RTC、FRAM、SHT30、RS485 主站、按钮和 LED。
- 分站使用 STC8H8K64U + `Stc8hBase`，负责本地电机闭环、编码器、电流检测和安全停止。
- 分站电机动作必须本地闭环完成，主控不能依赖实时通讯决定停止时机。
- 下料和门控共用分站硬件，通过软件模式区分。
- 下料支持按圈数和按克数，主控内部统一换算为目标脉冲。
- 门控不加限位开关，按编码器位置和配置行程闭环控制。
- 电流检测使用 INA240A2，用于过流、堵转和异常辅助判断；缺料检测不纳入当前成品需求。
- 分站电机驱动使用 AT8236。
- 当前硬件使用 4 位拨码，默认可发现物理地址为 `1..15`，地址 `0` 保留。
- 软件模型不能把业务设备总数限制为 15；拨码位数只是当前物理总线寻址能力，不是长期产品容量上限。
- 主控保存和协议传输统一使用 16-bit `busAddress`。如果硬件或固件能让分站以地址 `256` 响应，主控应允许保存、轮询和绑定该地址。
- 设备名称、显示顺序、计划和历史记录由主控保存，不能等同于 RS485 地址。

### 最新变更带来的修正

- 用户已经删除旧从站 PCB 文件，从站 PCB 尚未完成设计。
- 本文不再依据旧从站 PCB BOM、网表或通道迹象推定最终 IO、通道数量、电流检测数量或编码器数量。
- 文中保留 `地址 + 通道号` 的软件抽象，真实通道数和 IO 由新从站 PCB 确认。
- 分站 IO 按软件抽象和临时板级分配设计，拿到新 PCB 实际 IO 后替换板级映射。

### 工程默认值

这些默认值用于成品软件设计和联调起点，需要现场总线和电机实测校正：

- RS485：`9600 8N1`。
- 当前 4 位拨码硬件的默认发现扫描范围：`1..15`。已配置分站不受默认扫描范围限制，主控按保存的 `busAddress` 轮询。
- 单帧 payload 最大长度：`64` 字节。
- 主控响应超时：`80 ms`。
- 命令重试：最多 `2` 次。
- 空闲轮询：每个启用分站约 `1000 ms` 一次。
- 动作中轮询：相关通道约 `200 ms` 一次。
- 离线判定：连续 `5` 次通讯失败，或超过 `10 s` 无有效响应。
- 多下料器自动启动错峰：相邻设备默认间隔 `1000 ms`。
- 多下料器停止错峰：相邻设备默认间隔 `200 ms`。

### 待硬件实测

- 新从站 PCB 的通道数量、每通道编码器输入、电流采样输入和 AT8236 控制 IO。
- STC8H8K64U 在目标编码器最高转速下的可靠计数方式和最大脉冲频率。
- INA240A2 增益、电流采样电阻、ADC 量程、噪声、滤波窗口和保护阈值。
- AT8236 的刹车、滑行、PWM 频率、方向切换死区和故障表现。
- RS485 总线长度、节点数量、终端电阻、偏置、方向切换时序和现场干扰。
- ESP32 FULL profile 固件体积、OTA 分区和 FRAM 读写驱动细节。

## 成品范围控制

FarmAuto 按成品目标设计，不按“第一版先做、第二版再补”拆需求。工程实现可以分任务施工和验收，但产品范围只分“需要”和“不需要”。

成品必须保留的抽象：

- `deviceId`：业务设备稳定 ID，避免地址变化影响计划和历史。
- `busAddress + channelNo`：物理定位，避免一板多通道或地址扩展时重写模型。
- `actionId`：动作去重、记录和故障追踪需要。
- 分站本地闭环：这是安全边界，不能简化掉。

成品必须实现：

- `PING`、`GET_STATUS`、`SET_CHANNEL_CONFIG`、`START_ACTION`、`STOP_ACTION`、`CLEAR_FAULT`。
- 门控和下料两个状态机的基本闭环。
- 过流、堵转、超时、目标到达、看门狗安全态。
- 主控设备表、计划、动作记录、门控位置和故障结果持久化。

成品范围外，或只按明确用途保持简单：

- `IDENTIFY` 可以只做闪灯。
- `READ_DIAG` 只返回计数和最近故障，不做复杂诊断页面。
- 地址冲突只做“疑似冲突”标记，不做复杂自动判别。
- FRAM 迁移只支持当前 schema，不做多版本自动升级框架。
- 多总线、网关、自动全地址段暴力扫描不纳入成品需求。
- 分站 OTA、缺料检测、复杂电流曲线分析不纳入成品验收。

## 总体模型

FarmAuto 按三层模型实现：

1. **Station 物理分站**：RS485 节点，由当前拨码地址定位。
2. **Channel 分站通道**：分站内部的可控电机通道，通道号从 `1` 开始。
3. **Device 业务设备**：用户看到的门控或下料器，绑定到 `busAddress + channelNo`。

如果新 PCB 最终每块分站板只支持 1 个电机通道，则 `channelNo` 固定为 `1`，Web 不需要向普通用户暴露通道概念。保留通道抽象的目的只是避免协议和数据模型返工。

容量边界必须分清：

- 当前 4 位拨码只决定当前硬件在同一条 RS485 总线上默认可发现 `15` 个物理地址。
- 主控的软件 ID、设备表、计划和历史记录不按 `1..15` 建模，不能用地址当数组下标。
- 无论换成更多位拨码、烧录地址还是维护页面指定地址，业务设备、计划和记录仍引用 `deviceId`，不需要迁移成另一套模型。
- 主控允许保存当前默认扫描范围之外的 `busAddress`。例如硬件能以地址 `256` 响应时，软件应正常保存、轮询和绑定。
- 如果设备数超过单条 RS485 总线的实际可靠容量，应增加总线段、网关或分页管理，而不是让业务设备模型依赖当前拨码位数。

## 主控数据模型

### Station

`Station` 是主控侧保存的物理分站记录。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `stationId` | `uint16` | 主控内部稳定 ID |
| `busAddress` | `uint16` | 物理总线地址；当前硬件默认发现 `1..15`，但保存和轮询不限制在该范围 |
| `enabled` | `bool` | 是否参与轮询和调度 |
| `onlineState` | `enum` | `unknown/online/offline/error/conflict_suspected/reserved_address` |
| `capabilityFlags` | `uint32` | 分站能力摘要 |
| `channelCount` | `uint8` | 分站声明的通道数量，待 PCB 确认 |
| `firmwareVersion` | `uint16` | 分站固件版本 |
| `protocolVersion` | `uint8` | 协议版本 |
| `lastSeenAt` | `uint32` | 主控时间戳 |
| `lastError` | `uint16` | 最近通讯错误或协议错误 |

### Channel

`Channel` 是分站通道能力和运行状态的主控镜像，不直接等于业务设备。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `stationId` | `uint16` | 所属分站 |
| `channelNo` | `uint8` | `1..channelCount` |
| `mode` | `enum` | `unconfigured/feeder/door` |
| `configured` | `bool` | 是否已下发并确认配置 |
| `state` | `enum` | 通道运行状态 |
| `faultCode` | `uint16` | 当前故障码 |
| `activeActionId` | `uint32` | 当前动作 ID |
| `positionPulses` | `int32` | 当前编码器位置或本次动作进度 |
| `lastReportAt` | `uint32` | 最近状态回报时间 |

### Device

`Device` 是 Web、计划和历史记录使用的业务对象。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `deviceId` | `uint16` | 主控内部稳定 ID，计划和记录引用它 |
| `type` | `enum` | `door/feeder` |
| `name` | 固定长度字符串 | 用户可见名称 |
| `displayNo` | `uint16` | 用户可见编号 |
| `sortOrder` | `uint16` | 页面排序 |
| `stationId` | `uint16` | 绑定分站 |
| `channelNo` | `uint8` | 绑定通道 |
| `enabled` | `bool` | 是否参与计划和手动操作 |
| `archived` | `bool` | 是否归档 |
| `configVersion` | `uint16` | 配置版本，用于下发去重 |
| `lastKnownState` | `enum` | 主控侧业务状态 |

### DeviceConfig

主控保存业务参数，动作开始前把必要参数快照下发给分站。

通用字段：

- `pulsesPerTurn`：每圈脉冲，`uint32`。
- `directionInvert`：方向极性反转，`bool`。
- `encoderInvert`：编码器方向反转，`bool`。
- `defaultSpeedPermille`：默认速度，`0..1000`。
- `accelMs` / `decelMs`：加减速时间。
- `overCurrentMa`：过流阈值。
- `overCurrentHoldMs`：过流持续判定时间。
- `stallDetectMs`：堵转检测窗口。
- `stallMinDeltaPulses`：堵转窗口内最小脉冲变化。
- `maxRunMs`：单次动作最大运行时间。
- `brakeMode`：停止方式，默认 AT8236 刹车停止。

下料字段：

- `gramsPerTurnMg`：每圈克数，单位 `mg`。
- `maxFeedPulses`：单次最大下料脉冲。
- `feedDirection`：下料方向。

门控字段：

- `closedPositionPulses`：关闭位置，固定为 `0`。
- `openPositionPulses`：开门目标位置。
- `doorDirectionOpen`：开门方向。
- `doorDirectionClose`：关门方向。
- `positionValid`：主控认为门控位置是否可信。

## 下料量换算

主控负责把用户输入换算成目标脉冲，分站只执行目标脉冲。

- 按圈数：`targetPulses = turns_x1000 * pulsesPerTurn / 1000`。
- 按克数：`targetPulses = amountMg * pulsesPerTurn / gramsPerTurnMg`。

工程要求：

- 换算结果小于 `1` 脉冲时拒绝执行。
- 换算结果超过 `maxFeedPulses` 时拒绝执行。
- `gramsPerTurnMg == 0` 时不允许按克数执行，只允许按圈数维护试运行。
- 实际累计下料以分站回报的完成脉冲为准，主控写入记录。

## RS485 帧格式

### 基本原则

- 主控是唯一主动发起方，分站只响应被寻址命令。
- 分站不得主动抢占总线。
- 所有多字节整数使用小端序。
- CRC 使用 `CRC16/MODBUS`，复用 `Stc8hBase` 的 `util_crc16_modbus` 口径，覆盖 `version` 到 payload 的所有字节，不覆盖帧头。
- 接收端遇到 CRC 错误、长度错误或版本不支持时直接丢帧；只有能可靠解析目标地址和命令时才回错误响应。
- 地址 `0` 不绑定普通设备。不使用广播动作命令。

### 帧结构

| 字段 | 长度 | 说明 |
| --- | ---: | --- |
| `sof0` | 1 | 固定 `0xA5` |
| `sof1` | 1 | 固定 `0x5A` |
| `version` | 1 | 协议版本，当前为 `0x01` |
| `flags` | 1 | bit0=`response`，bit1=`error`，其余保留为 0 |
| `dst` | 2 | 目标地址；主控为 `0`，分站使用 16-bit `busAddress` |
| `src` | 2 | 源地址；主控为 `0`，分站使用 16-bit `busAddress` |
| `seq` | 1 | 主控递增序号，响应原样返回 |
| `cmd` | 1 | 命令码 |
| `len` | 2 | payload 长度，最大 `64` |
| `payload` | `len` | 命令数据 |
| `crc16` | 2 | CRC16，小端 |

### 时序

- 主控发送后切回接收态，等待响应。
- 分站收到完整有效帧后，等待至少 `2` 个字符时间再打开发送，使主控完成方向切换。
- 分站响应结束后立即关闭发送并回到接收态。
- 主控命令超时后最多重试 `2` 次；同一 `seq + cmd + actionId` 的重复命令必须可幂等处理。

## 命令与响应

### 命令码

| 命令 | 编码 | 方向 | 用途 |
| --- | ---: | --- | --- |
| `PING` | `0x01` | 主控到分站 | 基础通讯、版本、能力摘要 |
| `GET_STATUS` | `0x02` | 主控到分站 | 读取分站或指定通道状态 |
| `SET_CHANNEL_CONFIG` | `0x10` | 主控到分站 | 下发通道参数快照 |
| `START_ACTION` | `0x20` | 主控到分站 | 启动门控或下料动作 |
| `STOP_ACTION` | `0x21` | 主控到分站 | 停止指定通道当前动作 |
| `CLEAR_FAULT` | `0x22` | 主控到分站 | 清除可恢复故障 |
| `IDENTIFY` | `0x30` | 主控到分站 | 闪灯或短提示，用于现场识别 |
| `READ_DIAG` | `0x40` | 主控到分站 | 读取诊断快照 |

### 通用响应

所有响应 payload 以 `statusCode` 开头：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `statusCode` | `uint8` | `0` 表示成功，非 0 表示错误 |
| `stationState` | `uint8` | 分站总体状态 |
| `faultCode` | `uint16` | 分站或通道故障码，成功且无故障为 `0` |

错误响应必须设置 `flags.error=1`。如果请求包含 `actionId`，响应必须带回同一个 `actionId`。

### `PING`

请求 payload：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `masterProtocolVersion` | `uint8` | 主控协议版本 |

响应 payload：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `statusCode` | `uint8` | 状态码 |
| `stationState` | `uint8` | 分站状态 |
| `faultCode` | `uint16` | 总体故障 |
| `protocolVersion` | `uint8` | 分站协议版本 |
| `firmwareVersion` | `uint16` | 分站固件版本 |
| `effectiveBusAddress` | `uint16` | 当前生效总线地址 |
| `rawAddressInput` | `uint16` | 原始地址输入值；当前硬件为 4 位拨码值 |
| `channelCount` | `uint8` | 编译配置声明的通道数 |
| `capabilityFlags` | `uint32` | 能力位 |

### `GET_STATUS`

请求 payload：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `channelNo` | `uint8` | `0` 读取分站摘要，非 0 读取指定通道 |

通道状态响应 payload：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `statusCode` | `uint8` | 状态码 |
| `stationState` | `uint8` | 分站状态 |
| `faultCode` | `uint16` | 当前故障 |
| `channelNo` | `uint8` | 通道号 |
| `mode` | `uint8` | `0` 未配置，`1` 下料，`2` 门控 |
| `channelState` | `uint8` | 通道状态 |
| `activeActionId` | `uint32` | 当前或最近动作 ID |
| `positionPulses` | `int32` | 当前位置或本次累计脉冲 |
| `targetPulses` | `int32` | 当前目标 |
| `currentMa` | `uint16` | 滤波后电流 |
| `peakCurrentMa` | `uint16` | 本次动作峰值电流 |
| `runMs` | `uint32` | 本次动作已运行时间 |
| `completedPulses` | `uint32` | 本次动作完成脉冲 |
| `lastStopReason` | `uint8` | 最近停止原因 |

### `SET_CHANNEL_CONFIG`

请求 payload 是分站运行所需的安全参数快照，不包含设备名称、显示顺序、计划等主控业务数据。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `configVersion` | `uint16` | 主控配置版本 |
| `channelNo` | `uint8` | 通道号 |
| `mode` | `uint8` | `1` 下料，`2` 门控 |
| `flags` | `uint16` | 方向反转、编码器反转、刹车模式等 |
| `pulsesPerTurn` | `uint32` | 每圈脉冲 |
| `defaultSpeedPermille` | `uint16` | 默认速度 |
| `accelMs` | `uint16` | 加速时间 |
| `decelMs` | `uint16` | 减速时间 |
| `overCurrentMa` | `uint16` | 过流阈值 |
| `overCurrentHoldMs` | `uint16` | 过流持续时间 |
| `stallDetectMs` | `uint16` | 堵转窗口 |
| `stallMinDeltaPulses` | `uint16` | 堵转最小脉冲 |
| `maxRunMs` | `uint32` | 最大运行时间 |
| `maxActionPulses` | `uint32` | 单次动作最大脉冲 |
| `doorOpenPositionPulses` | `int32` | 门控开门位置；下料模式填 0 |

响应成功后，分站只保证运行期缓存生效。成品设计中，分站不保存用户业务配置到片内 EEPROM/IAP，避免 STC8H 参数区 split 和配置多源一致性风险。

### `START_ACTION`

请求 payload：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `actionId` | `uint32` | 主控生成的动作 ID |
| `channelNo` | `uint8` | 通道号 |
| `deviceType` | `uint8` | `1` 下料，`2` 门控 |
| `actionType` | `uint8` | 下料、开门、关门、校准移动等 |
| `targetMode` | `uint8` | `1` 相对脉冲，`2` 绝对位置 |
| `startPositionPulses` | `int32` | 主控认为的起始位置，用于门控绝对动作 |
| `targetPulses` | `int32` | 相对动作填目标增量，绝对动作填目标位置 |
| `direction` | `int8` | `1` 正向，`-1` 反向 |
| `speedPermille` | `uint16` | 本次速度 |
| `maxRunMs` | `uint32` | 本次最大运行时间 |
| `maxActionPulses` | `uint32` | 本次最大脉冲 |
| `configVersion` | `uint16` | 主控期望的配置版本 |

规则：

- 分站必须先校验通道已配置、模式匹配、无不可恢复故障、目标不超过限制。
- 相同 `actionId` 的重复 `START_ACTION` 不得重复启动电机。
- 如果动作已完成且分站仍缓存最近结果，重复请求返回最近结果。
- 如果通道正在执行其他 `actionId`，返回 `ERR_BUSY`。
- 所有动作都是有界动作，不设计无限运行命令。

### `STOP_ACTION`

请求 payload：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `actionId` | `uint32` | 停止动作 ID，主控生成 |
| `channelNo` | `uint8` | 通道号 |
| `stopMode` | `uint8` | 默认刹车停止 |

分站收到后必须立即进入安全停止流程，停止结果通过随后的 `GET_STATUS` 或响应返回。停止是安全动作，即使配置版本不匹配也应执行。

### `CLEAR_FAULT`

只清除可恢复故障，例如用户已确认堵转原因并解除机械阻塞。不可恢复或需要重新校准的故障必须保持，直到维护动作完成。

### `IDENTIFY`

用于现场识别，不允许驱动电机。成品只要求闪分站 LED；如果新 PCB 明确增加蜂鸣器，再增加短蜂鸣。

### `READ_DIAG`

用于维护页面读取低频诊断，不参与动作闭环。

建议响应 payload：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `statusCode` | `uint8` | 状态码 |
| `stationState` | `uint8` | 分站状态 |
| `faultCode` | `uint16` | 当前故障 |
| `uptimeMs` | `uint32` | 分站运行时间 |
| `resetReason` | `uint8` | 最近复位原因 |
| `rxFrameCount` | `uint32` | 已接收有效帧 |
| `crcErrorCount` | `uint16` | CRC 错误计数 |
| `timeoutCount` | `uint16` | 本地超时计数 |
| `watchdogResetCount` | `uint16` | 看门狗复位计数 |
| `adcRawCurrent` | `uint16` | 当前 ADC 原始值 |
| `supplyAdcRaw` | `uint16` | 电源采样原始值，若硬件不支持则为 0 |

## 状态码、故障码和停止原因

### 分站状态

| 编码 | 名称 | 说明 |
| ---: | --- | --- |
| `0` | `STATION_BOOTING` | 初始化中 |
| `1` | `STATION_READY` | 可通讯，可接受安全命令 |
| `2` | `STATION_CONFIG_REQUIRED` | 尚未收到有效通道配置 |
| `3` | `STATION_RUNNING` | 至少一个通道正在动作 |
| `4` | `STATION_FAULT` | 存在故障 |
| `5` | `STATION_ADDRESS_RESERVED` | 拨码地址为 0，禁止普通动作 |

### 通道状态

| 编码 | 名称 | 说明 |
| ---: | --- | --- |
| `0` | `CHANNEL_UNCONFIGURED` | 未配置 |
| `1` | `CHANNEL_IDLE` | 空闲 |
| `2` | `CHANNEL_RUNNING` | 正在执行动作 |
| `3` | `CHANNEL_STOPPING` | 正在停止 |
| `4` | `CHANNEL_COMPLETED` | 最近动作完成 |
| `5` | `CHANNEL_STOPPED` | 被停止 |
| `6` | `CHANNEL_FAULT` | 故障锁定 |
| `7` | `CHANNEL_POSITION_INVALID` | 门控位置不可信 |

### 协议状态码

| 编码 | 名称 | 说明 |
| ---: | --- | --- |
| `0x00` | `OK` | 成功 |
| `0x01` | `ERR_UNSUPPORTED_VERSION` | 协议版本不支持 |
| `0x02` | `ERR_BAD_LENGTH` | payload 长度错误 |
| `0x03` | `ERR_BAD_PARAM` | 参数越界或非法 |
| `0x04` | `ERR_CHANNEL_INVALID` | 通道不存在 |
| `0x05` | `ERR_NOT_CONFIGURED` | 通道未配置 |
| `0x06` | `ERR_MODE_MISMATCH` | 通道模式不匹配 |
| `0x07` | `ERR_BUSY` | 通道正在执行其他动作 |
| `0x08` | `ERR_FAULT_ACTIVE` | 存在未清除故障 |
| `0x09` | `ERR_ACTION_DUPLICATE` | 重复动作，未重新执行 |
| `0x0A` | `ERR_SAFETY_BLOCKED` | 安全条件不满足 |
| `0x0B` | `ERR_ADDRESS_RESERVED` | 地址 0 或保留地址状态 |
| `0x0C` | `ERR_INTERNAL` | 分站内部错误 |

### 分站/通道故障码

| 编码 | 名称 | 说明 |
| ---: | --- | --- |
| `0x0000` | `FAULT_NONE` | 无故障 |
| `0x0001` | `FAULT_OVER_CURRENT` | 过流保护触发 |
| `0x0002` | `FAULT_STALL` | 堵转，运行中脉冲变化不足 |
| `0x0003` | `FAULT_ENCODER_LOST` | 编码器信号异常或无信号 |
| `0x0004` | `FAULT_RUN_TIMEOUT` | 单次动作超时 |
| `0x0005` | `FAULT_TARGET_OVERRUN` | 超过目标或最大脉冲保护 |
| `0x0006` | `FAULT_CONFIG_INVALID` | 配置缺失或参数非法 |
| `0x0007` | `FAULT_DRIVER_ABNORMAL` | AT8236 输出或驱动异常，具体依赖硬件实测 |
| `0x0008` | `FAULT_CURRENT_SENSOR` | INA240A2/ADC 采样异常 |
| `0x0009` | `FAULT_WATCHDOG_RESET` | 看门狗复位后进入安全态 |
| `0x000A` | `FAULT_POSITION_INVALID` | 门控位置不可信，需要校准 |
| `0x000B` | `FAULT_RESERVED_ADDRESS` | 拨码地址为 0 |
| `0x000C` | `FAULT_COMMAND_REJECTED` | 分站明确拒绝命令，且响应没有更具体故障码 |

### 停止原因

| 编码 | 名称 | 说明 |
| ---: | --- | --- |
| `0` | `STOP_NONE` | 无 |
| `1` | `STOP_TARGET_REACHED` | 到达目标 |
| `2` | `STOP_MASTER_COMMAND` | 主控停止命令 |
| `3` | `STOP_OVER_CURRENT` | 过流停止 |
| `4` | `STOP_STALL` | 堵转停止 |
| `5` | `STOP_TIMEOUT` | 超时停止 |
| `6` | `STOP_TARGET_OVERRUN` | 超限停止 |
| `7` | `STOP_WATCHDOG` | 看门狗或复位安全停止 |
| `8` | `STOP_LOCAL_FAULT` | 其他本地故障 |

## 主控轮询与离线判定

### 启动扫描

1. 主控初始化 RS485。
2. 默认依次向地址 `1..15` 发送 `PING`，用于当前 4 位拨码硬件的现场发现。
3. 对主控中已保存但不在默认扫描范围内的地址，按保存的 `busAddress` 单独发送 `PING`。
4. 对有效响应记录 `stationId`、能力、固件和通道数量。
5. 对已绑定但无响应的地址标记为 `offline`，不删除绑定。
6. 地址 `0` 为保留地址，不参与普通扫描。若维护模式单独探测到地址 `0` 响应，只记录为 `reserved_address` 并提示现场调整拨码，不允许绑定。

### 正常轮询

- 空闲分站：轮询 `PING` 或 `GET_STATUS(channelNo=0)`。
- 已绑定通道：按设备需要轮询 `GET_STATUS(channelNo)`。
- 动作中通道：提高到约 `200 ms` 状态轮询，直到完成、停止或故障。
- 轮询调度必须给手动停止命令最高优先级。

### 离线与冲突

- 连续通讯失败达到阈值后，分站标记 `offline`。
- 离线不等于设备删除，计划引用和历史记录保持。
- 地址冲突无法直接“证明”多个分站同地址，只能根据异常响应推断：同一地址连续出现 CRC 错误、长度异常、响应字段跳变、多个尾部残留字节时标记 `conflict_suspected`。
- `conflict_suspected` 不允许执行动作，只允许停止、重新扫描和维护提示。

## 分站通用安全策略

分站必须满足以下本地闭环要求：

- 收到 `START_ACTION` 后，动作目标、方向、速度、最大运行时间、最大脉冲和保护阈值已经完整落在分站运行期缓存中。
- 动作执行期间即使主控暂时失联，分站也必须按目标到达、超时、过流、堵转等本地条件完成或保护停止。
- 通讯中断本身不触发停止，因为所有动作都是有界动作；不能因为主控短暂丢包导致门或下料器停在不可预期位置。
- 分站上电、复位、看门狗复位、地址为 0、配置非法时，所有电机输出必须保持关闭。
- 分站不得执行广播动作命令。
- 分站故障未清除前，不允许启动新的有风险动作。
- 停止命令不受配置版本限制，必须优先执行。

## 门控状态机

### 状态

| 状态 | 说明 |
| --- | --- |
| `DOOR_UNCONFIGURED` | 通道未配置为门控 |
| `DOOR_POSITION_INVALID` | 位置不可信，需要校准 |
| `DOOR_IDLE` | 空闲，位置可信但不强行判断开/关 |
| `DOOR_CLOSED` | 位置为关闭点 `0` |
| `DOOR_OPEN` | 位置达到开门目标 |
| `DOOR_OPENING` | 正在开门 |
| `DOOR_CLOSING` | 正在关门 |
| `DOOR_STOPPING` | 正在执行停止 |
| `DOOR_STOPPED` | 被停止，位置需回报主控 |
| `DOOR_FAULT` | 故障锁定 |

### 关键动作

- 开门：主控发送绝对目标 `openPositionPulses`，分站从 `startPositionPulses` 开始闭环到目标。
- 关门：主控发送绝对目标 `0`。
- 停止：分站立即刹车停止，并回报当前位置。
- 校准关闭：维护人员确认门在关闭位置后，主控把该设备位置设为 `0`，再下发配置/状态；分站不自行猜测绝对位置。
- 校准行程：维护流程更新 `openPositionPulses`，不使用限位开关自动学习。

### 转移规则

- `DOOR_POSITION_INVALID` 只允许识别、停止、清故障和校准，不允许普通开关门。
- `DOOR_IDLE/CLOSED/OPEN/STOPPED` 收到合法开门或关门命令后进入 `DOOR_OPENING/DOOR_CLOSING`。
- 运行中达到目标后进入 `DOOR_OPEN` 或 `DOOR_CLOSED`。
- 运行中收到停止命令进入 `DOOR_STOPPING`，输出关闭后进入 `DOOR_STOPPED`。
- 运行中触发过流、堵转、编码器异常、超时或超限，立即刹车并进入 `DOOR_FAULT`。
- 分站复位后如果主控没有重新同步位置，进入 `DOOR_POSITION_INVALID`。

### 门控位置持久化

- 分站回报的是当前动作中的本地位置。
- 主控是门控位置持久化的权威来源，写入 FRAM。
- 动作中主控按状态回报更新 FRAM 运行快照；写入应做节流，例如 `1 s` 或位置变化超过阈值才写。
- 动作完成、停止、故障时必须立即提交最终位置和结果。
- 如果主控在动作中离线或复位，恢复后必须重新读取分站状态；位置不一致时标记 `POSITION_INVALID` 并要求维护校准。

## 下料状态机

### 状态

| 状态 | 说明 |
| --- | --- |
| `FEEDER_UNCONFIGURED` | 通道未配置为下料 |
| `FEEDER_IDLE` | 空闲 |
| `FEEDER_FEEDING` | 正在下料 |
| `FEEDER_STOPPING` | 正在停止 |
| `FEEDER_COMPLETED` | 最近一次下料完成 |
| `FEEDER_STOPPED` | 被手动或调度停止 |
| `FEEDER_FAULT` | 故障锁定 |

### 转移规则

- `FEEDER_IDLE/COMPLETED/STOPPED` 收到合法下料命令后进入 `FEEDER_FEEDING`。
- `FEEDER_FEEDING` 到达目标脉冲后进入 `FEEDER_COMPLETED`。
- `FEEDER_FEEDING` 收到停止命令后进入 `FEEDER_STOPPING`，输出关闭后进入 `FEEDER_STOPPED`。
- `FEEDER_FEEDING` 触发过流、堵转、编码器异常、超时或超限后进入 `FEEDER_FAULT`。
- `FEEDER_FAULT` 需要主控维护流程 `CLEAR_FAULT` 后才能回到 `FEEDER_IDLE`。

### 下料记录

主控记录每次下料的：

- `actionId`。
- `deviceId`。
- 当时绑定的 `busAddress + channelNo`。
- 计划 ID 或手动来源。
- 目标脉冲、目标克数或目标圈数。
- 完成脉冲。
- 停止原因和故障码。
- 开始时间、结束时间、运行时长。
- 峰值电流和异常摘要。

## FRAM 数据布局

主控 FRAM 按 FM24CL64B 的 64Kbit / 8KiB 规划。FRAM 保存关键配置索引、当前运行设备的绑定缓存、计划摘要、运行快照和短事务日志；完整设备注册表和完整历史记录放 LittleFS 业务文件，App Events 只保存近期关键事件。

FRAM 容量不能成为“系统最多多少业务设备”的产品定义。FRAM 只保证当前启用设备和关键运行状态可靠恢复；地址数增加时，优先扩展 LittleFS 注册表和按需缓存策略，再评估是否需要更大 FRAM。

### 地址分区

| 地址范围 | 大小 | 用途 |
| --- | ---: | --- |
| `0x0000..0x00FF` | 256 B | Superblock A |
| `0x0100..0x01FF` | 256 B | Superblock B |
| `0x0200..0x05FF` | 1024 B | Station 运行缓存，按槽位保存，不按地址值直接下标 |
| `0x0600..0x0DFF` | 2048 B | 当前启用 Device 运行缓存，不作为设备总数上限 |
| `0x0E00..0x13FF` | 1536 B | 每日计划摘要和启用索引 |
| `0x1400..0x15FF` | 512 B | 全局配置、暂停状态、调度状态 |
| `0x1600..0x19FF` | 1024 B | 运行快照，门控位置、动作状态 |
| `0x1A00..0x1DFF` | 1024 B | Pending action / result journal 环形区 |
| `0x1E00..0x1FFF` | 512 B | 校准摘要、预留和迁移 scratch |

### 记录格式要求

每类记录都应包含：

- `magic`。
- `schemaVersion`。
- `recordSize`。
- `generation`。
- `validFlag`。
- `payload`。
- `crc16` 或 `crc32`。

### 提交策略

- Superblock A/B 双份保存，启动时选择 `generation` 最大且 CRC 正确的一份。
- 普通表记录采用“先写 payload 和 CRC，再写 valid/commit 标记”的顺序。
- 多记录事务先写 journal，再写目标记录，最后更新 superblock 的 `generation` 和提交点。
- 设备绑定、计划修改、暂停状态、动作开始、动作结束、故障结果必须同步提交。
- 门控运行中位置更新允许节流写入；动作结束、停止和故障必须立即写最终位置。
- LittleFS 历史写入失败时，FRAM journal 至少保留最近若干动作结果，供恢复后补写或提示记录不完整。

## PCB IO 替换方式

分站固件必须把业务逻辑和板级 IO 隔离：

- `Board` 层只暴露逻辑信号：`RS485_TX/RX/DE`、`DIP_ADDR[4]`、`MOTOR_PWM/IN1/IN2/STBY`、`ENC_A/ENC_B`、`CURRENT_ADC`、`LED_STATUS` 等。
- `MotorChannel` 不直接引用 STC8H 端口号，只调用 `BoardMotorIo`。
- `Encoder` 不直接绑定旧网表命名，只绑定 `BoardEncoderIo`。
- `CurrentSense` 不关心 INA240A2 接在哪个 ADC 管脚，只接收板级 ADC 通道号和换算参数。
- `StationCapabilities` 由编译期板级配置生成，主控通过 `PING` 读取。
- 新 PCB 完成后，只替换 `board_pins.h`、板级能力表和 ADC/编码器通道映射，不改变 RS485 协议和主控数据模型。

临时 IO 分配必须满足：

- 所有输出上电默认安全态。
- AT8236 方向和 PWM 切换有明确停止/死区流程。
- 编码器输入优先使用可稳定中断或定时采样的管脚。
- INA240A2 ADC 输入必须有标定参数入口。
- 拨码地址读取必须有上拉/下拉默认态说明。

## 主控审查结论与工程风险

- `地址 + 通道号` 抽象作为成品模型接受；若最终一板一通道，可隐藏但不删除。
- 当前 4 位拨码只限制默认发现范围 `1..15`，不能限制软件业务设备总数或手动配置地址；这是长期原则。
- RS485 默认 `9600 8N1`、`80 ms` 超时、`5` 次失败离线作为联调起点，最终按现场总线长度和干扰实测校正。
- 分站通讯中断时继续完成有界动作，而不是立即停止；这是本地闭环安全边界的一部分。
- 门控无绝对位置传感器，主控持久化位置依赖编码器连续性；断电、机械手动移动或分站复位后必须有校准流程。
- FRAM 8KiB 容量只能保存关键状态和短 journal，完整历史必须进入 LittleFS。
- 分站不持久化业务配置，避免 STC8H IAP/EEPROM split 和配置多源一致性风险；这意味着分站上电后必须等待主控下发配置才允许运行动作。
- 过流、堵转、编码器异常的默认阈值不能从旧项目照抄，必须实测后固化。
