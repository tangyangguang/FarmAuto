# 配置项、默认值与保存策略草案

## 目标

本文记录两个应用和公共库相关参数的分类、单位、范围、默认值、保存时机和待确认项。

默认值在实机测试前只能作为初始建议，不能视为最终量产参数。

## 通用规则

- 所有配置必须有单位。
- 所有配置必须有合法范围。
- Web 保存前必须校验。
- 应用系统参数优先注册到 Esp32Base App Config，由 Esp32Base 内置配置页面负责显示、校验、保存和只写变化字段。
- 业务系统不重复实现参数配置页和参数持久化；只实现运行控制页、状态页、维护动作和诊断记录。
- App Config 只放低频、结构化、偏系统/硬件/策略的参数。
- 业务运行数据、当前状态、一次性维护动作和需要专门交互的业务数据，放在应用业务页面。
- 运行状态只在停止、故障、端点校准、清计数、日期切换等关键点保存。
- 运行中是否允许修改配置必须逐项定义。
- AT24C128 主要保存关键可靠状态、校准快照、今日计数摘要和恢复所需记录，不作为普通系统参数配置页的唯一承载。
- Esp32Base App Config 的 groups/fields 容量需要在源码前按两个应用字段数设置构建宏；如果字段组织能力不足，先整理 Esp32Base 提示词，不在 FarmAuto 内另做配置系统。

分类规则：

| 分类 | 页面/存储 | 例子 |
| --- | --- | --- |
| 系统配置 | Esp32Base App Config + Esp32Base 持久化 | 电机速度、软启动、减速比、最大运行时间、安全阈值、是否启用某硬件 |
| 业务配置 | 应用业务页面 + 应用持久化服务 | 喂食目标、每日计划、通道基础信息、饲料桶容量 |
| 维护/校准数据 | 应用校准/诊断页面 + AT24C128 关键记录 | INA240 零点、每圈克数标定、端点快照、当前位置基准 |
| 运行状态 | 应用状态机 + AT24C128/flash | 当前位置、跳过今日、今日计数、当前估算余量、最近故障、长期原始记录 |

## 编码器计数口径

默认采用 `X1` 单边计数口径，也就是只统计单个边沿，默认值对应电机轴每圈 16 个计数。

只读查看老项目后看到：喂食器电机代码使用 `encoder.attachSingleEdge(encoderPinA, encoderPinB)`，并注明这样与“转一圈触发 16 个 AB 信号”对应；自动门电机代码也使用 `motorEncoder.attachSingleEdge(ENCODER_A_IO, ENCODER_B_IO)`。这只能作为硬件口径线索，新实现仍按公共电机库的 `X1 / X2 / X4` 配置独立设计。

默认计算：

```text
outputPulsesPerRev = gearRatio * motorShaftPulsesPerRev
```

但 `outputPulsesPerRev` 必须允许手动覆盖。原因是实际减速比、编码器输出口径、传动误差或用户实测值可能与标称值不完全一致。实现时建议同时保存：

- `gearRatioX100`
- `motorShaftPulsesPerRev`
- `countMode`
- `outputPulsesPerRev`
- `outputPulsesPerRevOverrideEnabled`

## Esp32FarmDoor 系统配置草案

这些参数建议优先注册到 Esp32Base App Config。

| 参数 | 单位 | 默认值 | 范围 | 运行中可改 | 备注 |
| --- | --- | --- | --- | --- | --- |
| motorSpeedPercent | % | 80 | 1-100 | 否 | 待实机确认 |
| softStartMs | ms | 1000 | 0-10000 | 否 | 独立软启动 |
| softStopMs | ms | 500 | 0-10000 | 否 | 独立软停止 |
| openTargetTurnsX100 | 0.01 圈 | 行程校准生成 | >0 | 否 | 开门行程圈数；可在 Esp32Base App Config 直接修改，也可由行程校准页“用当前位置更新开门目标”自动生成 |
| manualMoveMaxMs | ms | 1000 | 100-3000 | 否 | 单次手动运行最大时长 |
| manualMoveSpeedPercent | % | 30 | 10-100 | 否 | 行程校准手动运行速度 |
| gearRatioX100 | 0.01 | 13100 | >0 | 否 | 131:1 |
| motorShaftPulsesPerRev | pulses | 16 | >0 | 否 | 默认 X1 单边计数 |
| countMode | enum | X1 | X1/X2/X4 | 否 | 默认 X1，保持与标称 16 脉冲口径一致 |
| outputPulsesPerRev | pulses | 2096 | >0 | 否 | 可由参数计算，也允许手动覆盖 |
| outputPulsesPerRevOverrideEnabled | bool | false | true/false | 否 | true 时使用手动值 |
| travelAdjustStepTurnsX100 | 0.01 圈 | 25 | >0 | 否 | 行程校准手动运行默认步长，0.25 圈 |
| currentGuardEnabled | bool | true | true/false | 否 | 可关闭 |
| currentFaultThresholdMa | mA | 2500 | >0 | 否 | 待实机确认 |
| rsenseMilliOhm | mΩ | 5 | >0 | 否 | 待实物确认 |
| maxRunMs | ms | 目标运行估算 * 150% | >0 | 否 | 安全兜底，源码按行程校准结果生成初始值 |
| maxRunPulses | pulses | openTargetPulses * 150% | >0 | 否 | 第一版无限位默认更保守；下一阶段有上限位后可实测缩紧 |
| openLimitSwitchMode | enum | Disabled | Disabled/NormallyClosed/NormallyOpen | 否 | 第一版默认禁用，下一阶段优先启用开门/上限位 |
| closeLimitSwitchMode | enum | Disabled | Disabled/NormallyClosed/NormallyOpen | 否 | 关门/下限位，可选 |
| limitDebounceMs | ms | 50 | 5-500 | 否 | 限位稳定时间 |
| openLimitCalibrateSpeedPercent | % | 30 | 1-100 | 否 | 远程端点校准低速运行 |
| openLimitCalibrateMaxMs | ms | maxRunMs | >0 | 否 | 下一阶段限位端点校准最大时长 |
| openLimitCalibrateMaxPulses | pulses | maxRunPulses | >0 | 否 | 下一阶段限位端点校准最大脉冲 |
| maxCloseUnwindPulses | pulses | openTargetPulses * 150% | >0 | 否 | 关门最大放绳脉冲，防止过放反卷；第一版无限位默认更保守 |
| faultEmergencyOutputMode | enum | Coast | Coast/Brake | 否 | 故障停机默认倾向滑行，需实测确认 |
| motionCheckpointMinIntervalMs | ms | 2000 | 1000-10000 | 否 | 运行中断电恢复检查点最小时间间隔 |
| motionCheckpointMinTravelPercent | % | 5 | 1-20 | 否 | 运行中断电恢复检查点最小行程变化 |
| recoveredFirstMoveSpeedPercent | % | 50 | 1-100 | 否 | 断电恢复后第一次动作速度，实际使用 min(recoveredFirstMoveSpeedPercent, motorSpeedPercent) |

Esp32FarmDoor 校准/运行数据，不放在 App Config：

行程类参数在 Web 页面上优先用圈数展示和输入，内部按 `outputPulsesPerRev` 换算为脉冲保存。最大运行圈数和最大运行时间是保底保护边界，触发时默认记录保护停止或告警；只有连续重复触发或伴随电流、编码器、方向、限位冲突等异常时，才升级为故障。

- `ina240ZeroOffsetMv`：INA240A2 零点校准结果，由诊断页写入。
- `currentPositionPulses`：当前位置，由运行停止、故障或行程校准后保存。
- `openEndpointSnapshot` / `closeEndpointSnapshot`：端点快照，由行程校准流程保存。
- 最近故障、最近停止原因、长期原始记录索引。

## Esp32FarmFeeder 系统配置草案

这些参数建议优先注册到 Esp32Base App Config。三路喂食器电机虽然同系列，但转速比可能不同，因此速度、软启动、软停止、减速比、输出轴每圈脉冲等都按每路独立配置。

| 参数 | 单位 | 默认值 | 范围 | 运行中可改 | 备注 |
| --- | --- | --- | --- | --- | --- |
| feeder1MotorSpeedPercent | % | 100 | 1-100 | 否 | 每路独立 |
| feeder2MotorSpeedPercent | % | 100 | 1-100 | 否 | 每路独立 |
| feeder3MotorSpeedPercent | % | 100 | 1-100 | 否 | 每路独立 |
| feeder1SoftStartMs | ms | 1000 | 0-10000 | 否 | 每路独立 |
| feeder2SoftStartMs | ms | 1000 | 0-10000 | 否 | 每路独立 |
| feeder3SoftStartMs | ms | 1000 | 0-10000 | 否 | 每路独立 |
| feeder1SoftStopMs | ms | 500 | 0-10000 | 否 | 每路独立 |
| feeder2SoftStopMs | ms | 500 | 0-10000 | 否 | 每路独立 |
| feeder3SoftStopMs | ms | 500 | 0-10000 | 否 | 每路独立 |
| feeder1GearRatioX100 | 0.01 | 27000 | >0 | 否 | 每路可不同 |
| feeder2GearRatioX100 | 0.01 | 27000 | >0 | 否 | 每路可不同 |
| feeder3GearRatioX100 | 0.01 | 27000 | >0 | 否 | 每路可不同 |
| motorShaftPulsesPerRev | pulses | 16 | >0 | 否 | 默认 X1 单边计数 |
| countMode | enum | X1 | X1/X2/X4 | 否 | 默认 X1 |
| feeder1OutputPulsesPerRev | pulses | 4320 | >0 | 否 | 可计算，也允许手动覆盖 |
| feeder2OutputPulsesPerRev | pulses | 4320 | >0 | 否 | 可计算，也允许手动覆盖 |
| feeder3OutputPulsesPerRev | pulses | 4320 | >0 | 否 | 可计算，也允许手动覆盖 |
| outputPulsesPerRevOverrideEnabled | bool | false | true/false | 否 | true 时使用每路手动值 |
| startAllIntervalMs | ms | 1000 | >=0 | 否 | 顺序启动间隔 |
| maxRunMs | ms | 300000 | >0 | 否 | 默认 5 分钟 |
| maxRunPulses | pulses | 432000 | >0 | 否 | 默认 100 圈 |

当前硬件没有电流检测。未来如增加，每电机一个 INA240A2 芯片，对应配置再加入每路电流阈值和校准参数。

Esp32FarmFeeder 业务配置和维护数据，不放在 App Config：

| 数据 | 页面 | 保存建议 | 备注 |
| --- | --- | --- | --- |
| 每路目标模式 | 首页/计划页 | AT24C128 `FeederChannelTarget` | Grams/Revolutions，每路可独立选择；仅表示手动下料默认目标 |
| 每路目标克数 | 喂食控制/计划页 | AT24C128 `FeederChannelTarget` | 克数模式使用；计划内目标随 `FeederSchedule` 保存 |
| 每路目标圈数 | 喂食控制/计划页 | AT24C128 `FeederChannelTarget` | 圈数模式使用；计划内目标随 `FeederSchedule` 保存 |
| 每路每圈下料克数 | 基础信息页 | AT24C128 校准记录 | 手工录入，不发起实际运转测试 |
| 每路饲料桶容量 | 基础信息页 | 应用持久化服务 | 属于业务基础信息，不是硬件系统参数 |
| 每路当前估算余量 | 基础信息页的余量维护区 | AT24C128 关键状态 + 长期记录 | 补料和投喂扣减更新，不能放基础信息编辑表单或 App Config |
| 每日计划列表 | 计划页 | 应用持久化服务 | 最多 6 条；每个计划独立启用、时间、参与通道和每路目标；未配置时间时该计划不自动投喂 |
| 跳过今日/明日 | 计划页 | AT24C128 今日/明日状态 | 只影响具体日期，不删除长期计划，日期切换后自动清除过期状态 |
| 今日计划已尝试状态 | 首页/计划页 | AT24C128 今日状态 | 定时投喂启动后即记录；断电中断后不再次自动触发 |
| 今日计数 | 首页/记录服务 | AT24C128 今日状态 | 日期切换后归档 |
| 未完成投喂命令摘要 | 启动恢复/记录服务 | AT24C128 恢复记录 | 仅用于重启后记录 `PowerLossAborted` 和阻止自动续喂，不用于续投 |

App Config 字段计数：

- Esp32FarmDoor 当前建议注册 28 个字段，推荐构建配置 `ESP32BASE_APP_CONFIG_MAX_FIELDS >= 48`，给后续限位、电流和实机校准保留余量。
- Esp32FarmFeeder 当前建议注册 20 个字段，推荐构建配置 `ESP32BASE_APP_CONFIG_MAX_FIELDS >= 40`，给未来每路电流检测和少量新增参数保留余量。
- 两个应用是独立固件，字段数分别计算，不相加。

## 长期原始记录策略

推荐：

- 介质：ESP32 flash 文件系统，优先 LittleFS。
- 分区：1MB 起步；如果所选 ESP32 模组 flash 容量允许，优先 2MB。
- 格式：首版使用紧凑二进制 segment 作为内部存储，Web 导出时提供 JSON Lines 和 CSV。
- 分段：按天分段，单日超过大小上限时再切分。
- 写入：普通事件批量 flush，关键事件立即 flush。
- 轮转：容量不足时告警并覆盖最旧 segment，不停止设备运行。
- 告警：剩余 30% 进入 warning，剩余 10% 进入 maintenance。
- AT24C128：只保存长期记录索引摘要或最近写入位置，不保存多年原始记录正文。

原因：

- 多年原始记录容量远大于 AT24C128 适合范围。
- flash 文件系统更适合追加式业务记录和远程导出。
- AT24C128 更适合可更换的小型关键状态。

## 已确认

- 电机轴每圈脉冲数默认按 X1 单边计数口径。
- `outputPulsesPerRev` 允许手动覆盖。
- 三路喂食器速度、软启动、软停止都需要每路独立。
- 饲料桶当前估算余量不放 Esp32Base App Config，也不放基础信息编辑表单；通过补料、标记满桶或修正余量入口维护。
