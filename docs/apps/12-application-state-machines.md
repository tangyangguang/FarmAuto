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
Maintenance
Fault
```

状态说明：

- `Booting`：启动中，加载配置、状态和硬件自检。
- `PositionUnknown`：当前位置不可信；第一版允许远程按指定圈数试运行、标定当前位置和重新保存端点；下一阶段有上限位后优先允许远程低速开门直到触发上限位完成端点校准。
- `IdleClosed`：门处于已关闭位置。
- `IdleOpen`：门处于已打开位置。
- `IdlePartial`：门停在中间位置。
- `Opening`：正在开门。
- `Closing`：正在关门。
- `Stopping`：正在按停止策略停机。普通停止可软停止，故障停止可直接按设备配置 `Coast` 或 `Brake` 输出。
- `Maintenance`：维护模式，允许端点校准、格式化、校准等危险操作。端点示教、端点验证、下一阶段上限位寻零都作为 `activeFlow` 表达，不再提升为长期主状态。
- `Fault`：故障停机。限位断线、异常方向触发、上下限位冲突等下一阶段限位问题，都进入 `Fault`，通过 `faultReason=LimitFault/...` 区分；不单独设置长期业务主状态。

第一版无限位状态规则：

- `PositionUnknown` 下禁止普通 `OpenRequested` / `CloseRequested`。
- `PositionUnknown` 只允许进入 `Maintenance`，再通过 `activeFlow=EndpointTeaching` 或下一阶段 `activeFlow=LimitHoming` 处理。
- 端点维护完成后，如果 `openTargetPulses`、`maxRunPulses`、`maxCloseUnwindPulses` 都有效，才允许回到 `IdleClosed` / `IdleOpen` / `IdlePartial`。
- 维护模式支持直接设置开关门行程圈数或脉冲数，并支持小步微调。直接设置后建议保持 `Maintenance` 主状态，并设置 `activeFlow=EndpointVerifying` 执行端点验证；若跳过验证，位置来源应标记为低可信。
- 运行中断电后重启，应优先通过 motion journal 和最近位置检查点恢复到 `IdlePartial` 或可判断的端点状态；只有记录无效、越界或与限位冲突时才进入 `PositionUnknown`。
- 可信断电恢复不要求用户确认；恢复后的首次动作由系统自动套用保守速度和剩余距离限制。低可信恢复才要求远程确认或维护处理。
- 用户主动停止完成后，如果位置可信但不在端点，进入 `IdlePartial`，并通过 `lastStopReason=UserStop` 表达“用户主动停止”。不要再引入单独的 `Stopped` 主状态，避免和 `IdlePartial` 语义重叠。
- 再次开门时目标仍为开门目标，再次关门时目标为关闭点；不从当前位置重新定义端点。
- 第一版无限位端点示教需要远程视频、现场观察或明确机械标记辅助判断；如果无法确认位置，系统应保持 `PositionUnknown`，不开放普通开关门。

关键事件：

```text
BootCompleted
OpenRequested
CloseRequested
StopRequested
MaintenanceFlowRequested
TravelSetRequested
TravelAdjusted
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
- 第一版不使用限位开关时，远程端点示教、直接设置行程并验证，是退出 `PositionUnknown` 的正常路径。

## Esp32FarmFeeder 状态机

建议设备级状态：

```text
Booting
Idle
Starting
Running
Stopping
RollingDay
Degraded
Fault
Maintenance
```

每路电机状态仍由 `Esp32EncodedDcMotor` 管理；应用状态机只描述设备级调度语义，不把运行路数编码进状态名。

核心设计：

- 设备主状态不包含固定通道数量。
- 当前运行了几路，由 `runningChannelMask`、`runningCount` 和每路 channel state 表达。
- 哪些通道实际安装，由 `installedChannelMask` 表达。
- 哪些通道启用，由 `enabledChannelMask` 表达；未启用通道不参与运行，也不构成降级。
- 哪些通道参与当前批次，由 `requestedChannelMask` 表达。
- 哪些通道故障，由 `faultChannelMask` 表达。
- 首版硬件是 3 路，但状态模型应能自然支持只启用 1 路、只接 2 路，或未来扩展为 4 路。

通道状态建议：

```text
Disabled
Idle
Starting
Running
Stopping
Completed
Fault
Maintenance
```

每日定时投喂和手动投喂按通道独立仲裁，不做全局互斥。

通道级并发规则：

- 同一通道正在运行、启动或停止时，该通道新的手动或定时启动请求返回 `Busy`。
- 其他空闲、已安装、已启用且无故障的通道仍可手动启动。
- 定时计划触发时，只启动当时空闲、已安装、已启用且无故障的计划通道。
- 定时计划中已经运行的通道不排队、不补执行，应记录该通道本次计划 `BusySkipped` 或等价结果。
- 手动启动多个通道或启动全部时，已忙通道跳过，空闲可用通道可以继续启动；响应中必须返回 successMask、busyMask、faultMask 和 skippedMask。
- 如果请求的所有通道都不可启动，整体返回 `Busy`、`NotConfigured` 或 `Fault`，具体按原因决定。

断电恢复规则：

- 投喂运行中断电后，重启不自动续喂，不自动补喂，也不排队继续未完成通道。
- 启动时若发现上次存在未完成投喂命令，应先确保所有电机输出关闭，再把相关通道结果记录为 `PowerLossAborted`。
- 如果被中断的是定时计划，该日计划标记为已尝试执行但中断，不因来电时间晚于计划时间而再次触发。
- 如果被中断的是手动投喂，只显示中断结果和实际已完成计数，用户需要重新发起新的手动投喂。
- 今日累计和饲料桶余量只按已可靠提交的实际脉冲或圈数更新；存在不确定计数时，对应通道标记 `estimateConfidence=Low`。

`skipToday`、`timeValid`、`planEnabled`、`todayExecuted`、`scheduleAttemptedToday` 是单计划状态标志，不作为设备主状态。原因是它们可能与 `Idle`、`Running`、`Degraded` 同时存在；如果做成主状态，会掩盖真实运行状态。

状态说明：

- `Booting`：启动中，加载配置、今日计数和历史。
- `Idle`：所有启用通道均空闲，或未启用任何通道。
- `Starting`：正在按启动间隔启动一个或多个通道。
- `Running`：至少一个启用通道正在运行。具体运行数量由 `runningCount` 表达。
- `Stopping`：正在停止一个或多个通道，包括停止全部。
- `RollingDay`：日期变化，正在归档昨日数据并清零今日计数。
- `Degraded`：部分“已启用且应可用”的通道故障、缺失或不可运行，但仍有至少一个启用通道可运行。用户主动禁用的通道不算降级。
- `Fault`：系统级故障，或所有启用通道均不可运行。
- `Maintenance`：维护模式，允许清空计数、校准、格式化等危险操作。

状态优先级：

1. `Booting`
2. `Maintenance`
3. `Fault`
4. `RollingDay`
5. `Stopping`
6. `Starting`
7. `Running`
8. `Degraded`
9. `Idle`

示例：

- 只启用 1 路且该路运行：主状态 `Running`，`runningCount=1`。
- 只接了 2 路且只启用这两路：空闲时主状态 `Idle`，运行时主状态 `Running`，`installedChannelMask=0b011`，`enabledChannelMask=0b011`，`runningCount=2`。
- 未来扩展为 4 路且 3 路运行：主状态仍为 `Running`，`runningCount=3`。
- 3 路中 1 路故障、2 路可运行且当前空闲：主状态 `Degraded`。
- 3 路中 1 路故障、其他 2 路正在运行：主状态 `Running`，同时 snapshot 中 `faultChannelMask` 非 0，页面显示降级告警。

计划状态标志：

喂食器支持一天多个自动计划。以下标志按单个计划保存和展示，不能再按“全局一天一次”理解。

| 标志 | 说明 |
| --- | --- |
| `planEnabled` | 该计划已启用 |
| `planTimeConfigured` | 该计划已配置每日执行时间；未配置时不自动投喂 |
| `timeValid` | 当前日期/时间可信；不可信时暂停自动定时 |
| `skipToday` | 该计划今日已跳过；日期切换后自动清除 |
| `todayExecuted` | 该计划今日已成功完成，或所有计划通道都得到明确最终结果；不用于表达断电中断 |
| `scheduleAttemptedToday` | 该计划今日已经开始过；即使被断电中断，也不自动再次触发 |
| `scheduleMissedToday` | 该计划今日错过且不补投喂，仅记录事件 |

计划标志组合规则：

首版默认没有自动投喂计划；新增计划并启用前，必须配置执行时间、参与通道和每个参与通道的投喂目标。

1. `skipToday=true`：今日自动计划不触发，手动投喂不受影响。
2. `scheduleAttemptedToday=true`：该计划今日已经启动过；无论成功、部分成功还是断电中断，该计划都不再次自动触发。
3. `todayExecuted=true` 必须以 `scheduleAttemptedToday=true` 为前提；断电中断时 `todayExecuted=false`。
4. `scheduleMissedToday=true` 表示计划时间已错过且未触发；它不应与 `scheduleAttemptedToday=true` 同时出现。
5. 一个计划断电中断，不影响其他尚未到时间的计划按自身状态触发。

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
ScheduleChannelSkipped
FeederPowerLossAborted
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
- 时间无效时暂停自动定时投喂，但允许手动投喂；手动投喂受通道级 Busy、故障和维护状态限制。
