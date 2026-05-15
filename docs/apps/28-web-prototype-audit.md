# Web 静态原型自查记录

## 本轮自查范围

自查对象为 `docs/prototypes/web/` 下全部静态 HTML 页面。当前共 17 个页面：

- 原型入口：`index.html`
- 应用入口：`esp32-farmdoor.html`、`esp32-farmfeeder.html`
- Esp32Base 占位页：`esp32base-app-config.html`、`esp32base-logs.html`、`esp32base-ota-wifi.html`
- 自动门业务页：`farmdoor-dashboard.html`、`farmdoor-maintenance.html`、`farmdoor-records.html`、`farmdoor-diagnostics.html`
- 喂食器业务页：`feeder-dashboard.html`、`feeder-schedule.html`、`feeder-schedule-edit.html`、`feeder-buckets.html`、`feeder-calibration.html`、`feeder-records.html`、`feeder-diagnostics.html`

## 结构检查

已检查：

- 每个 HTML 页面都有标题和统一样式文件。
- 所有本地 `href` 链接目标均存在。
- 表单页面能通过页面链接打开。
- 业务页面均有顶部导航。
- 记录页、计划编辑页、行程校准页、标定页都有实际控件，不是空壳页面。
- 已清理 `#base` 这类不明确跳转，Esp32Base 系统参数、系统日志、OTA/无线网络改为独立占位页。

## 视觉与信息架构评估

当前原型整体符合“简洁、可远程维护、便于移动端阅读”的方向：

- 顶部导航短，业务页不混入过多系统项。
- 自动门首页只放高价值状态和常用操作。
- 自动门行程校准页只处理手动运行和端点标定，低频参数转到 Esp32Base App Config。
- 喂食器首页把运行进度整合到通道行，避免额外大卡片。
- 喂食器计划列表和编辑表单已拆成独立页面。
- 记录页把筛选、表格、分页放在同一卡片中，减少页面跳转和重复结构。

## 仍需补强

当前原型仍未完全冻结，下一轮建议补：

- 自动门运行中、故障、位置不可信三种首页状态。
- 自动门行程校准危险动作的二次确认弹窗或确认页。
- 喂食器单路手动投喂确认页面。
- 喂食器计划保存失败、未标定通道、删除计划确认。
- 诊断页把普通检查、校准、格式化存储分成不同风险层级。
- 记录页首版必须支持时间范围筛选。

## 验证命令

本轮执行了静态链接和结构检查。结论：当前 17 个 HTML 页面未发现缺失链接或基本结构问题。
