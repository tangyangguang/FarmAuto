# 应用状态机草案

## 目标

本文定义 Esp32FarmDoor 和 Esp32FarmFeeder 的应用级状态机草案，用于后续需求沟通和实现前评审。

公共库状态机只描述单个硬件部件；应用状态机描述设备业务语义、用户操作、故障恢复和断电重启行为。

## Esp32FarmDoor 状态机

建议状态：

```text
Booting
PositionUnknown
LimitFault
IdleClosed
IdleOpen
IdlePartial
Opening
Closing
SoftStopping
Stopped
Homing
Calibrating
Fault
Maintenance
```

状态说明：

- `Booting`：启动中，加载配置、状态和硬件自检。
- `PositionUnknown`：当前位置不可信，应优先允许远程低速关门，直到触发关门/下限位。
- `LimitFault`：关门/下限位断线、异常方向触发、限位未按预期触发，或可选上限位与下限位冲突。
- `IdleClosed`：门处于已关闭位置。
- `IdleOpen`：门处于已打开位置。
- `IdlePartial`：门停在中间位置。
- `Opening`：正在开门。
- `Closing`：正在关门。
- `SoftStopping`：正在按策略软停止。
- `Stopped`：用户主动停止后的稳定状态。
- `Homing`：低速寻找关门/下限位流程执行中。
- `Calibrating`：校准流程执行中。
- `Fault`：故障停机。
- `Maintenance`：维护模式，允许归零、格式化、校准等危险操作。

关键事件：

```text
BootCompleted
OpenRequested
CloseRequested
StopRequested
HomeRequested
CalibrationRequested
TargetReached
OpenLimitTriggered
CloseLimitTriggered
LimitConflictDetected
UnexpectedLimitTriggered
MotorStopped
OverCurrent
EncoderNoPulse
MaxRunTimeExceeded
MaxRunPulsesExceeded
PositionSaved
FaultCleared
ConfigChanged
```

待确认问题：

- 关门/下限位开关已确认为首版硬件必选项。
- 开门/上限位开关作为可选增强，不作为首版必需项。
- 断电重启后，已成功提交且与限位不冲突的保存位置应作为可信恢复依据。
- 归零建议定义为“电机低速关门，直到触发关门/下限位”，不依赖现场人工确认。
- 用户停止后再次开/关门的目标策略。
- 故障清除后是否回到 `PositionUnknown`，还是根据限位状态直接回到 `IdleClosed` / `IdleOpen`。

## Esp32FarmFeeder 状态机

建议设备级状态：

```text
Booting
Idle
RunningOne
RunningTwo
RunningThree
AllStarting
StoppingAll
HistoryRolling
ScheduledSkipped
FaultPartial
FaultAll
Maintenance
```

每路电机状态仍由 `Esp32EncodedDcMotor` 管理；应用状态机负责三路组合语义。

组合状态分类规则：

- 运行通道数为 0 且无故障：`Idle`。
- 运行通道数为 1：`RunningOne`。
- 运行通道数为 2：`RunningTwo`。
- 运行通道数为 3：`RunningThree`。
- 正在按间隔启动多个通道：`AllStarting`。
- 正在按策略停止多个通道：`StoppingAll`。
- 仅部分通道故障且仍有通道可运行：`FaultPartial`。
- 全部通道故障或系统级故障：`FaultAll`。

这种分类避免把“多路运行”含糊成一个状态，远程页面可以直接显示当前有几路在工作。

每日定时投喂和手动投喂互相独立，但不能同时发起。只要任意通道正在运行，新的手动或定时启动请求应返回 `Busy`；首版不排队、不自动补执行。

状态说明：

- `Booting`：启动中，加载配置、今日计数和历史。
- `Idle`：三路均空闲。
- `RunningOne`：仅 1 路运行中。
- `RunningTwo`：2 路运行中。
- `RunningThree`：3 路运行中。
- `AllStarting`：按顺序启动全部。
- `StoppingAll`：停止全部流程中。
- `HistoryRolling`：日期变化，正在归档昨日数据并清零今日计数。
- `ScheduledSkipped`：今日定时投喂已跳过，等待日期切换或用户取消跳过。
- `FaultPartial`：部分通道故障，其他通道继续运行或保持可运行。
- `FaultAll`：系统级故障，全部停止。
- `Maintenance`：维护模式，允许清空计数、校准、格式化等危险操作。

关键事件：

```text
StartChannelRequested
StopChannelRequested
StartAllRequested
StopAllRequested
ChannelTargetReached
ChannelFault
AllStopped
DateChanged
HistorySaved
TodayCountersCleared
DailyScheduleTriggered
SkipTodayRequested
SkipTodayCleared
FaultCleared
ConfigChanged
```

首版保护依据：

- 编码器无脉冲堵转检测。
- 最大运行时间。
- 最大运行脉冲或最大运行圈数。
- 启动全部时的顺序启动间隔。
- 单路故障时其他通道继续。

当前硬件没有电流检测芯片。未来如增加电流检测，推荐每个电机对应一个 INA240A2 芯片。

待确认问题：

- 清空当天计数是否只清统计，不影响配置和历史。
