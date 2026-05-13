# At24cRecordStore 记录存储库方案

## 定位

At24cRecordStore 是 AT24C/24LC/24AA 系列 I2C EEPROM 上的可靠记录存储库。它提供页写入、记录校验、版本和最新记录选择能力。

它不绑定 FarmDoor 或 FarmFeeder 的业务字段。

## 支持范围

首版实测目标是 AT24C128，但设计不写死具体容量。

在不过度设计的前提下，通过芯片配置支持常见型号：

- AT24C02 / AT24C04 / AT24C08 / AT24C16。
- AT24C32 / AT24C64。
- AT24C128 / AT24C256。

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
```

库可以提供常见型号预设，也允许应用显式传入配置。

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
headerCrc
payloadCrc
flags
```

应用为不同数据使用不同 `recordType`：

- FarmDoor 配置。
- FarmDoor 状态。
- FarmFeeder 配置。
- FarmFeeder 今日计数。
- FarmFeeder 历史记录。

## 可靠性策略

首版采用简单可靠策略，不做复杂文件系统。

建议：

- 每类记录使用一个固定记录区。
- 每个记录区保留两个或多个槽位。
- 写入新槽位成功且 CRC 校验通过后，作为最新记录。
- 读取时选择 sequence 最大且 CRC 有效的记录。
- 内容未变化时不写入。
- 格式化必须由应用显式触发，库不能静默清空。

暂不做：

- 通用数据库。
- 动态文件系统。
- 任意长度日志。
- 复杂坏块管理。
- 老项目格式迁移。

如果实测出现 EEPROM 页面损坏或频繁断电损坏，再评估是否增加坏块标记或更复杂的磨损均衡。

## 与应用的关系

At24cRecordStore 只保存和读取字节 payload。应用负责：

- 定义配置结构。
- 定义默认值。
- 校验字段合法性。
- 决定何时保存。
- 决定版本升级策略。
- 决定初始化或格式化时机。
