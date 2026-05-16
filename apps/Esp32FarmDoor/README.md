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

编译验证：

```bash
pio run -d apps/Esp32FarmDoor
```
