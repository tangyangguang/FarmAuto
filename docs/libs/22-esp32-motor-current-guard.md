# Esp32MotorCurrentGuard 电流保护库方案

## 定位

Esp32MotorCurrentGuard 是电机电流采样与保护判定库。它面向电机过流、堵转辅助判断和传感器状态监控，不做精密电能计量。

当前已知可能使用的芯片：

- INA240A2。
- ACS712。
- INA226。

三类芯片用途相近，但读取方式和噪声模型不同。因此库名不绑定具体芯片，内部通过具体芯片采样类适配。

首版真实实现 INA240A2。未来增加其他芯片时，应新增具体芯片采样类，不改变 `MotorCurrentGuard` 的保护判定层。

公共库不定义某个设备是否默认启用电流保护，也不提供固定业务阈值。是否启用、阈值是多少、故障后如何处理，都由上层应用根据硬件和实测参数配置。

## 分层设计

```text
Ina240A2AnalogSensor
Acs712AnalogSensor
Ina226I2cSensor

MotorCurrentGuard
  filter
  startup grace
  over-current confirmation
  sensor fault detection
  trace data output
```

首版只实现 `Ina240A2AnalogSensor`。ACS712 和 INA226 只保留芯片类命名和设计方向，不实现代码。

首版不抽正式 `CurrentSensor` 虚接口，也不要求继承层次。各芯片采样类只需要能产出相同的 `CurrentSample` 结构；`MotorCurrentGuard` 只消费 `CurrentSample`。

## INA240A2 采样类

INA240A2 是模拟输出电流检测放大器，适合 PWM 电机电流检测。

配置项：

- ADC 引脚。
- ADC attenuation。
- ADC 参考电压或校准参数。
- ADC 分辨率。
- INA240 增益，A2 默认 50V/V。
- Rsense 采样电阻值。
- 零点电压。
- 是否按双向电流处理。

换算模型：

```text
Vshunt = (Vout - Vref) / gain
I = Vshunt / Rsense
```

需要支持：

- 零点校准。
- ESP32 ADC 校准曲线或 eFuse 校准数据接入。
- ADC 原始值读取。
- 电压换算。
- mA 换算。
- ADC 饱和检测。
- 零点异常检测。
- 传感器读取失败检测。

## ACS712 未来采样类

ACS712 是霍尔模拟电流传感器，输出模拟电压。

未来配置项：

- ADC 引脚。
- 供电电压。
- 零点电压，通常接近 VCC/2。
- 型号灵敏度，单位 mV/A。
- 滤波参数。

ACS712 噪声和温漂通常比 INA240 更需要校准和滤波。它适合隔离测量，但不应假设和 INA240 拥有相同精度。

## INA226 未来采样类

INA226 是 I2C 数字电流、电压、功率监测芯片。

未来配置项：

- I2C 地址。
- Rsense。
- conversion time。
- averaging。
- alert 阈值。

INA226 可直接读取 shunt voltage、bus voltage、current 和 power。它更适合电源监测和较慢保护，不应默认承担最快速的 PWM 电机瞬态堵转保护。

## MotorCurrentGuard 判定逻辑

MotorCurrentGuard 不关心底层芯片，只接收电流采样值。

核心能力：

- 启动宽限：电机启动初期不判定过流。
- 滤波：首版使用一阶 IIR。
- 连续超限确认：超过阈值持续一段时间或连续 N 次后触发。
- 峰值电流记录。
- 传感器故障状态。
- 最近故障原因。

建议状态：

```text
Normal
StartupGrace
Warning
OverCurrent
SensorFault
Disabled
```

## 接口级设计

芯片采样类只负责采样和换算，保护器只负责保护判定。

建议 INA240A2 采样类：

```text
Ina240A2AnalogSensor
  begin()
  readSample()
  calibrateZero(sampleCount)
  status()
```

建议采样结果：

```text
CurrentSample
  ok
  sequence
  rawAdc
  voltageMv
  currentMa
  timestampMs
  sensorStatus
  sampleLost
```

建议保护器接口：

```text
MotorCurrentGuard
  configure(config)
  reset()
  update(sample, motorRunning, nowMs)
  snapshot()
  latestTracePoint()
```

`MotorCurrentGuard` 不主动读取硬件，方便后续接入不同芯片采样类，也方便测试。

## 配置结构

建议通用保护配置：

```text
MotorCurrentGuardConfig
  enabled
  warningThresholdMa
  faultThresholdMa
  startupGraceMs
  confirmationMs
  confirmationSamples
  filterAlpha
  sensorFaultPolicy
```

推荐默认值：

- `filterAlpha` 默认 0.2。
- `startupGraceMs` 默认 1000ms，上层可按电机软启动时长覆盖。
- 保护判定同时支持 `confirmationMs` 和 `confirmationSamples`。
- 公共库不提供固定 `warningThresholdMa` / `faultThresholdMa`，必须由上层配置。
- `sensorFaultPolicy` 默认按故障处理，不把不可信传感器当作正常保护。

INA240A2 采样配置：

```text
Ina240A2Config
  adcPin
  adcAttenuation
  adcReferenceMv
  adcResolutionBits
  gain
  rsenseMilliOhm
  zeroOffsetMv
  bidirectional
```

`gain` 默认 50，代表 INA240A2。`bidirectional` 默认启用，适合正反转电机的远程诊断。

## 状态快照

建议状态快照：

```text
CurrentGuardSnapshot
  state
  rawCurrentMa
  filteredCurrentMa
  peakCurrentMa
  thresholdMa
  warningThresholdMa
  faultThresholdMa
  sampleRateHz
  lastSampleMs
  overThresholdSinceMs
  consecutiveOverThresholdSamples
  adcSaturationCount
  sensorFaultCount
  warningSinceMs
  faultSinceMs
  sensorStatus
  faultReason
```

## 电流变化图表支持

公共库不生成 Web 图表、不输出 HTML/JS，也不依赖任何页面框架。

为了支持上层项目生成电流变化图表，公共库应提供稳定的时间序列数据来源：

```text
CurrentTracePoint
  timestampMs
  sequence
  rawAdc
  voltageMv
  rawCurrentMa
  filteredCurrentMa
  peakCurrentMa
  warningThresholdMa
  thresholdMa
  state
  sensorStatus
  faultReason
  sampleLost
  adcSaturated
```

推荐策略：

- 具体芯片采样类提供原始采样。
- `MotorCurrentGuard` 提供滤波后采样和状态。
- 上层项目负责保存最近一段时间的 ring buffer，并通过自己的 API 输出 JSON。
- 首版公共库只提供 `latestTracePoint()` 单点接口，不实现 `CurrentTraceBuffer` 工具类。

这样既能支持远程诊断图表，又不会把 Web、历史存储或业务展示塞进公共库。

高价值图表：

- 原始 ADC 曲线。
- 传感器输出电压曲线。
- 原始电流与滤波电流曲线。
- warning/fault 阈值线。
- 启动宽限区间。
- 连续超限确认区间。
- ADC 饱和点。
- 采样丢失点。
- 传感器异常和故障原因时间线。

INA240A2 是模拟电流检测放大器，不是数字传感器。首版不需要依赖复杂第三方库，重点应放在 ADC 校准、零点校准、滤波、阈值确认、采样异常诊断和 trace 数据输出。

## 故障原因

建议故障原因：

```text
None
Disabled
SensorReadFailed
AdcSaturated
ZeroOffsetInvalid
OverCurrent
SustainedOverCurrent
ConfigInvalid
```

`Warning` 状态不一定进入故障；只有达到确认条件后才进入 `OverCurrent`。

## 采样与滤波原则

- 采样频率由上层项目决定。
- 保护器不使用阻塞延时。
- 首版滤波使用一阶 IIR。
- 过流判定使用滤波值，峰值记录可保留原始值。
- 启动宽限期间仍记录电流，但不触发过流故障。
- PWM 噪声明显时，应通过采样频率、采样时刻、硬件滤波和 `filterAlpha` 实测调整。
- 采样中断或 sequence 不连续时，应在 snapshot 中反映采样丢失。
- 首版不做 RMS；远程诊断优先保留峰值、滤波值、饱和计数和采样丢失。

## INA240A2 ADC 与校准

INA240A2 的 ADC 配置必须由最大输出电压范围反推，不能写死。

建议原则：

- `adcAttenuation` 由 `adcReferenceMv`、INA240A2 输出范围和 ESP32 ADC 可测范围决定。
- 必须使用 Arduino ESP32 / ESP-IDF 可用的 ADC 校准能力，例如 eFuse Vref/two-point 校准或等效校准曲线；不能只用理想 0..4095 线性换算作为保护阈值依据。
- 如果目标芯片或 Core 版本无法提供可靠 ADC 校准，应用必须把电流保护标记为“需实测确认”，并提高阈值裕量，避免误触发或漏触发。
- 阈值设置页面应保留零点、实测空载电流、实测堵转/过载电流和 ADC 饱和计数，便于远程判断保护是否可信。
- 零点校准应在被测电机无电流或断电状态下执行。
- 校准结果只保存零点偏移，不在传感器类中自行持久化。
- PWM 噪声明显时，上层应固定采样周期，并通过实测决定是否避开 PWM 边沿。
- ADC 饱和应累加 `adcSaturationCount`，便于远程判断阈值失效或硬件异常。

## 首版边界

首版实现：

- INA240A2 模拟采样类。
- mA 换算。
- 零点校准参数。
- 启动宽限。
- 滤波。
- 连续超限确认。
- 传感器异常状态。

首版不实现：

- ACS712 采样类。
- INA226 采样类。
- 高精度电能计量。
- 自动停止电机。
- 配置持久化。

## 多通道使用原则

Esp32MotorCurrentGuard 按“一个芯片采样实例对应一个电流检测通道”设计。

如果一个设备有多个电机，且每个电机都有独立电流检测芯片，则上层项目创建多个芯片采样实例和多个保护器实例。公共库不负责判断这些通道属于哪个上层对象。

如果多个执行器共用一个总电流检测通道，总电流异常只能作为系统级异常，公共库不推断具体异常来源。

## 与上层项目的关系

Esp32MotorCurrentGuard 不直接停止电机，也不保存配置。上层项目决定：

- 是否启用电流保护。
- 阈值是多少。
- 过流后停止哪台电机。
- 如何记录事件。
- 如何在上层诊断界面显示。

## 日志与事件

公共库不直接依赖 Esp32Base 日志。

推荐提供可选事件回调：

```text
CurrentGuardEventSink
  onCurrentGuardEvent(event)
```

事件只表达通用保护语义，例如进入 warning、进入 over-current、传感器异常、故障清除。上层项目负责把事件接入 Esp32Base 日志、远程诊断页面或其他输出。
