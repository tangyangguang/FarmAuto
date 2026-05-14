# 需要用户确认的决策入口

## 目标

本文集中记录需要用户确认的设计决策，并标明应该阅读哪个文档。当前阶段仍只做文档和设计，不进入源码阶段。

后续所有“需要用户确认”的内容都应先汇总到本文，再从本文链接到详细方案文档，避免散落在各方案文档中。

## 确认方式

- 先看本文，确认当前有哪些决策需要回复。
- 对某一项有疑问时，再看“主文档”和“辅助文档”。
- 如果接受推荐值，可以直接回复“接受 Cx 推荐值”。
- 如果不接受，请说明要改哪一项，以及倾向值。

## 当前最需要确认

| 编号 | 确认事项 | 推荐处理 | 主文档 | 辅助文档 | 需要你回复什么 |
| --- | --- | --- | --- | --- | --- |
| C1 | 三个公共库是否按推荐决策进入接口冻结默认值 | 建议接受，实测项保留为后续校准 | `docs/libs/25-public-library-freeze-decisions.md` | `docs/libs/21-esp32-encoded-dc-motor.md`、`docs/libs/22-esp32-motor-current-guard.md`、`docs/libs/23-esp32-at24c-record-store.md` | 是否接受 C1；如不接受，指出具体项 |
| C2 | 公共库字段表是否作为源码骨架前的接口冻结基线 | 建议接受为语义基线，源码命名可小幅调整 | `docs/libs/26-public-library-interface-fields.md` | `docs/libs/25-public-library-freeze-decisions.md` | 是否接受 C2 |
| C3 | 公共库测试和 examples 是否作为首版验收要求 | 建议接受，避免源码阶段只写能编译但不可验证的库 | `docs/libs/27-public-library-test-checklist.md`、`docs/libs/28-public-library-examples.md` | `docs/17-test-and-acceptance.md` | 是否接受 C3 |
| C4 | AT24C128 record region、slotCount、高频写入预算和低层后端策略 | 建议接受两个应用各自独立 AT24C128 的预算草案，实机前按 writesPerDay 再算一遍；低层后端首版使用自研最小 `At24cDevice` | `docs/16-at24c-layout-budget.md` | `docs/libs/23-esp32-at24c-record-store.md`、`docs/libs/29-at24c-low-level-driver-evaluation.md` | 是否接受当前容量预算和低层后端策略 |
| C5 | 自动门故障时默认 `Coast` 的安全性 | 先按 `Coast` 写入方案，但标为必须实机验证 | `docs/apps/10-esp32-farmdoor-rewrite-plan.md` | `docs/apps/12-application-state-machines.md`、`docs/apps/14-configuration-and-defaults.md` | 是否确认先按 `Coast` 作为默认方案 |
| C6 | 自动门断电恢复后位置可信的判定规则 | 建议采用“提交成功 + 限位不冲突 + 状态记录完整”才可信 | `docs/apps/10-esp32-farmdoor-rewrite-plan.md` | `docs/apps/12-application-state-machines.md`、`docs/30-persistence-and-migration.md` | 是否接受该判定规则 |
| C7 | 自动门和喂食器实际 GPIO、LEDC、ADC、I2C 资源 | 进入硬件图或源码前必须确认 | `docs/15-hardware-resource-budget.md` | 两个应用文档和三个公共库文档 | 提供或确认硬件资源表 |
| C8 | 喂食器每日计划细节 | 建议首版只做每日一次或多次固定时间，支持跳过今日，不做复杂日历 | `docs/apps/11-esp32-farmfeeder-rewrite-plan.md` | `docs/apps/12-application-state-machines.md`、`docs/apps/14-configuration-and-defaults.md` | 确认每日时间、漏执行处理、时间无效处理 |
| C9 | 喂食器停止全部策略 | 建议普通停止同时请求所有运行通道软停止；故障急停同时请求所有运行通道急停 | `docs/apps/11-esp32-farmfeeder-rewrite-plan.md` | `docs/apps/12-application-state-machines.md` | 是否接受推荐停止策略 |
| C10 | Web/API 与远程维护范围 | 建议先接受当前本地 Web + JSON API + 危险操作二次确认 | `docs/apps/13-web-api-and-maintenance.md` | `docs/17-test-and-acceptance.md` | 是否接受首版 Web/API 范围 |
| C11 | 长期原始记录策略 | 多数已确认；实现前还需确认实际 flash 分区容量 | `docs/apps/18-long-term-records.md` | `docs/30-persistence-and-migration.md`、`docs/03-user-decisions.md` 的 D1-D8 | 确认实际分区容量或接受默认 1MB/2MB 策略 |
| C12 | Web 页面原型是否作为首版页面信息架构 | 建议接受，先按状态控制、配置、维护、记录四类页面实现 | `docs/apps/19-web-page-prototypes.md` | `docs/apps/13-web-api-and-maintenance.md` | 是否接受 C12；如不接受，指出页面或信息项调整 |
| C13 | 应用系统参数是否使用 Esp32Base App Config | 建议接受；业务系统不重复实现参数配置页和普通参数持久化 | `docs/apps/14-configuration-and-defaults.md` | Esp32Base README 的 App Config 说明 | 是否接受 C13 |
| C14 | 饲料桶余量感知路线 | 建议第一版软件扣减估算，下一阶段每路料桶称重 | `docs/apps/20-feeder-bucket-level-sensing.md` | `docs/apps/11-esp32-farmfeeder-rewrite-plan.md` | 是否接受 C14 |

## 公共库决策

### Esp32EncodedDcMotor

详细设计看 `docs/libs/21-esp32-encoded-dc-motor.md`，冻结前推荐决策看 `docs/libs/25-public-library-freeze-decisions.md`。

| 决策 | 推荐值 | 为什么需要确认 |
| --- | --- | --- |
| `countMode` 默认值 | `X1` | 推荐已明确，用户只需确认是否接受为默认 |
| `softStartMs` 默认值 | 1000ms | 不同机械负载可能不同，后续可实测覆盖 |
| `softStopMs` 默认值 | 500ms | 不同机械负载可能不同，后续可实测覆盖 |
| `minEffectiveSpeedPercent` 默认值 | 15% | 需要实测验证低速是否有力 |
| `normalStopMode` 默认值 | `SoftStopThenBrake` | 无人值守默认停稳，但机械冲击需实测 |
| `emergencyOutputMode` 默认值 | 公共库不固定，设备配置 `Brake` 或 `Coast` | 故障刹车可能造成机械冲击 |

### Esp32MotorCurrentGuard

详细设计看 `docs/libs/22-esp32-motor-current-guard.md`，冻结前推荐决策看 `docs/libs/25-public-library-freeze-decisions.md`。

| 决策 | 推荐值 | 为什么需要确认 |
| --- | --- | --- |
| 首版芯片 | 只实现 INA240A2 | 目前真实使用，ACS712/INA226 暂不实现 |
| 是否实现 `CurrentTraceBuffer` | 可选工具类，不作为首版必需 | 图表需要数据，但不一定要库内缓存 |
| `filterAlpha` 默认值 | 0.2 | 实测前起点，需按噪声调整 |
| `startupGraceMs` 默认值 | 1000ms 或跟随电机软启动 | 需要和具体电机启动行为匹配 |
| 传感器故障策略 | 默认按故障处理 | 无人值守场景下传感器不可信不能当正常 |

### Esp32At24cRecordStore

详细设计看 `docs/libs/23-esp32-at24c-record-store.md`，容量预算看 `docs/16-at24c-layout-budget.md`，冻结前推荐决策看 `docs/libs/25-public-library-freeze-decisions.md`。

| 决策 | 推荐值 | 为什么需要确认 |
| --- | --- | --- |
| 首版实测型号 | AT24C128 | 当前已知硬件目标 |
| 支持型号范围 | 首版承诺 AT24C128，推荐兼容 AT24C32/64/256/512 | AT24C02/04/08/16 如需特殊寻址代码，首版不支持 |
| 存储模型 | wear-levelled record ring | 独立最佳实践设计，避免老项目路径依赖 |
| 是否做全局动态 wear leveling | 首版不做 | 静态记录环更简单可靠；高频数据靠 slotCount 和保存频率解决 |
| 是否做坏块管理 | 首版不做 | 先用多槽、CRC、诊断；实测需要再加 |
| FRAM 支持 | 首版不纳入，未来独立库 | 避免 EEPROM 和 FRAM 概念混杂 |
| 高频记录 slotCount | 按 writesPerDay 估算后确认 | 直接影响 EEPROM 寿命 |

已确认原则：

- 只要涉及持久化存储，都必须把磨损均衡作为重要要求。
- 不允许为了快速实现而反复覆盖同一物理地址。
- 不同存储介质可以使用不同磨损均衡策略，但必须在方案中明确写出。
- 关键小状态优先使用可更换的 AT24C128；多年原始记录优先使用 ESP32 flash 文件系统。
- AT24C 和 ESP32 flash 都必须有损坏检测、容量诊断和远程告警。

## 应用需求决策

### 两个应用共同原则

长期记录看 `docs/apps/18-long-term-records.md`，持久化和迁移原则看 `docs/30-persistence-and-migration.md`。

| 决策 | 当前结论 | 备注 |
| --- | --- | --- |
| 关键小状态存储 | AT24C128 优先 | 外置、可更换；使用 wear-levelled record ring |
| 普通系统参数配置 | Esp32Base App Config 优先 | 复用基础库内置配置页、字段校验和低频持久化；应用不重复造配置系统 |
| 多年原始记录 | ESP32 flash 文件系统优先 | 容量更大；使用 LittleFS + segment 轮转 |
| 存储异常告警 | 必须支持 | AT24C 和 flash 都要暴露损坏检测、容量诊断和远程告警 |
| 写入压力分配 | 小状态重复写优先放 AT24C，大容量追加记录放 flash | 如果长期运行后有介质损耗，优先希望更容易更换的 AT24C 承担关键小写入 |
| 磨损均衡 | 所有持久化存储都必须设计 | 不允许固定地址反复覆盖 |
| 关键数据记录 | 尽量全面记录有价值数据，但不过度设计 | 原始值优先、结构化记录、按价值筛选，不记录无意义高频噪声 |

### Esp32FarmDoor

详细需求看 `docs/apps/10-esp32-farmdoor-rewrite-plan.md`，状态机看 `docs/apps/12-application-state-machines.md`，配置默认值看 `docs/apps/14-configuration-and-defaults.md`。

| 决策 | 当前结论 | 备注 |
| --- | --- | --- |
| 开门/上限位是否作为首版硬件必选项 | 第一版不加 | 便于直接烧录到现有设备；开门/上限位作为下一阶段优先增强 |
| 关门/下限位是否作为首版硬件必选项 | 否，可选预留 | 关门首版依赖编码器目标、最大放绳脉冲和最大运行时间防止过放 |
| 开门/上限位型号和安装 | 下一阶段推荐工业滚轮摆杆限位开关，门框顶部或卷线盘凸轮触发，预留 5mm 到 10mm 行程 | 首选 OMRON D4N/D4B-N；空间受限可用 OMRON D4CC；不推荐裸微动开关外露 |
| 故障时 `Brake` 还是 `Coast` | 倾向 `Coast` | 避免故障瞬间机械冲击；仍需实机验证 |
| 断电恢复后保存位置是否可信 | 存储提交成功且限位状态不冲突时应可信 | 重新上电后应能正常工作；若限位冲突或状态不一致，进入远程维护/故障 |
| 原始记录保存 | 尽量完整、尽量长，目标按多年考虑 | 首版优先考虑 ESP32 flash 文件系统，AT24C128 不作为多年原始日志主存储 |

### Esp32FarmFeeder

详细需求看 `docs/apps/11-esp32-farmfeeder-rewrite-plan.md`，状态机看 `docs/apps/12-application-state-machines.md`，配置默认值看 `docs/apps/14-configuration-and-defaults.md`。

| 决策 | 当前结论 | 备注 |
| --- | --- | --- |
| 是否需要定时投喂 | 需要 | 首版只支持每天执行 |
| 是否支持跳过今日 | 需要 | 跳过今日不删除长期计划 |
| 每日定时默认时间 | 不设置默认时间 | 未配置时间时不自动投喂 |
| 错过计划时间是否补投喂 | 不补投喂 | 只记录 missed 事件，避免无人值守重复投喂 |
| 日期/时间来源失败时 | 暂停自动定时，允许手动投喂 | 手动和定时独立，但不能同时执行 |
| 多路运行方式 | 顺序启动后并行运行 | 主要为降低启动电流叠加；启动间隔可配置 |
| 停止全部策略 | 普通停止同时请求各路软停止；故障急停同时请求各路急停 | 停止不产生启动浪涌，同时停止更安全 |
| 单路故障时其他路是否继续 | 继续 | 故障通道停止并记录，其他通道按计划继续 |
| 投喂目标配置 | 同时支持克数模式和圈数模式 | 克数贴近投喂目标，圈数适合标定前和调试 |
| 饲料桶容量 | 每路独立配置 | 每路饲料桶大小可能不同 |
| 饲料桶余量感知 | 第一版软件扣减估算，下一阶段每路独立称重 | 称重直接测重量，比超声/光电更适合“还剩多少饲料” |
| 历史记录保存粒度 | 原始记录尽量完整，时间尽量长，目标按多年考虑 | 首版优先考虑 ESP32 flash 文件系统；不能按旧方案只保留少量历史 |

## 尚需继续确认的应用细节

- 自动门故障 `Coast` 的实机安全性：看 `docs/apps/10-esp32-farmdoor-rewrite-plan.md`。
- 自动门保存位置可信的判定条件和限位交叉校验规则：看 `docs/apps/10-esp32-farmdoor-rewrite-plan.md` 和 `docs/apps/12-application-state-machines.md`。
- 自动门、喂食器实际 GPIO/LEDC/ADC/I2C 资源：看 `docs/15-hardware-resource-budget.md`。
- 喂食器每日计划默认时间、漏执行处理、时间无效处理：看 `docs/apps/11-esp32-farmfeeder-rewrite-plan.md` 和 `docs/apps/14-configuration-and-defaults.md`。
- 喂食器停止全部策略：看 `docs/apps/11-esp32-farmfeeder-rewrite-plan.md`。
- Web/API 首版范围和危险操作二次确认方式：看 `docs/apps/13-web-api-and-maintenance.md`。
- Web 页面原型和信息架构：看 `docs/apps/19-web-page-prototypes.md`。

## 已确认的长期记录决策

| 编号 | 问题 | 已确认值 | 原因 |
| --- | --- | --- | --- |
| D1 | ESP32 flash 文件系统分区容量 | 1MB 起步，若 flash 容量允许用 2MB | 多年记录需要空间；1MB 是较稳起点，2MB 更从容 |
| D2 | 长期记录分段方式 | 按天且超大小再切分 | 远程查询按日期自然，单日异常大量记录也能控制文件大小 |
| D3 | 容量满时策略 | 覆盖最旧并告警 | 无人值守场景不应停止记录；覆盖前告警，保留最近多年数据 |
| D4 | 导出格式 | JSON 分页 + CSV 导出 | JSON 适合 Web/API，CSV 适合人工分析 |
| D5 | 普通记录 flush 策略 | 条数或时间批量，关键事件立即 | 平衡掉电丢失风险和 flash 磨损 |
| D6 | AT24C 写入寿命告警阈值 | 70% warning，90% maintenance | 提前维护，避免等到不可恢复 |
| D7 | Flash 容量告警阈值 | 30% warning，10% maintenance | 更早提醒远程导出和维护，给无人值守设备留足处理时间 |
| D8 | 长期记录是否压缩 | 首版不压缩，使用紧凑二进制记录 | 降低实现和恢复复杂度；需要时再加导出侧压缩 |
