# Esp32At24cRecordStore 记录存储库方案

## 定位

Esp32At24cRecordStore 是 AT24C/24LC/24AA 系列 I2C EEPROM 上的可靠记录存储库。它提供页写入、记录校验、版本和最新记录选择能力。

它不绑定任何具体应用项目的业务字段。

## 支持范围

首版实测目标是 AT24C128，但设计不写死具体容量。

在不过度设计的前提下，通过芯片配置支持以下常见型号：

- AT24C02 / AT24C04 / AT24C08 / AT24C16。
- AT24C32 / AT24C64。
- AT24C128 / AT24C256 / AT24C512。

首版支持范围先限定在上述 AT24C/24LC/24AA 系列 I2C EEPROM。新增型号应优先通过配置预设支持，不为每个型号复制存储逻辑。

不同型号主要差异：

- 总容量。
- 页大小。
- 地址字节数。
- 小容量芯片可能使用 I2C 地址位选择内部地址。
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
  smallDeviceAddressBits
```

库可以提供常见型号预设，也允许应用显式传入配置。

这个设计参考成熟外部 EEPROM 库的做法：通过运行时配置描述容量、地址字节数和页大小，让上层把 EEPROM 看成连续地址空间，同时由库内部处理页写入限制。

小容量型号如果使用 I2C 地址位选择内部地址，应由 `At24cDevice` 在内部完成逻辑地址到设备地址和 word address 的映射，不能泄漏到记录存储层。

小容量寻址规则：

- `addressBytes=1` 且 `smallDeviceAddressBits>0` 时，逻辑地址高位映射到 I2C device address 低位。
- `deviceAddress = i2cAddress | ((logicalAddress >> 8) & ((1 << smallDeviceAddressBits) - 1))`。
- `wordAddress = logicalAddress & 0xFF`。
- `addressBytes=2` 时，`smallDeviceAddressBits` 必须为 0，完整逻辑地址写入 2 字节 word address。
- 所有映射后访问都必须检查 `logicalAddress + length <= capacityBytes`。

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

首版采用固定记录区，避免做复杂文件系统。

建议概念模型：

```text
RecordRegion
  recordType
  startAddress
  slotSize
  slotCount

StoreLayout
  magic
  layoutVersion
  regions[]
```

每个 `recordType` 对应一个记录区。记录区内使用多个固定大小槽位，写入时选择下一个槽位。读取时选择 sequence 最大且 CRC 有效的槽位。

固定槽位的好处：

- 实现简单。
- 断电恢复逻辑清晰。
- 不需要动态分配。
- 容量预算可提前计算。

代价：

- 空间利用率不如文件系统。
- 每类记录需要预留最大 payload 大小。

这个取舍符合当前设备配置、状态和小型历史记录的需求。

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
- CRC 算法进入源码前必须固定，后续变更需要提升 layout 或 schema version。
- header 必须保证不跨 EEPROM 页，避免提交 `Valid` 时出现跨页半写。

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

- 每类记录使用一个固定记录区。
- 每个记录区保留两个或多个槽位。
- 写入新槽位成功且 CRC 校验通过后，作为最新记录。
- 读取时选择 sequence 最大且 CRC 有效的记录。
- 内容未变化时不写入。
- 格式化必须由应用显式触发，库不能静默清空。
- 设备离线时返回 `DeviceOffline`，不能伪造默认成功。

暂不做：

- 通用数据库。
- 动态文件系统。
- 任意长度日志。
- 复杂坏块管理。
- 老项目格式迁移。

如果实测出现 EEPROM 页面损坏或频繁断电损坏，再评估是否增加坏块标记或更复杂的磨损均衡。

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

## 型号支持原则

首版必须支持 AT24C128。

其他型号通过 `At24cChipConfig` 支持，不在代码里散落型号判断。型号预设只做配置表：

```text
At24cPreset
  AT24C02
  AT24C04
  AT24C08
  AT24C16
  AT24C32
  AT24C64
  AT24C128
  AT24C256
  AT24C512
```

如果小容量型号的寻址方式与大容量型号差异明显，应把差异封装在 `At24cDevice` 内，不能泄漏到 `Esp32At24cRecordStore`。

## 首版边界

首版实现：

- AT24C128 配置。
- 可配置容量、页大小、地址字节数。
- 跨页读写。
- 写前比较。
- 固定记录区。
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
