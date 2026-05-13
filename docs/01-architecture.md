# FarmAuto 总体架构

## 目录结构

推荐工作区结构：

```text
FarmAuto/
  docs/
    00-overview.md
    01-architecture.md
    02-farmdoor-rewrite-plan.md
    03-farmfeeder-rewrite-plan.md
    04-encoded-dc-motor.md
    05-motor-current-guard.md
    06-at24c-record-store.md
    07-persistence-and-migration.md

  apps/
    FarmDoor/
    FarmFeeder/

  lib/
    EncodedDcMotor/
    MotorCurrentGuard/
    At24cRecordStore/

  old_prj/
```

当前阶段不创建 `apps/` 和 `lib/` 源码工程；以上结构先作为后续实施目标。

## 依赖方向

依赖方向必须保持单向：

```text
apps/FarmDoor      -> lib/* -> Esp32Base
apps/FarmFeeder    -> lib/* -> Esp32Base
old_prj            -> 只读参考，不参与依赖
```

公共库不能依赖应用项目。应用项目可以组合多个公共库完成业务控制。

## 应用边界

FarmDoor 负责自动门业务：

- 开门、关门、停止。
- 物理按钮。
- Web 状态与配置。
- 编码器位置、归零、校准。
- INA240A2 电流保护。
- 门相关配置、状态和事件持久化。

FarmFeeder 负责三路喂食业务：

- 三路独立启动/停止。
- 启动全部、停止全部。
- 单次喂食目标配置。
- 今日累计与 7 天历史。
- 清空当天计数。
- 电流保护与堵转检测。
- 喂食器相关配置、状态和事件持久化。

两个应用不共享业务逻辑，不做成同一个固件。

## 公共库准入原则

只有两个应用都明确需要，且不属于 Esp32Base 职责的能力，才进入 `lib/`。

首版规划三个公共库：

- `EncodedDcMotor`：带编码器 DC 电机运动控制。
- `MotorCurrentGuard`：电机电流采样与过流/堵转辅助保护。
- `At24cRecordStore`：AT24C 系列 I2C EEPROM 可靠记录存储。

暂不抽取的能力：

- Web 页面/API：应用差异大，留在各自应用。
- 业务历史统计：目前只属于 FarmFeeder。
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

- 两个应用方案边界清晰。
- 三个公共库职责清晰，不混入业务逻辑。
- 持久化结构版本化，并有明确初始化、升级和恢复原则。
- 老项目需求线索已评估，但新实现不继承老项目设计包袱。
- 首版不接入 Blinker、MQTT 或云控协议。
