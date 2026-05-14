# AT24C128 容量预算草案

## 目标

本文用于进入实现前估算每个独立设备上的 AT24C128 容量是否足够，并约束 record layout。

重要边界：

- `Esp32FarmDoor` 和 `Esp32FarmFeeder` 是两个独立应用、两套独立 PCB、两个独立固件。
- 每个设备各自使用自己的 ESP32 和自己的 AT24C128。
- 本文不能把两个应用的数据合并成同一颗 AT24C128 的布局。
- 下列表格是应用层容量预算，不代表 `Esp32At24cRecordStore` 公共库内置这些 recordType。

AT24C128 容量为 16KB。首版应避免复杂文件系统，采用 wear-levelled record ring 保存关键状态、校准快照、计划执行状态、今日计数和必要索引。

普通系统参数配置优先使用 Esp32Base App Config，不把 AT24C128 当作普通参数配置页面的唯一存储。需要断电恢复强一致的小状态，才进入 AT24C128 的记录环。

AT24C128 不适合作为多年原始事件记录的主存储。长期原始记录首版优先考虑 ESP32 flash 文件系统。

## 布局原则

- 每个独立固件拥有自己的 `StoreLayout`。
- 每类记录使用独立磨损均衡记录环。
- 每个记录环包含多个固定槽位。
- 每个槽位包含 record header 和 payload。
- 读取时选择 sequence 最大且 CRC 有效的槽位。
- 首版建议只使用 `Empty / Writing / Valid`，旧记录自然过期，不主动写 `Retired`。
- 高频记录必须按 writesPerDay 估算 slotCount。
- 字节序已确认固定为 little-endian；源码实现必须显式编码字段，不能直接写入未定义布局的结构体内存。

## Esp32FarmDoor 建议记录区

| recordType | 用途 | slotSize | slotCount | 总占用 | 备注 |
| --- | --- | ---: | ---: | ---: | --- |
| DoorRecoveryPolicy | 恢复策略快照 | 256B | 4 | 1024B | 与 Esp32Base App Config 关键参数一致性校验 |
| DoorState | 关键运行状态 | 128B | 16 | 2048B | 位置、目标、状态、故障摘要 |
| DoorCalibration | 校准参数 | 256B | 4 | 1024B | 行程、编码器、零点等 |
| DoorScheduleOrPolicy | 自动策略/保留 | 256B | 4 | 1024B | 后续若无自动策略可作为扩展区 |
| RecordIndex | flash 长期记录索引摘要 | 512B | 4 | 2048B | 不保存完整多年日志 |
| SystemMeta | 存储元信息 | 128B | 4 | 512B | layout version、格式化标记 |

总计约 7680B，AT24C128 剩余约 8704B，可作为扩展余量或增加高频状态槽位。

## Esp32FarmFeeder 建议记录区

| recordType | 用途 | slotSize | slotCount | 总占用 | 备注 |
| --- | --- | ---: | ---: | ---: | --- |
| FeederRecoveryPolicy | 恢复策略快照 | 512B | 4 | 2048B | 与 Esp32Base App Config 关键参数一致性校验 |
| FeederToday | 今日计数和执行状态 | 256B | 16 | 4096B | 高频度高于普通配置 |
| FeederSchedule | 每日计划 | 256B | 4 | 1024B | 每日执行、跳过今日 |
| FeederCalibration | 三路标定 | 256B | 4 | 1024B | 脉冲/圈数/克数换算 |
| RecordIndex | flash 长期记录索引摘要 | 512B | 4 | 2048B | 不保存完整多年日志 |
| SystemMeta | 存储元信息 | 128B | 4 | 512B | layout version、格式化标记 |

总计约 10752B，AT24C128 剩余约 5632B，可作为扩展余量或增加 `FeederToday` 槽位。

## 待确认

- record header 最终大小。
- 每个应用最终是否都使用 AT24C128，或是否存在某个应用换用更大容量 EEPROM。
- ESP32 flash 文件系统分区容量和长期记录轮转策略。
- 是否需要在 AT24C 中保存 flash 长期记录索引或摘要。
- sequence 使用 32 位还是 64 位。
- sequence 回绕规则。
- 每个高频 recordType 的预计 writesPerDay。

## 断电安全写入策略

首版固定采用双阶段提交，不保留“一次写入后只靠 CRC 判断有效”的备选方案。

固定写入流程：

1. 写入 `Writing` 状态记录。
2. 回读并校验 header 和 payload CRC。
3. 构造 `flags=Valid` 的 header，并重新计算 `headerCrc`。
4. 只重写同槽 header，且 header 必须保证不跨 EEPROM 页。
5. 回读 header，确认 `flags=Valid` 且 header CRC 正确。
6. 读取时忽略 `Writing`、非 `Valid` 或 CRC 无效记录。

该流程与 `docs/libs/23-esp32-at24c-record-store.md` 保持一致。虽然第二次写 header 会增加少量 EEPROM 写入，但它能把“已写入但未确认提交”的记录与“已提交有效记录”明确区分，断电恢复语义更清楚，适合无人值守设备。

## 断电安全与寿命影响分析

双阶段提交不是为了在掉电瞬间临时抢写大量数据。ESP32 检测到掉电后是否还有足够时间写 EEPROM，取决于电源保持电容、供电路径、brownout 阈值和写周期，首版不能假设一定可靠。

双阶段提交真正解决的是：正常运行中某次 EEPROM 写入被断电打断时，重启后不会把半写入记录当成最新有效记录。

如果不使用双阶段提交：

- 可以只写一次 `Valid` 记录并依靠 CRC 判断完整性。
- 大多数半写入记录会被 CRC 拦住。
- 但无法明确区分“提交中断”和“已经提交但局部损坏”。
- header 先写成功、payload 半写失败、旧数据残留等情况会让恢复语义更难解释。
- 远程诊断时只能看到 CRC 失败，不能清楚知道是提交中断。

使用双阶段提交的寿命影响：

- payload 仍然每次只写入一次。
- header 所在字节每次提交会写两次：一次 `Writing`，一次 `Valid`。
- 因为每个 recordType 都按 slot 轮转，额外 header 写入会分散到多个槽位。
- 如果 header 为 32B、slot 为 128B，则一次提交约等价于 128B + 32B 的写入量，物理写入量增加约 25%。
- 如果 slot 更大，额外比例更低；如果 slot 很小，额外比例更高。
- 对单个槽位的 header 字节来说，每轮写入会多消耗一次写周期，因此高频记录必须增加 `slotCount` 或降低保存频率。

寿命控制原则：

- 恢复策略快照、校准、计划等低频数据使用双阶段提交，寿命影响可以接受。
- 运行状态、今日计数等较高频数据也使用双阶段提交，但必须做写前比较、节流保存和 slot 轮转。
- 不允许在主循环中无变化反复保存同一状态。
- 可按事件、固定最小间隔或关键数值变化阈值保存。
- 对真正高频的原始运行记录，不写 AT24C128，写 ESP32 flash 文件系统的长期记录区。

结论：

- 首版保留双阶段提交。
- 不为了少写一次 header 牺牲断电恢复可解释性。
- 寿命问题通过 wear-levelled record ring、写前比较、保存节流和合理 slotCount 解决。
- 如果未来实测证明某个高频 recordType 寿命预算不足，优先增加 slotCount、降低保存频率或迁移到更适合高频写入的介质，而不是取消提交语义。
