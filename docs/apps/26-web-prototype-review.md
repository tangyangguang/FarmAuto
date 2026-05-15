# Web 页面原型确认稿

## 目标

本文用于 C12 的页面原型确认。它不定义代码实现，不替代 API 文档。

阅读顺序：

1. `docs/apps/23-esp32base-web-integration.md`：系统页和业务页边界。
2. `docs/apps/19-web-page-prototypes.md`：页面草图和信息架构。
3. 本文：按页面逐项确认首版需要展示和操作的内容。

## 全局导航

每个应用固件只运行一个业务应用。应用页面可以使用简短路径；Esp32Base 系统页面始终使用 `/esp32base/*`。

应用导航：

| 页面 | 自动门 | 喂食器 | 说明 |
| --- | --- | --- | --- |
| `/` | Dashboard | Dashboard | 首屏状态和常用控制 |
| `/control` | Control | Control | 可选；如果首页已经承载控制，首版可不单独做 |
| `/maintenance` | Maintenance | Maintenance | 危险维护、校准、诊断入口 |
| `/records` | Records | Records | 长期业务记录查询和导出 |
| `/diagnostics` | Diagnostics | Diagnostics | 业务诊断包、存储健康、最近事件 |
| `/schedule` | 不使用 | Schedule | 喂食器每日计划 |
| `/buckets` | 不使用 | Buckets | 喂食器饲料桶管理 |
| `/calibration` | 不使用 | Calibration | 喂食器下料标定 |

系统导航：

| 页面 | 路径 | 归属 |
| --- | --- | --- |
| 系统状态 | `/esp32base` | Esp32Base |
| 系统参数 | `/esp32base/app-config` | Esp32Base |
| 系统日志 | `/esp32base/logs` | Esp32Base |
| WiFi | `/esp32base/wifi` | Esp32Base |
| OTA | `/esp32base/ota` | Esp32Base |
| 系统工具 | `/esp32base/tools` | Esp32Base |

应用页面不使用 `/logs`、`/api/logs` 或 `/config`。业务记录统一使用 `/records` 和 `/api/app/records`。

## 通用布局

桌面首屏：

```text
┌────────────────────────────────────────────────────┐
│ 应用名称        在线/离线  时间状态  存储状态       │
├────────────────────────────────────────────────────┤
│ 关键告警条：故障 / 断电恢复 / 存储 warning / 低余量 │
├────────────────────────────────────────────────────┤
│ 当前状态摘要                                      │
├────────────────────────────────────────────────────┤
│ 主要操作按钮                                      │
├────────────────────────────────────────────────────┤
│ 运行曲线或通道表                                  │
├────────────────────────────────────────────────────┤
│ 最近事件                                          │
└────────────────────────────────────────────────────┘
```

手机首屏顺序：

1. 设备在线、时间、存储状态。
2. 红色/黄色告警条。
3. 当前状态。
4. 停止按钮或最关键安全按钮。
5. 常用动作。
6. 详情折叠区和最近事件。

长动作只发起命令并返回 `commandId`，页面不等待动作完成。

## Esp32FarmDoor 首页

首屏必须显示：

| 区域 | 字段 |
| --- | --- |
| 状态 | doorState、positionTrusted、positionSource、lastSavedAt |
| 位置 | currentPositionTurns、currentPositionPulses、openTargetTurns、closeTargetTurns |
| 保护 | maxRunTurns、maxRunMs、lastStopReason |
| 电机 | motorState、speed、outputPercent、remainingTurns |
| 电流 | 当前 mA、滤波 mA、阈值、保护状态 |
| 限位 | 第一版显示未安装/禁用；下一阶段显示上限位/下限位状态 |
| 存储 | AT24C、flash、最近写入错误 |

按钮：

| 按钮 | 显示条件 | 行为 |
| --- | --- | --- |
| 开门 | 位置可信且未运行 | 发起开门命令 |
| 关门 | 位置可信且未运行 | 发起关门命令 |
| 停止 | 运行中 | 立即停止当前动作 |
| 清除故障 | Fault | 进入故障清除流程 |
| 进入维护 | 始终可见，运行中只读 | 进入维护页 |

保护停止展示：

- 达到最大运行圈数或最大运行时间时，页面显示“保护停止”告警。
- 保护停止不直接等同故障。
- 连续重复保护停止、编码器异常、电流异常、方向异常或限位冲突时显示 Fault。

## Esp32FarmDoor 维护页

首版必须支持：

| 功能 | 说明 |
| --- | --- |
| 低速点动 | 开门方向、关门方向、停止 |
| 设置当前位置 | 可设置为关闭点或指定位置 |
| 保存开门目标 | 当前位置保存为开门目标 |
| 直接设置行程 | 输入圈数，支持微调 |
| 端点验证 | 低速执行一次开门/关门验证 |
| INA240A2 零点校准 | 电机停止且确认无电流 |
| 存储检查 | AT24C inspect、flash 容量、错误统计 |
| 导出诊断包 | 配置、状态、最近事件、存储健康 |

危险操作必须二次确认：

- 设置当前位置。
- 保存端点。
- 直接覆盖行程。
- 格式化业务存储。
- 清除长期记录。

## Esp32FarmDoor 记录页

筛选：

- 时间范围。
- 事件类型。
- stopReason / faultReason。

导出：

- JSON Lines。
- CSV。

记录页展示业务记录，不展示 Esp32Base 系统日志。系统日志只通过 `/esp32base/logs` 查看。

## Esp32FarmFeeder 首页

首屏必须显示：

| 区域 | 字段 |
| --- | --- |
| 全局 | feederState、runningCount、enabledChannelMask、faultChannelMask |
| 今日计划 | enabled、timeConfigured、timeMinutes、skipToday、scheduleAttemptedToday、todayExecuted、scheduleMissedToday |
| 通道表 | channel、enabled、installed、motorState、targetMode、target、todayGrams、remainPercent、faultReason |
| 存储 | AT24C、flash、记录范围、最近错误 |
| 告警 | PowerLossAborted、低余量、存储 warning、通道故障 |

按钮：

| 按钮 | 显示条件 | 行为 |
| --- | --- | --- |
| 启动全部 | 至少一个空闲可用通道 | 只启动空闲、启用、已安装、无故障通道 |
| 停止全部 | 任一路运行中 | 同时请求运行通道停止 |
| 单路启动 | 该路空闲可用 | 发起该路投喂 |
| 单路停止 | 该路运行中 | 停止该路 |
| 跳过今日 | 计划页或首页 | 只影响今日定时，不影响手动 |

断电中断展示：

- 投喂运行中断电后显示 `PowerLossAborted`。
- 页面必须显示被中断通道、已可靠记录脉冲、目标脉冲和“自动续喂已阻止”。
- 不提供“继续未完成投喂”按钮。
- 用户如需投喂，只能重新发起新的手动投喂。

## Esp32FarmFeeder 计划页

首版规则：

- 默认不开启每日自动投喂。
- 启用前必须设置执行时间。
- 必须选择参与通道。
- 每个参与通道必须有有效投喂目标。
- 未配置时间时不自动投喂。
- 时间无效时暂停自动定时，手动投喂仍可用。
- 错过计划不补投喂。
- 运行中断电后当天不再次自动触发。

页面字段：

| 字段 | 说明 |
| --- | --- |
| enabled | 每日计划是否启用 |
| timeMinutes | 执行时间 |
| channelMask | 参与通道 |
| skipToday | 今日跳过 |
| scheduleAttemptedToday | 今日计划是否已开始过 |
| todayExecuted | 今日计划是否明确完成 |
| scheduleMissedToday | 今日是否错过未执行 |

## Esp32FarmFeeder 饲料桶页

每路显示：

- 容量。
- 当前估算余量。
- 余量百分比。
- 低余量阈值。
- 严重低余量阈值。
- 预计可投喂次数。
- 最近补料记录。

操作：

- 设置当前余量。
- 记录加料量。
- 标记已加满。
- 修正容量。

第一版不加硬件余量传感器。下一阶段若只需要“快没料”告警，优先评估低成本红外/光电/电容点位检测；称重只作为连续余量估算方案。

## Esp32FarmFeeder 标定页

每路独立标定：

1. 选择通道。
2. 发起小剂量测试。
3. 显示实际脉冲、圈数和时长。
4. 用户输入实测重量。
5. 保存 `gramsPerRevolution`。

标定失败不覆盖旧值。

## 待用户确认

| 编号 | 问题 | 推荐值 |
| --- | --- | --- |
| W1 | 页面信息架构是否按本文拆分 | 接受当前拆分，源码阶段再做 HTML 原型 |
| W2 | 自动门首页是否合并控制页 | 首版合并到首页，减少页面数量 |
| W3 | 喂食器首页是否显示通道表而不是卡片 | 桌面用表格，手机用每路折叠块 |
| W4 | 业务记录导出是否首版必须做 CSV | 必须做，便于人工分析 |
| W5 | 危险操作 confirm token | 首版应用内实现短期 token，不要求 Esp32Base 先增强 |

