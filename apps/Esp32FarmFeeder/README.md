# Esp32FarmFeeder

`Esp32FarmFeeder` 是三路喂食器控制器应用工程。

当前状态：

- 已接入 Esp32Base FULL profile。
- 已接入 `FeederController` 纯业务状态机。
- 已接入每日多计划服务、手工下料目标解析、料桶余量维护、通道基础信息维护。
- 已接入喂食计划二进制编解码，后续可作为 AT24C128 `FeederSchedule` payload。
- 已接入今日通道累计脉冲/克数二进制编解码，后续可作为 AT24C128 `FeederToday` payload。
- 已接入手工下料默认目标二进制编解码，后续可作为 AT24C128 `FeederChannelTarget` payload。
- 已接入料桶估算余量二进制编解码，后续可作为 AT24C128 `FeederBucketState` payload；通道基础信息不写入该 payload。
- 已接入通道基础信息/标定参数二进制编解码，后续可作为 AT24C128 `FeederCalibration` payload；料桶余量不写入该 payload。
- 已固化喂食器 AT24C128 记录区布局，并用 host 测试校验容量、连续性和页对齐。
- 已接入计划表、今日累计、手工下料默认目标、料桶估算余量和通道标定参数到 `Esp32At24cRecordStore` 的读写 glue，并用 fake AT24C host 测试验证。
- 已在启动时初始化 AT24C128 I2C RecordStore，并恢复计划、今日累计、手工下料默认目标、料桶估算余量和通道标定参数；业务修改成功后写回对应记录区。
- 已接入首页、计划、记录、基础信息、诊断 5 个最小业务页面，系统参数/日志/OTA/WiFi 仍使用 Esp32Base 页面。
- 首版喂食器暂无需要放入 Esp32Base App Config 的已实现业务参数；通道启用、信号数、每圈克数和料桶容量统一通过“基础信息”业务页/API 维护。
- 已接入业务最近记录 RAM 缓冲、Flash 二进制追加记录和基础文件轮转。
- 已为业务命令分配 `commandId`，状态接口和业务记录都可关联最近命令。
- `GET /api/app/records` 支持从 Flash 记录分页读取，并支持 `startUnixTime`、`endUnixTime`、`eventType` 筛选；Flash 不可用时回退 RAM 最近记录。
- 已提供只读诊断 API、最近事件 API、清空今日状态和清除通道故障维护 API。
- 已接入 Esp32Base App Events，并通过 `FarmAutoEventLog` 统一记录补料、设置余量、计划跳过、清今日、清故障、基础信息修改和业务记录存储告警等非系统业务事件。
- 已接入危险操作确认 token；跳过/取消跳过计划实例、删除计划、清空今日、清除故障、料桶余量维护和基础信息修改都需要二次确认。
- 已接入 `Esp32At24cRecordStore`、`Esp32EncodedDcMotor`、`Esp32MotorCurrentGuard`。
- 默认三路通道均已安装且启用；底层保留 4 路数组容量，但首版业务 API 只暴露和接受 3 路。
- 已接入三路单向 LEDC PWM 输出和 PCNT 编码器适配；真实硬件输出由 Esp32Base App Config 的 `feeder/motorOutput` 控制，默认关闭。

当前尚未实现：

- 最终版业务页面的精细交互和视觉样式。
- 业务记录索引和跨文件查询。

## 当前 API

`/api/app/status`

- 返回应用类型、固件版本、设备状态、通道 mask、每路通道状态。
- 返回最近业务命令摘要 `recentCommand`，用于页面轮询和记录关联。
- 返回 `motorOutput` 真实状态、每路编码器快照、驱动输出和电机故障摘要；默认 `enabled=false`，启用后才会输出 PWM。

`/api/app/diagnostics`

- 返回只读业务诊断信息：通道状态、计划状态、料桶余量、默认目标、最近记录数量、Flash 可用状态。
- 返回 `motorOutput` 真实状态；默认 `enabled=false`。

`/api/app/events/recent`

- 从 Esp32Base App Events 读取，并由 `FarmAutoEventLog` 映射成业务语言字段。

`/api/app/feeders/manual-start`

- 按 `channelMask` 手工下料。
- 默认使用每路已保存目标；也可为本次请求传 `ch1Mode/ch1GramsX100/ch1RevolutionsX100`、`ch2...`、`ch3...` 覆盖本次目标，不写回默认目标。
- 未配置、参数无效或未标定的通道会被跳过。
- `feeder/motorOutput=false` 时返回业务错误，不输出 PWM；启用后按通道启动真实 PWM 和 PCNT 闭环。

`/api/app/feeders/start`

- 支持 `channelMask` 或单个 `channel`。
- 与 `manual-start` 一样，启用真实输出后才会驱动电机。

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
- 删除计划属于危险操作，需要确认 token。

`/api/app/schedule-occurrence/skip`
`/api/app/schedule-occurrence/cancel-skip`

- 跳过或取消跳过某个计划的指定日期执行实例；请求应传 `planId` 和 `date=YYYYMMDD`。
- 不传 `date` 时默认使用当前服务日期，页面首版必须支持今日和明日计划跳过。
- 属于危险操作，需要确认 token。

`/api/app/buckets`
`/api/app/buckets/set-remaining`
`/api/app/buckets/add-feed`
`/api/app/buckets/mark-full`

- 查看和维护每路料桶估算余量。
- 修改余量属于危险操作，需要确认 token。

`/api/app/base-info`
`/api/app/base-info/channel`

- 查看和维护每路通道基础信息。
- 修改基础信息属于危险操作，需要确认 token。

`/api/app/records`

- 参数：`start`、`limit`、`startUnixTime`、`endUnixTime`、`eventType`、`archive`。
- `archive=0` 读取当前记录文件，`archive=1..16` 读取轮转归档文件。
- 优先返回 Flash 业务记录；无 Flash 数据时返回 RAM 最近记录。

`/api/app/maintenance/clear-today`

- 空闲时清除今日计划执行状态，包括跳过、已尝试、已完成和已错过。
- 有通道运行时返回 `Busy`。
- 属于危险操作，需要确认 token。

`/api/app/maintenance/clear-fault`

- 支持 `channel` 或 `channelMask`，清除指定通道故障。
- 返回 `successMask` 和 `skippedMask`。
- 属于危险操作，需要确认 token。

危险操作确认流程：

1. 首次提交危险 API 时不带 `confirm=true`，服务端返回 `ConfirmRequired`、`actionId`、`resource`、`confirmToken` 和 `ttlMs`。
2. 用户确认后，使用相同业务参数再次提交，并附加 `confirm=true&confirmToken=<token>`。
3. token 绑定动作和资源，60 秒内有效，只能消费一次。

编译验证：

```bash
pio run -d apps/Esp32FarmFeeder
```
