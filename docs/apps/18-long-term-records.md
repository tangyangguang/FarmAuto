# 长期原始记录需求草案

## 目标

Esp32FarmDoor 和 Esp32FarmFeeder 都需要尽量完整、尽量长期保存原始运行记录，目标按多年数据考虑。

这些记录用于远程诊断、故障回溯、投喂统计、开关门行为分析和维护判断。该需求不应受旧方案容量限制影响，应按当前最佳实践重新设计。

具体事件类型和字段语义见 `docs/apps/22-record-event-schema.md`。本文只定义长期记录的范围、介质和存储策略。

关键数据记录原则：

- 尽量全面记录有价值数据。
- 优先保存原始值，显示和统计时再换算。
- 不过度设计，不记录无意义高频噪声。
- 高频传感器数据应先在 RAM 中聚合、降采样或记录摘要。
- 每类记录都应能解释远程诊断中的一个真实问题。

## 记录范围

Esp32FarmDoor 建议记录：

- 开门请求、开始、到位、停止。
- 关门请求、开始、到位、停止。
- 用户停止和故障停止。
- 开门/上限位触发。
- 可选关门/下限位触发。
- 远程端点校准和其他校准。
- 电流保护 warning/fault。
- 编码器无脉冲、最大时间、最大脉冲等保护。
- 配置变化摘要。
- 重启、断电恢复和位置恢复结果。

Esp32FarmFeeder 建议记录：

- 每日定时触发。
- 跳过今日和取消跳过今日。
- 单路启动、完成、停止、故障。
- 启动全部、各路实际启动时间、完成情况。
- 批量启动或定时触发的部分成功结果，包括 successMask、busyMask、faultMask、skippedMask。
- 投喂运行中断电后的中断结果，包括已可靠记录的实际脉冲、目标脉冲、是否阻止自动续喂、是否标记今日计划已尝试。
- 单路故障但其他路继续运行。
- 标定、清空今日计数、配置变化摘要。
- 日期切换、今日累计归档。
- 重启和计划恢复结果。

## 存储介质原则

AT24C128 不适合作为多年原始记录主存储。它容量小，更适合保存：

- 配置。
- 校准参数。
- 关键状态。
- 今日计数。
- 定时计划。
- 长期记录索引或摘要。

多年原始记录应单独评估存储介质：

| 介质 | 优点 | 风险 |
| --- | --- | --- |
| ESP32 flash 文件系统 | 不增加硬件，容量较大 | 需要确认 Esp32Base 文件系统、磨损均衡和导出能力 |
| 外部 FRAM | 高耐久，适合频繁小写入 | 容量和成本限制，不适合海量多年明细 |
| SD 卡 | 容量大，适合多年明细 | 硬件复杂度、文件系统一致性、卡质量和现场可靠性 |
| 外部 SPI flash | 容量较大，可控性强 | 需要额外硬件和文件/日志层设计 |

首版设计方向：

- AT24C 保存关键状态和索引。
- 多年原始记录优先使用 ESP32 flash 文件系统。
- 如果写入频率很高且记录量不大，再评估 FRAM。
- 如果 ESP32 flash 容量无法满足多年记录，再评估 SD 卡或外部 SPI flash。

## AT24C 与 ESP32 flash 职责划分

AT24C128 和 ESP32 flash 的寿命不能只看单点写入次数，还要看容量、写入模式和磨损均衡范围。

典型情况：

- AT24C128 EEPROM 常见规格可达约 100k 到 1M 次写入周期，具体以实际芯片数据手册为准。
- ESP32 外部 flash 常见擦写寿命通常按扇区约 100k 次量级估算，具体以模组 flash 芯片为准。
- LittleFS 会在文件系统层做面向 flash 的磨损均衡和掉电恢复，但应用仍不能高频小写。
- AT24C128 容量只有 16KB，适合小而关键的数据；ESP32 flash 容量更大，适合追加式长期记录。

合理划分：

| 数据类型 | 推荐介质 | 原因 |
| --- | --- | --- |
| 普通系统参数配置 | Esp32Base App Config | 复用基础库内置配置页、校验和低频持久化 |
| 恢复策略快照 | AT24C128 | 小、关键、低频写，外置芯片可更换，用于断电恢复交叉校验 |
| 校准参数 | AT24C128 | 小、关键、低频写，必须可靠恢复 |
| 当前位置/今日计数/跳过今日 | AT24C128 | 小型状态，重启后必须可信 |
| 长期原始事件记录 | ESP32 flash 文件系统 | 数据量大，适合追加和分段轮转 |
| 长期记录索引/摘要 | AT24C128 或 flash | 小索引可放 AT24C；完整记录不放 AT24C |
| 高频采样曲线 | RAM ring buffer，必要时降采样后写 flash | 不应每个采样点持久化 |

设计目标不是让某个介质尽快损坏，而是让不可避免的高价值小写入优先落在更容易更换的 AT24C128，并通过磨损均衡延长寿命；ESP32 flash 只承担大容量、低频批量、追加式写入，避免频繁覆盖同一位置。

如果必须在“外置可更换 AT24C”和“板载 flash”之间承担更多写入压力，优先让 AT24C 承担关键小状态的重复写入，前提是：

- 使用 wear-levelled record ring。
- 按 writesPerDay 分配足够 slotCount。
- 写前比较，内容不变不写。
- 不把大容量业务记录塞进 AT24C。

ESP32 flash 的保护策略：

- 使用追加式 segment。
- 批量 flush 普通记录。
- 关键事件才立即 flush。
- 按天分段，单日超过大小上限时再切分。
- 达到容量上限后告警并覆盖最旧 segment，不停止设备运行。
- 不在固定文件头频繁更新计数；索引可以低频重建或分散保存。

这个划分对应两类成熟实践：

- EEPROM 小记录：使用 circular buffer / rolling slot / sequence / CRC，把重复写入分散到多个 EEPROM 槽位，并能在掉电后扫描恢复最新有效记录。
- Flash 文件系统：使用 LittleFS 这类面向 flash 的文件系统，依靠动态块磨损均衡、copy-on-write 和掉电恢复机制，再由应用控制追加、批量 flush 和 segment 轮转。

## ESP32 flash 文件系统方案

首版优先基于 Esp32Base 的 `Esp32BaseFs` 实现长期原始记录。Esp32Base 当前默认文件系统为 LittleFS，并已向业务暴露二进制写入、追加、按偏移读取、按偏移覆盖、目录遍历和容量查询 API。

应用不直接 include `LittleFS.h`，只通过 Esp32Base 文件 API 访问：

```text
Esp32BaseFs
  appendBytes()
  readBytesAt()
  writeBytesAt()
  fileSize()
  listDir()
  totalBytes()
  usedBytes()
  freeBytes()
```

推荐目录：

```text
/records/
  door/
  feeder/
  index/
```

推荐文件策略：

- 使用二进制定长记录或小型 TLV 记录，不使用自由格式文本日志作为主记录。
- 按天分文件，并在单日文件超过固定大小时继续切分 segment。
- 每条记录带 magic、schema version、payload length、CRC。
- 写入采用追加为主，避免频繁改写文件中部。
- 每次写入可以先进入 RAM 缓冲，按条数、时间或关键事件触发 flush。
- 关键事件可以立即 flush，普通 INFO 记录批量 flush。
- 文件数量和容量达到阈值时远程告警。
- 容量达到上限时，推荐覆盖最旧 segment，同时保留告警和维护事件。

不建议：

- 把多年原始记录写入 Esp32Base 默认 `/logs/eb_app.log`。
- 把结构化原始记录混在普通文本日志中。
- 每次脉冲、每次 ADC 采样都写 flash。
- 在 HTTP handler 中执行长时间导出或全量扫描。

## 记录文件模型

推荐 segment header：

```text
RecordSegmentHeader
  magic
  schemaVersion
  recordSize
  createdUnixTime
  createdUptimeMs
  segmentSequence
  headerCrc
```

推荐 record header：

```text
RecordEvent
  timestamp
  deviceUptimeMs
  eventType
  source
  result
  value1
  value2
  flags
  crc
```

读取与后续导出：

- Web/API 必须分页读取，不一次性读取多年记录。
- 首版必须支持网页分页查看和筛选。
- 导出不是首版必须项；后续如实现，优先支持按日期范围、eventType 和 offset 分页。
- JSON 适合远程查看；CSV 适合下载分析；二进制适合完整备份，均作为后续增强。
- 文件损坏时只丢弃损坏 segment 或损坏记录，不影响其他 segment。

推荐默认值：

| 项目 | 推荐值 | 原因 |
| --- | --- | --- |
| 文件系统分区容量 | 1MB 起步，容量允许时 2MB | 多年记录需要空间，且要给 LittleFS 留余量 |
| 分段方式 | 按天 + 单日超大小切分 | 方便远程按日期查询，避免单文件过大 |
| 容量满策略 | 覆盖最旧 segment 并告警 | 无人值守设备应继续记录最近数据 |
| 查看和导出 | 首版网页分页查看；JSON Lines / CSV 导出后续增强 | 网页查看是基本能力，导出不是首版必须项 |
| 普通 flush | 条数或时间批量 | 减少 flash 写放大 |
| 关键事件 flush | 立即 flush | 降低故障前后关键记录丢失风险 |

## 磨损均衡要求

只要是持久化存储，都必须说明磨损均衡策略：

- AT24C：使用 wear-levelled record ring。
- ESP32 flash 文件系统：优先使用 LittleFS；LittleFS 具备面向 flash 的磨损均衡和掉电恢复设计，但应用仍必须控制写入频率、批量写入和容量轮转。
- FRAM：耐久度高，但仍应避免无意义高频写入。
- SD 卡：依赖卡内部控制器磨损均衡，但应用仍应批量写入、避免频繁 flush。

## 损坏检测与告警

系统应尽早发现存储异常，并通过 Web 状态、业务记录、系统日志和维护页面给出告警。

AT24C128 检测项：

- I2C 设备在线检测失败。
- 写入后 compare 失败。
- `Writing -> Valid` 提交流程失败。
- header CRC 或 payload CRC 错误。
- 无有效记录。
- sequence 不连续或回绕异常。
- 同一 recordType 的无效槽位数量持续增加。
- `estimatedWritesPerSlot` 接近保守寿命阈值。

ESP32 flash 文件系统检测项：

- LittleFS 挂载失败。
- `appendBytes()` / `writeBytesAt()` / `readBytesAt()` 返回失败。
- segment header CRC 或 record CRC 错误。
- 文件大小异常、segment 缺失或 sequence 不连续。
- 剩余容量低于阈值。
- 轮转失败或无法创建新 segment。
- 分页读取或后续导出时发现损坏记录。

告警建议：

- `StorageWarning`：容量低、少量记录 CRC 错误、部分旧 segment 损坏。
- `StorageFault`：关键恢复状态无法读取、AT24C 离线、flash 挂载失败、无法写入新关键状态。
- `StorageReadOnly`：仍可读取旧记录，但新记录无法可靠写入。
- `MaintenanceRequired`：需要现场更换 AT24C、处理记录、重新格式化或检查硬件。

远程状态应至少展示：

- AT24C 在线状态。
- AT24C 最近写入结果。
- AT24C 各 recordType 有效槽数量。
- flash FS ready 状态。
- flash 总容量、已用容量、剩余容量。
- 长期记录最早/最新时间。
- 最近一次记录写入错误。
- 是否建议维护；如果后续实现导出，再提示导出。

原则：

- 原始值优先，例如脉冲数、毫安、克数估算前的原始计数。
- 显示层再做换算。
- 记录首版必须可分页读取；导出作为后续增强。
- 记录文件或块必须带版本、CRC 或校验。
- 容量接近上限时必须有远程告警。

## Esp32Base 能力评估

当前 Esp32Base 已具备首版需要的基础文件 API：

- LittleFS 挂载。
- 二进制追加。
- 按偏移读取。
- 按偏移覆盖。
- 目录遍历。
- 文件大小查询。
- 容量查询。

当前不要求修改 Esp32Base。进入实现前仍需确认或实测：

- 目标分区表中 LittleFS 分区容量。
- Web/API 是否方便做记录分页读取。
- 如果后续实现导出，再评估长时间导出时 Watchdog 和 HTTP handler 的处理方式。
- 如果后续实现导出，再评估是否需要 Esp32Base 提供更通用的文件流式下载 API。

如后续确认 Esp32Base 能力不足，应使用以下提示词到 Esp32Base 项目处理，不在 FarmAuto 内打补丁：

```text
背景：
FarmAuto 需要在 ESP32 flash 文件系统上保存多年原始运行记录。记录采用结构化二进制 segment，首版要求网页分页读取、容量查询、容量告警，并且不能阻塞 Web、Watchdog 和设备主循环。导出是后续增强项。

当前已知能力：
Esp32Base 已提供 Esp32BaseFs，包括 appendBytes、readBytesAt、writeBytesAt、fileSize、listDir、totalBytes、usedBytes、freeBytes。默认文件系统为 LittleFS。

请评估并完善：
1. 是否需要提供更方便的分页读取辅助 API，支持按 offset/length 分块读取。
2. 如果后续实现导出，是否需要提供通用文件流式下载 API 和 Watchdog 友好辅助。
3. 是否需要文档明确 LittleFS 分区容量规划、磨损均衡边界和推荐写入模式。
4. 是否需要提供文件系统诊断 API：分区大小、剩余容量、文件数量、最大文件、最近错误。

要求：
- 不绑定 FarmAuto 业务字段。
- 不引入大型 Web 框架。
- 保持 profile 可裁剪。
- 不影响现有 FileLog。
```

## 已确认与待确认

- 首版使用 ESP32 flash 文件系统，优先 LittleFS。
- 分区容量 1MB 起步；如果 flash 容量允许，优先 2MB。
- 首版支持网页分页查看；JSON Lines / CSV 导出后续增强。
- 记录容量达到上限时告警并覆盖最旧记录，不停止设备运行。
- 按天分段，单日超过大小上限时再切分。

源码前仍需确认：

- 实际分区表和可用 flash 容量。
- 是否需要整理 Esp32Base 分页读取能力提示词；文件流式下载提示词等后续导出需求明确后再处理。
