# ADR 0023: 文件型 Memory、Soul 与 Skill 的边界

- 状态：提案
- 日期：2026-10-01

## 背景

产品计划把 Soul、长期 Memory 和 Skill 保存为文件系统中的 Markdown 文件，并在
每次构造模型输入时按需使用。文件格式相同不代表领域语义相同：Soul 描述 Agent
身份，Memory 保存可检索的长期内容，Skill 是上层显式注册后才可用的能力。
Session 历史另由 Storage Provider 保存；有文件系统时可以选择 JSONL 后端。

Core 不应依赖 POSIX、ESP-IDF 或 openvela 的文件系统 API，也不应通过扫描目录
隐式启用 Skill。本文规定所有权、读取与投影边界，不规定 Markdown 文件的具体
目录布局或新的公共 C 接口。

## 决定

### 领域归属与模型投影分离

| 来源 | 原始状态与加载责任 | 模型输入中的位置 | 启用条件 |
|---|---|---|---|
| Soul | Memory 模块管理的受信任身份文件 | 基础指令的一部分 | 产品显式配置；缺失时的行为由产品配置决定 |
| 长期 Memory | Memory 实现管理的 Markdown 文件与检索索引 | 经筛选的有界 Context 贡献 | 产品启用 Memory，且本轮检索命中 |
| Skill | 应用或可选加载器管理的 Markdown 文件 | 已注册 Skill 的有界指令贡献；相关 Tool 仍单独投影 | 上层显式注册并启用 |
| Session | 独立的 Session Storage Provider | 有序 `messages[]` | 本轮历史窗口非零且已绑定 Storage |

Soul 归 Memory 模块管理，但不按普通 Memory 条目参与相关性检索、排名、
过期淘汰或可选内容截断。它应在本轮开始前形成稳定快照；不能在一次模型请求
构造过程中因文件变化而改变。Soul 是产品管理的身份内容，不替代 Tool Policy、
权限检查或应用强制的安全规则。Memory 文件和历史 Session 内容不得自动提升
为 Soul 或系统权限。

Skill 文件存在不等于已注册。未注册的 Skill 对 Core 不可见，不参与 Context
组装，也不自动暴露 Tool。对于大量 Skill，应用可以先加载目录索引，再只读取
已注册且本轮被选中的内容；具体选择和加载策略不进入 Core 文件系统接口。

### 统一编排，而非统一领域 Storage ops

Core 负责本轮模型输入的统一编排、预算、顺序、取消和失败语义，不负责打开
Markdown 文件。Memory 与 Skill 可以复用由应用或平台提供的文件读取能力，
但不复用 `agent_session_storage_ops_t`：Session 的完整 turn 提交和历史顺序
不适合表示文件读取、Memory 检索或 Skill 注册。

```text
Application / platform filesystem
  |-- Memory loader: Soul snapshot + bounded Memory retrieval
  `-- Skill loader: explicitly registered Skill content

Session Storage Provider: bounded complete-turn history

Core projection
  |-- system_prompt: application base instructions, Soul, selected Skill/Memory
  |-- messages[]: Session history and current turn
  `-- tools[]: registered, model-visible Tool definitions
```

文件 I/O 若需跨平台复用，可在可选的文件后端定义窄接口并由应用或 Port 注入；
Core 公共 API 不预设 `open/read/write/rename`，也不要求所有平台具有文件系统。
Memory 的更新、遗忘和检索若形成真实需求，应使用独立的 Memory 领域契约，
不因底层都是 Markdown 而建立万能 Storage ops。无文件系统的平台仍可提供
等价的内存或非文件后端。

### 有界性、借用期与失败

- 应用为文件加载缓冲、索引和缓存单独预算；Core 只为最终投影使用有界 turn
  scratch。禁止读取无界文件后再截断，也不得用 Core heap fallback 补容量。
- 当前 `agent_config_t.system_prompt` 和 `agent_skill_t.content` 都是借用视图；
  启动时加载后绑定或注册，底层缓冲必须在引用期间有效。按需读取的内容只在
  本轮组装期间借用，Core 必须按 Context 契约复制或固定到模型调用完成。
- Soul 的加载或校验失败不能静默变成空人格。产品需显式选择是否允许无 Soul
  启动；若 Soul 被配置为必需，本轮不可因读取失败或预算不足而继续。
- 普通 Memory 或非必需 Skill 可按显式规则整项跳过；必需内容失败则中止本轮。
  文件 I/O 错误、文件格式错误与容量不足须分别可诊断，不允许半段 Markdown
  静默进入模型输入。Session 历史仍遵守 ADR 0016 的完整 turn 准入规则。
- 文件路径、内容来源和更新权限由产品控制。可写 Memory 文件不得通过同名
  文件或路径替换覆盖受信任的 Soul；文件更新应在 turn 边界生效。

### 与现有配置的关系

`agent_config_t.system_prompt` 保留为应用提供的基础指令入口。若产品同时
配置 Soul，默认投影顺序为基础指令、Soul、已选 Skill、Memory 结果；
Session 历史与 Tool 定义分别使用结构化投影。此顺序只规定确定性的组装，
不把提示词文本顺序当成安全边界。Soul 加载器不得暗中覆盖应用的基础指令。

## 不采用的方案

### Core 内置 Markdown 文件系统与目录扫描

这会把平台文件 API、目录规则和文件权限引入 Core，并使文件存在等同于
能力启用；拒绝。

### Session、Memory、Soul、Skill 共用一套领域 Storage ops

它们的提交、检索、注册和权限语义不同。统一物理 I/O 可以减少重复代码，
统一领域接口只会把差异隐藏到可选回调和类型判断中；拒绝。

### 把所有来源拼成一个不分类型的 Context 字符串

Session 必须保留消息角色与 Tool 配对，Tool 必须保留结构化 schema，Soul
与普通 Memory 也不能具有相同的信任等级；拒绝。

## 影响与实施状态

- 文件系统只出现在应用、Port 或可选文件后端；Agent Core 与介质解耦。
- 产品须分别预算 Soul/Skill 文件缓冲、Memory 检索状态、Session 后端和
  Core turn scratch；共用文件系统不等于共用内存或保留策略。
- `memory.h` 已引入独立领域 ops、借用绑定与整文读写，可选 Markdown 后端
  已实现，见 [ADR 0032](0032-memory-domain-management.md)。Skill registry 与
  Context Builder 尚未实现完整流程；JSONL 不承担 Markdown 加载，也不表示
  完整 `agent_run()` 投影链路已经完成。

## 待裁决

1. Soul 是每个 Agent 固定一份，还是按用户或 Session 切换？这决定加载时机、
   缓存键与跨用户隔离规则。
2. Soul 与 Skill 的 Markdown 是否需要元数据，以及具体目录和版本约定？
   应在真实加载器设计时确定，不由 Core 预设（加载器的交付裁决见
   [ADR 0029](0029-official-skill-file-loader.md)，格式冻结仍待其触发条件）。
3. 基础整文读取、替换与遗忘已由 ADR 0032 裁决；相关性检索、事实条目更新和
   多文档一致性仍需后续真实需求，不因领域 ops 已存在就预建。

## 验证要求

- 未注册 Skill 即使文件存在也不进入模型输入；注册后文件缓冲的借用期受检验。
- Soul 和普通 Memory 的权限隔离、必需内容失败、可选内容整项回滚及
  文件更新的 turn 边界可测试。
- 文件 I/O 或单对象容量失败不导致半条 Session 消息、半个 Skill 或半段
  Markdown 被投影；Core 不调用平台文件系统 API。

本 ADR 细化 ADR 0010 的 Memory 与 Context 边界，以及 ADR 0016 的
Session Storage 边界；不改变后者的完整 turn 语义。
