# Esp32FarmDoor Web/API 草案

## 目标

本文只定义 Esp32FarmDoor 自动门应用的页面、API 和维护语义。它不适用于 Esp32FarmFeeder。

公共 Web/API 约定见 `docs/apps/13-web-api-and-maintenance.md`。Esp32Base 系统页面边界见 `docs/apps/23-esp32base-web-integration.md`。

## 页面

Esp32FarmDoor 页面：

- `/`：自动门总览，显示门状态、位置、端点、电机、电流、存储和最近事件。
- `/control`：开门、关门、停止、清除故障。
- `/maintenance`：无限位端点示教、低速点动、端点验证、INA240A2 零点校准、存储检查。
- `/records`：自动门长期业务记录查询和导出。
- `/diagnostics`：自动门业务诊断包，含状态 snapshot、端点快照、最近事件、AT24C inspect、flash 记录范围、电流 trace 摘要。

系统参数使用 `/esp32base/app-config`，系统日志使用 `/esp32base/logs`。

## API

状态与记录：

- `GET /api/app/status`
- `GET /api/app/events/recent`
- `GET /api/app/records`
- `GET /api/app/records/export`
- `GET /api/app/diagnostics`

控制：

- `POST /api/app/door/open`
- `POST /api/app/door/close`
- `POST /api/app/door/stop`

维护：

- `POST /api/app/maintenance/jog`
- `POST /api/app/maintenance/set-position`
- `POST /api/app/maintenance/save-endpoints`
- `POST /api/app/maintenance/verify-endpoints`
- `POST /api/app/maintenance/calibrate-open-limit`
- `POST /api/app/maintenance/calibrate-current-zero`
- `POST /api/app/maintenance/format-app-storage`
- `POST /api/app/maintenance/clear-fault`

## Status Snapshot

`GET /api/app/status` 应只包含自动门字段：

- 应用状态：`PositionUnknown`、`IdleClosed`、`IdleOpen`、`IdlePartial`、`Opening`、`Closing`、`Stopping`、`Stopped`、`Maintenance`、`EndpointTeaching`、`EndpointVerifying`、`LimitHoming`、`Fault`。
- 当前位置：positionPulses、positionPercent、positionTrusted、positionSource、lastPositionSavedAt。
- 端点：closePositionPulses、openTargetPulses、maxRunPulses、maxCloseUnwindPulses。
- 限位：第一版显示禁用；下一阶段显示 openLimit/closeLimit 的启用、触发、断线和冲突状态。
- 电机：state、speedPps、outputPercent、remainingPulses、faultReason。
- 电流：enabled、currentMa、filteredMa、thresholdMa、guardState、faultReason。
- 存储：AT24C 在线状态、flash 剩余空间、最近写入错误、记录范围。
- 最近命令：commandId、command、source、startedAt、result。

不得包含喂食器通道、饲料桶、每日计划、今日投喂等字段。

## 危险操作

以下操作必须二次确认：

- 设置当前位置。
- 保存端点。
- 低速端点验证。
- 下一阶段上限位校准。
- INA240A2 零点校准。
- 格式化应用业务存储。
- 清除故障后恢复运行。

二次确认建议：

- 请求体包含 `confirm=true` 和短期 `confirmToken`。
- 响应返回 commandId。
- 页面轮询 `GET /api/app/status` 展示进度。
- 所有危险操作写入自动门长期业务记录。

## 第一版无限位维护

第一版不安装限位开关时：

- `PositionUnknown` 下禁止普通开门和关门。
- 只允许进入维护页执行低速点动、设置关闭点、保存开门目标和低速端点验证。
- `jog` 单次动作必须限制最大时长和最大脉冲。
- 端点验证成功前，不允许退出 `PositionUnknown` 进入普通控制。
- 没有远程视频、现场观察或机械标记时，不建议远程重新示教端点。

## 下一阶段限位增强

启用开门/上限位后：

- `POST /api/app/maintenance/calibrate-open-limit` 只发起低速寻限位流程，不在 HTTP handler 内等待完成。
- 触发上限位后立即按到位策略停机，保存开门端点。
- 达到最大时长或最大脉冲仍未触发上限位时，进入故障。
- 限位断线、异常方向触发、上下限位冲突都进入 `Fault`，通过 faultReason 区分。
