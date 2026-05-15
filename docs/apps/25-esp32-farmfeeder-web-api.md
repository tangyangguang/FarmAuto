# Esp32FarmFeeder Web/API 草案

## 目标

本文只定义 Esp32FarmFeeder 三路喂食器应用的页面、API 和维护语义。它不适用于 Esp32FarmDoor。

公共 Web/API 约定见 `docs/apps/13-web-api-and-maintenance.md`。Esp32Base 系统页面边界见 `docs/apps/23-esp32base-web-integration.md`。

## 页面

Esp32FarmFeeder 页面：

- `/`：喂食器总览，显示全局状态、每日计划、三路状态、今日累计、桶余量和最近事件。
- `/control`：单路启动/停止、启动全部、停止全部、跳过今日。
- `/schedule`：每日计划、执行时间、启用状态、跳过今日和下次执行预览。
- `/buckets`：每路饲料桶容量、估算余量、补料、低余量阈值。
- `/calibration`：每路下料标定、单路小剂量测试。
- `/maintenance`：清空今日计数、清除故障、存储检查、格式化应用业务存储。
- `/records`：喂食器长期业务记录查询和导出。
- `/diagnostics`：喂食器业务诊断包，含状态 snapshot、三路电机 snapshot、计划状态、桶余量、AT24C inspect、flash 记录范围。

系统参数使用 `/esp32base/app-config`，系统日志使用 `/esp32base/logs`。

## API

状态与记录：

- `GET /api/app/status`
- `GET /api/app/events/recent`
- `GET /api/app/records`
- `GET /api/app/records/export`
- `GET /api/app/diagnostics`

控制：

- `POST /api/app/feeders/{channel}/start`
- `POST /api/app/feeders/{channel}/stop`
- `POST /api/app/feeders/start-all`
- `POST /api/app/feeders/stop-all`

计划：

- `GET /api/app/schedule`
- `POST /api/app/schedule`
- `POST /api/app/schedule/skip-today`
- `POST /api/app/schedule/cancel-skip-today`

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
- 计划状态：scheduleEnabled、scheduleTimeConfigured、scheduleTimeMinutes、timeValid、skipToday、todayExecuted、scheduleMissedToday。
- 通道汇总：channelCount、installedChannelMask、enabledChannelMask、requestedChannelMask、runningChannelMask、faultChannelMask、runningCount。
- 通道状态：channel、enabled、installed、motorState、targetMode、targetPulses、targetGramsX100、todayPulses、todayGramsX100、faultReason。
- 饲料桶：capacityGrams、remainGrams、remainPercent、lowWarningPercent、criticalWarningPercent、estimatedFeedCount、estimatedDays。
- 存储：AT24C 在线状态、flash 剩余空间、最近写入错误、记录范围。
- 最近命令：commandId、source、channelMask、startedAt、result。

不得包含自动门位置、端点、限位、开门目标等字段。

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
