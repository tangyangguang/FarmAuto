# Esp32FarmDoor Web/API 草案

## 目标

本文只定义 Esp32FarmDoor 自动门应用的页面、API 和维护语义。它不适用于 Esp32FarmFeeder。

公共 Web/API 约定见 `docs/apps/13-web-api-and-maintenance.md`。Esp32Base 系统页面边界见 `docs/apps/23-esp32base-web-integration.md`。

## 页面

Esp32FarmDoor 页面：

- `/`：自动门总览，显示门状态、位置、保护状态、常用操作、上次运行回放和最近事件。
- `/records`：自动门长期业务记录查询；导出作为后续增强。
- `/calibration`：行程校准，含标定关门基准、用当前位置更新开门目标、直接设置行程和微调行程。手动运行和端点验证等真实电机动作进入硬件适配阶段。
- `/diagnostics`：自动门业务诊断信息，含状态 snapshot、最近事件、GPIO/ADC/AT24C/flash 只读检查。

系统参数、系统日志、OTA、WiFi 和文件系统格式化由 Esp32Base 自己提供，不进入自动门业务导航和静态原型。

## API

状态与记录：

- `GET /api/app/status`
- `GET /api/app/events/recent`
- `GET /api/app/records`
- `GET /api/app/records?archive=1`
- `GET /api/app/records/export`，后续增强项，首版不强制实现
- `GET /api/app/diagnostics`

控制：

- `POST /api/app/door/open`
- `POST /api/app/door/close`
- `POST /api/app/door/stop`

维护：

- `POST /api/app/maintenance/set-position`
- `POST /api/app/maintenance/set-travel`
- `POST /api/app/maintenance/adjust-travel`
- `POST /api/app/maintenance/clear-fault`

硬件适配阶段再增加的维护 API：

- `POST /api/app/maintenance/manual-move`
- `POST /api/app/maintenance/verify-endpoints`
- `POST /api/app/maintenance/calibrate-open-limit`
- `POST /api/app/maintenance/calibrate-current-zero`

## Status Snapshot

`GET /api/app/status` 应只包含自动门字段：

- 应用标识：appKind=`FarmDoor`、firmwareVersion、schemaVersion。
- 应用状态：`PositionUnknown`、`IdleClosed`、`IdleOpen`、`IdlePartial`、`Opening`、`Closing`、`Fault`。
- 维护流程：首版不单独暴露 activeFlow；硬件适配阶段如增加手动运行、端点验证、限位校准或电流零点校准，再增加 activeFlow 字段。
- 当前位置：positionPulses、positionPercent、positionTrustLevel、positionSource、lastPositionSavedAt。
- 端点：closedPulses、openTargetPulses、maxRunPulses、maxRunMs。
- 行程：openTurnsX100、outputPulsesPerRev。
- 限位：第一版显示禁用；下一阶段显示 openLimit/closeLimit 的启用、触发、断线和冲突状态。
- 电机：当前返回 `motorOutput.enabled=false`；AT8236/PCNT 接入后再扩展 state、speedPps、outputPercent、remainingPulses、faultReason。
- 电流：首版返回 INA240A2 编译开关和运行开关；GPIO33 ADC 原始值在 diagnostics 中查看。电流换算、零点校准和保护停机策略在硬件实测后接入。
- 存储：AT24C 在线状态、record store ready、flash 记录分页信息。
- 最近命令：commandId、command、source、startedAt、result。
- 最近停止原因：lastStopReason，例如 TargetReached、UserStop、MaxRunTime、MaxRunPulses、OverCurrent、EncoderNoPulse。

`positionTrustLevel` 固定为枚举，不再同时使用 bool 和字符串：

- `Trusted`：位置记录和恢复条件可信，可正常远程开关门。
- `Limited`：位置大致可信但未经验证；页面必须提示，首次动作自动限速和限幅。
- `Untrusted`：位置不可用，只允许维护/行程校准流程。

`positionPercent` 计算规则：

- `positionPercent = clamp(positionPulses / max(openTargetPulses, 1) * 100, 0, 120)`。
- 超过 100% 时页面应使用告警样式，表示超过当前开门目标。

不得包含喂食器通道、饲料桶、每日计划、今日投喂等字段。

## 危险操作

以下操作必须二次确认：

- 设置当前位置。
- 直接设置行程圈数或脉冲数。
- 微调行程。
- 清除故障后恢复运行。

以下危险操作属于硬件适配阶段：

- 低速手动运行。
- 低速端点验证。
- 下一阶段上限位校准。
- INA240A2 零点校准。

二次确认建议：

- 请求体包含 `confirm=true` 和短期 `confirmToken`。
- 响应返回 commandId。
- 页面轮询 `GET /api/app/status` 展示进度。
- 所有危险操作写入自动门长期业务记录。

## 第一版无限位维护

第一版不安装限位开关时：

- `PositionUnknown` 下禁止普通开门和关门。
- 允许进入行程校准页执行手动运行、设置关闭点、保存开门目标、直接设置行程圈数或脉冲数、微调行程和低速端点验证。
- 当前无电机输出版本只支持设置关闭点、保存开门目标、直接设置行程圈数或脉冲数、微调行程；低速手动运行和低速端点验证等真实电机动作进入硬件适配阶段。
- 硬件适配阶段的 `manual-move` 单次动作必须限制最大时长和最大脉冲，并允许随时停止。
- 端点验证成功前，不建议退出 `PositionUnknown` 进入普通控制；如果用户选择直接设置行程但跳过验证，必须标记为低可信恢复来源并持续提示。
- 没有远程视频、现场观察或机械标记时，不建议远程重新示教端点。

## 断电恢复

运行中断电后应尽量远程恢复：

- 通过 motion journal 和最近位置检查点恢复当前位置。
- 检查点有效且在安全范围内时，恢复为 `IdlePartial` 或可判断的端点状态。
- 可信恢复后的第一次动作不需要 Web 确认；系统自动使用 `recoveredFirstMoveSpeedPercent`，最大脉冲和最大时间按剩余距离保守计算。
- 低可信恢复、恢复记录冲突或连续恢复失败时，才要求 Web 确认、进入维护模式或保持故障。
- 检查点无效、越界或与限位冲突时，才进入 `PositionUnknown` 或 `Fault`。

## 下一阶段限位增强

启用开门/上限位后：

- `POST /api/app/maintenance/calibrate-open-limit` 只发起低速寻限位流程，不在 HTTP handler 内等待完成。
- 触发上限位后立即按到位策略停机，保存开门端点。
- 普通开门/关门达到最大时长或最大圈数时，先保护停止并记录 warning/保护事件；是否升级为 `Fault` 取决于当时命令、位置可信度、编码器/电流状态和限位状态。
- 只有在“明确执行上限位校准/寻限位流程”且达到该流程的最大时长或最大圈数仍未触发上限位时，才按校准失败进入 `Fault`。
- 限位断线、异常方向触发、上下限位冲突都进入 `Fault`，通过 faultReason 区分。
