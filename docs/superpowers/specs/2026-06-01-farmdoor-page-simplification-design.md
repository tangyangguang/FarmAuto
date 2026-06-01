# FarmDoor Page Simplification Design

## Goal

FarmDoor 页面要更像现场操作界面：首页直接显示高价值状态和按钮，校准页去掉 token 式复杂确认，诊断页直接显示硬件状态，用户页面不再引导查看 JSON。

## Scope

- 首页删除快速入口。
- 首页把门状态、开门、关门、停止合并为一个状态与操作面板。
- 首页故障存在时显示故障原因和清除故障按钮。
- 校准页删除确认 token 输入和“申请 token”流程。
- 校准页危险操作使用浏览器弹窗二次确认。
- 诊断页删除故障处理，把硬件信息直接显示在页面上。
- FarmDoor 页面避免使用 `sendMetric()` 和 `sendInfoRowCompact*()`，减少 Esp32Base helper 自动生成的粗体视觉。
- 保留现有 API 路径，不修改持久化格式。

## Behavior

首页状态面板显示：控制器状态、门位判断、当前位置、开门目标、最近停止原因、电机输出、故障原因。操作按钮直接位于同一面板。

校准页表单直接 POST 到现有维护 API。危险动作在表单 `onsubmit` 中使用 `confirm()`；后端不再要求 `confirmToken`。

诊断页显示 AT24C、当前采样、电机输出、按钮和编码器状态。状态 API 和诊断 API 保留给调试，但用户页面不再提供 JSON 链接。

## Validation

- 增加结构检查脚本，确认首页、校准页、诊断页满足简化要求。
- 运行既有记录/事件页面检查脚本。
- 编译 FarmDoor 固件。
