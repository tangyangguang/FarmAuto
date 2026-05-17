# Esp32FarmFeeder

`Esp32FarmFeeder` 是三路喂食器控制器应用工程。

当前状态：

- 已接入 Esp32Base FULL profile。
- 已接入 `FeederController` 纯业务状态机。
- 已接入每日多计划服务、手工下料目标解析、料桶余量维护、通道基础信息维护。
- 已接入喂食计划/今日状态二进制编解码，后续可作为 AT24C128 payload。
- 已接入手工下料默认目标二进制编解码，后续可作为 AT24C128 `FeederChannelTarget` payload。
- 已接入料桶估算余量二进制编解码，后续可作为 AT24C128 `FeederBucketState` payload；通道基础信息不写入该 payload。
- 已固化喂食器 AT24C128 记录区布局，并用 host 测试校验容量、连续性和页对齐。
- 已接入计划表、手工下料默认目标和料桶估算余量到 `Esp32At24cRecordStore` 的读写 glue，并用 fake AT24C host 测试验证。
- 已接入首页、计划、记录、基础信息、诊断 5 个最小业务页面，系统参数/日志/OTA/WiFi 仍使用 Esp32Base 页面。
- 已接入业务最近记录 RAM 缓冲、Flash 二进制追加记录和基础文件轮转。
- `GET /api/app/records` 支持从 Flash 记录分页读取，并支持 `startUnixTime`、`endUnixTime`、`eventType` 筛选；Flash 不可用时回退 RAM 最近记录。
- 已提供只读诊断 API、最近事件 API、清空今日状态和清除通道故障维护 API。
- 已接入 `Esp32At24cRecordStore`、`Esp32EncodedDcMotor`、`Esp32MotorCurrentGuard` 作为后续实现依赖。
- 默认三路通道均已安装且启用；底层保留 4 路数组容量，但首版业务 API 只暴露和接受 3 路。
- 当前不会输出 PWM，也不会驱动任何电机。

当前尚未实现：

- 今日执行状态写入/读取 AT24C128。
- AT24C128 Wire 设备接入应用启动流程。
- 最终版业务页面的精细交互和视觉样式。
- GPIO、编码器、PWM 和 AT24C128 硬件适配。
- 业务记录索引和跨文件查询。
- 真实电机输出和编码器计数。

## 当前 API

`/api/app/status`

- 返回应用类型、固件版本、设备状态、通道 mask、每路通道状态。
- 明确返回 `motorOutput.enabled=false`，不会执行下料动作。

`/api/app/diagnostics`

- 返回只读业务诊断信息：通道状态、计划状态、料桶余量、默认目标、最近记录数量、Flash 可用状态。
- 明确返回 `motorOutput.enabled=false`。

`/api/app/events/recent`

- 返回 RAM 最近业务记录。

`/api/app/feeders/manual-start`

- 按 `channelMask` 手工下料。
- 每路使用当前默认目标；未配置或未标定的通道会被跳过。
- 当前只更新业务状态机，不输出 PWM。

`/api/app/feeders/start`

- 支持 `channelMask` 或单个 `channel`。
- 与 `manual-start` 一样，当前只更新业务状态机。

`/api/app/feeders/stop`

- 支持 `channelMask` 或单个 `channel` 停止。

`/api/app/feeders/stop-all`

- 停止所有正在运行的通道。

`/api/app/schedules`

- 查看多计划、今日/明日执行状态和时间同步状态。

`/api/app/schedules/create`
`/api/app/schedules/update`
`/api/app/schedules/delete`

- 管理计划蓝本。

`/api/app/schedule-occurrence/skip`
`/api/app/schedule-occurrence/cancel-skip`

- 跳过或取消跳过某个计划的今日执行实例。

`/api/app/buckets`
`/api/app/buckets/set-remaining`
`/api/app/buckets/add-feed`
`/api/app/buckets/mark-full`

- 查看和维护每路料桶估算余量。

`/api/app/base-info`
`/api/app/base-info/channel`

- 查看和维护每路通道基础信息。

`/api/app/records`

- 参数：`start`、`limit`、`startUnixTime`、`endUnixTime`、`eventType`。
- 优先返回 Flash 业务记录；无 Flash 数据时返回 RAM 最近记录。

`/api/app/maintenance/clear-today`

- 空闲时清除今日计划执行状态，包括跳过、已尝试、已完成和已错过。
- 有通道运行时返回 `Busy`。

`/api/app/maintenance/clear-fault`

- 支持 `channel` 或 `channelMask`，清除指定通道故障。
- 返回 `successMask` 和 `skippedMask`。

编译验证：

```bash
pio run -d apps/Esp32FarmFeeder
```
