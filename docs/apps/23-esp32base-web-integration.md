# Esp32Base Web 集成边界

## 目标

本文定义 FarmAuto 应用 Web 页面、业务 API、业务记录与 Esp32Base 内置系统页面之间的边界。具体业务 API 按应用拆分，自动门见 `docs/apps/24-esp32-farmdoor-web-api.md`，喂食器见 `docs/apps/25-esp32-farmfeeder-web-api.md`。

核心原则：

- Esp32Base 负责系统级 Web、系统日志、系统工具、OTA、WiFi、认证和 App Config。
- FarmAuto 应用负责业务状态、控制命令、维护流程、业务记录和业务诊断。
- 页面和 API 命名必须能让远程维护时一眼区分“系统问题”和“业务问题”。
- 不重复实现 Esp32Base 已有系统能力；如 Esp32Base 能力不足，只整理提示词到 Esp32Base 项目处理。

## Esp32Base 内置能力

已确认的 Esp32Base 系统页面和 API 采用 `/esp32base/*` 命名空间：

| 能力 | 路径 | FarmAuto 使用方式 |
| --- | --- | --- |
| 系统状态 | `/esp32base` | 查看芯片、固件、网络、存储、Watchdog 和启动信息 |
| 系统 API | `/esp32base/api/*` | 只用于系统级状态、芯片、固件、网络、OTA 等 |
| WiFi 设置 | `/esp32base/wifi` | 配网和清除 WiFi 凭据 |
| Web 认证 | `/esp32base/auth` | 管理本地 Web 认证 |
| 系统日志 | `/esp32base/logs` | 查看运行时文本日志 |
| 系统工具 | `/esp32base/tools` | 主机名、文件日志、重启等系统维护能力 |
| App Config | `/esp32base/app-config` | 低频系统参数和硬件参数配置 |
| OTA | `/esp32base/ota` | 固件升级 |

Esp32Base Runtime FileLog 默认写入 `/logs/eb_app.log`，它是系统级运行诊断日志，不是 FarmAuto 的长期业务记录。

## 系统日志与业务记录

必须区分三类数据：

| 类型 | 归属 | 推荐名称 | 用途 | 保存方式 |
| --- | --- | --- | --- | --- |
| 系统日志 | Esp32Base | System Logs / 系统日志 | 启动、WiFi、OTA、文件系统、route 错误、运行时异常 | Esp32Base FileLog，入口 `/esp32base/logs` |
| 最近业务事件 | 应用层 | Recent Events / 最近事件 | 首页和诊断页快速理解最近发生了什么 | 应用 RAM 小环形缓冲，可同步写入长期记录 |
| 长期业务记录 | 应用层 | Records / 业务记录 | 开关门、投喂、故障、维护、配置变化、补料和存储告警 | 应用记录服务，ESP32 flash segment + 必要索引 |

命名规则：

- 应用页面不使用 `/logs` 作为业务入口。
- 业务长期记录页面命名为 `/records`。
- 应用诊断页面命名为 `/diagnostics`。
- 应用 API 不使用 `/api/logs`，业务事件用 `/api/app/events/recent`，长期记录用 `/api/app/records`。
- 只有 Esp32Base 的运行时文本日志称为“系统日志”。

## 页面命名

每个固件只运行一个应用，因此应用页面可以使用简短路径；系统页面仍保留 `/esp32base/*`，但 FarmAuto 静态原型只制作业务页面，不制作 Esp32Base 系统页面。

通用应用页面：

| 路径 | 名称 | 归属 |
| --- | --- | --- |
| `/` | 应用首页 / Dashboard | 应用 |
| `/records` | 业务记录 / Records | 应用 |
| `/diagnostics` | 业务诊断 / Diagnostics | 应用 |
| `/esp32base/app-config` | 系统参数 / App Config | Esp32Base，业务原型不制作 |
| `/esp32base/logs` | 系统日志 / System Logs | Esp32Base，业务原型不制作 |

应用专用页面：

| 路径 | 名称 | 归属 |
| --- | --- | --- |
| `/calibration` | 自动门行程校准 | Esp32FarmDoor |
| `/schedule` | 喂食器计划 / Schedule | Esp32FarmFeeder |
| `/base-info` | 喂食器基础信息 / Base Info | Esp32FarmFeeder |

不建议使用 `/config` 作为应用页面。低频系统参数优先进入 `/esp32base/app-config`；业务基础信息使用 `/base-info`，避免和 Esp32Base 系统配置混淆。

## API 命名

Esp32Base 系统 API 使用 `/esp32base/api/*`。FarmAuto 应用 API 统一使用 `/api/app/*`，避免和基础库系统 API 或未来通用 API 冲突。

两个应用是不同固件、不同设备、不同 WebServer，因此可以各自使用同名的 `/api/app/status`、`/api/app/records` 等通用入口。但 payload、业务字段、页面和处理逻辑必须按应用独立定义，不能把自动门字段和喂食器字段混在同一个结构中。

通用应用 API：

| API | 用途 |
| --- | --- |
| `GET /api/app/status` | 应用业务状态 snapshot |
| `GET /api/app/events/recent` | 最近业务事件 |
| `GET /api/app/records` | 长期业务记录分页查询 |
| `GET /api/app/records/export` | 长期业务记录导出，后续增强项 |
| `GET /api/app/diagnostics` | 应用诊断包摘要 |
| `POST /api/app/maintenance/clear-fault` | 清除业务故障，危险操作 |

具体业务 API 不在本文展开：

- Esp32FarmDoor API 见 `docs/apps/24-esp32-farmdoor-web-api.md`。
- Esp32FarmFeeder API 见 `docs/apps/25-esp32-farmfeeder-web-api.md`。

如果 Esp32Base route 数量或 RAM 压力不足，源码前可把部分命令收敛到 `POST /api/app/command`，但对外文档仍应保留清晰的业务 action 名称和 payload schema。

## App Config 使用边界

优先放入 Esp32Base App Config：

- 电机方向、速度、软启动、软停止、计数模式、最大运行时间。
- 编码器参数、减速比、输出轴每圈脉冲。
- INA240A2 硬件参数、阈值和启用状态。
- 长期记录容量告警阈值、是否启用相关功能。
- 喂食器多通道启动间隔、保护阈值和硬件启用状态。

不能放入 Esp32Base App Config：

- 当前门位置、行程校准中的临时状态。
- 每日计划时间、每路目标、通道基础信息、饲料桶容量和余量维护等需要专门交互的业务配置。
- 当前估算余量、补料动作、跳过今日运行态。
- 今日计数、最近故障、最近事件。
- 长期业务记录正文。
- 任何需要高频写入或随业务动作变化的数据。

这些业务数据由应用页面、AT24C 关键状态和 ESP32 flash 长期记录共同管理。

## 导航集成

FarmAuto 业务原型只展示应用导航：Dashboard、Control、Maintenance、Records、Diagnostics，以及 Feeder 的 Schedule、Buckets、Calibration。

系统日志、OTA、WiFi、系统工具、文件系统格式化等基础库能力由 Esp32Base 自己提供，FarmAuto 业务页不重复做原型、不复制系统日志入口，也不能把业务记录写入 `/logs/eb_app.log`。

## 源码前检查

进入源码前必须用 Esp32Base 当前 head 做一次能力验证，不按“未来会支持”推进 FarmAuto 源码。

检查清单：

- Esp32Base route 数量是否足够承载应用页面和 `/api/app/*`。
- App Config groups/fields 容量是否足够。
- App Config 是否支持当前所需字段类型、范围校验、默认值和分组展示。
- Esp32BaseFs 是否具备 appendBytes、readBytesAt、writeBytesAt、fileSize、listDir、totalBytes、usedBytes、freeBytes，且 LittleFS 分区容量可配置到 1MB 起步。
- 记录导出是否需要 Esp32Base 增强流式下载能力。
- 同步 Web handler 在 200ms 目标内是否稳定。
- Web Auth、系统日志、OTA、WiFi 页面是否可同时启用。
- OTA 由 Esp32Base 提供，FarmAuto 不实现 OTA 页面、上传、回滚或 mark-valid 业务逻辑；源码前只确认基础库 OTA 能力可用。
- 危险操作 confirm token 首版由应用实现短期 token；暂不要求 Esp32Base 提供通用能力。
- 系统日志页面和应用记录页面在导航上是否清楚区分。

如果上述能力在 Esp32Base 中不足，只整理 Esp32Base 项目提示词，不在 FarmAuto 中实现重复系统能力。

## 当前只读核对结果

截至本轮文档检查，Esp32Base 当前目录已有以下能力证据：

- `Esp32BaseAppConfig` 支持 int、decimal、bool、enum 字段；groups/fields 由 `ESP32BASE_APP_CONFIG_MAX_GROUPS` 和 `ESP32BASE_APP_CONFIG_MAX_FIELDS` 编译期配置，范围分别为 1..16 和 1..128。
- `Esp32BaseFs` 已暴露 appendBytes、readBytesAt、writeBytesAt、fileSize、listDir、totalBytes、usedBytes、freeBytes 等长期记录所需底层文件 API。
- `Esp32BaseFileLog` 已存在，适合系统日志，不作为 FarmAuto 业务记录。
- `Esp32BaseWeb` 已有 Auth 开关和校验接口。
- OTA 文档和代码中已有 rollback / mark-valid timeout 能力。FarmAuto 不重新定义 OTA 策略；如果应用构建需要打开或关闭相关宏，应按 Esp32Base 文档执行。

仍需源码前实测：

- FarmAuto 两个应用各自实际 App Config 字段数量是否低于 128。
- 实际 route 数量、JSON 响应大小和 handler 200ms 目标是否满足。
- LittleFS 分区表是否给业务记录至少 1MB，容量允许时 2MB。
- Esp32Base OTA 是否在目标分区表和目标板上完成实机验证。
