# 硬件资源预算草案

## 目标

本文记录 GPIO、PWM、编码器、ADC、I2C 等资源预算，避免进入实现阶段后出现资源冲突。

## Esp32FarmDoor

| 资源 | 用途 | 默认值 | 待确认 |
| --- | --- | --- | --- |
| GPIO33 | INA240A2 ADC | 是 | ADC attenuation 和校准 |
| GPIO36 | 按钮 1 | 是 | 具体用途 |
| GPIO39 | 开门按钮 | 是 | 输入上拉策略 |
| GPIO34 | 关门按钮 | 是 | 输入上拉策略 |
| GPIO35 | 停止按钮 | 是 | 输入上拉策略 |
| GPIO25 | 编码器 A | 是 | 计数模式 |
| GPIO26 | 编码器 B | 是 | 计数模式 |
| GPIO16 | AT8236 IN1/PWM1 | 是 | LEDC channel |
| GPIO17 | AT8236 IN2/PWM2 | 是 | LEDC channel |
| GPIO21 | I2C SDA | 是 | 与 AT24C 共用 |
| GPIO22 | I2C SCL | 是 | 与 AT24C 共用 |
| GPIO32 | 关门/下限位开关 | 推荐 | 首版 P0，防止放绳过量后反向卷绕 |
| GPIO27 | 开门/上限位开关 | 可选预留 | 非首版必需，若 PCB 方便可预留 |

待确认：

- LEDC 频率和分辨率。
- Arduino ESP32 Core 2.x/3.x LEDC API 差异适配。
- ESP32Encoder 或 PCNT 资源占用。
- 关门/下限位开关 GPIO 推荐先用 GPIO32，后续实际硬件可调整。
- 开门/上限位开关不是首版必选；如果 PCB 有余量，推荐预留 GPIO27。
- 限位开关接法建议优先常闭 NC，并明确外部上拉/下拉、电平极性和断线诊断方式。
- ADC attenuation 和实际可测电压范围。

## Esp32FarmFeeder

| 资源 | 用途 | 默认值 | 待确认 |
| --- | --- | --- | --- |
| GPIO16 | 喂食器 1 PWM | 是 | LEDC channel |
| GPIO17 | 喂食器 2 PWM | 是 | LEDC channel |
| GPIO18 | 喂食器 3 PWM | 是 | LEDC channel |
| GPIO19 | 预留 PWM | 是 | 是否保留 |
| GPIO33 / GPIO32 | 喂食器 1 编码器 A/B | 是 | 计数模式 |
| GPIO26 / GPIO25 | 喂食器 2 编码器 A/B | 是 | 计数模式 |
| GPIO14 / GPIO27 | 喂食器 3 编码器 A/B | 是 | 计数模式 |
| GPIO21 / GPIO22 | I2C SDA/SCL | 是 | AT24C |

当前硬件没有电流检测芯片。未来如增加，推荐每个电机对应一个 INA240A2 芯片，并新增 3 路 ADC 资源预算。

待确认：

- 三路编码器是否会消耗 ESP32 PCNT 资源，是否足够。
- PWM 频率和分辨率是否三路一致。
- 三路是否允许同时运行。
- 未来新增 INA240A2 时 ADC 引脚规划。

## 公共库实现约束

Esp32EncodedDcMotor：

- 需要明确 LEDC channel 分配由上层传入，避免库内全局抢占。
- 需要明确编码器计数后端，首版可使用 ESP32Encoder，但文档应记录 PCNT 资源限制。
- 需要支持编码器方向反转配置。
- 需要支持计数模式配置：x1 / x2 / x4 是否实现需冻结。

Esp32MotorCurrentGuard：

- 需要明确 ADC attenuation。
- 需要明确 ADC 校准方式。
- 需要明确采样频率建议。
- 需要明确 INA240A2 零点校准流程。

Esp32At24cRecordStore：

- 需要明确 I2C 地址。
- 需要明确容量、页大小、地址字节数。
- 首版只要求 AT24C128；推荐兼容 2 字节地址 AT24C32/64/256/512。
- AT24C02/04/08/16 如需特殊地址位寻址，首版不支持。
