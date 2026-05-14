# 公共库测试清单

## 目标

本文记录三个公共库进入源码骨架后需要满足的测试维度。当前仍为文档阶段，不写测试代码。

## Esp32EncodedDcMotor

单元测试：

- 空闲状态接受运行命令。
- 运行中收到新运行命令返回 `Busy`。
- 故障状态拒绝运行命令并返回 `FaultActive`。
- `AlreadyAtTarget`、`TargetTooSmall`、`ConfigMissing` 可被稳定触发。
- `SoftStarting` 按 `softStartMs` 推进。
- `SoftStopping` 按 `softStopMs` 推进。
- `Braking` 按 `brakeMs` 退出。
- `EmergencyStop` 按 `emergencyOutputMode` 输出并进入 `Fault`。
- `X1` / `X2` / `X4` 计数模式换算正确。
- 电机方向和编码器方向独立反转后方向判断正确。

硬件在环：

- LEDC channel、频率、分辨率配置生效。
- AT8236 双 PWM H 桥正反转输出正确。
- 编码器后端可持续返回 `int64_t` 位置。
- 编码器断线或无脉冲时触发故障。
- Brake 与 Coast 对机械冲击的差异有实测记录。

## Esp32MotorCurrentGuard

单元测试：

- INA240A2 mA 换算正确。
- 零点校准参数生效。
- `filterAlpha` 改变滤波响应。
- `startupGraceMs` 期间不触发过流故障。
- `confirmationMs` 和 `confirmationSamples` 都满足后才进入故障。
- ADC 饱和累加 `adcSaturationCount`。
- 采样 sequence 不连续时能反映采样丢失。
- `CurrentTracePoint` 包含阈值和故障原因。

硬件在环：

- INA240A2 零点校准。
- 不同 ADC attenuation 下读数范围确认。
- 正常正转/反转负载电流记录。
- 堵转或接近堵转电流记录。
- PWM 噪声下采样频率和滤波参数对比。
- 传感器断线、短路或饱和能被远程诊断。

## Esp32At24cRecordStore

单元测试：

- AT24C128 preset 参数正确。
- AT24C02/04/08/16 小容量地址映射公式正确。
- 跨页读取和写入正确拆分。
- header 不跨页约束可被校验。
- `Writing` 记录断电后被忽略。
- `Valid` 提交时 flags 和 header CRC 同步更新。
- payload CRC 错误时记录被忽略。
- header CRC 错误时返回或记录 `HeaderCrcMismatch`。
- sequence 回绕时仍能选择最新记录。
- 写前比较相同内容返回 `Unchanged`。
- `inspect(recordType)` 能列出每个 slot 的状态。

硬件在环：

- AT24C128 在线检测。
- ACK polling 或写周期等待可靠。
- 写入中断电后能读取上一个有效记录。
- `Writing -> Valid` 之间断电后不会误选坏记录。
- I2C 设备离线时返回 `DeviceOffline`。
- 页边界和容量边界访问不越界。

## 发布前检查

- 公共库文档不含具体应用业务术语。
- examples 覆盖最小用法和错误处理。
- event sink 可为空。
- event sink 不分配大对象、不做长耗时操作。
- snapshot 字段足够支撑远程诊断。
- 未修改 Esp32Base。

