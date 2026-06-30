# FarmAuto RS485 协议、数据模型与状态机设计

本文是 FarmAuto 成品工程设计基线，细化 `docs/05-system-architecture.md` 中的协议、数据模型、状态机和持久化边界。

## 设计输入与约束

已确认决策：

- 主控使用 ESP32 + `Esp32Base`，负责联网、Web、配置、调度、记录、RTC、FRAM、SHT30、RS485 主站、按钮和 LED。
- 分站使用 STC8H8K64U + `Stc8hBase`，负责本地电机闭环、编码器、电流检测和安全停止。
- 一块分站板只控制一个双向直流有刷减速电机。
- 门控和下料使用同一种分站硬件能力，都是 AT8236 + 霍尔 AB 相编码器 + INA240A2 + RS485 + RUN/ERR LED。
- 门控通常使用正反转；下料通常只使用一个方向，但底层动作能力仍然是双向电机。
- 分站电机动作必须本地闭环完成，主控不能依赖实时通讯决定停止时机。
- RS485 地址为 1 字节；地址 `0` 保留，普通地址为 `1..127`，主控默认扫描全部普通地址。
- 设备名称、显示顺序、计划和历史记录由主控保存，不能等同于 RS485 地址。
- 下料支持按圈数和按克数，主控内部统一换算为目标脉冲。
- 门控不加限位开关，按主控保存的位置和配置行程执行开门、关门和校准。
- 电流检测用于过流、堵转和异常辅助判断；缺料检测不纳入当前成品需求。

工程默认值用于联调起点，最终按实测校正：

- RS485：`9600 8N1`。
- 单帧 payload 最大长度：`64` 字节。
- 主控响应超时：`80 ms`。
- 命令重试：最多 `2` 次。
- 空闲轮询：每个启用分站约 `1000 ms` 一次。
- 动作中轮询：约 `200 ms` 一次。
- 离线判定：连续 `5` 次通讯失败，或超过 `10 s` 无有效响应。
- 多下料器自动启动错峰：相邻设备默认间隔 `1000 ms`。
- 多下料器停止错峰：相邻设备默认间隔 `200 ms`。

待硬件实测：

- 新从站 PCB 的 AT8236、霍尔 AB 相编码器、INA240A2、地址输入、LED 和 RS485 DE/RE 实际 IO。
- STC8H8K64U 在目标霍尔 AB 相编码器最高转速下的可靠计数方式和最大脉冲频率。
- INA240A2 增益、电流采样电阻、ADC 量程、噪声、滤波窗口和保护阈值。
- AT8236 的刹车、滑行、PWM 频率、方向切换死区和故障表现。
- RS485 总线长度、节点数量、终端电阻、偏置、方向切换时序和现场干扰。
- ESP32 FULL profile 固件体积、OTA 分区和 FRAM 读写驱动细节。

## 成品范围控制

成品必须保留的抽象：

- `deviceId`：业务设备稳定 ID，避免地址变化影响计划和历史。
- `busAddress`：物理 RS485 地址，范围 `1..127`。
- `actionId`：动作去重、记录和故障追踪需要。
- 通用有界电机动作：方向、目标脉冲、速度、超时、最大脉冲和保护参数。
- 分站本地闭环：这是安全边界，不能简化掉。

成品必须实现：

- `PING`、`GET_STATUS`、`SET_MOTOR_CONFIG`、`START_ACTION`、`STOP_ACTION`、`CLEAR_FAULT`。
- 门控和下料两个主控业务状态机。
- 分站通用电机动作状态机。
- 过流、堵转、超时、目标到达、看门狗安全态。
- 主控设备表、计划、动作记录、门控位置和故障结果持久化。

成品范围外，或只按明确用途保持简单：

- `IDENTIFY` 可以只做闪灯。
- `READ_DIAG` 只返回计数和最近故障，不做复杂诊断页面。
- 地址冲突只做“疑似冲突”标记，不做复杂自动判别。
- FRAM 迁移只支持当前 schema，不做多版本自动升级框架。
- 多总线、网关不纳入当前成品需求。
- 分站 OTA、缺料检测、复杂电流曲线分析不纳入成品验收。

## 总体模型

FarmAuto 按三层模型实现：

1. **Station 物理分站**：一个 RS485 地址上的单电机执行节点。
2. **MotorAction 通用电机动作**：分站执行的有界动作，不关心最终业务是门控还是下料。
3. **Device 业务设备**：用户看到的门控或下料器，由主控绑定到一个分站地址。

主控业务模型区分 `door` 和 `feeder`：

- 门控：开门目标、关闭点、当前位置、校准、开门/关门/停止、门控计划。
- 下料：每输出圈脉冲、每圈克数、单次下料量、手动下料、自动下料计划和错峰启动。

分站底层不拆成门控硬件和下料硬件。分站只执行一次明确的电机动作，并回报动作进度、停止原因和故障。

## 主控数据模型

### Station

`Station` 是主控侧保存的物理分站记录。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `stationId` | `uint16` | 主控内部稳定 ID |
| `busAddress` | `uint8` | 物理总线地址，范围 `1..127` |
| `enabled` | `bool` | 是否参与轮询和调度 |
| `onlineState` | `enum` | `unknown/online/offline/error/conflict_suspected/reserved_address` |
| `capabilityFlags` | `uint32` | 分站能力摘要 |
| `firmwareVersion` | `uint16` | 分站固件版本 |
| `protocolVersion` | `uint8` | 协议版本 |
| `lastSeenAt` | `uint32` | 主控时间戳 |
| `lastError` | `uint16` | 最近通讯错误或协议错误 |

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
| `enabled` | `bool` | 是否参与计划和手动操作 |
| `archived` | `bool` | 是否归档 |
| `configVersion` | `uint16` | 配置版本，用于下发去重 |
| `lastKnownState` | `enum` | 主控侧业务状态 |

计划和历史记录引用 `deviceId`，不直接引用 RS485 地址。记录详情保存当时的 `busAddress` 快照，方便诊断和设备替换。

### MotorConfig

主控保存业务参数，动作开始前把分站运行所需的安全参数快照下发给分站。

通用字段：

- `pulsesPerTurn`：每输出圈脉冲，`uint32`，用于适配不同减速比或替换电机。
- `directionInvert`：方向极性反转，`bool`。
- `encoderInvert`：编码器方向反转，`bool`。
- `defaultSpeedPermille`：默认速度，`0..1000`。
- `accelMs` / `decelMs`：加减速时间。
- `overCurrentMa`：过流阈值。
- `overCurrentHoldMs`：过流持续判定时间。
- `stallDetectMs`：堵转检测窗口。
- `stallMinDeltaPulses`：堵转窗口内最小脉冲变化。
- `maxRunMs`：单次动作最大运行时间。
- `maxActionPulses`：单次最大动作脉冲。
- `brakeMode`：停止方式，默认 AT8236 刹车停止。

下料字段：

- `gramsPerTurnMg`：每圈克数，单位 `mg`。
- `feedDirection`：默认下料方向。

门控字段：

- `closedPositionPulses`：关闭位置，固定为 `0`。
- `openPositionPulses`：开门目标位置。
- `doorDirectionOpen`：开门方向。
- `doorDirectionClose`：关门方向。

主控保存明确的 `positionPulses`；如果现场机械位置和记录不一致，维护人员执行校准，把当前位置重新设为关闭点或指定位置。

## 下料量换算

主控负责把用户输入换算成目标脉冲，分站只执行目标脉冲。

- 按圈数：`targetPulses = turns_x1000 * pulsesPerTurn / 1000`。
- 按克数：`targetPulses = amountMg * pulsesPerTurn / gramsPerTurnMg`。

工程要求：

- 换算结果小于 `1` 脉冲时拒绝执行。
- 换算结果超过 `maxActionPulses` 时拒绝执行。
- `gramsPerTurnMg == 0` 时不允许按克数执行，只允许按圈数维护试运行。
- 实际累计下料以分站回报的完成脉冲为准，主控写入记录。

## RS485 帧格式

基本原则：

- 主控是唯一主动发起方，分站只响应被寻址命令。
- 分站不得主动抢占总线。
- 所有多字节整数使用小端序。
- CRC 使用 `CRC16/MODBUS`，复用 `Stc8hBase` 的 `util_crc16_modbus` 口径，覆盖 `version` 到 payload 的所有字节，不覆盖帧头。
- 接收端遇到 CRC 错误、长度错误或版本不支持时直接丢帧；只有能可靠解析目标地址和命令时才回错误响应。
- 地址 `0` 不绑定普通设备。不使用广播动作命令。

帧结构：

| 字段 | 长度 | 说明 |
| --- | ---: | --- |
| `sof0` | 1 | 固定 `0xA5` |
| `sof1` | 1 | 固定 `0x5A` |
| `version` | 1 | 协议版本，当前为 `0x01` |
| `flags` | 1 | bit0=`response`，bit1=`error`，其余保留为 0 |
| `dst` | 1 | 目标地址；主控为 `0`，分站为 `1..127` |
| `src` | 1 | 源地址；主控为 `0`，分站为 `1..127` |
| `seq` | 1 | 主控递增序号，响应原样返回 |
| `cmd` | 1 | 命令码 |
| `len` | 1 | payload 长度，最大 `64` |
| `payload` | `len` | 命令数据 |
| `crc16` | 2 | CRC16，小端 |

时序：

- 主控发送后切回接收态，等待响应。
- 分站收到完整有效帧后，等待至少 `2` 个字符时间再打开发送，使主控完成方向切换。
- 分站响应结束后立即关闭发送并回到接收态。
- 主控命令超时后最多重试 `2` 次；同一 `seq + cmd + actionId` 的重复命令必须可幂等处理。

## 命令与响应

### 命令码

| 命令 | 编码 | 方向 | 用途 |
| --- | ---: | --- | --- |
| `PING` | `0x01` | 主控到分站 | 基础通讯、版本、能力摘要 |
| `GET_STATUS` | `0x02` | 主控到分站 | 读取分站和电机状态 |
| `SET_MOTOR_CONFIG` | `0x10` | 主控到分站 | 下发电机参数和保护参数快照 |
| `START_ACTION` | `0x20` | 主控到分站 | 启动一次有界电机动作 |
| `STOP_ACTION` | `0x21` | 主控到分站 | 停止当前动作 |
| `CLEAR_FAULT` | `0x22` | 主控到分站 | 清除可恢复故障 |
| `IDENTIFY` | `0x30` | 主控到分站 | 闪灯，用于现场识别 |
| `READ_DIAG` | `0x40` | 主控到分站 | 读取诊断快照 |

### 通用响应

所有响应 payload 以 `statusCode` 开头：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `statusCode` | `uint8` | `0` 表示成功，非 0 表示错误 |
| `stationState` | `uint8` | 分站总体状态 |
| `faultCode` | `uint16` | 当前故障码，成功且无故障为 `0` |

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
| `effectiveBusAddress` | `uint8` | 当前生效总线地址 |
| `rawAddressInput` | `uint8` | 原始地址输入值 |
| `capabilityFlags` | `uint32` | 能力位 |

### `GET_STATUS`

请求 payload 为空。

响应 payload：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `statusCode` | `uint8` | 状态码 |
| `stationState` | `uint8` | 分站状态 |
| `faultCode` | `uint16` | 当前故障 |
| `motorState` | `uint8` | 电机状态 |
| `activeActionId` | `uint32` | 当前或最近动作 ID |
| `positionPulses` | `int32` | 当前电机位置或本次累计位置 |
| `targetPulses` | `int32` | 当前目标 |
| `currentMa` | `uint16` | 滤波后电流 |
| `peakCurrentMa` | `uint16` | 本次动作峰值电流 |
| `runMs` | `uint32` | 本次动作已运行时间 |
| `completedPulses` | `uint32` | 本次动作完成脉冲 |
| `lastStopReason` | `uint8` | 最近停止原因 |

### `SET_MOTOR_CONFIG`

请求 payload 是分站运行所需的安全参数快照，不包含设备名称、显示顺序、计划等主控业务数据。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `configVersion` | `uint16` | 主控配置版本 |
| `flags` | `uint16` | 方向反转、编码器反转、刹车模式等 |
| `pulsesPerTurn` | `uint32` | 每输出圈脉冲 |
| `defaultSpeedPermille` | `uint16` | 默认速度 |
| `accelMs` | `uint16` | 加速时间 |
| `decelMs` | `uint16` | 减速时间 |
| `overCurrentMa` | `uint16` | 过流阈值 |
| `overCurrentHoldMs` | `uint16` | 过流持续时间 |
| `stallDetectMs` | `uint16` | 堵转窗口 |
| `stallMinDeltaPulses` | `uint16` | 堵转最小脉冲 |
| `maxRunMs` | `uint32` | 最大运行时间 |
| `maxActionPulses` | `uint32` | 单次动作最大脉冲 |

响应成功后，分站只保证运行期缓存生效。成品设计中，分站不保存用户业务配置到片内 EEPROM/IAP，避免 STC8H 参数区 split 和配置多源一致性风险。

### `START_ACTION`

请求 payload：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `actionId` | `uint32` | 主控生成的动作 ID |
| `deviceType` | `uint8` | `1` 下料，`2` 门控，仅用于记录和诊断 |
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

- 分站必须先校验已收到有效配置、无不可恢复故障、目标不超过限制。
- 相同 `actionId` 的重复 `START_ACTION` 不得重复启动电机。
- 如果动作已完成且分站仍缓存最近结果，重复请求返回最近结果。
- 如果电机正在执行其他 `actionId`，返回 `ERR_BUSY`。
- 所有动作都是有界动作，不设计无限运行命令。

### `STOP_ACTION`

请求 payload：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `actionId` | `uint32` | 停止动作 ID，主控生成 |
| `stopMode` | `uint8` | 默认刹车停止 |

分站收到后必须立即进入安全停止流程。停止是安全动作，即使配置版本不匹配也应执行。

### `CLEAR_FAULT`

只清除可恢复故障，例如用户已确认堵转原因并解除机械阻塞。对于一般动作失败，下一次动作前重新做可执行条件检查；不把“清除故障”设计成日常必需步骤。

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
| `2` | `STATION_CONFIG_REQUIRED` | 尚未收到有效电机配置 |
| `3` | `STATION_RUNNING` | 电机正在动作 |
| `4` | `STATION_FAULT` | 存在故障 |
| `5` | `STATION_ADDRESS_RESERVED` | 地址为 0，禁止普通动作 |

### 电机状态

| 编码 | 名称 | 说明 |
| ---: | --- | --- |
| `0` | `MOTOR_UNCONFIGURED` | 未配置 |
| `1` | `MOTOR_IDLE` | 空闲 |
| `2` | `MOTOR_RUNNING` | 正在执行动作 |
| `3` | `MOTOR_STOPPING` | 正在停止 |
| `4` | `MOTOR_COMPLETED` | 最近动作完成 |
| `5` | `MOTOR_STOPPED` | 被停止 |
| `6` | `MOTOR_FAULT` | 故障 |

### 协议状态码

| 编码 | 名称 | 说明 |
| ---: | --- | --- |
| `0x00` | `OK` | 成功 |
| `0x01` | `ERR_UNSUPPORTED_VERSION` | 协议版本不支持 |
| `0x02` | `ERR_BAD_LENGTH` | payload 长度错误 |
| `0x03` | `ERR_BAD_PARAM` | 参数越界或非法 |
| `0x04` | `ERR_NOT_CONFIGURED` | 电机未配置 |
| `0x05` | `ERR_BUSY` | 电机正在执行其他动作 |
| `0x06` | `ERR_FAULT_ACTIVE` | 存在未处理故障 |
| `0x07` | `ERR_ACTION_DUPLICATE` | 重复动作，未重新执行 |
| `0x08` | `ERR_SAFETY_BLOCKED` | 安全条件不满足 |
| `0x09` | `ERR_ADDRESS_RESERVED` | 地址 0 或保留地址状态 |
| `0x0A` | `ERR_INTERNAL` | 分站内部错误 |

### 故障码

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
| `0x000A` | `FAULT_RESERVED_ADDRESS` | 地址为 0 |
| `0x000B` | `FAULT_COMMAND_REJECTED` | 分站明确拒绝命令，且响应没有更具体故障码 |

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
2. 默认依次向地址 `1..127` 发送 `PING`。
3. 对有效响应记录 `stationId`、能力和固件版本。
4. 对已绑定但无响应的地址标记为 `offline`，不删除绑定。
5. 地址 `0` 为保留地址，不参与普通扫描。若维护模式单独探测到地址 `0` 响应，只记录为 `reserved_address` 并提示现场调整地址，不允许绑定。

### 正常轮询

- 空闲分站：轮询 `PING` 或 `GET_STATUS`。
- 已绑定设备：按设备需要轮询 `GET_STATUS`。
- 动作中设备：提高到约 `200 ms` 状态轮询，直到完成、停止或故障。
- 轮询调度必须给手动停止命令最高优先级。

### 离线与冲突

- 连续通讯失败达到阈值后，分站标记 `offline`。
- 离线不等于设备删除，计划引用和历史记录保持。
- 地址冲突无法直接证明多个分站同地址，只能根据异常响应推断：同一地址连续出现 CRC 错误、长度异常、响应字段跳变、多个尾部残留字节时标记 `conflict_suspected`。
- `conflict_suspected` 不允许执行动作，只允许停止、重新扫描和维护提示。

## 分站通用安全策略

分站必须满足以下本地闭环要求：

- 收到 `START_ACTION` 后，动作目标、方向、速度、最大运行时间、最大脉冲和保护阈值已经完整落在分站运行期缓存中。
- 动作执行期间即使主控暂时失联，分站也必须按目标到达、超时、过流、堵转等本地条件完成或保护停止。
- 通讯中断本身不触发停止，因为所有动作都是有界动作；不能因为主控短暂丢包导致门或下料器停在不可预期位置。
- 分站上电、复位、看门狗复位、地址为 0、配置非法时，电机输出必须保持关闭。
- 分站不得执行广播动作命令。
- 停止命令不受配置版本限制，必须优先执行。

## 通用电机动作状态机

分站状态机只描述电机动作，不承载复杂业务规则。

| 状态 | 说明 |
| --- | --- |
| `MOTOR_UNCONFIGURED` | 未收到有效配置 |
| `MOTOR_IDLE` | 空闲 |
| `MOTOR_RUNNING` | 正在执行有界动作 |
| `MOTOR_STOPPING` | 正在停止 |
| `MOTOR_COMPLETED` | 最近动作到达目标 |
| `MOTOR_STOPPED` | 被主控停止 |
| `MOTOR_FAULT` | 触发保护或硬件异常 |

转移规则：

- `MOTOR_IDLE/MOTOR_COMPLETED/MOTOR_STOPPED` 收到合法动作后进入 `MOTOR_RUNNING`。
- `MOTOR_RUNNING` 到达目标后进入 `MOTOR_COMPLETED`。
- `MOTOR_RUNNING` 收到停止命令后进入 `MOTOR_STOPPING`，输出关闭后进入 `MOTOR_STOPPED`。
- `MOTOR_RUNNING` 触发过流、堵转、编码器异常、超时或超限后立即刹车并进入 `MOTOR_FAULT`。
- 一般动作失败记录故障和停止原因；下一次动作前重新做安全条件检查。不可恢复硬件故障必须保持阻止动作，直到维护处理。

## 门控业务状态机

门控业务状态由主控维护。

| 状态 | 说明 |
| --- | --- |
| `DOOR_IDLE` | 空闲 |
| `DOOR_CLOSED` | 当前位置为关闭点 `0` |
| `DOOR_OPEN` | 当前位置达到开门目标 |
| `DOOR_OPENING` | 正在开门 |
| `DOOR_CLOSING` | 正在关门 |
| `DOOR_STOPPED` | 被停止，主控保存分站回报的位置 |
| `DOOR_FAULT` | 最近动作触发故障或保护 |

关键动作：

- 开门：主控发送绝对目标 `openPositionPulses`。
- 关门：主控发送绝对目标 `0`。
- 停止：分站立即刹车停止，并回报当前位置。
- 校准关闭：维护人员确认门在关闭位置后，主控把该设备当前位置设为 `0`。
- 校准行程：维护流程更新 `openPositionPulses`，不使用限位开关自动学习。

位置持久化：

- 主控是门控位置持久化的权威来源，写入 FRAM。
- 动作中主控按状态回报更新 FRAM 运行快照；写入应做节流，例如 `1 s` 或位置变化超过阈值才写。
- 动作完成、停止、故障时必须立即提交最终位置和结果。
- 如果现场机械位置和记录不一致，维护人员执行校准。

## 下料业务状态机

| 状态 | 说明 |
| --- | --- |
| `FEEDER_IDLE` | 空闲 |
| `FEEDER_FEEDING` | 正在下料 |
| `FEEDER_STOPPING` | 正在停止 |
| `FEEDER_COMPLETED` | 最近一次下料完成 |
| `FEEDER_STOPPED` | 被手动或调度停止 |
| `FEEDER_FAULT` | 最近动作触发故障或保护 |

转移规则：

- `FEEDER_IDLE/COMPLETED/STOPPED` 收到合法下料命令后进入 `FEEDER_FEEDING`。
- `FEEDER_FEEDING` 到达目标脉冲后进入 `FEEDER_COMPLETED`。
- `FEEDER_FEEDING` 收到停止命令后进入 `FEEDER_STOPPING`，输出关闭后进入 `FEEDER_STOPPED`。
- `FEEDER_FEEDING` 触发过流、堵转、编码器异常、超时或超限后进入 `FEEDER_FAULT`。

下料记录包含：

- `actionId`。
- `deviceId`。
- 当时绑定的 `busAddress`。
- 计划 ID 或手动来源。
- 目标脉冲、目标克数或目标圈数。
- 完成脉冲。
- 停止原因和故障码。
- 开始时间、结束时间、运行时长。
- 峰值电流和异常摘要。

## FRAM 数据布局

主控 FRAM 按 FM24CL64B 的 64Kbit / 8KiB 规划。FRAM 保存关键配置索引、当前运行设备的绑定缓存、计划摘要、运行快照和短事务日志；完整设备注册表和完整历史记录放 LittleFS 业务文件，App Events 只保存近期关键事件。

FRAM 容量不能成为“系统最多多少业务设备”的产品定义。FRAM 只保证当前启用设备和关键运行状态可靠恢复。

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

- `Board` 层只暴露逻辑信号：`RS485_TX/RX/DE`、`ADDR_INPUT`、`MOTOR_PWM/IN1/IN2/STBY`、`ENC_A/ENC_B`、`CURRENT_ADC`、`LED_RUN`、`LED_ERR` 等。
- `MotorDriver` 不直接引用 STC8H 端口号，只调用 `BoardMotorIo`。
- `Encoder` 不直接绑定旧网表命名，只绑定 `BoardEncoderIo`，读取霍尔 A/B 两相并按固定计数口径累计脉冲。
- `CurrentSense` 不关心 INA240A2 接在哪个 ADC 管脚，只接收板级 ADC 输入编号和换算参数。
- `StationCapabilities` 由编译期板级配置生成，主控通过 `PING` 读取。
- 新 PCB 完成后，只替换 `board_pins.h`、板级能力表和 ADC/编码器映射，不改变 RS485 协议和主控业务模型。

临时 IO 分配必须满足：

- 所有输出上电默认安全态。
- AT8236 方向和 PWM 切换有明确停止/死区流程。
- 霍尔 AB 相编码器输入优先使用可稳定中断或定时采样的管脚。
- INA240A2 ADC 输入必须有标定参数入口。
- 地址输入必须有上拉/下拉默认态说明。

## 主控审查结论与工程风险

- 一板一电机作为成品模型接受。
- 门控和下料共用通用分站能力，区别放在主控业务层。
- RS485 地址为 1 字节，地址 `0` 保留，普通地址 `1..127`，主控默认扫描全部普通地址。
- RS485 默认 `9600 8N1`、`80 ms` 超时、`5` 次失败离线作为联调起点，最终按现场总线长度和干扰实测校正。
- 分站通讯中断时继续完成有界动作，而不是立即停止；这是本地闭环安全边界的一部分。
- 门控无绝对位置传感器，主控持久化位置依赖编码器连续性；现场位置不一致时通过校准修正。
- FRAM 8KiB 容量只保存关键状态和短 journal，完整历史进入 LittleFS。
- 分站不持久化业务配置，避免 STC8H IAP/EEPROM split 和配置多源一致性风险；这意味着分站上电后必须等待主控下发配置才允许运行动作。
- 过流、堵转、编码器异常的默认阈值不能从旧项目照抄，必须实测后固化。
