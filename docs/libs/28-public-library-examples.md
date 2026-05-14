# 公共库最小 Examples 设计

## 目标

本文定义三个公共库未来源码阶段应提供的最小 examples。当前只做设计，不创建源码。

examples 应验证公共库独立可用，不包含具体应用业务语义。

## 通用原则

- 每个 example 必须能独立说明目标硬件、接线、配置和预期输出。
- examples 不依赖 FarmAuto 应用项目。
- examples 可以可选接入 Esp32Base 日志，但核心用法不强依赖 Esp32Base。
- examples 不保存真实生产数据。
- examples 必须展示错误处理和 snapshot 读取。
- examples 不做大型 Web 页面。

## Esp32EncodedDcMotor

### `basic_at8236_position`

目标：

- 演示 AT8236 双 PWM H 桥驱动。
- 演示 AB 编码器位置读取。
- 演示 `requestMoveByPulses()`、`update(nowMs)` 和 `snapshot()`。

覆盖：

- `MotorHardwareConfig`。
- `EncoderBackendConfig`。
- `MotorMotionProfile`。
- `MotorProtection`。
- `MotorStopPolicy`。
- `SoftStarting` / `Running` / `SoftStopping` / `Braking` / `Idle`。

不覆盖：

- 多电机编排。
- 外部保护。
- 持久化。

### `single_pwm_smoke`

目标：

- 演示单向 PWM 驱动。
- 验证库不强绑定 H 桥。

覆盖：

- `SinglePwmDriver`。
- 速度输出。
- `requestStop()`。

### `fault_and_external_stop`

目标：

- 演示 `EncoderNoPulse`、`MaxRunTimeExceeded` 和 `ExternalFault`。
- 演示故障后必须 `clearFault()` 才能继续。

覆盖：

- 故障结果码。
- `EmergencyStop`。
- `FaultActive`。
- event sink。

### `trace_points`

目标：

- 演示 `MotorTracePoint` 输出。
- 演示上层如何维护固定容量 ring buffer 用于位置、速度和状态图表。

要求：

- ring buffer 属于 example 中的上层维护示例，不是电机核心类的必需依赖。
- ring buffer 必须固定容量。
- 不动态扩容。
- 不生成 Web 图表，只输出可序列化数据。

## Esp32MotorCurrentGuard

### `ina240a2_basic`

目标：

- 演示 `Ina240A2AnalogSensor` 采样。
- 演示零点校准、mA 换算和 ADC 饱和诊断。

覆盖：

- `Ina240A2Config`。
- `CurrentSample`。
- `sensorStatus`。
- `calibrateZero(sampleCount)`。

### `guard_threshold`

目标：

- 演示 `MotorCurrentGuard` 对 `CurrentSample` 做滤波、启动宽限和连续超限确认。

覆盖：

- `MotorCurrentGuardConfig`。
- `filterAlpha`。
- `confirmationMs`。
- `confirmationSamples`。
- `CurrentGuardSnapshot`。

### `trace_points`

目标：

- 演示 `CurrentTracePoint` 输出。
- 演示上层如何维护固定容量 ring buffer 用于图表。

要求：

- ring buffer 属于 example 中的上层维护示例，不是保护核心类的必需依赖。
- ring buffer 必须固定容量。
- 不动态扩容。
- 不生成 Web 图表，只输出可序列化数据。

## Esp32At24cRecordStore

### `basic_record_store`

目标：

- 演示 AT24C128 初始化。
- 演示 layout、recordType、writeRecord 和 readLatest。

覆盖：

- `At24cChipConfig`。
- `RecordRegion`。
- `StoreLayout`。
- `writeRecord()`。
- `readLatest()`。

### `wear_level_ring`

目标：

- 演示同一 recordType 连续写入时按槽轮转。
- 演示 `sequence` 和 `inspect(recordType)`。

覆盖：

- `slotCount`。
- `writeClass`。
- `nextSlotIndex`。
- `estimatedWritesPerSlot`。

### `power_loss_recovery`

目标：

- 演示 `Writing` 半成品记录被忽略。
- 演示最新 `Valid` 记录恢复。

方式：

- 使用测试桩或示例开关模拟提交中断。
- 展示 header CRC、payload CRC 和 flags 的判断结果。

## examples 进入源码前检查

- 每个 example 都有 README。
- 每个 example 都写清楚硬件风险。
- 每个 example 都有串口输出或 snapshot 打印。
- 不出现具体应用业务词。
- 不要求修改 Esp32Base。
- 不依赖 old_prj。
