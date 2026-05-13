# EncodedDcMotor 公共电机库方案

## 定位

EncodedDcMotor 是带编码器 DC 电机的通用运动控制库。它解决电机驱动、编码器计数、目标运动、软启动、软停止和基础运动保护问题。

它不理解自动门或喂食器业务。

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

- `SinglePwmDriver`：单向 PWM 输出，用于喂食器。
- `At8236HBridgeDriver`：双 PWM H 桥，用于自动门。

## 与 MotorCurrentGuard 的关系

EncodedDcMotor 不直接读取电流。它只暴露运动状态和故障停机接口。

应用层负责组合：

```text
业务控制器
  EncodedDcMotor
  MotorCurrentGuard
```

当 MotorCurrentGuard 判定过流时，应用层调用电机的故障停机接口，并记录业务事件。

## 不包含内容

EncodedDcMotor 不包含：

- 开门、关门、投喂语义。
- 电流采样。
- Web/API。
- AT24C 持久化。
- 日志持久化。
- 多电机编排策略。
