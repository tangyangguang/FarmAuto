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
Stopping
Stopped
Maintenance
EndpointTeaching
EndpointVerifying
LimitHoming
Fault
```

状态说明：

- `Booting`：启动中，加载配置、状态和硬件自检。
- `PositionUnknown`：当前位置不可信；第一版允许远程低速点动、设置当前位置和重新保存端点；下一阶段有上限位后优先允许远程低速开门直到触发上限位完成端点校准。
- `IdleClosed`：门处于已关闭位置。
- `IdleOpen`：门处于已打开位置。
- `IdlePartial`：门停在中间位置。
- `Opening`：正在开门。
- `Closing`：正在关门。
- `Stopping`：正在按停止策略停机。普通停止可软停止，故障停止可直接按设备配置 `Coast` 或 `Brake` 输出。
- `Stopped`：用户主动停止后的稳定状态。
- `Maintenance`：维护模式，允许端点校准、格式化、校准等危险操作。
- `EndpointTeaching`：第一版无限位端点示教中，通过低速点动、设置关闭点、保存开门目标建立位置基准。
- `EndpointVerifying`：端点低速验证中，验证关闭点、开门目标、安全上限和保护参数。
- `LimitHoming`：下一阶段启用开门/上限位后，低速寻找上限位并建立开门端点。
- `Fault`：故障停机。限位断线、异常方向触发、上下限位冲突等下一阶段限位问题，都进入 `Fault`，通过 `faultReason=LimitFault/...` 区分；不单独设置长期业务主状态。

第一版无限位状态规则：

- `PositionUnknown` 下禁止普通 `OpenRequested` / `CloseRequested`。
- `PositionUnknown` 只允许进入 `Maintenance`，再进入 `EndpointTeaching` 或下一阶段 `LimitHoming`。
- 端点维护完成后，如果 `openTargetPulses`、`maxRunPulses`、`maxCloseUnwindPulses` 都有效，才允许回到 `IdleClosed` / `IdleOpen` / `IdlePartial`。
- 运行中断电后重启，除非能证明上次已经稳定停止并成功保存位置，否则进入 `PositionUnknown`。
- 用户主动停止后进入 `Stopped`，再次开门时目标仍为开门目标，再次关门时目标为关闭点；不从当前位置重新定义端点。
- `Stopped` 不是故障状态。它表达“用户主动停止后位置可信但不一定在端点”，通常可转入 `IdlePartial` 或继续执行用户下一条开/关命令。
- 第一版无限位端点示教需要远程视频、现场观察或明确机械标记辅助判断；如果无法确认位置，系统应保持 `PositionUnknown`，不开放普通开关门。

关键事件：

```text
BootCompleted
OpenRequested
CloseRequested
StopRequested
EndpointTeachingRequested
EndpointVerifyRequested
LimitHomingRequested
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

已确认规则：

- 故障清除后默认根据位置可信度恢复：可信则回到对应 Idle 状态，不可信则回到 `PositionUnknown`。
- 开门/上限位作为下一阶段优先增强；关门/下限位作为可选增强。
- 第一版不使用限位开关时，远程端点示教和低速验证是退出 `PositionUnknown` 的唯一正常路径。

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

`skipToday`、`timeValid`、`scheduleEnabled`、`todayExecuted` 是计划状态标志，不作为设备主状态。原因是它们可能与 `Idle`、`RunningOne`、`FaultPartial` 同时存在；如果做成主状态，会掩盖真实运行状态。

状态说明：

- `Booting`：启动中，加载配置、今日计数和历史。
- `Idle`：三路均空闲。
- `RunningOne`：仅 1 路运行中。
- `RunningTwo`：2 路运行中。
- `RunningThree`：3 路运行中。
- `AllStarting`：按顺序启动全部。
- `StoppingAll`：停止全部流程中。
- `HistoryRolling`：日期变化，正在归档昨日数据并清零今日计数。
- `FaultPartial`：部分通道故障，其他通道继续运行或保持可运行。
- `FaultAll`：系统级故障，全部停止。
- `Maintenance`：维护模式，允许清空计数、校准、格式化等危险操作。

计划状态标志：

| 标志 | 说明 |
| --- | --- |
| `scheduleEnabled` | 每日计划已启用 |
| `scheduleTimeConfigured` | 已配置每日执行时间；未配置时不自动投喂 |
| `timeValid` | 当前日期/时间可信；不可信时暂停自动定时 |
| `skipToday` | 今日定时已跳过；日期切换后自动清除 |
| `todayExecuted` | 今日计划已执行 |
| `scheduleMissedToday` | 今日计划错过且不补投喂，仅记录事件 |

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
ScheduleMissed
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

已确认规则：

- 清空当天计数只清当天统计和当前状态摘要，不删除配置、不删除长期原始记录；必须二次确认并写入事件。
- 跳过今日只影响自动定时投喂，不影响手动投喂。
- 时间无效时暂停自动定时投喂，但允许手动投喂；手动投喂仍受 Busy 和维护状态限制。
