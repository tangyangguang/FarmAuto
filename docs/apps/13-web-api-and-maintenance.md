# Web/API 与维护功能草案

## 目标

本文定义两个应用的本地 Web 页面和 API 草案，用于后续评估 Esp32Base Web 能力、路由数量、handler 耗时和危险操作安全性。

首版 Web/API 必须保持简单、同步 handler 不做长耗时操作。危险操作必须二次确认。

设备按无人值守运行设计。Web/API 应优先支持远程查看、远程恢复和远程诊断；只有系统能明确判断继续远程操作有机械风险时，才返回需要现场处理的故障码。

页面原型和信息架构见 `docs/apps/19-web-page-prototypes.md`。页面工作流细化见 `docs/apps/21-web-workflows.md`。本文侧重 API、路由和维护语义；页面排版不在本文展开。

## 通用页面

建议页面：

- `/`：总览页。
- `/control`：控制页。
- `/config`：配置页。
- `/maintenance`：维护页。
- `/logs`：最近事件或日志入口。

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
- 保存配置前先校验范围。
- 危险操作需要确认 token 或二次确认参数。

配置页面原则：

- 普通系统参数优先使用 Esp32Base App Config 内置页面。
- 应用自定义 Web 页面只负责状态、控制、维护动作、记录查询和诊断展示。
- 如果 App Config 的字段容量、页面组织或校验能力不足，不在 FarmAuto 中临时绕开，应整理提示词到 Esp32Base 项目处理。

## Esp32FarmDoor API 草案

状态：

- `GET /api/status`
- `GET /api/config`
- `GET /api/logs`

控制：

- `POST /api/door/open`
- `POST /api/door/close`
- `POST /api/door/stop`

配置：

- `POST /api/config`

维护：

- `POST /api/maintenance/jog`
- `POST /api/maintenance/set-position`
- `POST /api/maintenance/save-endpoints`
- `POST /api/maintenance/calibrate-open-limit`
- `POST /api/maintenance/calibrate`
- `POST /api/maintenance/format-storage`
- `POST /api/maintenance/clear-fault`

危险操作：

- 端点校准。
- 校准。
- 格式化存储。
- 清除故障后恢复运行。

`GET /api/status` 应包含是否启用开门/上限位、可选关门/下限位、当前位置可信度、最近一次端点维护结果和是否需要现场处理。

第一版不要求限位开关：

- `POST /api/maintenance/jog`：远程低速点动，只允许短时运行，必须限制最大时长和最大脉冲。
- `POST /api/maintenance/set-position`：设置当前位置，只在电机停止且二次确认后允许。
- `POST /api/maintenance/save-endpoints`：保存开门/关门端点目标，必须二次确认。
- `POST /api/maintenance/verify-endpoints`：发起一次低速验证流程，验证开门目标、关门目标和安全上限。

推荐二次确认方式：

- 请求体包含 `confirm=true` 和页面生成的短期 `confirmToken`。
- 危险操作的响应必须返回 commandId，页面继续轮询状态。
- `jog` 每次只允许短动作，不允许“一直按住一直走”的长阻塞 HTTP 请求。
- `set-position`、`save-endpoints`、`format-storage`、`clear-fault` 都记录长期事件。

下一阶段启用开门/上限位后：

- `POST /api/maintenance/calibrate-open-limit`：远程低速开门直到触发开门/上限位。API 只发起校准流程，不在 HTTP handler 内阻塞等待完成。

已确认与后续项：

- 下一阶段启用限位后，`calibrate-open-limit` 和 `set-position` 是否都保留；建议前者为运行到开门/上限位，后者仅保留为受限维护功能。
- 单独的微调上/下命令首版不做成普通控制按钮；维护页用短时 `jog` 承载。
- 需要导出诊断信息；配置导出可后续根据 Esp32Base App Config 能力评估。

## Esp32FarmFeeder API 草案

状态：

- `GET /api/status`
- `GET /api/config`
- `GET /api/history`
- `GET /api/records`
- `GET /api/logs`

控制：

- `POST /api/feeders/1/start`
- `POST /api/feeders/2/start`
- `POST /api/feeders/3/start`
- `POST /api/feeders/1/stop`
- `POST /api/feeders/2/stop`
- `POST /api/feeders/3/stop`
- `POST /api/feeders/start-all`
- `POST /api/feeders/stop-all`
- `POST /api/feeders/skip-today`
- `POST /api/feeders/cancel-skip-today`

配置：

- `POST /api/config`

维护：

- `POST /api/maintenance/clear-today`
- `POST /api/maintenance/calibrate-feed-rate`
- `POST /api/maintenance/test-channel`
- `POST /api/maintenance/format-storage`
- `POST /api/maintenance/clear-fault`

危险操作：

- 清空当天计数。
- 标定每圈下料量。
- 格式化存储。
- 跳过今日定时投喂。

已确认规则：

- 克数模式和圈数模式在页面上使用每路独立 segmented control；切换模式不删除另一个模式的已保存目标值。
- 需要一键测试单路固定小剂量，用于方向、编码器和下料验证；必须限制最大运行时间和最大脉冲。
- 长期原始记录使用分页查询，导出 JSON Lines 和 CSV。

## 路由预算

Esp32Base Web 能力进入实现前需要确认：

- 可注册 route 数量。
- 同步 handler 建议耗时。
- Web Auth 首版默认启用；如果只在封闭内网临时调试，可由构建配置关闭，但正式设备不建议关闭。
- 静态页面放置方式。
- JSON 解析和响应大小限制。

如果 route 数量受限，应合并为少量资源式 API，例如：

- `POST /api/command`
- `GET /api/status`
- `GET /api/config`
- `POST /api/config`
- `POST /api/maintenance`
