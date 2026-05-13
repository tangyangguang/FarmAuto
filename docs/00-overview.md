# FarmAuto 工作区总览

## 项目定位

FarmAuto 是养殖自动化设备 monorepo，首版规划两个独立应用：

- Esp32FarmDoor：鸡舍/养殖场自动门控制器。
- Esp32FarmFeeder：三路喂食器控制器。

本工作区只引用同级目录的 Esp32Base 基础库：

```text
/Users/tyg/dir/claude_dir/Esp32Base
```

FarmAuto 不修改 Esp32Base 内任何文件。遇到明确属于 Esp32Base 的能力缺口或可复现 bug 时，只整理问题提示词，让用户到 Esp32Base 项目处理。

## 阶段范围

当前阶段只做分析、策划和文档，不进入编码阶段，不创建 PlatformIO 工程，不实现源码。

首版产品目标是本地可靠控制：

- 设备提供本地 Web 控制和状态查看。
- 远程访问由 VPN、Tailscale、路由器端口映射或内网穿透解决。
- 首版不接入 Blinker、MQTT 或云端控制协议。

## 老项目使用原则

`old_prj/` 下两个老项目只读参考，不能修改：

- `old_prj/prj1_RC_Lifting_Door_Client_V5_AT8236_newPCB`
- `old_prj/prj2_RC_Feeder_V2_newPCB`

老项目只用于识别需求线索，不参考其实现方案、类设计、存储格式、流程写法或历史包袱。新项目应先完善需求，再独立给出当前最佳方案。

## 正式文档阅读顺序

- `00-overview.md`：工作区目标、阶段范围、只读规则。
- `01-architecture.md`：monorepo 架构、应用边界、公共库准入原则。
- `02-roadmap.md`：整体实施路线图和阶段验收标准。
- `apps/10-esp32-farmdoor-rewrite-plan.md`：Esp32FarmDoor 自动门重写方案。
- `apps/11-esp32-farmfeeder-rewrite-plan.md`：Esp32FarmFeeder 三路喂食器重写方案。
- `apps/12-application-state-machines.md`：应用状态机草案。
- `apps/13-web-api-and-maintenance.md`：Web/API 与维护功能草案。
- `apps/14-configuration-and-defaults.md`：配置项、默认值与保存策略草案。
- `libs/20-public-library-boundaries.md`：公共库边界与 Esp32Base 关系。
- `libs/21-esp32-encoded-dc-motor.md`：Esp32EncodedDcMotor 公共电机库方案。
- `libs/22-esp32-motor-current-guard.md`：Esp32MotorCurrentGuard 电流保护库方案。
- `libs/23-esp32-at24c-record-store.md`：Esp32At24cRecordStore 记录存储库方案。
- `libs/24-library-extraction-plan.md`：公共库独立化计划。
- `30-persistence-and-migration.md`：持久化、版本、升级和恢复原则。
- `15-hardware-resource-budget.md`：硬件资源预算草案。
- `16-at24c-layout-budget.md`：AT24C 容量预算草案。
- `17-test-and-acceptance.md`：测试与验收计划草案。
