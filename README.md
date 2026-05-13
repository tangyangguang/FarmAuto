# FarmAuto

FarmAuto 是养殖自动化设备 monorepo，规划包含两个独立应用：

- FarmDoor：鸡舍/养殖场自动门控制器。
- FarmFeeder：三路喂食器控制器。

当前阶段只做分析、策划和文档，不进入编码阶段。

正式文档按编号放在 `docs/` 中，建议从 [docs/00-overview.md](docs/00-overview.md) 开始阅读。

## 重要边界

- 只引用同级目录的 Esp32Base，不修改 Esp32Base。
- `old_prj/` 只读参考，只用于识别和完善需求，不参考其实现方案。
- 首版不接入 Blinker、MQTT 或云端控制协议。
- 公共库只沉淀两个应用都真实需要的硬件/存储能力。

