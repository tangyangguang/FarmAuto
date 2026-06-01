# FarmDoor Record And Event Pages Design

## Goal

FarmDoor 的用户导航要把开关门记录和业务事件分成两个独立页面，并在页面内直接显示分页列表。普通用户不应被引导去打开 JSON。

## Scope

- `/records` 改为“开关门记录”页面。
- 新增 `/events` 作为“业务事件”页面。
- 导航显示：首页、开关门记录、业务事件、校准、诊断。
- 保留现有 JSON API：`/api/app/records` 和 `/api/app/events/recent`，用于调试和后续联调。
- 不修改 `DoorRecord` 持久化格式，不修改 Esp32Base。

## Data Sources

- 开关门记录继续使用 `DoorRecordFileStore` 的分页读取能力。
- 当前文件和归档文件继续通过 `archive` 参数选择。
- 当文件系统没有可读记录时，记录页回退显示 RAM 最近记录。
- 业务事件继续通过 `FarmAutoEventLog::readLatest()` 读取 Esp32Base App Events。

## Page Behavior

### 开关门记录

页面直接显示 HTML 表格，不提供筛选表单。表格列为：序号、时间、事件类型、命令、结果、位置变化、行程变化。分页使用 `Esp32BaseWeb::sendPagination()`，页面参数只保留 `page`、`per`。

### 业务事件

页面包含 HTML 表格。表格列为：ID、时间、级别、领域、动作、目标、消息、详情。分页使用 `Esp32BaseWeb::sendPagination()`，页面参数使用 `page` 和 `per`。

## Error Handling

- 无效分页参数显示页面级告警，不输出 JSON。
- 指定归档无记录时显示空表格和分页状态。
- 业务事件存储未就绪或读取失败时显示告警，同时保留已能读取的内容。

## Testing

- 增加轻量检查脚本，验证 FarmDoor 注册 `/events` 导航和页面、`/records` 不再显示业务事件 JSON 入口、两个页面都调用 Esp32Base 分页能力。
- 运行 FarmDoor 现有单元测试。
- 运行 FarmDoor 编译。
