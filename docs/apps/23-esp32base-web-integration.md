# Esp32Base Web 集成边界

## 目标

本文定义 FarmAuto 应用 Web 页面、业务 API、业务记录与 Esp32Base 内置系统页面之间的边界。

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
| 系统工具 | `/esp32base/tools` | 主机名、文件日志、重启、格式化文件系统等 |
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

每个固件只运行一个应用，因此应用页面可以使用简短路径；系统页面仍保留 `/esp32base/*`。

通用应用页面：

| 路径 | 名称 | 归属 |
| --- | --- | --- |
| `/` | 应用首页 / Dashboard | 应用 |
| `/control` | 控制页 / Control | 应用 |
| `/maintenance` | 维护页 / Maintenance | 应用 |
| `/records` | 业务记录 / Records | 应用 |
| `/diagnostics` | 业务诊断 / Diagnostics | 应用 |
| `/esp32base/app-config` | 系统参数 / App Config | Esp32Base |
| `/esp32base/logs` | 系统日志 / System Logs | Esp32Base |

Esp32FarmFeeder 额外建议页面：

| 路径 | 名称 | 归属 |
| --- | --- | --- |
| `/schedule` | 投喂计划 / Schedule | 应用 |
| `/buckets` | 饲料桶 / Buckets | 应用 |
| `/calibration` | 下料标定 / Calibration | 应用 |

不建议使用 `/config` 作为应用页面。低频系统参数优先进入 `/esp32base/app-config`；业务配置应按领域命名，例如 `/schedule`、`/buckets`、`/calibration`。

## API 命名

Esp32Base 系统 API 使用 `/esp32base/api/*`。FarmAuto 应用 API 统一使用 `/api/app/*`，避免和基础库系统 API 或未来通用 API 冲突。

通用应用 API：

| API | 用途 |
| --- | --- |
| `GET /api/app/status` | 应用业务状态 snapshot |
| `GET /api/app/events/recent` | 最近业务事件 |
| `GET /api/app/records` | 长期业务记录分页查询 |
| `GET /api/app/records/export` | 长期业务记录导出 |
| `GET /api/app/diagnostics` | 应用诊断包摘要 |
| `POST /api/app/maintenance/format-app-storage` | 格式化应用业务存储，危险操作 |
| `POST /api/app/maintenance/clear-fault` | 清除业务故障，危险操作 |

Esp32FarmDoor API：

| API | 用途 |
| --- | --- |
| `POST /api/app/door/open` | 发起开门 |
| `POST /api/app/door/close` | 发起关门 |
| `POST /api/app/door/stop` | 停止当前运动 |
| `POST /api/app/maintenance/jog` | 维护点动 |
| `POST /api/app/maintenance/set-position` | 设置当前位置，危险操作 |
| `POST /api/app/maintenance/save-endpoints` | 保存开关门端点，危险操作 |
| `POST /api/app/maintenance/verify-endpoints` | 发起低速端点验证 |
| `POST /api/app/maintenance/calibrate-open-limit` | 下一阶段上限位校准 |
| `POST /api/app/maintenance/calibrate-current-zero` | INA240A2 零点校准 |

Esp32FarmFeeder API：

| API | 用途 |
| --- | --- |
| `POST /api/app/feeders/{channel}/start` | 发起单路投喂 |
| `POST /api/app/feeders/{channel}/stop` | 停止单路 |
| `POST /api/app/feeders/start-all` | 顺序启动全部 |
| `POST /api/app/feeders/stop-all` | 停止全部 |
| `POST /api/app/schedule/skip-today` | 跳过今日定时投喂 |
| `POST /api/app/schedule/cancel-skip-today` | 取消跳过今日 |
| `POST /api/app/buckets/{channel}/set-remaining` | 设置当前估算余量 |
| `POST /api/app/buckets/{channel}/add-feed` | 记录加料量 |
| `POST /api/app/buckets/{channel}/mark-full` | 标记已加满 |
| `POST /api/app/maintenance/clear-today` | 清空当天计数，危险操作 |
| `POST /api/app/maintenance/calibrate-feed-rate` | 标定每圈下料量 |
| `POST /api/app/maintenance/test-channel` | 单路小剂量测试 |

如果 Esp32Base route 数量或 RAM 压力不足，源码前可把部分命令收敛到 `POST /api/app/command`，但对外文档仍应保留清晰的业务 action 名称和 payload schema。

## App Config 使用边界

优先放入 Esp32Base App Config：

- 电机方向、速度、软启动、软停止、计数模式、最大运行时间。
- 编码器参数、减速比、输出轴每圈脉冲。
- INA240A2 硬件参数、阈值和启用状态。
- 长期记录容量告警阈值、是否启用相关功能。
- 喂食器每日计划时间、启动间隔、每路默认目标。

不能放入 Esp32Base App Config：

- 当前门位置、端点维护中的临时状态。
- 当前估算余量、补料动作、跳过今日运行态。
- 今日计数、最近故障、最近事件。
- 长期业务记录正文。
- 任何需要高频写入或随业务动作变化的数据。

这些业务数据由应用页面、AT24C 关键状态和 ESP32 flash 长期记录共同管理。

## 导航集成

推荐导航分组：

- 应用导航：Dashboard、Control、Maintenance、Records、Diagnostics，以及 Feeder 的 Schedule、Buckets、Calibration。
- 系统导航：System Status、App Config、System Logs、WiFi、OTA、Auth、System Tools。

应用诊断页可以提供跳转到 `/esp32base/logs` 的链接，但不能把系统日志复制成业务记录，也不能把业务记录写入 `/logs/eb_app.log`。

## 源码前检查

进入源码前需要确认：

- Esp32Base route 数量是否足够承载应用页面和 `/api/app/*`。
- App Config groups/fields 容量是否足够。
- 记录导出是否需要 Esp32Base 增强流式下载能力。
- 危险操作 confirm token 由应用实现还是需要 Esp32Base 提供通用能力。
- 系统日志页面和应用记录页面在导航上是否清楚区分。

如果上述能力在 Esp32Base 中不足，只整理 Esp32Base 项目提示词，不在 FarmAuto 中实现重复系统能力。
