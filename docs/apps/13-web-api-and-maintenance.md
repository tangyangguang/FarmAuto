# Web/API 与维护功能草案

## 目标

本文定义两个应用的本地 Web 页面和业务 API 草案，用于后续评估 Esp32Base Web 能力、路由数量、handler 耗时和危险操作安全性。

首版 Web/API 必须保持简单，同步 handler 不做长耗时操作。危险操作必须二次确认。

设备按无人值守运行设计。Web/API 应优先支持远程查看、远程恢复和远程诊断；只有系统能明确判断继续远程操作有机械风险时，才返回需要现场处理的故障码。

页面原型和信息架构见 `docs/apps/19-web-page-prototypes.md`。页面工作流细化见 `docs/apps/21-web-workflows.md`。Esp32Base Web 集成边界见 `docs/apps/23-esp32base-web-integration.md`。

## Esp32Base 与应用边界

Esp32Base 已提供系统页面和系统 API：

- `/esp32base`：系统状态。
- `/esp32base/logs`：系统日志。
- `/esp32base/app-config`：低频系统参数和硬件参数配置。
- `/esp32base/tools`、`/esp32base/wifi`、`/esp32base/ota`、`/esp32base/auth`：系统维护能力。
- `/esp32base/api/*`：系统 API。

FarmAuto 应用页面和 API 不复用这些路径表达业务语义：

- 应用业务 API 统一使用 `/api/app/*`。
- 应用业务记录页面使用 `/records`，不使用 `/logs`。
- 应用业务诊断页面使用 `/diagnostics`。
- 业务最近事件使用 `/api/app/events/recent`。
- 业务长期记录使用 `/api/app/records`。

系统日志和业务记录的区别：

- Esp32Base 系统日志用于 WiFi、OTA、启动、文件系统、route 错误和运行时调试。
- FarmAuto 业务记录用于开关门、投喂、维护、故障、配置变化、补料和存储告警。
- 业务长期记录不写入 `/logs/eb_app.log`，也不通过 `/esp32base/logs` 查询。

## 通用页面

通用应用页面：

- `/`：应用总览页。
- `/control`：控制页。
- `/maintenance`：维护页。
- `/records`：业务记录页。
- `/diagnostics`：业务诊断页。

Esp32FarmFeeder 额外页面：

- `/schedule`：投喂计划页。
- `/buckets`：饲料桶页。
- `/calibration`：下料标定页。

配置页面原则：

- 普通系统参数优先使用 Esp32Base App Config 内置页面 `/esp32base/app-config`。
- 应用自定义页面只负责状态、控制、维护动作、业务记录查询和业务诊断展示。
- 不建议新增应用 `/config` 页面；业务配置按领域命名，例如 `/schedule`、`/buckets`、`/calibration`。
- 如果 App Config 的字段容量、页面组织或校验能力不足，不在 FarmAuto 中临时绕开，应整理提示词到 Esp32Base 项目处理。

## 通用 API 约定

建议返回 JSON：

```json
{
  "ok": true,
  "code": "Ok",
  "message": "",
  "data": {}
}
```

错误码建议：

```text
Ok
Busy
InvalidState
InvalidArgument
NotConfigured
HardwareFault
StorageError
ConfirmRequired
Forbidden
LimitConflict
UnexpectedLimit
LimitNotReached
OnsiteRequired
```

handler 原则：

- 不使用长阻塞 delay。
- 不在请求中执行长时间运动流程，只发起命令。
- 保存或变更参数前先校验范围。
- 危险操作需要确认 token 或二次确认参数。

通用应用 API：

- `GET /api/app/status`
- `GET /api/app/events/recent`
- `GET /api/app/records`
- `GET /api/app/records/export`
- `GET /api/app/diagnostics`

## Esp32FarmDoor API 草案

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

危险操作：

- 端点校准。
- 设置当前位置。
- INA240A2 零点校准。
- 格式化应用业务存储。
- 清除故障后恢复运行。

`GET /api/app/status` 应包含是否启用开门/上限位、可选关门/下限位、当前位置可信度、最近一次端点维护结果、最近故障和是否需要现场处理。

第一版不要求限位开关：

- `POST /api/app/maintenance/jog`：远程低速点动，只允许短时运行，必须限制最大时长和最大脉冲。
- `POST /api/app/maintenance/set-position`：设置当前位置，只在电机停止且二次确认后允许。
- `POST /api/app/maintenance/save-endpoints`：保存开门/关门端点目标，必须二次确认。
- `POST /api/app/maintenance/verify-endpoints`：发起一次低速验证流程，验证开门目标、关门目标和安全上限。

推荐二次确认方式：

- 请求体包含 `confirm=true` 和页面生成的短期 `confirmToken`。
- 危险操作的响应必须返回 commandId，页面继续轮询状态。
- `jog` 每次只允许短动作，不允许“一直按住一直走”的长阻塞 HTTP 请求。
- `set-position`、`save-endpoints`、`format-app-storage`、`clear-fault` 都记录长期业务事件。

下一阶段启用开门/上限位后：

- `POST /api/app/maintenance/calibrate-open-limit`：远程低速开门直到触发开门/上限位。API 只发起校准流程，不在 HTTP handler 内阻塞等待完成。

已确认与后续项：

- 下一阶段启用限位后，`calibrate-open-limit` 和 `set-position` 是否都保留；建议前者为运行到开门/上限位，后者仅保留为受限维护功能。
- 单独的微调上/下命令首版不做成普通控制按钮；维护页用短时 `jog` 承载。
- 需要导出业务诊断信息；系统日志仍通过 `/esp32base/logs` 查看。

## Esp32FarmFeeder API 草案

控制：

- `POST /api/app/feeders/{channel}/start`
- `POST /api/app/feeders/{channel}/stop`
- `POST /api/app/feeders/start-all`
- `POST /api/app/feeders/stop-all`
- `POST /api/app/schedule/skip-today`
- `POST /api/app/schedule/cancel-skip-today`

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

危险操作：

- 清空当天计数。
- 标定每圈下料量。
- 格式化应用业务存储。
- 跳过今日定时投喂。
- 设置或修正当前饲料桶估算余量。

已确认规则：

- 克数模式和圈数模式在页面上使用每路独立 segmented control；切换模式不删除另一个模式的已保存目标值。
- 需要一键测试单路固定小剂量，用于方向、编码器和下料验证；必须限制最大运行时间和最大脉冲。
- 长期原始记录使用分页查询，导出 JSON Lines 和 CSV。
- 系统日志仍通过 `/esp32base/logs` 查看，业务记录通过 `/records` 和 `/api/app/records` 查看。

## 路由预算

Esp32Base Web 能力进入实现前需要确认：

- 可注册 route 数量。
- 同步 handler 建议耗时。
- Web Auth 首版默认启用；如果只在封闭内网临时调试，可由构建配置关闭，但正式设备不建议关闭。
- 静态页面放置方式。
- JSON 解析和响应大小限制。
- App Config groups/fields 容量。

如果 route 数量受限，应合并为少量资源式 API，例如：

- `GET /api/app/status`
- `POST /api/app/command`
- `POST /api/app/maintenance`
- `GET /api/app/records`
- `GET /api/app/diagnostics`

即使源码阶段合并 handler，文档层仍应保留清晰的业务命令名、payload schema 和权限/确认规则。
