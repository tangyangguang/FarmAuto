# Web/API 与维护功能草案

## 目标

本文定义两个应用的本地 Web 页面和 API 草案，用于后续评估 Esp32Base Web 能力、路由数量、handler 耗时和危险操作安全性。

首版 Web/API 必须保持简单、同步 handler 不做长耗时操作。危险操作必须二次确认。

设备按无人值守运行设计。Web/API 应优先支持远程查看、远程恢复和远程诊断；只有系统能明确判断继续远程操作有机械风险时，才返回需要现场处理的故障码。

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

- `POST /api/maintenance/calibrate-open-limit`
- `POST /api/maintenance/set-position`
- `POST /api/maintenance/calibrate`
- `POST /api/maintenance/format-storage`
- `POST /api/maintenance/clear-fault`

危险操作：

- 端点校准。
- 校准。
- 格式化存储。
- 清除故障后恢复运行。

`GET /api/status` 应包含开门/上限位、可选关门/下限位、当前位置可信度、最近一次端点校准结果和是否需要现场处理。`POST /api/maintenance/calibrate-open-limit` 建议语义为远程低速开门直到触发开门/上限位：API 只发起校准流程，不在 HTTP handler 内阻塞等待完成。

待确认：

- `calibrate-open-limit` 和 `set-position` 是否都需要；建议前者为运行到开门/上限位，后者仅保留为受限维护功能。
- 是否需要单独的微调上/下命令。
- 是否需要导出配置或诊断信息。

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
- `POST /api/maintenance/format-storage`
- `POST /api/maintenance/clear-fault`

危险操作：

- 清空当天计数。
- 标定每圈下料量。
- 格式化存储。
- 跳过今日定时投喂。

待确认：

- 克数模式和圈数模式的页面切换方式。
- 是否需要一键测试单路固定小剂量。
- 长期原始记录导出格式和分页方式。

## 路由预算

Esp32Base Web 能力进入实现前需要确认：

- 可注册 route 数量。
- 同步 handler 建议耗时。
- Web Auth 是否启用。
- 静态页面放置方式。
- JSON 解析和响应大小限制。

如果 route 数量受限，应合并为少量资源式 API，例如：

- `POST /api/command`
- `GET /api/status`
- `GET /api/config`
- `POST /api/config`
- `POST /api/maintenance`
