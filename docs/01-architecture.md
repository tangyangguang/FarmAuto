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
- Web 页面原型、控制 API 和远程维护流程。
- 编码器位置、开门/上限位、远程端点校准。
- 关门/下限位作为可选增强，不作为首版必需条件。
- INA240A2 电流保护。
- 门相关配置、状态和事件持久化。

Esp32FarmFeeder 负责三路喂食业务：

- 三路独立启动/停止。
- 启动全部、停止全部。
- Web 页面原型、控制 API 和远程维护流程。
- 单次喂食目标配置。
- 每路今日累计、长期原始记录和多年历史查询。
- 每路饲料桶容量配置和余量估算。
- 清空当天计数。
- 每日定时投喂、跳过今日、不补投错过计划。
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
- 日志展示：优先使用 Esp32Base 文件日志，应用可维护小型最近事件缓存。
- 物理按钮交互：不同应用语义不同，暂不抽库。

## Esp32Base 使用原则

FarmAuto 应使用 Esp32Base 提供的基础能力，例如 WiFi、Web、运行时日志、I2C 总线、文件系统、Watchdog 和 OTA profile。

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
