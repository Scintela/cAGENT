# ADR 0032: Memory 领域管理与可选 Markdown 后端

- 状态：已实现基础契约、Markdown 后端和 Context 显式文档投影；ReAct 接入未实现
- 日期：2026-10-02
- 关联：[ADR 0023](0023-file-backed-memory-soul-skill.md)、[ADR 0024](0024-session-to-memory-extraction.md)、[ADR 0027](0027-minimal-markdown-memory-layout.md)、[ADR 0031](0031-prefabricated-platform-file-storage.md)

## 背景与选择

共享 File Store 已有字节读写、整文件替换和平台实现，但它不负责区分 Soul、
User、长期事实与每日笔记，也不负责 Agent 生命周期。直接读取 `USER.md` 不等于
已经完成 Memory 管理；反过来，不能因需要 Memory 就把文件系统塞进 Core。

选择第二种分层方式：**Core 保留独立的 Memory 领域契约、绑定与有界投影机制，
可选 Provider 实现文档映射和存储，平台 Port 实现物理 I/O。** 不把 Memory 管理
全部下放给应用，也不让 Core 直接调用文件 ops。

## 架构与目录

```text
Application
  -> agent_set_memory / agent_memory_read / replace / forget
  -> Core Memory                          领域契约、空闲约束、借用与回调保护
  -> agent_memory_t                       同步 ops + context
  -> Markdown Memory Provider             文档分类、限额、日期与文件名映射
  -> agent_file_store_t                    字节 I/O、发布结果
  -> POSIX / ESP-IDF / OpenVela Port       实际文件系统
```

```text
include/agent/memory.h                       领域类型与应用入口
src/memory/memory_mgr.c                      绑定、调用、结果校验与 scratch 投影
src/memory/memory_internal.h                 私有投影接口
providers/memory/markdown/
  include/agent_markdown_memory.h            可选后端配置与调用方状态
  src/memory_markdown.c                      文档映射、有界读取、替换与遗忘
providers/storage/files/                    已有共享字节契约
ports/*/storage/                            已有平台实现，不新增 memory I/O 目录
```

Core 不包含 File Store 头文件，不认识路径、Markdown、JSONL 或平台 SDK。
自定义 RAM/NVS/远端后端可直接实现 Memory ops，不要求模拟文件系统。
Markdown 后端是独立可选包，不依赖 JSON codec 或 Session Storage。

## 文档与操作契约

| 逻辑类型 | Markdown 示例映射 | 基础操作与限制 |
|---|---|---|
| `AGENT_MEMORY_SOUL` | `SOUL.md` | 可读；普通 replace/forget 一律拒绝 |
| `AGENT_MEMORY_USER` | `USER.md` | 可读、整文替换、整文遗忘；用户范围由应用授权 |
| `AGENT_MEMORY_FACTS` | `MEMORY.md` | 同上；不包含 Session 全量历史 |
| `AGENT_MEMORY_NOTE` | 独立目录内的 `YYYY-MM-DD.md` | 按显式日期访问，不扫描或默认注入全部笔记 |

Core 的 `agent_memory_key_t` 是逻辑标识，不是文件路径。只有 NOTE 需要 `id`，
其格式由 Provider 定义。Markdown 后端校验真实公历日期，包括闰年；应用负责
提供可信日期和时区，不能从 Runtime 单调毫秒推算日历，也不能接受模型任意路径。

固定文档名与 Store 由应用配置；上述文件名不是 Core 硬编码。每类
`max_bytes=0` 表示禁用，至少启用一类。没有目录扫描、隐式建文件、挂载或格式化。

read 必需，replace/forget 可选；未绑定、禁用类别或缺失能力明确返回
NOT_SUPPORTED，缺文件返回 NOT_FOUND。空文件是成功的零字节文档，不等于缺文件。
只提供整文操作，不预建检索索引、事实条目 CRUD、追加笔记或通用查询语言。

## 生命周期、内存与信任

- `agent_set_memory()` 复制 ops 和 context 值，不接管 Provider。NULL 解绑；
  CONFIGURING/READY 时可绑定、更换、读取与更新，ACTIVE 或回调中返回 BUSY。
- `agent_destroy()` 不销毁借用后端；回调中不允许销毁 Agent。Provider、Store
  状态、ops 和配置文本必须覆盖使用期，解绑后才可重初始化或释放。
- ops 同步完成，不保留单次输入、输出或结果指针。`active`/`in_callback` 是
  重入保护，不是线程锁；应用协调所有实例、外部编辑和读改写任务。
- 读取到调用方独立缓冲，capacity 包括终止符空间，字节限额不包括终止符。
  拒绝超容量与内嵌 NUL，不静默截断；不承诺 Markdown/UTF-8 内容校验。
- Provider 没有 heap、文件描述符缓存或文本缓存。旧缓冲内容不随文件更新改变，
  下次显式读取返回新版本；缓冲复用或释放前，返回 view 保持有效。
- 私有 `agent_memory_project()` 将一个显式文档读进 turn arena，先预留
  `max_bytes + 1`，失败回退 mark，成功仅保留实际长度加终止符。需要同时存在的
  文档快照会占用累计 scratch，预算不足返回 CAPACITY；调用方不能依靠实际
  文件较小绕过预留预算。Context 已通过显式文档选择调用此入口；ReAct 驱动尚未接入。
- Root、用户选择与文件名映射只能来自可信配置；相同 Store/name 的固定文档
  别名和与文档共用绑定的 notes Store 被拒绝。不同 Store context 是否映射到
  同一实际目录无法由通用层证明，应用必须保证物理目录与权限隔离。
- Soul 不能通过普通 Memory 更新；授权管理员仍可走独立产品配置流程。
  User/Memory/笔记文本不会因被读取而获得执行授权或自动成为高信任 system 指令。

## 变更结果不是成功标志

所有修改返回 `agent_error_t`，另通过 `agent_memory_change_t` 描述变更事实。
它只属于写操作的领域结果，不改变扁平错误体系，也不是 error source。

| change | 含义 | 应用处理 |
|---|---|---|
| `UNCHANGED` | 本次操作未替换或删除文档 | 可依据错误处理；不保证其他写入方没有修改 |
| `APPLIED` | 本次版本已发布或删除已完成 | 即使返回错误也不盲重试；按错误确认同步/耐久性 |
| `UNKNOWN` | 是否改变无法确认 | 重新读取并核对后决策，不盲重放 |

成功写操作必须报告 APPLIED；Provider 若报告非法枚举，或成功但不报告 APPLIED，
Core 返回 IO + UNKNOWN。未完成结果的自定义 Provider 必须保守报告 UNKNOWN。

Markdown replace 将 File Store 的 `published` 映射为 APPLIED，发布后的同步
失败仍保留这个结果。remove 缺少发布结果，除成功、未找到和未提供操作等明确
情况外，错误报告 UNKNOWN；不能根据删除失败推断文件还在。

整文件替换不是多文档事务，也不提供 CAS。读改写全过程由应用串行化。
APPLIED 不意味着掉电已验收；具体同步保证取决于 Store 配置与目标介质。
forget 不是安全擦除，也不删除原始 Session、备份或外部引用。

## 构建与验证

普通 CMake 使用 `AGENT_BUILD_FILE_STORE=ON` 和
`AGENT_BUILD_MARKDOWN_MEMORY=ON`，链接 `cagent::memory_markdown`；默认关闭。
ESP-IDF 组件通过 `CONFIG_AGENT_MEMORY_MARKDOWN` 选择，依赖
`CONFIG_AGENT_FILE_STORE`，平台文件入口独立选择。Kconfig 不是必需机制。
OpenVela 可通过普通目标装配或把通用 Provider 源加入应用构建；Port 不自动
拉入 Memory/JSONL。所有源使用一致的 Core build Profile。

已验证 Core 生命周期、借用绑定、回调重入、读取容量、写入结果与 scratch
回退；Markdown 真实 Host 文件、用户隔离、Soul 拒写、日期、只读能力和
发布前/后的失败；可选 Host/模拟 IDF/NuttX 构建与 JSON 独立性；新增两组
契约通过 AddressSanitizer/UndefinedBehaviorSanitizer。

Context 已实现显式选择、预算和快照接入，见 [Context 接口](api/context.md)。
仍未实现：自动文档发现、相关性检索、Memory Tool、后台摘要提取与
多文档版本事务。真实 ESP-IDF/OpenVela 固件、Flash 磨损和掉电行为仍需产品验证。
开发流程与 API 使用见 [Memory 开发记录](../development/memory.md)。
