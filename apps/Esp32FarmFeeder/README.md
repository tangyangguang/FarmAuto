# Esp32FarmFeeder

`Esp32FarmFeeder` 是三路喂食器控制器应用工程。

当前状态是最小可编译骨架：

- 已接入 Esp32Base FULL profile。
- 已接入 `FeederController` 纯业务状态机。
- 已接入 `Esp32At24cRecordStore`、`Esp32EncodedDcMotor`、`Esp32MotorCurrentGuard` 作为后续实现依赖。
- 默认三路通道均已安装且启用。
- 当前不会输出 PWM，也不会驱动任何电机。

当前尚未实现：

- 喂食计划持久化。
- 手工下料 API。
- 业务记录。
- GPIO、编码器、PWM 和 AT24C128 硬件适配。
- 饲料桶余量维护。

## 当前 API

`/api/app/status`

- 返回应用类型、固件版本、设备状态、通道 mask、每路通道状态。
- 明确返回 `motorOutput.enabled=false`，不会执行下料动作。

编译验证：

```bash
pio run -d apps/Esp32FarmFeeder
```
