# FarmAuto Codex 项目规则

## 基本原则

- 先整体评估，再给方案或改动；不要跳过需求澄清。
- 不做临时补丁，不为了旧实现背负设计包袱。
- 简单优先，只做当前目标需要的最小合理方案。
- 发现多种理解时要列出来；不清楚就说明哪里不清楚并询问。
- 每次回复都要说明当前还剩哪些工作。

## 当前阶段

- 当前阶段以分析、策划和文档为主。
- 未经用户明确同意，不进入编码阶段。
- 未经用户明确同意，不创建 PlatformIO 应用工程或公共库源码骨架。
- 可以创建和修改 Markdown 文档、README、AGENTS.md、.gitignore 等规划类文件。

## 仓库与目录

- 工作区：`/Users/tyg/dir/claude_dir/FarmAuto`。
- 正式文档统一放在 `docs/`，并使用编号方便阅读。
- 应用文档放在 `docs/apps/`。
- 公共库文档放在 `docs/libs/`。
- 未来应用目录命名使用 `apps/Esp32FarmDoor` 和 `apps/Esp32FarmFeeder`。
- 未来公共库目录命名使用 `lib/EncodedDcMotor`、`lib/MotorCurrentGuard`、`lib/At24cRecordStore`。

## 只读边界

- Esp32Base 位于 `/Users/tyg/dir/claude_dir/Esp32Base`，本项目只能引用，不能修改。
- `old_prj/` 只读参考，不能修改，不能提交到 GitHub。
- 老项目只用于提取和完善需求，不参考其架构、类设计、存储格式或实现流程。
- 发现 Esp32Base 能力缺口或 bug 时，只整理提示词让用户到 Esp32Base 项目处理，不在 FarmAuto 中打补丁。

## 应用与公共库边界

- 使用 Esp32Base 的应用项目名称必须以 `Esp32` 开头。
- 应用项目只放具体业务：自动门、三路喂食、Web 交互、业务状态和历史统计。
- 公共库只放可跨项目复用的硬件或存储能力。
- 公共库文档不出现具体应用项目独有逻辑。
- 公共库核心逻辑尽量不强绑定 Esp32Base；需要时通过薄适配层接入。

## 持久化原则

- 用户数据、配置、日志、统计、校准参数必须保护。
- 持久化结构变化前必须先给出版本、迁移、备份或恢复方案。
- 首版不迁移 old_prj 的 AT24C 数据，但 FarmAuto 新格式一旦使用，后续必须保护。

## Git 工作流

- 仓库远端：`https://github.com/tangyangguang/FarmAuto.git`。
- 完成一项明确工作后要及时提交并推送。
- 提交前检查 `git status`，避免提交 `old_prj/`、`.pio/`、本地缓存或敏感信息。
- 不使用破坏性 git 命令，除非用户明确要求。

## 回复要求

- 回复使用中文。
- 说明已完成内容、验证情况、提交哈希（如有）。
- 每次结尾都要列出剩余工作或下一步建议。
