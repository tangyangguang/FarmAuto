# Web 页面原型确认稿

## 目标

本文用于 C12 的页面原型确认。它不定义代码实现，不替代 API 文档。

静态 HTML 原型入口：

- `docs/prototypes/web/index.html`
- `docs/prototypes/web/esp32-farmdoor.html`
- `docs/prototypes/web/esp32-farmfeeder.html`

当前已拆成最终独立页面文件，并使用顶部导航展示最终页面结构：

- 自动门：`farmdoor-dashboard.html`、`farmdoor-maintenance.html`、`farmdoor-records.html`、`farmdoor-diagnostics.html`
- 喂食器：`feeder-dashboard.html`、`feeder-schedule.html`、`feeder-schedule-edit.html`、`feeder-buckets.html`、`feeder-calibration.html`、`feeder-records.html`、`feeder-diagnostics.html`
- `esp32-farmdoor.html` 和 `esp32-farmfeeder.html` 仅作为页面列表入口保留。

阅读顺序：

1. `docs/apps/23-esp32base-web-integration.md`：系统页和业务页边界。
2. `docs/apps/19-web-page-prototypes.md`：页面草图和信息架构。
3. 本文：按页面逐项确认首版需要展示和操作的内容。
4. `docs/apps/27-web-prototype-professional-review.md`：专业评审结论和剩余改进项。

## 全局导航

每个应用固件只运行一个业务应用。应用页面可以使用简短路径；Esp32Base 系统页面始终使用 `/esp32base/*`。

应用导航：

| 页面 | 自动门 | 喂食器 | 说明 |
| --- | --- | --- | --- |
| `/` | 首页 | 首页 | 首屏状态和常用控制 |
| `/control` | 合并到首页 | 合并到通道操作 | 如果首页已经承载控制，首版不单独做 |
| `/maintenance` | 行程校准 | 维护 | 自动门手动运行和端点标定；喂食器危险维护和校准入口 |
| `/records` | 记录 | 记录 | 长期业务记录网页查询；导出作为后续增强 |
| `/diagnostics` | 诊断 | 诊断 | 业务诊断包、存储健康、最近事件 |
| `/schedule` | 不使用 | 每日计划 | 喂食器每日计划 |
| `/buckets` | 不使用 | 饲料桶 | 喂食器饲料桶管理 |
| `/calibration` | 不使用 | 下料标定 | 喂食器下料标定 |

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
│ 关键告警条：故障 / 断电恢复 / 存储告警 / 低余量 │
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

保护停止展示：

- 达到最大运行圈数或最大运行时间时，页面显示“保护停止”告警。
- 保护停止不直接等同故障。
- 连续重复保护停止、编码器异常、电流异常、方向异常或限位冲突时显示“故障”。

## Esp32FarmDoor 行程校准页

首版必须支持：

| 功能 | 说明 |
| --- | --- |
| 手动运行 | 输入本次转动圈数和速度百分比，选择开门方向或关门方向执行，可随时停止 |
| 当前状态 | 显示当前位置和当前开门目标 |
| 标定为关门状态 | 手动运行后，把当前位置保存为关闭基准 |
| 用当前位置更新开门目标 | 手动运行后，自动把当前位置换算为新的开门目标圈数 |
| 参数入口 | 开门目标、保护最大圈数、保护最大时长等低频参数进入 Esp32Base App Config |
| 设备检查与校准 | INA240A2、AT24C、Flash 检查放到诊断页，避免行程校准页混杂 |

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

首版要求：

- 必须支持网页分页查看。
- 必须支持按时间范围、事件类型和故障原因筛选。
- 导出 JSON Lines / CSV 不是首版必须项，后续按远程维护需要再做。

记录页展示业务记录，不展示 Esp32Base 系统日志。系统日志只通过 `/esp32base/logs` 查看。

## Esp32FarmFeeder 首页

首屏必须显示：

| 区域 | 字段 |
| --- | --- |
| 全局 | feederState、runningCount、enabledChannelMask、faultChannelMask |
| 今日计划 | enabled、timeConfigured、timeMinutes、skipToday、scheduleAttemptedToday、todayExecuted、scheduleMissedToday |
| 通道表 | channel、enabled、installed、motorState、targetMode、target、todayGrams、remainPercent、faultReason |
| 存储 | AT24C、flash、记录范围、最近错误 |
| 告警 | 断电中断、低余量、存储告警、通道故障 |

运行中展示：

- 喂食器首页不需要运行曲线。
- 如果某路正在下料，首页应在通道表中持续显示该路下料过程：目标、已完成圈数/脉冲、估算克数、已运行时间、剩余目标和当前电机状态。
- 运行中数据用于远程判断“正在正常下料”，不是长期高频记录。

按钮：

| 按钮 | 显示条件 | 行为 |
| --- | --- | --- |
| 启动全部 | 至少一个空闲可用通道 | 只启动空闲、启用、已安装、无故障通道 |
| 停止全部 | 任一路运行中 | 同时请求运行通道停止 |
| 单路启动 | 该路空闲可用 | 发起该路投喂 |
| 单路停止 | 该路运行中 | 停止该路 |
| 跳过今日 | 计划页或首页 | 只影响今日定时，不影响手动 |

断电中断展示：

- 投喂运行中断电后显示“断电中断”。
- 页面必须显示被中断通道、已可靠记录脉冲、目标脉冲和“自动续喂已阻止”。
- 不提供“继续未完成投喂”按钮。
- 用户如需投喂，只能重新发起新的手动投喂。

## Esp32FarmFeeder 计划页

首版规则：

- 默认没有自动投喂计划。
- 支持一天多个计划，每个计划有独立执行时间。
- 每个计划必须选择参与通道。
- 每个计划中每个参与通道必须有有效投喂目标。
- 未配置任何计划时不自动投喂。
- 时间无效时暂停自动定时，手动投喂仍可用。
- 单个计划错过后不补投喂。
- 单个计划运行中断电后，该计划当天不再次自动触发；其他未到时间的计划仍按其自身状态判断。

页面字段：

| 字段 | 说明 |
| --- | --- |
| planId | 计划编号 |
| enabled | 该计划是否启用 |
| timeMinutes | 该计划执行时间 |
| channelMask | 该计划参与通道 |
| skipToday | 今日跳过 |
| per-channel target | 该计划每路目标 |
| attemptedToday | 该计划今日是否已开始过 |
| executedToday | 该计划今日是否明确完成 |
| missedToday | 该计划今日是否错过未执行 |

## Esp32FarmFeeder 饲料桶页

每路显示：

- 容量。
- 当前估算余量。
- 余量百分比。
- 低余量阈值。
- 严重低余量阈值。
- 按当前计划可投喂次数。
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
2. 手工输入每圈下料克数。
3. 选择参数来源，例如手工输入或历史实测。
4. 保存 `gramsPerRevolution`。

标定页不做实际运转测试。若后续需要电机试运行，应作为维护测试功能单独确认，不混入参数标定页。

## 待用户确认

| 编号 | 问题 | 推荐值 |
| --- | --- | --- |
| W1 | 页面信息架构是否按本文拆分 | 已确认：先按当前拆分推进；HTML 原型已生成，下一步逐页确认内容 |
| W2 | 自动门首页是否合并控制页 | 已确认：合并 |
| W3 | 喂食器首页使用表格还是卡片 | 不预设；以展示合理、手机查看和操作体验好为准 |
| W4 | 记录导出是否首版必须做 | 已确认：不是必须；网页查看是基本能力，导出后续再做 |
| W5 | 危险操作二次确认令牌 | 已确认：首版应用内实现短期令牌，不要求 Esp32Base 先增强 |
| W6 | 原型是否拆成最终独立页面 | 已完成：使用顶部导航展示最终页面结构，下一步逐页确认最终页面效果 |
