# Web/API 公共约定

## 目标

本文只定义两个应用共同遵守的 Web/API 规则，不定义具体业务接口。

两个应用项目是独立固件、独立 PCB、独立页面和独立 API 实现：

- Esp32FarmDoor 的页面和 API 见 `docs/apps/24-esp32-farmdoor-web-api.md`。
- Esp32FarmFeeder 的页面和 API 见 `docs/apps/25-esp32-farmfeeder-web-api.md`。

源码阶段也必须分开：

- `apps/Esp32FarmDoor/` 只实现自动门页面、API、业务状态和记录。
- `apps/Esp32FarmFeeder/` 只实现喂食器页面、API、业务状态和记录。

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

说明：`/api/app/*` 在两个应用里可以同名，因为它们运行在不同固件和不同设备上，不会部署到同一个 WebServer。文档按应用拆分，避免误解为共享业务 API。

## 系统日志与业务记录

- Esp32Base 系统日志用于 WiFi、OTA、启动、文件系统、route 错误和运行时调试。
- FarmAuto 业务记录用于自动门、喂食器各自的动作、维护、故障、配置变化和业务状态变化。
- 业务长期记录不写入 `/logs/eb_app.log`，也不通过 `/esp32base/logs` 查询。
- 应用诊断页可以链接到 `/esp32base/logs`，但不能把系统日志复制成业务记录。

## 通用页面规则

每个应用可以使用相同的基础页面路径，因为每个设备只运行一个应用：

- `/`：应用总览页。
- `/control`：控制页。
- `/maintenance`：维护页。
- `/records`：业务记录页。
- `/diagnostics`：业务诊断页。

配置页面原则：

- 普通系统参数优先使用 Esp32Base App Config 内置页面 `/esp32base/app-config`。
- 应用自定义页面只负责状态、控制、维护动作、业务记录查询和业务诊断展示。
- 不建议新增应用 `/config` 页面。
- 业务配置按领域命名，例如喂食器的 `/schedule`、`/buckets`、`/calibration`。
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
- 命令类 API 返回 commandId 或当前业务状态，由页面轮询进度。

通用应用 API 语义：

- `GET /api/app/status`：当前应用业务状态 snapshot。
- `GET /api/app/events/recent`：最近业务事件。
- `GET /api/app/records`：长期业务记录分页查询。
- `GET /api/app/records/export`：长期业务记录导出。
- `GET /api/app/diagnostics`：业务诊断包摘要。

这些 API 的 payload 由各应用独立定义，不能把自动门字段和喂食器字段放在同一个结构里。

## 路由预算

Esp32Base Web 能力进入实现前需要确认：

- 可注册 route 数量。
- 同步 handler 建议耗时。
- Web Auth 首版默认启用；如果只在封闭内网临时调试，可由构建配置关闭，但正式设备不建议关闭。
- 静态页面放置方式。
- JSON 解析和响应大小限制。
- App Config groups/fields 容量。

如果 route 数量受限，各应用可以在各自固件内合并为少量资源式 API，例如：

- `GET /api/app/status`
- `POST /api/app/command`
- `POST /api/app/maintenance`
- `GET /api/app/records`
- `GET /api/app/diagnostics`

即使源码阶段合并 handler，文档层仍应保留清晰的业务命令名、payload schema 和权限/确认规则。
