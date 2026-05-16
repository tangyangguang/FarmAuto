# 应用长期记录事件 Schema 草案

## 目标

本文定义 Esp32FarmDoor 和 Esp32FarmFeeder 长期原始记录的事件类型和字段语义。它面向应用层记录服务，不属于公共库 API。

首版目标不是做复杂数据库，而是用紧凑、可导出、可诊断的结构化事件支持多年记录。

## 通用记录头

每条记录都应包含通用头：

| 字段 | 类型 | 单位 | 说明 |
| --- | --- | --- | --- |
| `schemaVersion` | uint16 | - | 应用记录 schema 版本 |
| `eventType` | enum uint16 | - | 事件类型 |
| `result` | enum uint16 | - | Ok / Busy / Fault / Cancelled / Timeout / NotConfigured 等 |
| `unixTime` | int64 | s | NTP 有效时填写；无效时为 0 |
| `uptimeMs` | uint32 | ms | 设备启动后毫秒数 |
| `sequence` | uint32 | - | 应用长期记录递增序号 |
| `source` | enum uint8 | - | Web / Button / Schedule / Maintenance / System |
| `channel` | uint8 | - | 0 表示全局；1..3 表示喂食器通道 |
| `flags` | uint16 | - | 位标记，例如 timeValid、positionTrustLevel |
| `payloadLength` | uint16 | bytes | payload 长度 |
| `headerCrc` | uint32 | - | 固定头 CRC |
| `payloadCrc` | uint32 | - | payload CRC |

记录原则：

- 原始值优先，例如 pulses、durationMs、currentMa、gramsX100。
- 显示层再换算成圈数、百分比、文本。
- 不记录每个编码器脉冲。
- 高频 trace 只在 RAM 中短期保留；长期记录只保存摘要或故障窗口摘要。
- 记录格式采用固定头 + 变长 payload，文件模型见 `docs/apps/18-long-term-records.md`。
- CRC 算法首版统一使用 CRC-32/ISO-HDLC；后续改变算法必须提升 schemaVersion。实现上优先复用 `Esp32At24cRecordStore` 提供的 `Crc32IsoHdlc` helper，避免应用记录服务和 AT24C 记录库各写一套。
- 所有字段固定 little-endian 且必须字段级显式序列化，禁止直接写 C++ 结构体内存。
- 克数统一使用 `*GramsX100` 或 `*GramsPerRevX100`，单位为 0.01g；不再在事件 payload 中混用整数克字段。
- `uptimeMs` 是 32 位启动后毫秒数，约 49.7 天回绕；它只用于短窗口诊断和相对时间展示。长期记录查询、排序、回溯和导出必须依赖 `unixTime`、`sequence` 和文件 segment 顺序，不能依赖 `uptimeMs` 单独判断先后。

## 通用事件类型

| eventType | 说明 | 典型 payload |
| --- | --- | --- |
| `Boot` | 启动完成 | bootReason、firmwareVersion、configVersion |
| `TimeSyncChanged` | 时间同步状态变化 | oldValid、newValid、unixTime |
| `ConfigChanged` | 系统配置变化摘要 | ns/key 或配置组、oldHash、newHash |
| `StorageWarning` | 存储 warning/maintenance | medium、freeBytes、errorCount |
| `StorageFault` | 存储错误 | medium、operation、errorCode |
| `MaintenanceAction` | 维护动作 | action、target、result |
| `FaultCleared` | 清除故障 | previousFault、positionTrustLevel |

## Esp32FarmDoor 事件

### 运动事件

| eventType | 触发时机 | payload 字段 |
| --- | --- | --- |
| `DoorCommandRequested` | 用户或按钮请求开/关/停 | command、targetPulses、positionPulses |
| `DoorMotionStarted` | 电机开始运动 | direction、startPositionPulses、targetPulses、speedPercent |
| `DoorMotionStopped` | 电机停止 | stopReason、endPositionPulses、durationMs、deltaPulses |
| `DoorTargetReached` | 达到目标 | targetType、targetPulses、endPositionPulses、durationMs |
| `DoorUserStopped` | 用户停止 | positionPulses、durationMs |
| `DoorFaultStopped` | 故障停止 | faultReason、positionPulses、currentMa、durationMs |

`stopReason` 建议值：

- `TargetReached`
- `UserStop`
- `OverCurrent`
- `EncoderNoPulse`
- `MaxRunTime`
- `MaxRunPulses`
- `UnexpectedLimit`
- `StorageFault`

### 端点维护事件

| eventType | 触发时机 | payload 字段 |
| --- | --- | --- |
| `MaintenanceFlowRequested` | 进入维护子流程 | flowName、entryReason、positionTrustLevel |
| `DoorManualMove` | 手动运行完成 | direction、durationMs、deltaPulses、stopReason |
| `DoorPositionSet` | 设置当前位置 | oldPositionPulses、newPositionPulses、reason |
| `DoorTravelSet` | 直接设置行程 | oldTravelPulses、newTravelPulses、travelTurnsX100、source |
| `DoorTravelAdjusted` | 微调行程 | oldTravelPulses、newTravelPulses、deltaPulses、deltaTurnsX100 |
| `DoorEndpointSaved` | 保存开门/关门端点 | endpointType、positionPulses、maxRunPulses |
| `DoorEndpointVerified` | 低速端点验证完成 | openOk、closeOk、durationMs、maxObservedCurrentMa |
| `DoorPositionTrustChanged` | 位置可信度变化 | oldTrustLevel、newTrustLevel、reason |
| `DoorMotionCheckpoint` | 运行中低频位置检查点 | command、direction、positionPulses、targetPulses、checkpointReason |
| `DoorPowerLossRecovered` | 断电后位置恢复 | recoveredPositionPulses、source、confidence、journalSequence |

第一版无限位场景必须记录：

- 进入端点维护。
- 每次手动运行。
- 设置关闭点。
- 保存开门目标。
- 直接设置或微调行程。
- 生成安全上限。
- 低速验证结果。
- 运行中断电后的恢复结果。

### 传感器和电流事件

| eventType | 触发时机 | payload 字段 |
| --- | --- | --- |
| `DoorCurrentWarning` | 电流接近阈值 | currentMa、filteredMa、thresholdMa |
| `DoorCurrentFault` | 电流保护故障 | currentMa、filteredMa、thresholdMa、durationMs |
| `DoorLimitChanged` | 下一阶段限位状态变化 | limitId、active、positionPulses |
| `DoorLimitFault` | 下一阶段限位异常 | reason、openLimit、closeLimit |

## Esp32FarmFeeder 事件

### 投喂事件

| eventType | 触发时机 | payload 字段 |
| --- | --- | --- |
| `FeederManualRequested` | 手动投喂请求 | channelMask、targetMode、targetPulses |
| `FeederScheduleTriggered` | 每日计划触发 | scheduleTimeMinutes、channelMask |
| `FeederScheduleMissed` | 错过计划且不补投 | scheduleTimeMinutes、reason |
| `FeederScheduleChannelSkipped` | 计划触发时跳过单路 | channel、reason、runningCommandId |
| `FeederStartAllRequested` | 启动全部请求 | channelMask、startIntervalMs |
| `FeederChannelStarted` | 单路开始 | channel、targetPulses、speedPercent |
| `FeederChannelCompleted` | 单路完成 | channel、actualPulses、durationMs、gramsX100 |
| `FeederPowerLossAborted` | 投喂运行中断电后重启 | commandId、source、channelMask、actualPulsesMask、actualPulses[]、targetPulses[]、scheduleAttemptedToday、autoResumeBlocked |
| `FeederChannelStopped` | 单路停止 | channel、stopReason、actualPulses、durationMs |
| `FeederChannelFault` | 单路故障 | channel、faultReason、actualPulses、durationMs |
| `FeederBatchCompleted` | 一次全部/计划完成 | requestedMask、successMask、busyMask、faultMask、skippedMask、durationMs |

字段说明：

- `channelMask` 使用 bit0..bit2 表示 1..3 路。
- `requestedMask` / `successMask` / `busyMask` / `faultMask` / `skippedMask` 使用同一 bit 口径；未来扩展 4 路时继续使用 bit3。
- `gramsX100` 使用 0.01g 定点数。
- `actualPulses` 永远保存原始脉冲数。
- 部分成功不应只保存文本摘要，必须保存各 mask，方便远程统计和故障回溯。

### 计划和今日状态事件

| eventType | 触发时机 | payload 字段 |
| --- | --- | --- |
| `FeederSkipTodaySet` | 设置跳过今日 | scheduleDate、enabled |
| `FeederTodayCleared` | 清空今日计数 | oldPulses1..3、oldGramsX1001..3 |
| `FeederDayRolled` | 日期切换归档 | date、pulses1..3、gramsX1001..3 |

规则：

- 清空今日计数不删除长期记录。
- 跳过今日不删除每日计划。
- 时间无效导致自动计划暂停时，应记录 `FeederScheduleMissed` 或 `TimeSyncChanged`。

### 标定和饲料桶事件

| eventType | 触发时机 | payload 字段 |
| --- | --- | --- |
| `FeederCalibrationRun` | 固定圈数标定运行完成 | channel、actualPulses、revolutionsX100、durationMs |
| `FeederCalibrationSaved` | 保存每圈下料克数 | channel、oldGramsPerRevX100、newGramsPerRevX100 |
| `FeederTestRun` | 单路小剂量测试 | channel、actualPulses、durationMs、result |
| `BucketRefilled` | 补料记录 | channel、oldRemainGramsX100、newRemainGramsX100、addedGramsX100 |
| `BucketSetRemain` | 手动设置余量 | channel、oldRemainGramsX100、newRemainGramsX100 |
| `BucketEstimateChanged` | 投喂后扣减 | channel、oldRemainGramsX100、newRemainGramsX100、usedGramsX100 |
| `BucketLowWarning` | 低余量告警 | channel、remainGramsX100、thresholdPercent |
| `BucketEstimateUnderflow` | 估算余量不足但仍投喂 | channel、remainBeforeGramsX100、usedGramsX100 |

规则：

- 标定失败不更新旧标定值。
- 单路小剂量测试不计入今日正式投喂量。
- 补料和余量设置必须进入长期记录。

## 导出字段建议

JSON Lines：

- 保留通用头和完整 payload 字段。
- 字段名使用英文稳定命名。
- 时间同时输出 `unixTime` 和 `uptimeMs`。

CSV：

- 固定列：time、uptimeMs、eventType、source、channel、result。
- 常用值列：pulses、durationMs、gramsX100、currentMa、faultReason。
- 事件独有字段可放入 `details` JSON 字符串，避免 CSV 列无限膨胀。

## 版本和兼容

- 首版 schemaVersion 从 1 开始。
- 新增字段优先追加，不改变旧字段语义。
- 删除或改变单位必须提升 schemaVersion。
- 读取旧记录时，未知 eventType 可以显示为 `UnknownEvent`，但不能导致记录页崩溃。
