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

## 网络安全边界

首版 Web 以本地局域网 HTTP 为默认前提，认证和会话能力复用 Esp32Base。它不是公网暴露接口，也不把 HTTP Basic/Cookie 当作互联网级安全方案。

推荐部署边界：

- 设备只接入可信局域网、专用 WiFi 或 VPN。
- 不做端口映射到公网。
- 正式设备默认启用 Esp32Base Web Auth。
- 危险操作仍需要应用级 confirm token，即使已经登录。
- 如果未来需要公网或跨网访问，应先评估 HTTPS、反向代理/VPN、证书、会话超时和审计，不在 FarmAuto 首版临时加简单开关。

## 系统日志与业务记录

- Esp32Base 系统日志用于 WiFi、OTA、启动、文件系统、route 错误和运行时调试。
- FarmAuto 业务记录用于自动门、喂食器各自的动作、维护、故障、配置变化和业务状态变化。
- 业务长期记录不写入 `/logs/eb_app.log`，也不通过 `/esp32base/logs` 查询。
- 应用诊断页只展示业务诊断信息，不复制系统日志，也不重复制作系统日志、OTA、WiFi 或系统参数入口。

## 通用页面规则

每个应用可以使用相同的基础页面路径，因为每个设备只运行一个应用。实际页面以各应用 Web/API 文档为准：

- `/`：应用总览页。
- `/records`：业务记录页。
- `/diagnostics`：业务诊断页。

配置页面原则：

- 普通系统参数优先使用 Esp32Base App Config 内置页面 `/esp32base/app-config`。
- 应用自定义页面只负责状态、控制、维护动作、业务记录查询和业务诊断展示。
- 不建议新增应用 `/config` 页面。
- 业务配置按领域命名，例如喂食器的 `/schedule`、`/base-info`。
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
- 单个同步 handler 的目标耗时不超过 200ms。
- 预计超过 200ms 的操作必须转为命令模式：HTTP 只创建命令并返回 `commandId`，后台 `update(nowMs)` 或任务分片执行，页面轮询状态。
- INA240 零点校准、AT24C inspect、长期记录扫描/导出、端点验证、运动控制等都不能在 HTTP handler 中长时间阻塞。

## 危险操作确认 token

危险操作首版由应用实现最小确认 token，不依赖 Esp32Base 新能力：

- 页面先请求或触发 `ConfirmRequired`，服务端生成 `confirmToken`。
- token 由服务端随机生成，不能是固定字符串或可预测计数。
- token 绑定 `actionId`、目标资源和当前登录会话；不能跨动作复用。
- token TTL 不超过 60 秒。
- token 一次性消费，成功或失败后立即失效。
- 危险 API 必须同时提交 `confirm=true`、`confirmToken` 和原始动作参数。
- token 校验失败返回 `ConfirmRequired` 或 `Forbidden`，并写入业务事件或系统安全日志摘要。

如果多个应用重复实现后发现需要统一，再整理提示词到 Esp32Base；首版不为此阻塞业务页面设计。

## commandId 生命周期

`commandId` 用于远程轮询、业务记录和断电后解释上一次动作：

- 类型推荐 `uint32_t`，单次启动内单调递增。
- 每个应用固件内部使用一个全局序列，不按通道单独编号；喂食器通过 `channelMask` 或 `channel` 区分通道。
- 创建命令时立即分配，响应和长期记录都使用同一个 id。
- `commandId=0` 表示无活动命令。
- 重启后不为了 commandId 单独写入持久化计数器，避免增加 AT24C/Flash 磨损；长期记录用 `bootId + commandId + sequence` 区分不同启动周期。
- 溢出按 uint32 回绕处理，但记录比较只用于短窗口诊断；长期唯一性依赖 bootId/unixTime/uptime/sequence 组合。
- 自动门运行中断电恢复需要保留上次运动命令 id；喂食器运行中断电后 `FeederPowerLossAborted` 必须记录被中断 commandId。

## 时间可信规则

时间相关功能仅影响自动计划和按日期展示，不影响手动操作：

- 未完成 NTP/RTC 同步前，`timeValid=false`，自动计划不触发。
- 时间首次同步成功后才允许计算“今日计划是否错过”。
- 设备启动时如果时间尚未可信，计划状态保持 `timePending` 或等价字段，不立即记为 missed。
- 时间可信后，如果某计划时间已经过去，不补投喂，记录该计划 `scheduleMissedToday=true`，但不影响其他尚未到时间的计划。
- 明显时间回跳、跳变过大或同步失败超过阈值时，自动计划暂停并记录 `TimeSyncChanged` 或诊断告警。
- 手动下料与时间可信无关，只受通道 Busy、故障和配置状态约束。

通用应用 API 语义：

- `GET /api/app/status`：当前应用业务状态 snapshot。
- `GET /api/app/events/recent`：最近业务事件。
- `GET /api/app/records`：长期业务记录分页查询。
- `GET /api/app/records/export`：长期业务记录导出，后续增强项，首版不强制实现。
- `GET /api/app/diagnostics`：业务诊断包摘要。

这些 API 的 payload 由各应用独立定义，不能把自动门字段和喂食器字段放在同一个结构里。

记录页首版必须支持按时间范围筛选，并结合事件类型、通道或故障原因做分页查询。导出仍是后续增强项，但网页分页查看不能只靠页码翻找多年记录。

## Web Auth 初始化

正式设备默认启用 Esp32Base Web Auth。首版推荐流程：

- 首次烧录或格式化后，设备进入 Esp32Base 配网/初始化流程。
- 用户必须设置非默认 Web Auth 用户名和密码后，才允许进入业务控制页面。
- 如果 Esp32Base 当前初始化流程无法强制修改默认凭据，FarmAuto 应在首页和诊断页显示“默认凭据风险”告警，并把“强制首次修改凭据”整理为 Esp32Base 项目提示词。
- 不建议量产设备长期使用 `admin/admin` 或写死密码。
- 当前测试调试阶段，日志中的 WiFi 密码和 Web Auth 密码保持明文输出，方便串口排查配置问题；FarmAuto 不要求 Esp32Base 对密码日志做脱敏。

## 路由预算

Esp32Base Web 能力进入实现前需要确认：

- 可注册 route 数量。
- 同步 handler 是否能稳定满足应用目标耗时 200ms。
- Web Auth 首版默认启用；如果只在封闭内网临时调试，可由构建配置关闭，但正式设备不建议关闭。
- 静态页面放置方式。
- JSON 解析和响应大小限制。
- App Config groups/fields 容量。
- Esp32Base 当前 head 是否已具备 App Config、FileLog、文件系统 API、Auth、OTA/WiFi 页面和足够 route 容量；源码前必须用清单验证，不能只按未来能力设计。

如果 route 数量受限，各应用可以在各自固件内合并为少量资源式 API，例如：

- `GET /api/app/status`
- `POST /api/app/command`
- `POST /api/app/maintenance`
- `GET /api/app/records`
- `GET /api/app/diagnostics`

即使源码阶段合并 handler，文档层仍应保留清晰的业务命令名、payload schema 和权限/确认规则。
