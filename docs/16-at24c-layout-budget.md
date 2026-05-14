# Esp32At24cRecordStore 容量预算草案

## 目标

本文用于进入实现前估算 AT24C128 容量是否足够，并约束 record layout。

AT24C128 容量为 16KB。首版应避免复杂文件系统，采用 wear-levelled record ring 保存关键配置、状态、计划、今日计数和必要索引。

AT24C128 不适合作为多年原始事件记录的主存储。长期原始记录需要单独评估 ESP32 flash 文件系统、外部 FRAM、SD 卡或其他介质。

## 布局原则

- 每类记录使用独立磨损均衡记录环。
- 每个记录环包含多个固定槽位。
- 每个槽位包含 record header 和 payload。
- 读取时选择 sequence 最大且 CRC 有效的槽位。
- 首版建议只使用 `Empty / Writing / Valid`，旧记录自然过期，不主动写 `Retired`。
- 高频记录必须按 writesPerDay 估算 slotCount。

## 建议记录区

| recordType | 用途 | slotSize | slotCount | 总占用 | 备注 |
| --- | --- | --- | --- | --- | --- |
| DoorConfig | 自动门配置 | 256B | 4 | 1024B | 仅应用使用 |
| DoorState | 自动门运行状态 | 128B | 8 | 1024B | 位置/故障 |
| DoorCalibration | 自动门校准 | 256B | 4 | 1024B | INA240 零点等 |
| FeederConfig | 喂食器配置 | 512B | 4 | 2048B | 三路配置 |
| FeederToday | 喂食器今日计数 | 256B | 8 | 2048B | 三路累计 |
| FeederSchedule | 喂食器定时计划 | 256B | 4 | 1024B | 每日计划/跳过今日 |
| RecordIndex | 长期原始记录索引摘要 | 512B | 4 | 2048B | 不保存完整多年日志 |
| SystemMeta | 存储元信息 | 128B | 4 | 512B | layout version |

总计约 10752B，AT24C128 剩余约 5632B，可作为扩展余量。

上述表是容量预算草案，不代表公共库内置这些 recordType。公共库只提供记录机制，recordType 由上层项目定义。

## 待确认

- record header 最终大小。
- 是否为 FarmDoor 和 FarmFeeder 分别使用独立芯片或独立固件中的同类布局。
- 长期原始记录使用 ESP32 flash 文件系统、外部 FRAM、SD 卡还是其他介质。
- 是否需要在 AT24C 中保存长期记录索引或摘要。
- sequence 使用 32 位还是 64 位。
- sequence 回绕规则。
- 字节序固定为 little-endian 还是显式编码。

## 断电写入建议

首版写入流程：

1. 写入 `Writing` 状态记录。
2. 回读并校验 header 和 payload CRC。
3. 将同槽状态更新为 `Valid`，或用完整重写方式写入 Valid 记录。
4. 读取时忽略 `Writing` 或 CRC 无效记录。

需要进一步确认第 3 步是否会造成额外写入风险。如果可以通过一次完整记录写入并以 CRC 有效作为准入条件，首版可不单独写状态翻转，进一步减少 EEPROM 写次数。
