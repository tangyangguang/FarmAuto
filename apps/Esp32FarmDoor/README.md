# Esp32FarmDoor

`Esp32FarmDoor` 是自动门控制器应用工程。

当前状态：

- 已接入 Esp32Base FULL profile。
- 已接入 `Esp32At24cRecordStore`、`Esp32EncodedDcMotor`、`Esp32MotorCurrentGuard`。
- 已接入自动门业务状态机 `DoorController`。
- 已提供状态、诊断、最近事件、Flash/RAM 记录、开门、关门、停止、位置标定、行程设置、行程微调和清除故障 API。
- 已接入自动门恢复状态二进制编解码，后续可作为 AT24C128 payload。
- 已固化自动门 AT24C128 记录区布局，并用 host 测试校验容量、连续性和页对齐。
- 已接入首页、记录、校准、诊断 4 个最小业务页面，系统参数/日志/OTA/WiFi 仍使用 Esp32Base 页面。
- 已接入自动门业务最近记录 RAM 缓冲、Flash 二进制追加记录和基础文件轮转。
- 已记录当前 PCB 默认引脚，包括 INA240A2 输出 GPIO33。
- 已提供 `FARMAUTO_FARMDOOR_ENABLE_INA240A2` 编译开关，默认打开软件支持，但运行配置默认不启用电流保护动作。
- 所有业务控制 API 当前只更新业务状态机，明确返回 `motorOutput.enabled=false`，不会输出 PWM，也不会驱动 AT8236。

当前尚未实现：

- 最终版业务页面的精细交互和视觉样式。
- AT24C128 Wire 设备适配。
- AT24C128 关键状态写入/读取。
- AT8236 LEDC 驱动适配。
- 编码器 PCNT 适配。
- GPIO33 INA240A2 电流换算、零点校准和保护停机策略。

## 当前 API

`/api/app/status`

- 返回应用类型、固件版本、门状态、当前位置、行程配置、INA240A2 配置状态和电机/编码器概要。
- 该接口不执行任何硬件动作。

`/api/app/diagnostics`

- 只读硬件诊断接口，适合首次烧录后检查当前 PCB。
- 返回按钮 GPIO 电平、编码器 A/B 当前电平、GPIO33 ADC 原始值和 8 次采样的 min/max/avg、AT24C128 `0x50` 在线状态。
- 明确返回 `motorOutput.enabled=false`，不会输出 PWM，也不会驱动 AT8236。

`/api/app/events/recent`
`/api/app/records`

- `/api/app/events/recent` 返回 RAM 最近业务记录。
- `/api/app/records` 优先返回 Flash 业务记录；无 Flash 数据时回退 RAM 最近记录。
- `/api/app/records` 参数：`start`、`limit`、`startUnixTime`、`endUnixTime`、`eventType`。
- 记录覆盖开门/关门/停止、位置标定、行程设置、行程微调和清除故障。

`/api/app/door/open`

- 请求业务状态机进入开门状态。
- 如果位置未可信标定，返回 `PositionUntrusted`。
- 当前不输出电机 PWM。

`/api/app/door/close`

- 请求业务状态机进入关门状态。
- 如果位置未可信标定，返回 `PositionUntrusted`。
- 当前不输出电机 PWM。

`/api/app/door/stop`

- 请求业务状态机停止当前动作。
- 当前还没有真实编码器位置接入，因此停止位置使用当前 snapshot 位置。

`/api/app/maintenance/set-position`

- `position=closed`：把当前位置标为关门基准。
- `position=open`：把当前位置标为开门位置。
- `position=unknown`：标记位置不可信。
- `positionPulses=<value>&trustLevel=Trusted|Limited|Untrusted`：直接设置位置和可信等级。

`/api/app/maintenance/set-travel`

- `openTurnsX100=<value>`：按 0.01 圈设置开门目标。
- `openTargetPulses=<value>`：按编码器脉冲设置开门目标。
- 可选 `maxRunPulses=<value>`；未提供时按开门目标的 150% 生成保底上限。
- 成功时同步写入 Esp32Base App Config 的 `door/openTurns`。

`/api/app/maintenance/adjust-travel`

- `deltaTurnsX100=<value>` 或 `deltaPulses=<value>`：对当前开门目标做微调。
- 成功时同步写入 Esp32Base App Config 的 `door/openTurns`。

`/api/app/maintenance/clear-fault`

- 清除业务状态机故障。

编译验证：

```bash
pio run -d apps/Esp32FarmDoor
```

首次烧录建议流程：

```bash
pio run -d apps/Esp32FarmDoor -t upload
```

启动后先访问 Esp32Base 提供的网络/系统页面完成 WiFi 或 AP 模式确认，再访问：

```text
/api/app/diagnostics
```

先确认：

- `at24c.online` 是否为 `true`。
- `buttons.open/close/stop` 按下和松开时是否变化。
- `encoder.a/b` 手动转动电机输出轴时是否变化。
- `currentSensor.rawAvg` 是否有稳定读数，`rawMin` 和 `rawMax` 差值是否较小。

这些信息确认前，不建议启用任何电机输出或自动门动作。
