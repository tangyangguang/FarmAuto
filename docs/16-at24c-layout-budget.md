# Esp32At24cRecordStore 容量预算草案

## 目标

本文用于进入实现前估算 AT24C128 容量是否足够，并约束 record layout。

AT24C128 容量为 16KB。首版应避免复杂文件系统，采用固定记录区和多槽 latest record。

## 布局原则

- 每类记录使用固定区域。
- 每个区域包含多个固定槽位。
- 每个槽位包含 record header 和 payload。
- 读取时选择 sequence 最大且 CRC 有效的槽位。
- 首版建议只使用 `Empty / Writing / Valid`，旧记录自然过期，不主动写 `Retired`。

## 建议记录区

| recordType | 用途 | slotSize | slotCount | 总占用 | 备注 |
| --- | --- | --- | --- | --- | --- |
| DoorConfig | 自动门配置 | 256B | 4 | 1024B | 仅应用使用 |
| DoorState | 自动门运行状态 | 128B | 8 | 1024B | 位置/故障 |
| DoorCalibration | 自动门校准 | 256B | 4 | 1024B | INA240 零点等 |
| FeederConfig | 喂食器配置 | 512B | 4 | 2048B | 三路配置 |
| FeederToday | 喂食器今日计数 | 256B | 8 | 2048B | 三路累计 |
| FeederHistory | 喂食器 7 天历史 | 512B | 4 | 2048B | 可一次保存 7 天 |
| SystemMeta | 存储元信息 | 128B | 4 | 512B | layout version |

总计约 9728B，AT24C128 剩余约 6656B，可作为扩展余量。

上述表是容量预算草案，不代表公共库内置这些 recordType。公共库只提供记录机制，recordType 由上层项目定义。

## 待确认

- record header 最终大小。
- 是否为 FarmDoor 和 FarmFeeder 分别使用独立芯片或独立固件中的同类布局。
- 是否需要设备事件日志进入 AT24C；首版建议不要，优先用 Esp32Base 文件日志。
- FeederHistory 是保存 7 天为一个 payload，还是每天一个 recordType/slot。
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
