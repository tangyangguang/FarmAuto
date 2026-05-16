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
| DoorMotionJournal | 运动意图记录 | 128B | 8 | 1024B | 运行开始写入，用于运行中断电恢复 |
| DoorMotionCheckpoint | 运行中位置检查点 | 128B | 32 | 4096B | 低频保存位置，不保存每个脉冲 |
| DoorCalibration | 校准参数 | 256B | 4 | 1024B | 行程、编码器、零点等 |
| RecordIndex | flash 长期记录索引摘要 | 512B | 4 | 2048B | 不保存完整多年日志 |
| SystemMeta | 存储元信息 | 128B | 4 | 512B | layout version、格式化标记 |

总计约 11776B，AT24C128 剩余约 4608B，可作为扩展余量或增加高频状态槽位。首版不为“未来可能”的自动策略预留独立记录区，后续真实需要时再通过 layout version 增加。

DoorMotionCheckpoint 是自动门首版为了“运行中断电后尽量远程恢复”增加的高价值小状态。它不保存原始历史，不保存每个编码器脉冲，只保存低频位置检查点。

## Esp32FarmFeeder 建议记录区

| recordType | 用途 | slotSize | slotCount | 总占用 | 备注 |
| --- | --- | ---: | ---: | ---: | --- |
| FeederRecoveryPolicy | 恢复策略快照 | 512B | 4 | 2048B | 与 Esp32Base App Config 关键参数一致性校验 |
| FeederToday | 今日计数和执行状态 | 256B | 16 | 4096B | 高频度高于普通配置 |
| FeederSchedule | 多个每日计划 | 512B | 4 | 2048B | 每槽保存计划表快照：最多 6 条计划 |
| FeederChannelTarget | 手动下料默认目标 | 256B | 4 | 1024B | 每路默认 targetMode、targetGramsX100、targetRevolutionsX100、targetPulses |
| FeederBucketState | 料桶估算余量 | 256B | 4 | 1024B | 每路当前估算余量、补料时间、欠料标记 |
| FeederCalibration | 三路标定 | 256B | 4 | 1024B | 脉冲/圈数/克数换算 |
| RecordIndex | flash 长期记录索引摘要 | 512B | 4 | 2048B | 不保存完整多年日志 |
| SystemMeta | 存储元信息 | 128B | 4 | 512B | layout version、格式化标记 |

总计约 13824B，AT24C128 剩余约 2560B，可作为扩展余量或增加 `FeederToday` 槽位。

`FeederSchedule` 首版冻结 `maxPlans=6`。按 512B 槽位估算，扣除 record header、schema、全局计划状态和 CRC 后，每条计划仍可保存时间、启用状态、参与通道、今日状态以及 3 路目标。6 条计划覆盖多数一天多次投喂场景，并保留后续字段扩展余量；如果未来需要超过 6 条，应提升 schema/layout version 并重新预算 slotSize。

投喂目标分两类保存：

- 计划内目标：随每条计划保存在 `FeederSchedule` payload 中。
- 手动下料默认目标：保存在 `FeederChannelTarget`，供首页/单路手动下料使用。
- 料桶当前估算余量：保存在 `FeederBucketState`，属于业务运行状态，不与通道基础信息/标定参数混写。

`FeederToday` payload 至少包含：

- `dateKey` 和今日 schema version。
- `planAttemptedBitmap`、`planExecutedBitmap`、`planMissedBitmap`、`planSkipTodayBitmap`，每个 bitmap 覆盖 `maxPlans=6`。
- `perChannelTodayPulses[3]`。
- `perChannelTodayGramsX100[3]`。
- 最近一次未完成投喂 commandId、source、channelMask 和可靠计数摘要。

## 已冻结与源码前复核项

已冻结：

- 首版实测目标为 AT24C128。
- sequence 使用 `uint32_t`，按半区间回绕比较选择最新有效槽。
- record header 使用固定字段显式序列化，禁止直接写 C++ 结构体内存。
- CRC 使用 CRC-32/ISO-HDLC。
- 高频记录必须按 `writesPerDay` 估算 slotCount。

源码前仍需复核：

- record header 最终字节数和页内对齐，确保 header 不跨 EEPROM 页。
- 每个 recordType 的最终 slotSize 与 payload 字段预算。
- 自动门和喂食器的实际 `writesPerDay` 是否超过本文估算。
- ESP32 flash 文件系统分区容量和长期记录轮转策略。
- 是否需要在 AT24C 中保存 flash 长期记录索引或摘要。

## 自动门检查点寿命预算

推荐默认策略：

- 运动开始时写 `DoorMotionJournal`。
- 运动过程中写 `DoorMotionCheckpoint`。
- 检查点默认最小间隔 `checkpointMinIntervalMs = 2000ms`。
- 检查点默认最小位移 `checkpointMinTravelPercent = 5%`。
- 两个条件取更慢者，也就是至少间隔 2 秒，并且位置变化有意义时才保存。
- 运动结束、故障停机或用户停止时写最终 `DoorState`，并写长期业务记录到 ESP32 flash。

默认估算：

| 场景 | 估算 |
| --- | --- |
| 单次开门或关门 20 秒 | 约 10 个检查点 |
| 每天 10 次运动 | 约 100 个检查点/天 |
| `DoorMotionCheckpoint` 32 个槽位 | 约 3.2 次/槽/天 |
| 按 100k 次保守写寿命估算 | payload 约 85 年量级；header 因双阶段提交按约 43 年量级估算 |

EEPROM 寿命应按页写入而不只是字节写入理解。AT24C128 常见页大小为 64B，一次页写会消耗该页内相关单元的写入寿命。首版要求：

- record header 不跨 EEPROM 页。
- 高频记录的 slot 起始地址按页边界规划，避免一个 header 或 payload 横跨多个页导致写放大。
- 寿命估算表同时看“每槽写入次数”和“热点页写入次数”。
- 双阶段提交会让 header 所在页每条记录写两次；高频 recordType 必须通过 `slotCount` 和保存频率覆盖该写放大。
- 不同厂商 AT24C128 endurance 可能不同；源码前按实际芯片 datasheet 和实测写周期复核。

该估算只用于说明默认策略足够保守。源码前应按实测开关门时长、每天动作次数和实际芯片 endurance 重算。

如果未来出现高频调试、频繁手动运行或异常反复运行：

- 手动运行默认不按普通运行频率写检查点，只在动作结束写状态。
- 连续手动运行应合并为维护事件摘要。
- 页面应显示 AT24C 写入估算和寿命告警。
- 如果 `DoorMotionCheckpoint` 的 estimatedWritesPerSlot 增长过快，优先增大保存间隔或降低检查点写入，而不是取消双阶段提交。

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
