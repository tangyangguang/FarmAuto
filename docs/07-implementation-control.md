# FarmAuto 实现阶段控制方法

本文用于约束代码实现阶段，目标是防止代码偏离产品架构、复杂化或失控。

## 总原则

- 按已确认架构推进：主控管业务，分站管执行能力，协议传递有界动作。
- 先做可验证纵向闭环，再横向补模块。
- 不按代码行数设目标；代码多少由实际职责决定，但每个模块必须有清楚边界。
- 不为未确认的大场景提前做框架、插件系统、多总线网关或复杂抽象。
- 使用 `Esp32Base` 和 `Stc8hBase` 前，先验证基础库自身环境和能力入口。
- 基础库缺少通用能力时，不在 FarmAuto 项目内绕过或复制实现；先沉淀到基础库，再回到 FarmAuto 引用。
- 每个阶段结束都提交并推送，保持历史可回退。

## 每轮实现前检查

开始一个实现任务前，必须明确：

- 本轮目标是什么。
- 本轮不做什么。
- 依赖哪些已确认文档。
- 会新增或修改哪些目录。
- 如何在没有硬件时验证。

如果发现需求会改变协议、数据模型、硬件边界或持久化结构，先同步文档，再写代码。

如果实现任务需要基础库能力，必须先确认：

- `Esp32Base` 或 `Stc8hBase` 的本地路径存在。
- 相关文档和示例能定位到。
- 基础库自检命令可运行，或明确记录当前无法运行的原因。
- FarmAuto 只调用基础库公开能力，不复制基础库内部实现。

## 当前纵向闭环顺序

第一条闭环按以下顺序推进：

1. `shared/protocol`：RS485 帧、CRC、命令码、payload 编解码。
2. 分站动作控制纯逻辑：配置、启动、目标到达、停止、过流、堵转、超时。
3. fake 分站：用协议命令驱动动作控制，用模拟编码器和电流生成状态。
4. 主控最小调用骨架：绑定一个地址，下发配置和手动下料动作，轮询到完成。
5. 记录最小闭环：写入动作开始、完成、停止原因和完成脉冲。

当前已经补到可见纵向闭环：

- ESP32 主控工程和 STC8H8K64U 分站工程均可编译。
- 分站协议节点能在 host smoke 中响应 `PING`、`SET_MOTOR_CONFIG`、`START_ACTION`、`GET_STATUS`、`STOP_ACTION` 和 `CLEAR_FAULT`。
- 主控 RS485 transport 已接入可配置 UART、RX/TX/DE、`115200 8N1` 默认波特率和请求超时。
- `/feed` 和 `/door` 手动入口能在 RS485 未配置时 dry-run 构造动作；RS485 就绪时会下发电机配置、启动动作，并由主控动作运行时轮询到终态后写入 LittleFS 动作记录。
- `/devices` 保存业务设备和分站绑定，显示设备启用状态、名称、显示顺序、分站在线状态，并提供设备启停、名称编辑、显示顺序调整、绑定分站和分站 `CLEAR_FAULT` 维护入口。
- 动作记录保存当时的 `deviceId`、设备名称快照和 `busAddress`，设备后续改名或重新绑定不改变历史记录展示。
- `/bus` 支持扫描 `1..127` 地址并把应答分站写入设备注册表。
- 主控后台在无动作运行时低频轮询启用分站，更新在线/离线/错误状态。
- 主控日志覆盖手动动作预览、真实发送、动作跟踪、轮询失败、动作终态、分站状态变化和总线扫描摘要，便于无板和上板阶段对照。

下一步优先继续补齐“可见、可验证”的主控功能闭环；自动计划、FRAM pending-action journal、真实巴法云/微信发送、分站 OTA、缺料检测和真实板级 IO 联调仍然后置。

## 模块边界

`shared/protocol` 只允许包含：

- 帧格式。
- CRC。
- 命令码、状态码、故障码和能力位。
- payload 读写。
- 字节流解析。

`firmware/station-stc8h/src/action` 只允许包含：

- 通用电机动作状态机。
- 本地保护判断。
- 输出给底层驱动的期望状态。

它不能直接访问 GPIO、PWM、ADC、UART 或具体引脚。

主控业务层只通过 RS485 master 接口操作分站，不直接假设分站内部实现。

## 验证要求

没有 PCB 时，至少保持这些 smoke test 可运行：

```sh
pio run -d firmware/master-esp32
pio run -d firmware/station-stc8h
make -C tools/protocol_smoke
make -C tools/action_smoke
make -C tools/fake_station_smoke
make -C tools/master_loop_smoke
make -C tools/feed_service_smoke
make -C tools/door_service_smoke
make -C tools/station_node_smoke
make -C tools/station_board_smoke test
make -C tools/action_record_smoke
make -C tools/device_registry_smoke test
```

后续新增 fake 分站和主控闭环后，也必须提供 host-side smoke test，验证：

- 主控发 `SET_MOTOR_CONFIG`。
- 主控发 `START_ACTION`。
- fake 分站进入运行。
- fake 编码器推进到目标。
- fake 分站回 `MOTOR_COMPLETED`。
- 主控读取状态并形成动作结果。

## 停止条件

出现以下情况时，不继续堆代码，先回到文档或方案：

- 需要改 RS485 帧格式或命令含义。
- 需要改设备/分站/动作核心模型。
- 需要写死未确认 PCB 引脚或硬件参数。
- 需要的基础库能力不存在、不可用或 API 不适合当前用途。
- 一个模块开始承担两个以上层级职责。
- 为了未来可能性引入当前闭环用不到的大框架。
- smoke test 不能覆盖当前新增逻辑的主要路径。

## 提交策略

- 明确阶段完成后直接提交并推送。
- 每个提交只包含一个清晰主题。
- 不把无关改动混进功能提交。
- 如果工作区存在用户改动，能一起提交且同主题则提交；不同主题则分开提交。
