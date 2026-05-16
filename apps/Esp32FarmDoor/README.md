# Esp32FarmDoor

`Esp32FarmDoor` 是自动门控制器应用工程。

当前状态是最小可编译骨架：

- 已接入 Esp32Base FULL profile。
- 已接入 `Esp32At24cRecordStore`、`Esp32EncodedDcMotor`、`Esp32MotorCurrentGuard`。
- 已记录当前 PCB 默认引脚，包括 INA240A2 输出 GPIO33。
- 已提供 `FARMAUTO_FARMDOOR_ENABLE_INA240A2` 编译开关，默认打开软件支持，但运行配置默认不启用电流保护动作。

当前尚未实现：

- 自动门业务状态机。
- Web 业务页面和 `/api/app/*`。
- AT24C128 Wire 设备适配。
- AT8236 LEDC 驱动适配。
- 编码器 PCNT 适配。
- GPIO33 ADC 采样和校准。

## 当前 API

`/api/app/status`

- 返回应用类型、固件版本、骨架状态、INA240A2 配置状态和电机/编码器概要。
- 该接口不执行任何硬件动作。

`/api/app/diagnostics`

- 只读硬件诊断接口，适合首次烧录后检查当前 PCB。
- 返回按钮 GPIO 电平、编码器 A/B 当前电平、GPIO33 ADC 原始值和 8 次采样的 min/max/avg、AT24C128 `0x50` 在线状态。
- 明确返回 `motorOutput.enabled=false`，不会输出 PWM，也不会驱动 AT8236。

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
