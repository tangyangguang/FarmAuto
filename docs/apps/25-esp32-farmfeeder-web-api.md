# Esp32FarmFeeder Web/API 草案

## 目标

本文只定义 Esp32FarmFeeder 三路喂食器应用的页面、API 和维护语义。它不适用于 Esp32FarmDoor。

公共 Web/API 约定见 `docs/apps/13-web-api-and-maintenance.md`。Esp32Base 系统页面边界见 `docs/apps/23-esp32base-web-integration.md`。

## 页面

Esp32FarmFeeder 页面：

- `/`：喂食器总览，显示全局状态、多个每日计划摘要、三路状态、今日累计、桶余量和最近事件。
- `/control`：单路启动/停止、启动全部、停止全部、跳过今日。
- `/schedule`：多个每日计划、每个计划的执行时间、参与通道、每路目标、跳过今日和下次执行预览。
- `/buckets`：每路饲料桶容量、估算余量、补料、低余量阈值。
- `/calibration`：每路下料参数，手工输入每圈下料克数。
- `/records`：喂食器长期业务记录查询和导出。
- `/diagnostics`：喂食器业务诊断包和维护工具，含状态 snapshot、三路电机 snapshot、计划状态、桶余量、AT24C inspect、flash 记录范围、存储检查和故障处理入口。

系统参数使用 `/esp32base/app-config`，系统日志使用 `/esp32base/logs`。

## API

状态与记录：

- `GET /api/app/status`
- `GET /api/app/events/recent`
- `GET /api/app/records`
- `GET /api/app/records/export`，后续增强项，首版不强制实现
- `GET /api/app/diagnostics`

控制：

- `POST /api/app/feeders/{channel}/start`
- `POST /api/app/feeders/{channel}/stop`
- `POST /api/app/feeders/start-all`
- `POST /api/app/feeders/stop-all`

计划：

- `GET /api/app/schedules`
- `POST /api/app/schedules`
- `PUT /api/app/schedules/{planId}`
- `DELETE /api/app/schedules/{planId}`
- `POST /api/app/schedules/{planId}/skip-today`
- `POST /api/app/schedules/{planId}/cancel-skip-today`

投喂目标：

- `GET /api/app/feeders/targets`
- `POST /api/app/feeders/{channel}/target`

饲料桶：

- `GET /api/app/buckets`
- `POST /api/app/buckets/{channel}/set-remaining`
- `POST /api/app/buckets/{channel}/add-feed`
- `POST /api/app/buckets/{channel}/mark-full`

维护：

- `POST /api/app/maintenance/clear-today`
- `POST /api/app/maintenance/calibrate-feed-rate`
- `POST /api/app/maintenance/test-channel`
- `POST /api/app/maintenance/format-app-storage`
- `POST /api/app/maintenance/clear-fault`

## Status Snapshot

`GET /api/app/status` 应只包含喂食器字段：

- 全局状态：`Idle`、`Starting`、`Running`、`Stopping`、`RollingDay`、`Degraded`、`Fault`、`Maintenance`。
- 计划状态：planCount、nextPlanId、nextPlanTimeMinutes、timeValid，以及每个计划的 enabled、timeMinutes、skipToday、scheduleAttemptedToday、todayExecuted、scheduleMissedToday。
- 通道汇总：channelCount、installedChannelMask、enabledChannelMask、requestedChannelMask、runningChannelMask、faultChannelMask、runningCount。
- 通道状态：channel、enabled、installed、motorState、targetMode、targetPulses、targetGramsX100、todayPulses、todayGramsX100、faultReason。
- 饲料桶：capacityGrams、remainGrams、remainPercent、lowWarningPercent、criticalWarningPercent、estimatedFeedCount、estimatedDays。
- 存储：AT24C 在线状态、flash 剩余空间、最近写入错误、记录范围。
- 最近命令：commandId、source、channelMask、startedAt、result。

不得包含自动门位置、端点、限位、开门目标等字段。

## 计划字段

`GET /api/app/schedules` 返回：

```json
{
  "timeValid": true,
  "nextPlanId": 2,
  "nextRunUnixTime": 1778801400,
  "plans": [
    {
      "planId": 1,
      "enabled": true,
      "timeConfigured": true,
      "timeMinutes": 450,
      "channelMask": 7,
      "targets": [
        {"channel": 1, "targetMode": "Grams", "targetGramsX100": 7000},
        {"channel": 2, "targetMode": "Revolutions", "targetRevolutionsX100": 100}
      ],
      "skipToday": false,
      "scheduleAttemptedToday": true,
      "todayExecuted": false,
      "scheduleMissedToday": false
    }
  ]
}
```

计划状态语义：

- `scheduleAttemptedToday`：该计划今日已经开始过。即使运行中断电中断，也保持 true，用于阻止晚些时候来电后该计划再次自动触发。
- `todayExecuted`：该计划今日已成功完成，或所有计划通道都得到明确最终结果；不用于表达断电中断。
- `scheduleMissedToday`：该计划时间已错过且未触发，且不会补投喂。
- 组合规则：`todayExecuted=true` 必须以 `scheduleAttemptedToday=true` 为前提；断电中断时 `scheduleAttemptedToday=true` 且 `todayExecuted=false`；`scheduleMissedToday=true` 不应与 `scheduleAttemptedToday=true` 同时出现。
- 一个计划断电中断，不影响其他尚未到时间的计划按自身状态触发。

`POST /api/app/schedules` 新增计划，`PUT /api/app/schedules/{planId}` 修改计划：

- `enabled`：是否启用每日计划。
- `timeMinutes`：当天 0..1439 分钟；未提供或显式清空表示不配置时间。
- `channelMask`：计划包含哪些通道，只能包含已安装且已启用通道。
- `targets`：该计划每个参与通道的目标，可以是克数或圈数。
- 自动投喂默认无计划。启用计划时必须同时满足：`enabled=true`、`timeMinutes` 已配置、`channelMask` 非 0，且参与通道都有有效投喂目标。

规则：

- 未配置时间时，该计划视为未启用。
- `channelMask=0` 时不执行计划。
- 修改计划不清除长期记录。
- 修改计划应写入 `ConfigChanged` 或计划变更业务事件。
- `skipToday` 是单个计划的当天运行状态，不通过新增/修改计划接口设置，只通过专用 skip/cancel API。

## 投喂目标字段

`GET /api/app/feeders/targets` 返回每路目标：

```json
{
  "channels": [
    {
      "channel": 1,
      "enabled": true,
      "targetMode": "Grams",
      "targetGramsX100": 7000,
      "targetRevolutionsX100": 100,
      "targetPulses": 4320,
      "gramsPerRevX100": 7000,
      "calibrated": true
    }
  ]
}
```

`POST /api/app/feeders/{channel}/target` 可修改：

- `targetMode`：`Grams` 或 `Revolutions`。
- `targetGramsX100`：克数模式目标，单位 0.01g。
- `targetRevolutionsX100`：圈数模式目标，单位 0.01 圈。

规则：

- 切换模式不删除另一个模式的已保存目标值。
- 克数模式要求该通道已完成 `gramsPerRevX100` 标定；未标定返回 `NotConfigured`。
- 最终执行目标必须换算为 `targetPulses`。
- 保存目标时不改变今日计数、桶余量或长期记录正文。

## 饲料桶字段

`GET /api/app/buckets` 返回：

```json
{
  "channels": [
    {
      "channel": 1,
      "capacityGrams": 5000,
      "remainGrams": 3200,
      "remainPercent": 64,
      "lowWarningPercent": 20,
      "criticalWarningPercent": 10,
      "estimatedFeedCount": 45,
      "estimatedDays": 15,
      "lastRefillUnixTime": 1778790000
    }
  ]
}
```

饲料桶写入 API：

- `set-remaining`：直接设置 `remainGrams`，必须二次确认。
- `add-feed`：增加 `addedGrams`，结果不得超过 `capacityGrams`。
- `mark-full`：把 `remainGrams` 设置为 `capacityGrams`。

规则：

- `capacityGrams`、低余量阈值和严重低余量阈值属于饲料桶页面的业务配置，不放 Esp32Base App Config。
- 修改容量时默认不改变当前余量；如需要同步填满，使用 `mark-full`。
- 每次补料、设置余量、投喂扣减和低余量告警都写入长期业务记录。
- 余量不得扣成负数；扣减后小于 0 时显示 0，并记录 underflow。

## 下料参数字段

`POST /api/app/maintenance/calibrate-feed-rate` 输入：

```json
{
  "channel": 1,
  "gramsPerRevX100": 7000,
  "source": "Manual"
}
```

规则：

- 下料参数页只手工输入每圈下料克数，不发起实际运转测试。
- `gramsPerRevX100 <= 0` 时拒绝保存。
- 保存失败不覆盖旧标定值。
- 保存成功后，克数模式可用，并写入 `FeederCalibrationSaved`。

## 运行规则

- 手动投喂和定时投喂按通道独立仲裁，不做全局互斥。
- 同一通道正在运行、启动或停止时，该通道新启动请求返回 `Busy`。
- 其他空闲、已安装、已启用且无故障的通道仍可手动启动。
- 定时计划触发时，只启动当时空闲、已安装、已启用且无故障的计划通道；已运行通道记录 busy skipped，不排队、不补执行。
- 启动全部只启动已启用、已安装、空闲且无故障的通道，按 `startAllIntervalMs` 顺序启动，已启动通道可以并行运行。
- 启动类响应必须返回 successMask、busyMask、faultMask 和 skippedMask，方便页面显示部分成功。
- 普通停止全部同时请求所有运行通道软停止。
- 故障或急停停止全部同时请求所有运行通道急停。
- 单路故障时，该通道停止并记录故障，其他通道继续运行。
- 未配置每日时间时，不自动定时投喂。
- 日期/时间无效时暂停自动定时投喂，但允许手动投喂。
- 错过计划时间不补投喂，只记录 missed 事件。

## 断电恢复

- 投喂运行中断电后，重启不自动续喂、不自动补喂。
- 启动时若发现未完成投喂命令，必须先确保所有电机输出关闭，再记录 `PowerLossAborted`。
- 定时计划被断电中断后，当天标记为已尝试执行但中断，不因晚些时候来电而再次自动触发。
- 手动投喂被断电中断后，页面显示中断结果、已可靠记录的实际脉冲和估算克数；用户可重新发起新的手动投喂。
- 如果最后一段计数不确定，对应通道余量估算标记为低可信，并在桶余量页面提示需要校准。

## 部分成功响应

启动全部、定时计划触发、手动多通道启动都可能部分成功。

推荐 `data`：

```json
{
  "commandId": 123,
  "requestedMask": 7,
  "acceptedMask": 6,
  "successMask": 0,
  "busyMask": 1,
  "faultMask": 0,
  "skippedMask": 1,
  "pendingMask": 6,
  "channelResults": [
    {"channel": 1, "result": "Busy", "reason": "AlreadyRunning"},
    {"channel": 2, "result": "Accepted", "reason": ""},
    {"channel": 3, "result": "Accepted", "reason": ""}
  ]
}
```

语义：

- `requestedMask`：用户或计划请求的通道。
- `acceptedMask`：本次已接受并准备启动的通道。
- `pendingMask`：已接受但尚未完成启动/运行的通道。
- `successMask`：已完成且成功的通道，命令进行中可为 0。
- `busyMask`：请求时已经忙的通道。
- `faultMask`：因故障不可启动的通道。
- `skippedMask`：被跳过的通道，可能由 busy、disabled、not installed、not configured 等原因造成。
- `channelResults`：页面展示的逐通道结果来源。

HTTP 层建议：

- 至少一个通道 accepted 时，整体 `ok=true`，`code=PartialAccepted` 或 `Ok`，由 `channelResults` 展示部分成功。
- 所有请求通道都不可启动时，整体 `ok=false`，`code=Busy` / `Fault` / `NotConfigured`。
- 命令完成后，`FeederBatchCompleted` 记录最终 requested/success/busy/fault/skipped mask。

## 危险操作

以下操作必须二次确认：

- 清空今日计数。
- 跳过今日定时投喂。
- 设置或修正当前饲料桶估算余量。
- 标定每圈下料量。
- 格式化应用业务存储。
- 清除故障后恢复运行。

所有危险操作写入喂食器长期业务记录。

## 首版无电流检测

当前硬件没有电流检测芯片，首版不能依赖电流保护判断堵转。

首版保护依据：

- 编码器无脉冲。
- 最大运行时间。
- 最大运行圈数或最大运行脉冲。
- 启动全部时的顺序启动间隔。

未来如增加电流检测，推荐每个电机对应一个 INA240A2 芯片，三路独立采样、独立阈值、独立故障诊断。
