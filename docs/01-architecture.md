# FarmAuto 总体架构

## 目录结构

推荐工作区结构：

```text
FarmAuto/
  docs/
    00-overview.md
    01-architecture.md
    02-roadmap.md
    03-user-decisions.md
    30-persistence-and-migration.md
    apps/
      10-esp32-farmdoor-rewrite-plan.md
      11-esp32-farmfeeder-rewrite-plan.md
      12-application-state-machines.md
      13-web-api-and-maintenance.md
      14-configuration-and-defaults.md
      18-long-term-records.md
      19-web-page-prototypes.md
      20-feeder-bucket-level-sensing.md
      21-web-workflows.md
      22-record-event-schema.md
      23-esp32base-web-integration.md
      24-esp32-farmdoor-web-api.md
      25-esp32-farmfeeder-web-api.md
    libs/
      20-public-library-boundaries.md
      21-esp32-encoded-dc-motor.md
      22-esp32-motor-current-guard.md
      23-esp32-at24c-record-store.md
      24-library-extraction-plan.md
      25-public-library-freeze-decisions.md
      26-public-library-interface-fields.md
      27-public-library-test-checklist.md
      28-public-library-examples.md
      29-at24c-low-level-driver-evaluation.md
    15-hardware-resource-budget.md
    16-at24c-layout-budget.md
    17-test-and-acceptance.md

  apps/
    Esp32FarmDoor/
    Esp32FarmFeeder/

  lib/
    Esp32EncodedDcMotor/
    Esp32MotorCurrentGuard/
    Esp32At24cRecordStore/

  old_prj/
```

当前阶段不创建 `apps/` 和 `lib/` 源码工程；以上结构先作为后续实施目标。

源码阶段必须保持目录边界清晰：

- `apps/Esp32FarmDoor/` 只放自动门应用固件、页面/API、业务状态机、应用配置和应用持久化 schema。
- `apps/Esp32FarmFeeder/` 只放喂食器应用固件、页面/API、业务状态机、应用配置和应用持久化 schema。
- `lib/Esp32EncodedDcMotor/` 只放通用带编码器 DC 电机库。
- `lib/Esp32MotorCurrentGuard/` 只放通用电流采样与保护判定库。
- `lib/Esp32At24cRecordStore/` 只放通用 AT24C 记录存储库。
- 两个应用不能互相 include 源码；只能共同依赖 `lib/` 和 Esp32Base。
- 公共库不能 include `apps/` 内任何文件。
- 应用独有 recordType、Web route、默认配置和业务文案必须留在对应应用目录。
- Web/API 文档也按应用拆分：公共约定写在 `docs/apps/13-web-api-and-maintenance.md`，自动门写在 `docs/apps/24-esp32-farmdoor-web-api.md`，喂食器写在 `docs/apps/25-esp32-farmfeeder-web-api.md`。

## 依赖方向

依赖方向必须保持单向：

```text
apps/Esp32FarmDoor      -> lib/* -> Esp32Base
apps/Esp32FarmFeeder    -> lib/* -> Esp32Base
old_prj            -> 只读参考，不参与依赖
```

公共库不能依赖应用项目。应用项目可以组合多个公共库完成业务控制。

## 应用边界

Esp32FarmDoor 负责自动门业务：

- 开门、关门、停止。
- 物理按钮。
- Web 状态与配置。
- Web 页面原型、自动门控制 API 和远程维护流程。
- 编码器位置、无限位首版远程端点维护。
- 开门/上限位作为下一阶段优先增强；关门/下限位作为可选增强。
- INA240A2 电流保护。
- 门相关配置、状态和事件持久化。

Esp32FarmFeeder 负责三路喂食业务：

- 首页支持所选通道手动下料和单路停止。
- 定时计划支持多通道顺序启动，运行中支持停止全部。
- Web 页面原型、喂食器控制 API 和远程维护流程。
- 单次手动下料目标配置。
- 每路今日累计、长期原始记录和多年历史查询。
- 每路饲料桶容量配置和余量估算；第一版软件扣减，下一阶段优先称重增强。
- 清空当天计数。
- 每日定时投喂、跳过某次计划执行、不补投错过计划。
- 编码器无脉冲、最大时间、最大脉冲保护。
- 电流检测仅做未来硬件预留。
- 喂食器相关配置、状态和事件持久化。

两个应用不共享业务逻辑，不做成同一个固件。

## 公共库准入原则

只有多个项目可能复用，且不属于 Esp32Base 职责的能力，才进入 `lib/`。公共库文档不写具体应用项目独有逻辑；具体应用如何组合公共库，写在 `docs/apps/`。

首版规划三个公共库：

- `Esp32EncodedDcMotor`：带编码器 DC 电机运动控制。
- `Esp32MotorCurrentGuard`：电机电流采样与过流/堵转辅助保护。
- `Esp32At24cRecordStore`：AT24C 系列 I2C EEPROM 可靠记录存储。

暂不抽取的能力：

- Web 页面/API：应用差异大，留在各自应用。
- 业务历史统计：属于具体应用。
- 开门/关门/投喂语义：留在应用控制器。
- 系统日志展示：使用 Esp32Base 文件日志和 `/esp32base/logs`。
- 业务记录展示：应用维护最近事件缓存和长期结构化记录，不写入 Esp32Base 系统日志。
- 物理按钮交互：不同应用语义不同，暂不抽库。

## Esp32Base 使用原则

FarmAuto 应使用 Esp32Base 提供的基础能力，例如 WiFi、Web、运行时日志、I2C 总线、文件系统、Watchdog、App Config 和 OTA profile。

Web 集成边界见 `docs/apps/23-esp32base-web-integration.md`：

- Esp32Base 系统页面和系统 API 使用 `/esp32base/*`。
- FarmAuto 应用业务 API 使用 `/api/app/*`。
- `/api/app/*` 在两个应用固件中可以同名，因为它们运行在不同设备上；具体 payload 和业务字段必须按应用独立定义。
- Esp32Base 系统日志入口为 `/esp32base/logs`，用于排查运行时和基础设施问题。
- FarmAuto 业务长期记录入口为 `/records` 和 `/api/app/records`，用于开关门、投喂、维护、故障和配置变化回溯。
- 普通低频系统参数优先注册到 `/esp32base/app-config`；业务运行数据不放 App Config。

如果实施时发现 Esp32Base 缺少必要能力，或存在可复现 bug，不在 FarmAuto 内打补丁。应整理给 Esp32Base 项目的提示词，包含：

- 背景。
- 当前需求。
- 现状问题。
- 复现方式或证据。
- 影响范围。
- 为什么 FarmAuto 不应临时绕开。

## 首版成功标准

- 两个 Esp32 应用方案边界清晰。
- 三个公共库职责清晰，不混入业务逻辑。
- 持久化结构版本化，并有明确初始化、升级和恢复原则。
- 老项目需求线索已评估，但新实现不继承老项目设计包袱。
- 首版不接入 Blinker、MQTT 或云控协议。
