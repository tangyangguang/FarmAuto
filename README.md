# FarmAuto

FarmAuto 是养殖自动化设备 monorepo，规划包含两个独立应用：

- Esp32FarmDoor：鸡舍/养殖场自动门控制器。
- Esp32FarmFeeder：三路喂食器控制器。

当前阶段已从分析、策划和文档进入源码阶段。公共库源码骨架已创建，`apps/Esp32FarmDoor` 和 `apps/Esp32FarmFeeder` 已创建最小可编译应用骨架。

正式文档按编号放在 `docs/` 中，建议从 [docs/00-overview.md](docs/00-overview.md) 开始阅读。

## 重要边界

- 只引用同级目录的 Esp32Base，不修改 Esp32Base。
- `old_prj/` 只读参考，只用于识别和完善需求，不参考其实现方案。
- 首版不接入 Blinker、MQTT 或云端控制协议。
- 公共库沉淀可跨项目复用的硬件/存储能力，不包含具体应用项目独有逻辑。

## Source Status

Current source work includes public library skeletons under `lib/` and minimal compile-ready application shells under `apps/Esp32FarmDoor` and `apps/Esp32FarmFeeder`.
Implemented host-tested application logic currently includes business record codecs, record file rotation, feeder scheduling, feeder bucket state handling, fixed payload codecs for AT24C-backed recovery/state data, and AT24C128 RecordStore startup restore/write-back glue.
The current firmware still does not output real motor PWM or read real encoder counts; those hardware paths require bench validation before enabling.

## Core Board Smoke Test

For a new ESP32 board, upload both firmware and the LittleFS image. The firmware uses Esp32Base `FULL` profile and expects a LittleFS partition for file log and business records.

```bash
platformio run -d apps/Esp32FarmDoor -e esp32e_full -t upload --upload-port /dev/cu.usbserial-130
platformio run -d apps/Esp32FarmDoor -e esp32e_full -t uploadfs --upload-port /dev/cu.usbserial-130

platformio run -d apps/Esp32FarmFeeder -e esp32e_full -t upload --upload-port /dev/cu.usbserial-130
platformio run -d apps/Esp32FarmFeeder -e esp32e_full -t uploadfs --upload-port /dev/cu.usbserial-130
```

Without AT24C128 or motor hardware connected, startup should still complete. I2C warnings are expected on a bare core board; PWM and motor output remain disabled in the current firmware.

After the board joins WiFi, run non-destructive HTTP smoke tests:

```bash
./tools/smoke_http.sh door http://192.168.2.156 admin admin
./tools/smoke_http.sh feeder http://192.168.2.156 admin admin
```

The smoke script checks business pages, read-only JSON APIs, and unauthenticated API rejection. It does not trigger motor output, calibration writes, feeding, or door movement.
