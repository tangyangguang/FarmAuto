# MotorCurrentGuard 电流保护库方案

## 定位

MotorCurrentGuard 是电机电流采样与保护判定库。它面向电机过流、堵转辅助判断和传感器状态监控，不做精密电能计量。

当前已知可能使用的芯片：

- INA240A2。
- ACS712。
- INA226。

三类芯片用途相近，但读取方式和噪声模型不同。因此库名不绑定具体芯片，内部通过传感器后端适配。

## 分层设计

```text
CurrentSensor
  Ina240A2AnalogSensor
  Acs712AnalogSensor
  Ina226I2cSensor

CurrentGuard
  filter
  startup grace
  over-current confirmation
  sensor fault detection
```

首版只实现 INA240A2 后端。ACS712 和 INA226 先保留接口和文档设计，不实现代码。

## INA240A2 后端

INA240A2 是模拟输出电流检测放大器，适合 PWM 电机电流检测。

配置项：

- ADC 引脚。
- ADC 参考电压或校准参数。
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
- ADC 原始值读取。
- 电压换算。
- mA 换算。
- ADC 饱和检测。
- 零点异常检测。

## ACS712 未来后端

ACS712 是霍尔模拟电流传感器，输出模拟电压。

未来配置项：

- ADC 引脚。
- 供电电压。
- 零点电压，通常接近 VCC/2。
- 型号灵敏度，单位 mV/A。
- 滤波参数。

ACS712 噪声和温漂通常比 INA240 更需要校准和滤波。它适合隔离测量，但不应假设和 INA240 拥有相同精度。

## INA226 未来后端

INA226 是 I2C 数字电流、电压、功率监测芯片。

未来配置项：

- I2C 地址。
- Rsense。
- conversion time。
- averaging。
- alert 阈值。

INA226 可直接读取 shunt voltage、bus voltage、current 和 power。它更适合电源监测和较慢保护，不应默认承担最快速的 PWM 电机瞬态堵转保护。

## CurrentGuard 判定逻辑

CurrentGuard 不关心底层芯片，只接收电流采样值。

核心能力：

- 启动宽限：电机启动初期不判定过流。
- 滤波：一阶滤波或小窗口平均。
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

## 与上层项目的关系

MotorCurrentGuard 不直接停止电机，也不保存配置。上层项目决定：

- 是否启用电流保护。
- 阈值是多少。
- 过流后停止哪台电机。
- 如何记录事件。
- 如何在 Web 上显示。

如果多个执行器共用一个总电流检测通道，总电流异常时通常只能判断系统级异常，不能可靠定位到具体执行器。需要上层项目在硬件设计和控制策略中明确处理。
