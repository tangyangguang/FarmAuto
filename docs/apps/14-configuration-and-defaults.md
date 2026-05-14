# 配置项、默认值与保存策略草案

## 目标

本文记录两个应用和公共库相关配置项的单位、范围、默认值、保存时机和待确认项。

默认值在实机测试前只能作为初始建议，不能视为最终量产参数。

## 通用规则

- 所有配置必须有单位。
- 所有配置必须有合法范围。
- Web 保存前必须校验。
- 应用系统参数优先注册到 Esp32Base App Config，由 Esp32Base 内置配置页面负责显示、校验、保存和只写变化字段。
- 业务系统不重复实现参数配置页和参数持久化；只实现运行控制页、状态页、维护动作和诊断记录。
- 运行状态只在停止、故障、端点校准、清计数、日期切换等关键点保存。
- 运行中是否允许修改配置必须逐项定义。
- AT24C128 主要保存关键可靠状态、校准快照、今日计数摘要和恢复所需记录，不作为普通系统参数配置页的唯一承载。
- Esp32Base App Config 的 groups/fields 容量需要在源码前按两个应用字段数设置构建宏；如果字段组织能力不足，先整理 Esp32Base 提示词，不在 FarmAuto 内另做配置系统。

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
| openLimitSwitchMode | enum | Disabled | Disabled/NormallyClosed/NormallyOpen | 否 | 第一版默认禁用，下一阶段优先启用开门/上限位 |
| closeLimitSwitchMode | enum | Disabled | Disabled/NormallyClosed/NormallyOpen | 否 | 关门/下限位，可选 |
| limitDebounceMs | ms | 50 | 5-500 | 否 | 限位稳定时间 |
| openLimitCalibrateSpeedPercent | % | 30 | 1-100 | 否 | 远程端点校准低速运行 |
| openLimitCalibrateMaxMs | ms | 待确认 | >0 | 否 | 端点校准最大时长 |
| openLimitCalibrateMaxPulses | pulses | 待确认 | >0 | 否 | 端点校准最大脉冲 |
| maxCloseUnwindPulses | pulses | 待确认 | >0 | 否 | 关门最大放绳脉冲，防止过放反卷 |
| faultEmergencyOutputMode | enum | Coast | Coast/Brake | 否 | 故障停机默认倾向滑行，需实测确认 |

## Esp32FarmFeeder 配置草案

| 配置项 | 单位 | 默认值 | 范围 | 运行中可改 | 备注 |
| --- | --- | --- | --- | --- | --- |
| motorSpeedPercent | % | 100 | 1-100 | 否 | 待实机确认 |
| softStartMs | ms | 1000 | 0-10000 | 否 | 三路共用或每路独立待确认 |
| softStopMs | ms | 500 | 0-10000 | 否 | 三路共用或每路独立待确认 |
| feeder1TargetMode | enum | Revolutions | Grams/Revolutions | 否 | 每路可独立选择 |
| feeder2TargetMode | enum | Revolutions | Grams/Revolutions | 否 | 每路可独立选择 |
| feeder3TargetMode | enum | Revolutions | Grams/Revolutions | 否 | 每路可独立选择 |
| feeder1TargetGrams | g | 待标定 | >0 | 否 | 克数模式使用 |
| feeder2TargetGrams | g | 待标定 | >0 | 否 | 克数模式使用 |
| feeder3TargetGrams | g | 待标定 | >0 | 否 | 克数模式使用 |
| feeder1TargetRevolutionsX100 | 0.01 圈 | 待确认 | >0 | 否 | 圈数模式使用 |
| feeder2TargetRevolutionsX100 | 0.01 圈 | 待确认 | >0 | 否 | 圈数模式使用 |
| feeder3TargetRevolutionsX100 | 0.01 圈 | 待确认 | >0 | 否 | 圈数模式使用 |
| feeder1GramsPerRevX100 | 0.01g/圈 | 7000 | >0 | 否 | 默认约 70g/圈 |
| feeder2GramsPerRevX100 | 0.01g/圈 | 7000 | >0 | 否 | 每路应可标定 |
| feeder3GramsPerRevX100 | 0.01g/圈 | 7000 | >0 | 否 | 每路应可标定 |
| feeder1BucketCapacityGrams | g | 待配置 | >0 | 可改 | 饲料桶容量 |
| feeder2BucketCapacityGrams | g | 待配置 | >0 | 可改 | 饲料桶容量 |
| feeder3BucketCapacityGrams | g | 待配置 | >0 | 可改 | 饲料桶容量 |
| feeder1BucketRemainGrams | g | 待设置 | >=0 | 可改 | 当前估算余量 |
| feeder2BucketRemainGrams | g | 待设置 | >=0 | 可改 | 当前估算余量 |
| feeder3BucketRemainGrams | g | 待设置 | >=0 | 可改 | 当前估算余量 |
| gearRatioX100 | 0.01 | 27000 | >0 | 否 | 270:1 |
| motorShaftPulsesPerRev | pulses | 16 | >0 | 否 | 单边计数口径需确认 |
| outputPulsesPerRev | pulses | 4320 | >0 | 否 | 可由配置计算或覆盖 |
| startAllIntervalMs | ms | 1000 | >=0 | 否 | 顺序启动间隔 |
| stopAllMode | enum | StopAllNow | StopAllNow/EmergencyStopAll | 否 | 普通停止同时请求各路软停止 |
| maxRunMs | ms | 300000 | >0 | 否 | 默认 5 分钟 |
| maxRunPulses | pulses | 432000 | >0 | 否 | 默认 100 圈 |
| dailyScheduleEnabled | bool | false | true/false | 否 | 未配置时间时不自动投喂 |
| dailyScheduleTimeMinutes | min/day | 未配置 | 0-1439 | 否 | 配置后才可启用每日计划 |
| skipToday | bool | false | true/false | 可改 | 只影响当天，日期切换后自动清除 |
| longTermRecordRetentionDays | days | 待确认 | >0 | 否 | 多年原始记录目标，存储介质待定 |

当前硬件没有电流检测。未来如增加，每电机一个 INA240A2 芯片，对应配置再加入每路电流阈值和校准参数。

## 待确认

- 电机轴每圈脉冲数的计数口径是单边、双边还是四倍频。
- `outputPulsesPerRev` 是否总是由参数计算，还是允许手动覆盖。
- 三路喂食器是否需要每路独立速度、软启动、软停止。
- 长期原始记录使用的存储介质和容量策略。
