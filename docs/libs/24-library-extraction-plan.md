# 公共库独立化计划

## 当前决策

当前不一次性把三个公共库全部独立出去。

原因：

- 三个公共库仍处于接口设计阶段。
- API 尚未经过真实应用验证。
- 过早独立会增加仓库、版本、发布和依赖管理成本。
- 应用需求尚未完全确认，公共库边界仍可能小幅调整。

但公共库独立化是近期目标。当前阶段要按未来独立库标准设计 FarmAuto 内部 `lib/`，避免后续拆分困难。

## 分两步推进

### 第一步：在 FarmAuto 内按独立库标准设计

目标：

- 公共库文档保持应用无关。
- 公共库 API 不出现具体应用业务词汇。
- 公共库核心逻辑尽量不强绑定 Esp32Base。
- 明确每个库未来独立时需要的 `library.json`、README、examples 和测试策略。

完成条件：

- 三个公共库接口级方案冻结。
- 每个库的配置、状态、错误码和首版边界明确。
- 至少一个真实设备项目对公共库的组合方式明确。
- 没有应用业务逻辑泄漏到公共库文档或未来源码中。

### 第二步：按稳定度逐个拆出独立仓库

拆分不应一次完成，而应按稳定度和复用价值逐个推进。

推荐顺序：

1. `Esp32At24cRecordStore`
2. `Esp32EncodedDcMotor`
3. `Esp32MotorCurrentGuard`

理由：

- `Esp32At24cRecordStore` 业务耦合最低，最适合先独立。
- `Esp32EncodedDcMotor` 需要通过至少两个真实设备场景验证后再拆。
- `Esp32MotorCurrentGuard` 涉及多个传感器后端，首版只有 INA240A2，成熟度最低，最后拆更稳。

## 独立仓库命名建议

`Esp32At24cRecordStore`：

- 推荐名称：`Esp32At24cRecordStore`。
- FarmAuto 内和近期独立仓库均使用 `Esp32At24cRecordStore`。
- 原因：项目公共库统一使用 `Esp32` 前缀，且首版面向 ESP32 项目验证。
- 如果未来确实发展为跨 Arduino 平台通用库，再另行评估是否去掉 `Esp32` 前缀。

`Esp32EncodedDcMotor`：

- 推荐名称：`Esp32EncodedDcMotor`。
- 原因：首版大概率依赖 ESP32 LEDC、ESP32Encoder 和 ESP32 Arduino Core。
- 如果后续抽出跨平台驱动接口，再考虑去掉 `Esp32` 前缀。

`Esp32MotorCurrentGuard`：

- 推荐名称：`Esp32MotorCurrentGuard`。
- 如果实现依赖 ESP32 ADC、LEDC 时序或 ESP32 Wire 细节，使用 `Esp32MotorCurrentGuard`。
- 如果未来保护判定层能完全脱离 ESP32，可再评估是否拆出更通用的核心库。

## 每个库拆分前必须满足的条件

通用条件：

- 有清晰 README。
- 有 `library.json`。
- 有至少 2 个 examples。
- 有首版 API 文档。
- 有版本号和 CHANGELOG。
- 有明确 license。
- 不依赖 FarmAuto 应用代码。
- 不包含具体应用项目业务词汇。

质量条件：

- 能独立编译。
- 至少有基础单元测试或示例验证流程。
- 错误码和状态输出足够诊断问题。
- API 命名稳定，没有明显会马上推翻的设计。

## FarmAuto 如何引用独立库

独立前：

```text
FarmAuto/lib/<LibraryName>
```

独立后：

- 应用项目通过 PlatformIO `lib_deps` 引用 GitHub 仓库或发布版本。
- FarmAuto 不再复制库源码。
- FarmAuto 文档保留使用说明和版本约束。
- 库自身 README 负责通用用法。

## 与 Esp32Base 的关系

独立公共库不应默认成为 Esp32Base 的一部分。

只有满足以下条件，才考虑沉淀到 Esp32Base：

- 多个无关项目都需要。
- 能力属于 ESP32 系统底座，而不是具体硬件部件。
- API 已稳定。
- 加入 Esp32Base 不会显著增加体积和概念负担。

当前三个库都更适合作为独立硬件/存储库，而不是并入 Esp32Base。

## 当前近期计划

近期先完成：

1. 冻结三个公共库接口级方案。
2. 完善真实设备项目需求设计。
3. 根据应用需求确认公共库首版边界。
4. 再决定是否先拆出 `Esp32At24cRecordStore`。

在以上工作完成前，不创建三个独立仓库。
