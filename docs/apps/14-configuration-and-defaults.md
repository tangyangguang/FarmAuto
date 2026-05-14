# 配置项、默认值与保存策略草案

## 目标

本文记录两个应用和公共库相关配置项的单位、范围、默认值、保存时机和待确认项。

默认值在实机测试前只能作为初始建议，不能视为最终量产参数。

## 通用规则

- 所有配置必须有单位。
- 所有配置必须有合法范围。
- Web 保存前必须校验。
- 配置变更时写入持久化。
- 运行状态只在停止、故障、归零、清计数、日期切换等关键点保存。
- 运行中是否允许修改配置必须逐项定义。

## Esp32FarmDoor 配置草案

| 配置项 | 单位 | 默认值 | 范围 | 运行中可改 | 备注 |
| --- | --- | --- | --- | --- | --- |
| motorSpeedPercent | % | 80 | 1-100 | 否 | 待实机确认 |
| softStartMs | ms | 1000 | 0-10000 | 否 | 独立软启动 |
| softStopMs | ms | 500 | 0-10000 | 否 | 独立软停止 |
| openTargetTurnsX100 | 0.01 圈 | 待确认 | >0 | 否 | 开门目标 |
| jogTurnsX100 | 0.01 圈 | 待确认 | >0 | 否 | 微调量 |
| gearRatioX100 | 0.01 | 13100 | >0 | 否 | 131:1 |
| motorShaftPulsesPerRev | pulses | 16 | >0 | 否 | 单边计数口径需确认 |
| outputPulsesPerRev | pulses | 2096 | >0 | 否 | 可由配置计算或覆盖 |
| currentGuardEnabled | bool | true | true/false | 否 | 可关闭 |
| currentFaultThresholdMa | mA | 2500 | >0 | 否 | 待实机确认 |
| rsenseMilliOhm | mΩ | 5 | >0 | 否 | 待实物确认 |
| ina240ZeroOffsetMv | mV | 待校准 | ADC 范围内 | 否 | 维护流程写入 |
| maxRunMs | ms | 待确认 | >0 | 否 | 安全兜底 |
| maxRunPulses | pulses | 待确认 | >0 | 否 | 安全兜底 |
| limitSwitchMode | enum | NormallyClosed | Disabled/NormallyClosed/NormallyOpen | 否 | 无硬件时设为 Disabled |
| limitDebounceMs | ms | 50 | 5-500 | 否 | 上/下限位稳定时间 |
| homingSpeedPercent | % | 30 | 1-100 | 否 | 远程归零低速运行 |
| homingMaxMs | ms | 待确认 | >0 | 否 | 归零最大时长 |
| homingMaxPulses | pulses | 待确认 | >0 | 否 | 归零最大脉冲 |
| faultEmergencyOutputMode | enum | Coast | Coast/Brake | 否 | 故障停机默认倾向滑行，需实测确认 |

## Esp32FarmFeeder 配置草案

| 配置项 | 单位 | 默认值 | 范围 | 运行中可改 | 备注 |
| --- | --- | --- | --- | --- | --- |
| motorSpeedPercent | % | 100 | 1-100 | 否 | 待实机确认 |
| softStartMs | ms | 1000 | 0-10000 | 否 | 三路共用或每路独立待确认 |
| softStopMs | ms | 500 | 0-10000 | 否 | 三路共用或每路独立待确认 |
| feeder1TargetGrams | g | 待确认 | >0 | 否 | 建议主配置 |
| feeder2TargetGrams | g | 待确认 | >0 | 否 | 建议主配置 |
| feeder3TargetGrams | g | 待确认 | >0 | 否 | 建议主配置 |
| feeder1GramsPerRevX100 | 0.01g/圈 | 7000 | >0 | 否 | 默认约 70g/圈 |
| feeder2GramsPerRevX100 | 0.01g/圈 | 7000 | >0 | 否 | 每路应可标定 |
| feeder3GramsPerRevX100 | 0.01g/圈 | 7000 | >0 | 否 | 每路应可标定 |
| gearRatioX100 | 0.01 | 27000 | >0 | 否 | 270:1 |
| motorShaftPulsesPerRev | pulses | 16 | >0 | 否 | 单边计数口径需确认 |
| outputPulsesPerRev | pulses | 4320 | >0 | 否 | 可由配置计算或覆盖 |
| startAllIntervalMs | ms | 1000 | >=0 | 否 | 顺序启动间隔 |
| stopAllIntervalMs | ms | 200 | >=0 | 否 | 是否需要待确认 |
| maxRunMs | ms | 300000 | >0 | 否 | 默认 5 分钟 |
| maxRunPulses | pulses | 432000 | >0 | 否 | 默认 100 圈 |
| dailyScheduleEnabled | bool | true | true/false | 否 | 每天定时投喂 |
| dailyScheduleTimeMinutes | min/day | 待确认 | 0-1439 | 否 | 每日执行时间 |
| skipToday | bool | false | true/false | 可改 | 只影响当天，日期切换后自动清除 |
| longTermRecordRetentionDays | days | 待确认 | >0 | 否 | 多年原始记录目标，存储介质待定 |

当前硬件没有电流检测。未来如增加，每电机一个 INA240 芯片，对应配置再加入每路电流阈值和校准参数。

## 待确认

- 电机轴每圈脉冲数的计数口径是单边、双边还是四倍频。
- `outputPulsesPerRev` 是否总是由参数计算，还是允许手动覆盖。
- 喂食器目标配置是否最终以克数为主。
- 三路喂食器是否需要每路独立速度、软启动、软停止。
- 日期来源失败时是否允许喂食和记录历史。
- 长期原始记录使用的存储介质和容量策略。
