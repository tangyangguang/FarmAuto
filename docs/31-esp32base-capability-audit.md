# Esp32Base 能力复用审计

本文记录 FarmAuto 编码阶段对 `/Users/tyg/dir/claude_dir/Esp32Base` 的只读审计结论。后续新增应用功能前，必须先检查本文和 Esp32Base 当前 API，避免重复造基础能力。

## 审计结论

当前 FarmAuto 代码没有自行实现 WiFi、NTP、WebServer、OTA、系统日志、LittleFS、NVS 配置框架或系统重启流程。已编码部分主要复用 `Esp32Base::begin()` / `handle()`、`Esp32BaseWeb`、`Esp32BaseNtp`、`Esp32BaseAppConfig` 和 `ESP32BASE_LOG_*`。

需要继续坚持的原则：

- 不直接 include `WiFi.h`、`WebServer.h`、`LittleFS.h`、`Preferences.h`、`Update.h` 或 SNTP 相关头文件。
- 不直接调用 `ESP.restart()`、`esp_restart()`、`configTime()`、`sntp_*()`。
- 不自己实现系统日志、系统配置页、WiFi 配网、OTA 页面、认证页面、文件系统格式化页面。
- 如果 Esp32Base 能力不足，只整理提示词到 Esp32Base 项目处理，不在 FarmAuto 内打补丁。

## Esp32Base 已有能力

| 能力 | Esp32Base API / 页面 | FarmAuto 使用策略 |
| --- | --- | --- |
| 固件信息与生命周期 | `Esp32Base::setFirmwareInfo()`、`begin()`、`handle()` | 两个应用统一调用，不另做启动框架 |
| 日志 | `ESP32BASE_LOG_*`、`Esp32BaseLog`、`Esp32BaseFileLog`、`/esp32base/logs` | 系统日志走基础库；业务记录另建结构化记录，不写入系统日志文件 |
| NVS 小配置 | `Esp32BaseConfig`、`Esp32BaseAppConfig`、`/esp32base/app-config` | 系统/低频参数注册到 App Config；业务运行状态不放这里 |
| WiFi / 配网 | `Esp32BaseWiFi`、`/esp32base/wifi` | 只复用，不做业务 WiFi 页面 |
| NTP / 可信时间 | `Esp32BaseNtp::snapshot()`、`timestamp()`、`onTimeSynced()`、`bootId`、`uptimeSec` | 自动投喂只用基础库时间快照，不自己对时 |
| mDNS / DNS captive portal | `Esp32BaseMdns`、`Esp32BaseDns` | 不在应用内重复实现 |
| Web / API | `Esp32BaseWeb` route、auth、chunked response、JSON helpers | 业务 API 用 `/api/app/*`；系统页面保持 `/esp32base/*` |
| Web Auth | `Esp32BaseWeb::checkAuth()`、内置 `/esp32base/auth` | 正式设备复用基础认证，不做独立用户系统 |
| OTA | `Esp32BaseOta`、`/esp32base/ota` | FarmAuto 不实现 OTA 页面、上传、回滚或 mark-valid 逻辑 |
| LittleFS 文件 API | `Esp32BaseFs` | 多年业务记录后续通过该 API 实现，不直接操作 LittleFS |
| 系统重启 | `Esp32BaseSystem::restart(reason)` | 应用如需重启必须走基础库生命周期 |
| Watchdog / Health | `Esp32BaseWatchdog`、`Esp32BaseHealth` | 不自己喂狗或做系统健康页 |
| 事件总线 | `Esp32BaseBus` | 只有多个模块确实需要解耦事件时再用；当前不强行引入 |

## 当前代码复用检查

| 项目 | 当前状态 |
| --- | --- |
| `apps/Esp32FarmDoor` | 使用 Esp32Base FULL profile、Web API helper、App Config、日志；未直接使用 WiFi/WebServer/LittleFS/Preferences/NTP/OTA |
| `apps/Esp32FarmFeeder` | 使用 Esp32Base FULL profile、Web API helper、App Config、日志、NTP snapshot；未直接实现基础网络或文件系统功能 |
| `lib/*` 公共库 | 核心逻辑不依赖 Esp32Base，保持可独立复用；日志、Web、配置由应用薄接入 |

`platformio.ini` 中列出 `WiFi`、`WebServer`、`LittleFS` 等库是因为 Esp32Base FULL profile 的链接依赖，不代表 FarmAuto 应用代码可以直接使用这些底层库。

允许的例外：

- 应用可以使用标准 C/C++ 的轻量时间换算函数，例如 `localtime_r()`，但时间来源必须来自 `Esp32BaseNtp::snapshot()` 或其他 Esp32Base 可信时间 API。应用不得自己启动 SNTP、配置 NTP server 或判断时间是否可信。

## 后续编码前检查清单

新增功能前先问：

1. 是否属于系统能力：WiFi、OTA、系统日志、系统配置、文件系统格式化、认证、NTP、Watchdog、重启、设备状态？
2. Esp32Base 是否已有公开 API 或页面？
3. FarmAuto 是否只需要业务薄接入，而不是重新实现底层？
4. 如果 Esp32Base 没有，是否真的阻塞当前需求？
5. 如果阻塞，是否应该整理 Esp32Base 项目提示词，而不是在 FarmAuto 内绕开？

## 当前应保持在 FarmAuto 的能力

这些不是 Esp32Base 职责，仍应放在 FarmAuto：

- 自动门业务状态机、行程校准、端点可信度、卷绳保护策略。
- 喂食器通道状态机、计划执行、手工下料、料桶余量、单路目标换算。
- 业务 API `/api/app/*` 的字段定义和业务校验。
- 业务记录 schema、AT24C 关键状态布局、长期业务记录 segment。
- AT24C 记录存储、电机控制、电流保护等 FarmAuto 公共硬件库。

## 发现的注意点

- FarmFeeder API 路由数已超过 Esp32Base 默认 16，但 Esp32Base 明确支持 `ESP32BASE_WEB_MAX_ROUTES` 编译期配置；当前应用设置为 32，属于正确复用基础库能力，不需要改基础库。
- FarmFeeder 计划执行已改为读取 `Esp32BaseNtp::snapshot()`，不再需要任何应用级 NTP 设计。
- 长期记录后续应使用 `Esp32BaseFs`，并避免在 HTTP handler 中全量扫描或长时间阻塞。
- 危险操作二次确认当前文档仍规划为应用内最小 token；如果两个应用大量重复后再评估是否沉淀到 Esp32Base。
