# Esp32At24cRecordStore 记录存储库方案

## 定位

Esp32At24cRecordStore 是 AT24C/24LC/24AA 系列 I2C EEPROM 上的可靠、磨损均衡记录存储库。它提供页写入、记录校验、版本、最新记录选择和按槽轮转写入能力。

它不绑定任何具体应用项目的业务字段。

## 支持范围

首版实测目标是 AT24C128。源码首版只承诺 AT24C128，并优先兼容不需要特殊寻址逻辑的 2 字节地址 AT24C/24LC/24AA 型号。

首版推荐支持：

- AT24C32 / AT24C64。
- AT24C128 / AT24C256 / AT24C512。

AT24C02 / AT24C04 / AT24C08 / AT24C16 等小容量型号一般不是本项目目标硬件。如果这些型号需要 I2C device address 位参与内部寻址，首版可以不支持。只有在不引入额外特殊代码、或后续确有实物需求时，才作为可选能力加入。

新增型号应优先通过配置预设支持，不为每个型号复制存储逻辑。

不同型号主要差异：

- 总容量。
- 页大小。
- 地址字节数。
- 写周期。
- 单次页写不能跨页。

## 芯片配置

建议概念模型：

```text
At24cChipConfig
  i2cAddress
  capacityBytes
  pageSizeBytes
  addressBytes
  writeCycleMs
  maxWriteChunkBytes
```

库可以提供常见型号预设，也允许应用显式传入配置。

这个设计参考成熟外部 EEPROM 库的做法：通过运行时配置描述容量、地址字节数和页大小，让上层把 EEPROM 看成连续地址空间，同时由库内部处理页写入限制。

首版只要求 `addressBytes=2` 的连续逻辑地址空间。所有访问都必须检查 `logicalAddress + length <= capacityBytes`。

如果未来确实需要 AT24C04/08/16 这类使用 I2C device address 位参与内部寻址的小容量型号，应新增明确的可选寻址策略，并把差异封装在 `At24cDevice` 内，不能泄漏到记录存储层。

常见 preset 建议：

| 型号 | capacityBytes | pageSizeBytes | addressBytes |
| --- | ---: | ---: | ---: |
| AT24C32 | 4096 | 32 | 2 |
| AT24C64 | 8192 | 32 | 2 |
| AT24C128 | 16384 | 64 | 2 |
| AT24C256 | 32768 | 64 | 2 |
| AT24C512 | 65536 | 128 | 2 |

不同厂商兼容型号可能存在页大小、写周期或地址脚解释差异。preset 是常见默认值，进入硬件实测前仍应核对具体芯片数据手册。

## 接口级设计

建议分为低层设备访问和高层记录存储两层。

低层设备访问：

```text
At24cDevice
  begin(wire, config)
  isOnline()
  read(address, buffer, length)
  write(address, data, length)
  compare(address, data, length)
```

高层记录存储：

```text
Esp32At24cRecordStore
  begin(device, layout)
  format(layoutVersion)
  readLatest(recordType, buffer, bufferLength)
  writeRecord(recordType, schemaVersion, payload, payloadLength)
  inspect(recordType)
```

`format()` 必须由上层项目显式调用，不能在读取失败时自动格式化。

## 布局设计

首版采用 wear-levelled record ring，不做复杂文件系统。

建议概念模型：

```text
RecordRegion
  recordType
  startAddress
  slotSize
  slotCount
  writeClass

StoreLayout
  magic
  layoutVersion
  regions[]
```

每个 `recordType` 对应一个磨损均衡记录环。记录环内包含多个固定大小槽位，写入时按 sequence 选择下一个槽位，读取时选择 sequence 最大且 CRC 有效的槽位。

这种设计独立于旧项目存储格式，采用小型 EEPROM 常见的 circular buffer / rolling slot 思路：把同一类数据的重复写入分散到多个物理槽位上，并用 sequence 和 CRC 在重启后恢复最新有效记录。

固定槽位记录环的好处：

- 实现简单。
- 断电恢复逻辑清晰。
- 不需要动态分配。
- 容量预算可提前计算。
- 天然提供 recordType 内的磨损均衡。

代价：

- 空间利用率不如文件系统。
- 每类记录需要预留最大 payload 大小。
- 不自动在不同 recordType 之间迁移容量。

这个取舍比动态文件系统和全局日志结构简单，也更适合 ESP32 小型无人值守设备远程维护。容量和磨损均衡能力由 `slotCount` 和写入频率共同决定。

该设计采用 EEPROM 小记录存储的经典实践：circular buffer / rolling slot / sequence / CRC。每次写入移动到下一个槽位，启动时扫描所有槽位并选择最新有效 sequence；CRC 和 `Writing -> Valid` 提交过程用于处理掉电和局部写入失败。

## 基础能力

EEPROM 访问：

- 初始化 I2C 设备。
- 检测设备是否在线。
- 按地址读取字节。
- 按地址写入字节。
- 跨页读取。
- 跨页写入。
- 页边界安全拆分。
- 写完成等待或 ACK polling。
- 写前比较，内容相同不写。

记录存储：

- record header。
- payload。
- CRC 校验。
- schema version。
- record type。
- sequence。
- 最新有效记录选择。

## 记录格式

建议记录头包含：

```text
magic
recordType
schemaVersion
sequence
payloadLength
payloadCrc
headerCrc
flags
```

首版记录状态 flag：

```text
Empty
Writing
Valid
```

提交流程：

1. 选择下一个槽位。
2. 构造 `flags=Writing` 的完整 header 和 payload，并计算对应的 `headerCrc` 与 `payloadCrc`。
3. 写入完整 header + payload。
4. 回读完整记录并校验 header CRC 与 payload CRC。
5. 构造 `flags=Valid` 的 header，并重新计算 `headerCrc`。
6. 只重写 header，且 header 必须位于同一页内。
7. 回读 header，确认 `flags=Valid` 且 header CRC 正确。
8. 旧记录保持不变，读取时由 sequence 决定最新记录。

如果第 2 步到第 7 步之间断电，读取时忽略未完成、非 `Valid` 或 CRC 无效的记录。

首版不主动写 `Retired`。旧记录自然保留，读取时只选择 sequence 最新且 CRC 有效的 `Valid` 记录。这样可以少一次 EEPROM 写入，也降低断电流程复杂度。

CRC 和字节序规则：

- 固定使用小端字节序。
- `payloadCrc` 覆盖 payload 全部字节。
- `headerCrc` 覆盖 record header 中除 `headerCrc` 自身以外的字段。
- CRC 算法首版固定为 CRC-32/ISO-HDLC；后续变更需要提升 layout version 或 schema version，并提供迁移/兼容读取方案。
- 所有 header 和 payload 字段必须字段级显式序列化，禁止直接把 C++ 结构体内存写入 EEPROM；结构体对齐、padding 和编译器差异不能进入持久化格式。
- header 必须保证不跨 EEPROM 页，避免提交 `Valid` 时出现跨页半写。

CRC32 实现归属：

- `Esp32At24cRecordStore` 首版提供一个小型 `Crc32IsoHdlc` helper。
- 应用长期记录服务可以复用该 helper，避免 AT24C 记录和 flash 记录各写一套 CRC32。
- helper 不依赖 EEPROM 设备类，也不依赖 Esp32Base。
- 如果以后 Esp32Base 已提供同等 CRC32 helper，再评估是否切换；首版不因此阻塞。

sequence 规则：

- `sequence` 使用 `uint32_t` 递增。
- 读取最新记录时使用半区间回绕比较，避免 sequence 回绕后误选旧记录。
- 格式化后首条记录 sequence 从 1 开始。

上层项目为不同数据使用不同 `recordType`，例如：

- 设备配置。
- 运行状态。
- 校准参数。
- 统计记录。
- 历史记录。

## 可靠性策略

首版采用简单可靠策略，不做复杂文件系统。

建议：

- 每类记录使用一个磨损均衡记录环。
- 每个记录环保留两个或多个槽位，高频写入记录必须分配更多槽位。
- 写入新槽位成功且 CRC 校验通过后，作为最新记录。
- 读取时选择 sequence 最大且 CRC 有效的记录。
- 内容未变化时不写入。
- 格式化必须由应用显式触发，库不能静默清空。
- 设备离线时返回 `DeviceOffline`，不能伪造默认成功。

## 磨损均衡策略

Esp32At24cRecordStore 首版必须具备明确的静态磨损均衡策略。

核心策略：

- 每个 `recordType` 使用独立记录环。
- 每次写入同一 `recordType` 时，不覆盖原槽位，而是写入下一个槽位。
- 下一个槽位由最新有效 `sequence` 和 `slotCount` 计算得到。
- 重启后通过扫描该记录环，选择最新有效 `sequence`。
- 写入前比较 payload，相同内容返回 `Unchanged`，不消耗 EEPROM 写周期。
- 高频记录通过更大的 `slotCount` 获得更高寿命。

寿命估算：

```text
expectedDays = endurancePerCell * slotCount / writesPerDay
```

其中 `endurancePerCell` 使用芯片数据手册的保守值。应用项目在分配 layout 时，应按每类记录的预计写入频率选择 `slotCount`，而不是所有记录平均分配。

`writeClass` 用于表达写入频率建议：

```text
Rare
Normal
Frequent
Hot
```

推荐含义：

- `Rare`：安装、标定或固件升级时才写。
- `Normal`：用户配置偶尔变化。
- `Frequent`：设备运行状态、计数、统计等会周期性更新。
- `Hot`：可能频繁变化的数据；如需要每天大量写入，应优先考虑 FRAM 或减少保存频率。

首版不做全局动态 wear leveling，也不做垃圾回收。原因是 AT24C 容量小、应用数据结构明确，静态记录环更容易验证、恢复和远程诊断。只有当真实设备出现多个高频 recordType、容量利用率不足或写入寿命预算不够时，才评估全局日志式布局。

诊断要求：

- `inspect(recordType)` 必须能看到每个槽位状态。
- `inspect(recordType)` 应给出 `nextSlotIndex` 和 `estimatedWritesPerSlot`。
- 上层应能远程查看 layout 是否给高频记录分配了足够槽位。
- `inspect(recordType)` 应提供足够数据支持磨损均衡和可靠性图表。
- `totalWrites` 不要求精确持久化计量，首版优先由最新有效 `sequence` 推算最低写入次数。
- `estimatedLifetimePercent` 是寿命估算值，不是精密磨损计量；断电、格式化、layout 变更或 sequence 回绕都可能影响精度。
- 如应用需要长期显示更准确的写入统计，应在应用层保存可选摘要，不能让记录库为了统计反复额外写 EEPROM。

高价值诊断图表：

- 每个 recordType 的写入次数。
- 每个 recordType 的预计寿命百分比。
- 每个槽位的有效/无效状态分布。
- 每个槽位的 sequence 分布。
- CRC 错误次数趋势。
- 写后校验失败次数趋势。
- `Unchanged` 跳过写入次数。
- 设备离线、ACK timeout、compare failed 等错误时间线。

这些图表由上层项目生成，公共库只提供 `inspect()`、结果码、事件和计数器，不生成 UI。

暂不做：

- 通用数据库。
- 动态文件系统。
- 任意长度日志。
- 全局动态 wear leveling。
- 垃圾回收。
- 复杂坏块管理。
- 既有私有格式迁移。

如果实测出现 EEPROM 页面损坏或频繁断电损坏，再评估是否增加坏块标记。坏块管理不应在首版预先复杂化。

## 与 I2C FRAM 的关系

I2C FRAM 和 AT24C EEPROM 在上层看起来都像 I2C 随机访问非易失存储，但底层特性不同：

- FRAM 通常没有 EEPROM 写周期等待。
- FRAM 写入耐久度远高于 EEPROM。
- FRAM 更适合频繁状态保存、计数、事件记录和掉电前快速写入。
- FRAM 容量、价格、供货和封装选择与 AT24C EEPROM 不同。

本库首版不把 FRAM 纳入 `Esp32At24cRecordStore` 的承诺范围。原因是库名和首版目标都明确面向 AT24C 系列 EEPROM；把 FRAM 强行塞入会让页写、写周期、ACK polling 等概念变得不清晰。

为了以后迁移到 FRAM 不返工，记录格式、CRC、sequence、固定槽位等记录层规则应尽量保持介质无关。但首版不抽正式 `RandomAccessNvMemory` 接口，也不把 FRAM 后端写进本库。

如果未来真实使用 FRAM，推荐新增独立库，例如 `Esp32I2cFramRecordStore`。当 AT24C 和 FRAM 两个库的记录层代码出现明确重复，并且两个库都经过实机验证后，再考虑抽取 `Esp32RecordStoreCore`。当前阶段不提前改名，也不承诺 FRAM 行为。

成熟常见的 I2C FRAM 可优先关注：

- Fujitsu / Kioxia MB85RC 系列，例如 MB85RC256V。
- Infineon / Cypress FM24V 系列，例如 FM24V02A。

两类都属于成熟 I2C FRAM 路线。若容量 32KB 够用，256-Kbit 级别通常是资料、模块和示例最容易找到的起点。

## 结果与错误

建议操作结果：

```text
Ok
NotInitialized
DeviceOffline
InvalidConfig
LayoutMismatch
OutOfRange
PayloadTooLarge
ReadFailed
WriteFailed
CrcMismatch
HeaderCrcMismatch
NoValidRecord
Unchanged
AckTimeout
CompareFailed
FormatRequired
```

`Unchanged` 表示写前比较发现 payload 未变化，库没有执行 EEPROM 写入。

`inspect(recordType)` 建议返回：

```text
RecordInspect
  deviceOnline
  regionFound
  regionUsedBytes
  validSlotCount
  latestSequence
  latestSchemaVersion
  nextSlotIndex
  totalWrites
  unchangedSkips
  crcErrorCount
  verifyFailedCount
  ackTimeoutCount
  estimatedWritesPerSlot
  estimatedLifetimePercent
  lastError
  slots[]

RecordSlotInspect
  slotIndex
  address
  flags
  sequence
  schemaVersion
  payloadLength
  headerCrcOk
  payloadCrcOk
```

诊断 API 使用原则：

- 库层 `inspect(recordType)` 可以返回完整结构，方便测试和深度诊断。
- 应用 `/api/app/diagnostics` 默认只返回 region 摘要，不默认展开 `slots[]`。
- 只有显式请求某个 recordType 的 slot 明细时，才返回 `slots[]`，避免 ESP32 JSON 响应过大。

## 型号支持原则

首版必须支持 AT24C128。

其他型号通过 `At24cChipConfig` 支持，不在代码里散落型号判断。型号预设只做配置表：

```text
At24cPreset
  AT24C32
  AT24C64
  AT24C128
  AT24C256
  AT24C512
```

如果未来支持小容量特殊寻址型号，应把差异封装在 `At24cDevice` 内，不能泄漏到 `Esp32At24cRecordStore`。

## 首版边界

首版实现：

- AT24C128 配置。
- 可配置容量、页大小、地址字节数。
- 跨页读写。
- 写前比较。
- 磨损均衡记录环。
- 多槽最新记录选择。
- CRC 校验。

首版不实现：

- 通用文件系统。
- 动态记录分配。
- 复杂坏块管理。
- 加密。
- 压缩。
- 老格式迁移。

## 独立库准备

未来如果拆成独立库，应至少包含：

- `library.json`。
- `README.md`。
- `examples/basic_read_write`。
- `examples/record_store`。
- `examples/write_if_changed`。
- 型号预设表。
- 容量和页边界测试说明。

公共 API 应避免暴露具体项目的 record type，只提供整数或枚举式 record type，由上层项目定义含义。

## 设计参考

- SparkFun External EEPROM Arduino Library 使用运行时 memory type 或显式 memory size/address bytes/page size 配置，并由库内部处理页写限制。
- PlatformIO `library.json` 用于声明独立库元数据、依赖、平台、框架和导出规则；本库未来独立时应按该格式准备。
- Microchip / Atmel AVR101 High Endurance EEPROM Storage 使用 circular buffer 提升 EEPROM 高频参数写入寿命；本库的 record ring 采用同类思路，但使用 recordType、sequence 和 CRC 做通用化。

## 与上层项目的关系

Esp32At24cRecordStore 只保存和读取字节 payload。上层项目负责：

- 定义配置结构。
- 定义默认值。
- 校验字段合法性。
- 决定何时保存。
- 决定版本升级策略。
- 决定初始化或格式化时机。

## 日志与事件

公共库不直接依赖 Esp32Base 日志，也不自行持久化运行日志。

推荐提供可选事件回调：

```text
RecordStoreEventSink
  onRecordStoreEvent(event)
```

事件只表达通用存储语义，例如设备离线、读写失败、CRC 不匹配、无有效记录、写入成功、写入跳过。上层项目负责把事件接入 Esp32Base 日志、远程诊断页面或其他输出。
