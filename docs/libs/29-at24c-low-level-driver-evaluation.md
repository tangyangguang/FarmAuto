# AT24C 低层后端复用评估

## 目标

本文评估 `Esp32At24cRecordStore` 源码阶段的低层 EEPROM 访问策略。

待决策问题：

- `At24cDevice` 是否直接依赖成熟 Arduino EEPROM 库。
- 如果不直接依赖，为什么不算重复造底层轮子。
- 源码阶段必须保留哪些成熟库最佳实践。

## 结论

首版推荐自研最小 `At24cDevice` 低层访问层，不直接把 RobTillaart I2C_EEPROM、SparkFun External EEPROM 或 JC_EEPROM 作为核心硬依赖。

原因：

- `Esp32At24cRecordStore` 需要统一、明确的错误码，例如 `DeviceOffline`、`OutOfRange`、`AckTimeout`、`CompareFailed`。
- 记录层需要严格控制页边界、header 不跨页、双阶段提交和写后校验流程。
- 首版不支持需要特殊地址位映射的 AT24C02/04/08/16，避免为低价值型号增加特殊代码。
- 本库需要在 `inspect()` 中暴露写入、校验和错误诊断，不应被第三方库的内部错误模型遮住。
- 低层能力范围很小：在线检测、连续读、跨页写、写完成等待或 ACK polling、compare、范围检查。为了这些能力引入硬依赖，收益不一定大于适配成本。

这不是否定成熟库价值。源码实现前必须参考成熟库接口和行为，避免手写随意实现；如果后续实测发现成熟库能更好满足错误模型和兼容性，可以新增可选 adapter。

## 候选库评估

| 候选 | 优点 | 风险或不匹配 | 结论 |
| --- | --- | --- | --- |
| SparkFun External EEPROM | 支持运行时 memory type；可设置 memory size、address bytes、page size；处理页写限制；支持多种 24xx 型号 | 主要目标是通用易用访问；错误码、inspect 诊断、双阶段提交需要本库再封装 | 作为实现参考和后续可选 adapter 候选，不作为首版硬依赖 |
| RobTillaart I2C_EEPROM | 支持 24LC512/256/64/32/16/08/04/02/01；有 read/write/verify/update 思路；MIT license；有 PlatformIO 元数据 | README 明确地址范围由用户负责校验；错误模型需要再映射；不应把范围校验责任外移给应用 | 作为实现参考和后续可选 adapter 候选，不作为首版硬依赖 |
| JC_EEPROM | 支持较宽容量范围；处理 page/device boundary；返回 I2C 状态和地址错误；文档提醒 Wire buffer 限制 | 设备寻址假设较强；对 24xx1025 等特殊型号有限制；本项目首版不需要多芯片连续地址空间 | 作为补充参考，不作为首版硬依赖 |

## 首版 `At24cDevice` 必须具备的低层能力

```text
At24cDevice
  begin(wire, config)
  isOnline()
  read(address, buffer, length)
  write(address, data, length)
  compare(address, data, length)
  waitWriteComplete(timeoutMs)
```

实现要求：

- 所有 public 方法先做 `address + length <= capacityBytes` 范围检查。
- 写入必须按 EEPROM 页边界拆分，不能跨页直接写。
- 写入必须考虑 Wire buffer 限制，不能假设一次可写整页。
- 支持 ACK polling 或等效写完成等待，并能返回超时。
- 支持写后回读比较。
- 首版只要求 2 字节地址 AT24C 型号；如未来支持小容量特殊寻址，差异只能在 `At24cDevice` 内部处理。
- 不在低层设备类中理解 recordType、CRC、schema 或业务 payload。

## 与成熟库的关系

源码实现时应吸收成熟库的做法：

- SparkFun 的运行时容量、地址字节数、页大小配置思路。
- RobTillaart 的 read/write/verify/update 分工思路。
- JC_EEPROM 对页边界、设备边界和 Wire buffer 限制的提醒。

但首版核心 API 不暴露第三方库类型，不要求用户安装第三方 EEPROM 库。

未来可以增加：

- `At24cDevice` 自研后端。
- `At24cDeviceSparkFunAdapter`。
- `At24cDeviceRobTillaartAdapter`。

只有当 adapter 经过 AT24C128 实测，并且错误码、页写、ACK polling、范围检查和 2 字节地址型号访问都能满足本库要求时，才考虑把它作为推荐后端。

## 源码前验收

进入 `Esp32At24cRecordStore` 源码实现前，应确认：

- 首版使用自研最小 `At24cDevice`。
- README 说明 preset 只是常见默认值，必须核对具体芯片 datasheet。
- examples 说明页大小、写周期、I2C 地址和 write protect 引脚都需要实物确认。
- 测试覆盖 AT24C128 页边界、ACK polling、写后比较、2 字节地址访问和越界访问。

## 参考资料

- SparkFun External EEPROM Arduino Library：https://github.com/sparkfun/SparkFun_External_EEPROM_Arduino_Library
- RobTillaart I2C_EEPROM：https://github.com/RobTillaart/I2C_EEPROM
- JC_EEPROM：https://github.com/JChristensen/JC_EEPROM
