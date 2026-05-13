# 应用状态机草案

## 目标

本文定义 Esp32FarmDoor 和 Esp32FarmFeeder 的应用级状态机草案，用于后续需求沟通和实现前评审。

公共库状态机只描述单个硬件部件；应用状态机描述设备业务语义、用户操作、故障恢复和断电重启行为。

## Esp32FarmDoor 状态机

建议状态：

```text
Booting
PositionUnknown
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
- `PositionUnknown`：当前位置不可信，需要人工确认或重新归零。
- `IdleClosed`：门处于已关闭位置。
- `IdleOpen`：门处于已打开位置。
- `IdlePartial`：门停在中间位置。
- `Opening`：正在开门。
- `Closing`：正在关门。
- `SoftStopping`：正在按策略软停止。
- `Stopped`：用户主动停止后的稳定状态。
- `Homing`：归零流程执行中。
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

- 是否存在物理上/下限位开关。
- 断电重启后保存的位置是否默认可信。
- 归零是“人工确认当前位置为零”，还是“电机运行到机械零点”。
- 用户停止后再次开/关门的目标策略。
- 故障清除后是否回到 `PositionUnknown`。

## Esp32FarmFeeder 状态机

建议设备级状态：

```text
Booting
Idle
SingleRunning
AllStarting
MultiRunning
StoppingAll
HistoryRolling
FaultPartial
FaultAll
Maintenance
```

每路电机状态仍由 Esp32EncodedDcMotor 管理；应用状态机负责三路组合语义。

状态说明：

- `Booting`：启动中，加载配置、今日计数和历史。
- `Idle`：三路均空闲。
- `SingleRunning`：单路运行中。
- `AllStarting`：按顺序启动全部。
- `MultiRunning`：多路运行中。
- `StoppingAll`：停止全部流程中。
- `HistoryRolling`：日期变化，正在归档昨日数据并清零今日计数。
- `FaultPartial`：部分通道故障，其他通道策略待确认。
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
FaultCleared
ConfigChanged
```

首版保护依据：

- 编码器无脉冲堵转检测。
- 最大运行时间。
- 最大运行脉冲或最大运行圈数。
- 启动全部时的顺序启动间隔。

当前硬件没有电流检测芯片。未来如增加电流检测，推荐每个电机对应一个 INA240 芯片。

待确认问题：

- 是否允许三路同时运行，还是启动全部后也需要串行运行。
- 单路故障时，其他正在运行通道是否继续。
- 停止全部是立即停全部，还是顺序停止。
- 日期来源失败时是否允许喂食。
- 清空当天计数是否只清统计，不影响配置和历史。
