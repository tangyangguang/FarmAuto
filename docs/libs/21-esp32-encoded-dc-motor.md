# Esp32EncodedDcMotor 公共电机库方案

## 定位

Esp32EncodedDcMotor 是带编码器 DC 电机的通用运动控制库。它解决电机驱动、编码器计数、目标运动、软启动、软停止和基础运动保护问题。

它不理解任何具体应用业务。

## 支持场景

首版应支持：

- 单向 PWM 电机。
- AT8236 双 PWM H 桥正反转电机。
- AB 相编码器。
- 指定脉冲运动。
- 指定圈数运动。
- 绝对位置运动。
- 回零运动。

## 状态机

核心状态：

```text
Idle
Starting
Running
SoftStopping
Braking
Fault
```

状态含义：

- `Idle`：空闲，无 PWM 输出。
- `Starting`：软启动阶段。
- `Running`：目标速度运行阶段。
- `SoftStopping`：软停止阶段。
- `Braking`：执行刹车动作。
- `Fault`：故障停机，需要应用层确认或清除。

## 核心能力

编码器能力：

- 读取当前累计脉冲。
- 设置当前累计脉冲。
- 清零累计脉冲。
- 读取运行段脉冲增量。
- 计算 RPM/PPS。

运动目标：

- 运行指定脉冲数。
- 运行指定输出轴圈数。
- 运行到绝对脉冲位置。
- 运行回零。

速度控制：

- 目标速度百分比。
- 软启动时长 `softStartMs`。
- 软停止时长 `softStopMs`。
- 最小有效速度 `minEffectiveSpeedPercent`。
- 控制 tick 间隔 `controlTickMs`。

停止策略：

- `Coast`：滑行。
- `Brake`：刹车。
- `SoftStopThenCoast`：软停止后滑行。
- `SoftStopThenBrake`：软停止后刹车。
- `EmergencyStop`：故障时立即关闭或刹车。

保护能力：

- 启动宽限。
- 编码器无脉冲堵转检测。
- 最大运行时间。
- 最大运行脉冲。
- 目标越界保护。
- 编码器计数异常检测。

事件输出：

- 启动。
- 到达目标。
- 停止完成。
- 急停。
- 故障。

## 配置结构

建议概念模型：

```text
MotorKinematics
  motorShaftPulsesPerRev
  gearRatio
  outputPulsesPerRev

MotorMotionProfile
  speedPercent
  softStartMs
  softStopMs
  minEffectiveSpeedPercent
  controlTickMs

MotorProtection
  startupGraceMs
  stallCheckIntervalMs
  minPulseDelta
  maxRunMs
  maxRunPulses

MotorStopPolicy
  normalStopMode
  faultStopMode
```

`outputPulsesPerRev` 可以由 `motorShaftPulsesPerRev * gearRatio` 得出，也允许应用显式覆盖，便于实测校准。

## 接口级设计

首版接口应围绕一个电机实例展开，避免为了多电机编排增加复杂抽象。

建议概念接口：

```text
Esp32EncodedDcMotor
  begin()
  update(nowMs)
  configureKinematics(config)
  configureMotionProfile(config)
  configureProtection(config)
  configureStopPolicy(config)
  setPositionPulses(pulses)
  resetPosition()
  positionPulses()
  runPulses(direction, pulses, speedPercent)
  runRevolutions(direction, revolutions, speedPercent)
  runToPosition(targetPulses, speedPercent)
  runToZero(speedPercent)
  requestStop(mode)
  emergencyStop(reason)
  clearFault()
  snapshot()
```

`update(nowMs)` 必须非阻塞，由上层项目在主循环或调度器中周期调用。

这种设计参考成熟电机库的常见模式：配置目标后立即返回，实际运动由主循环中反复调用 `run/update` 推进。这样不会阻塞 Web、按钮、日志、看门狗和其他电机。

## 状态快照

建议状态快照包含：

```text
MotorSnapshot
  state
  direction
  currentSpeedPercent
  targetSpeedPercent
  positionPulses
  segmentStartPulses
  targetPulses
  pulsesPerSecond
  rpm
  elapsedMs
  lastPulseMs
  faultReason
```

状态快照只反映电机控制状态，不包含业务字段。

## 故障原因

建议故障原因：

```text
None
InvalidConfig
InvalidCommand
DriverError
EncoderNoPulse
EncoderDirectionMismatch
MaxRunTimeExceeded
MaxRunPulsesExceeded
TargetOutOfRange
EmergencyStopRequested
ExternalFault
```

`ExternalFault` 用于上层项目把电流保护、限位开关或其他外部保护结果传入电机库，电机库不直接判断这些外部来源。

## 软启动和软停止

软启动和软停止是核心能力，必须支持独立时长：

- `softStartMs`：从 0 或当前速度平滑升到目标速度。
- `softStopMs`：从当前速度平滑降到 0 或最小有效速度。
- `minEffectiveSpeedPercent`：低于该速度时可直接停止，避免低速无力导致误判。

默认速度曲线先采用线性曲线。只有实机证明线性曲线不合适时，再考虑增加其他曲线。

启动宽限应至少覆盖软启动初期，避免电机低速阶段因为脉冲少而误判堵转。

## 命令处理原则

- 空闲状态可以接受新的运行命令。
- 运行中收到新的运行命令，首版默认拒绝并返回 `Busy`，不自动打断。
- 运行中收到停止命令，按停止策略进入 `SoftStopping`、`Braking` 或立即停机。
- 故障状态只接受 `clearFault()` 或安全停机相关命令。
- 所有命令返回明确结果，不通过隐式串口日志表达失败。

建议命令结果：

```text
Ok
Busy
InvalidArgument
InvalidState
NotInitialized
DriverRejected
```

## 驱动后端

库应把运动控制和驱动输出分开。

驱动后端接口负责：

- 初始化引脚和 PWM。
- 设置方向。
- 设置 PWM 占空比。
- 刹车。
- 滑行。
- 关闭输出。

首版驱动后端：

- `SinglePwmDriver`：单向 PWM 输出。
- `At8236HBridgeDriver`：双 PWM H 桥。

驱动后端不读取编码器，不做目标运动，不做业务判断。

## 与 Esp32MotorCurrentGuard 的关系

Esp32EncodedDcMotor 不直接读取电流。它只暴露运动状态和故障停机接口。

上层项目负责组合：

```text
业务控制器
  Esp32EncodedDcMotor
  Esp32MotorCurrentGuard
```

当 Esp32MotorCurrentGuard 判定过流时，上层项目调用电机的故障停机接口，并记录业务事件。

## 首版边界

首版实现：

- 单电机实例。
- 单向 PWM 驱动。
- AT8236 双 PWM H 桥驱动。
- AB 相编码器累计计数。
- 指定脉冲、指定圈数、绝对位置、回零。
- 独立软启动和软停止。
- 编码器无脉冲、最大时间、最大脉冲保护。

首版不实现：

- PID 闭环速度控制。
- 多电机同步控制。
- 复杂 S 曲线加减速。
- 限位开关直接管理。
- 电流采样。
- 持久化。

## 设计参考

- AccelStepper 的位置控制接口采用 `moveTo()` 设置目标、`run()` 反复推进运动的非阻塞模型；本库也应保持同类设计原则，但面向带编码器 DC 电机而不是步进电机。
- 多电机协调不进入首版核心库。首版先保证单电机状态机稳定，后续如多个项目真实需要，再考虑独立编排层。

## 不包含内容

Esp32EncodedDcMotor 不包含：

- 具体业务动作语义。
- 电流采样。
- Web/API。
- AT24C 持久化。
- 日志持久化。
- 多电机编排策略。
