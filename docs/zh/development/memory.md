# Memory 领域管理开发记录

日期：2026-10-02。依据：[ADR 0032](../adr/0032-memory-domain-management.md)。
代码提交：`fd2e0ce`（Memory 领域管理、Markdown 后端与测试）。

## 本次范围

`src/memory/` 从空骨架变成平台无关的领域管理层。可选 Markdown 后端接入现有
File Store，支持 Soul、User、长期事实和日期笔记的整文读取、替换与遗忘。
不改 Session 保留策略，不从历史自动提取事实，不新增平台专属 Memory 文件层。
Context/ReAct 仍未完成，以下读写示例不等于端到端对话。

## 类型与 API

| 类型 | 用途与寿命 |
|---|---|
| `agent_memory_kind_t` | SOUL/USER/FACTS/NOTE，保持来源与写权限区别 |
| `agent_memory_key_t` | 类型与逻辑 id；非 NOTE 的 id 必须为空 |
| `agent_memory_change_t` | UNCHANGED/APPLIED/UNKNOWN，描述修改是否发生，不替代错误码 |
| `agent_memory_ops_t` | 同步 read、可选 replace/forget；指针只在调用期间借用 |
| `agent_memory_t` | ops 值与借用 context；Core 复制绑定，不接管后端 |
| `agent_markdown_memory_document_t` | 文档 Store、单组件文件名与字节上限；0 禁用 |
| `agent_markdown_memory_config_t` | 三个固定文档映射、独立 notes Store 与笔记上限 |
| `agent_markdown_memory_t` | 调用方持有的 Provider 状态，无缓存、heap 或保留文件句柄 |

| 接口 | 行为 |
|---|---|
| `agent_set_memory(agent, binding)` | 空闲绑定/更换；NULL 解绑；借用后端不被 destroy |
| `agent_memory_read(...)` | 空闲读取整个文档到调用方缓冲，返回该缓冲 view |
| `agent_memory_replace(...)` | 空闲、可信应用发起的整文替换；拒绝 Soul |
| `agent_memory_forget(...)` | 空闲整文遗忘；拒绝 Soul；不删除 Session 或安全擦除 |
| `agent_markdown_memory_init(...)` | 验证映射与读能力，复制配置；不访问文件系统 |
| `agent_markdown_memory_bind(...)` | 输出借用的 Memory ops/context |
| `agent_memory_project(...)` | 私有 driver 接口：显式文档 -> arena 快照，失败回退；尚未接入 Context |

Core 仅保留绑定状态，不持有文档大缓冲。应用维持 Provider、Store ops/context、
路径工作缓冲和配置文件名的寿命；输出与这些状态必须互不重叠。Provider 已绑定
时不能重初始化其状态；先解绑并排空调用。共享同一 Store 或物理文件的读写，
包括外部编辑器与其他任务，由应用统一串行化，重入标记不能替代线程锁。

## 数据流

```text
应用读取 USER
  -> Core 校验空闲状态、逻辑 key 与缓冲
  -> Provider 选择受授权的 user Store/name，合并文档上限与调用预算
  -> File Store size/read/EOF 检查 -> 平台 Port
  -> 校验整文容量与内嵌 NUL -> 调用方文本缓冲
  -> Core 返回借用该缓冲的 view

应用更新 MEMORY
  -> Core 校验权限类别、输入与空闲状态
  -> Provider 校验整文上限 -> File Store replace
  -> Port 临时文件、写入、同步、rename、按配置同步目录
  -> published + error -> change + error -> 应用决定核对或重试

未来 Context 路径
  -> 编排选择显式 key 与本轮预算
  -> agent_memory_project 在 scratch 预留 max_bytes+1
  -> 同一 read ops -> 成功保留实际长度+1，失败回退
  -> Context 以合适的来源/信任级别使用快照（此步尚未实现）
```

读取辅助并不生成文件系统原子快照；上述“快照”指稳定文件读到独立缓冲之后的
内存副本。旧副本直到缓冲复用仍有效，下次读取得到更新后的内容；不会返回指向
后端共享缓存的易失 view。

## 应用装配示例

下面两个 Store 已由应用挂载并授权：`documents` 对应固定文档目录，`notes`
对应独立 `memory/` 目录。平台初始化分别使用
`agent_port_espidf_file_store_init()`、`agent_port_openvela_file_store_init()` 或
`agent_posix_file_store_init()`。创建目录、账户授权、文件初始内容及日期来源
都属于应用。完整 Host 文件操作用例在 `tests/providers/memory_markdown/contract.c`。

```c
#include <agent.h>
#include <agent_markdown_memory.h>

agent_error_t app_bind_memory(agent_t* agent,
                              agent_markdown_memory_t* state,
                              agent_file_store_t documents,
                              agent_file_store_t notes)
{
    agent_markdown_memory_config_t config = {0};
    agent_memory_t binding;
    agent_error_t status;

    config.soul.store = documents;
    config.soul.name = agent_string_view("SOUL.md", sizeof("SOUL.md") - 1u);
    config.soul.max_bytes = 1024u;
    config.user.store = documents;
    config.user.name = agent_string_view("USER.md", sizeof("USER.md") - 1u);
    config.user.max_bytes = 2048u;
    config.facts.store = documents;
    config.facts.name = agent_string_view("MEMORY.md", sizeof("MEMORY.md") - 1u);
    config.facts.max_bytes = 4096u;
    config.notes = notes;
    config.note_max_bytes = 2048u;

    status = agent_markdown_memory_init(state, &config);
    if (status == AGENT_OK) status = agent_markdown_memory_bind(state, &binding);
    if (status == AGENT_OK) status = agent_set_memory(agent, &binding);
    return status;
}
```

`state` 由应用持有，寿命覆盖 Agent 绑定使用期；函数内的 config/binding 可离开
作用域，因为结构值已复制，其借用对象仍需有效。上限只是示例，不预分配这些
大小的文档缓冲。可以不启用 notes 或 Soul，未配置项保留零初始化。

读取 User 的示例，capacity 包含 NUL 空间：

```c
agent_memory_key_t key = {AGENT_MEMORY_USER, {NULL, 0u}};
char user_text[2049];
agent_string_view_t view;
agent_error_t status = agent_memory_read(agent, &key, user_text,
                                         sizeof(user_text), 2048u, &view);
```

这里的数组用于说明调用方式，大缓冲宜放静态区域或应用管理的内存，不必放
任务栈。缺文件、未启用、文件超上限/缓冲不足、嵌入 NUL 分别返回
NOT_FOUND、NOT_SUPPORTED、CAPACITY、PARSE，不自动用空内容掩盖失败。
必需内容失败应中止应用当前流程，可选内容是否整项跳过由应用明确决定。

更新同一 User：

```c
agent_memory_change_t change;
agent_error_t status = agent_memory_replace(agent, &key,
    agent_string_view("Preferred language: Chinese\n", 28u), &change);
```

状态返回前，输入已消费，不会被 Provider 保留。成功必须为 APPLIED；错误且
APPLIED/UNKNOWN 时，核对内容与同步状态，不重复覆盖。读改写必须覆盖整个
读取到替换阶段的串行保护；没有 CAS 或多文件事务。笔记采用显式日期 key，
不默认加载全部历史，也没有隐式追加或自动摘要。

## 验证与剩余工作

```sh
bash tests/memory/compile.sh
bash tests/providers/memory_markdown/compile.sh
bash tests/build/file_store/compile.sh
CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' bash tests/memory/compile.sh
CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' bash tests/providers/memory_markdown/compile.sh
```

已通过新增契约、C/C++ 头文件检查、Host 实际文件读写、失败注入、可选构建与
全部现有 Host 回归脚本。测试验证普通 Memory 更新不修改 Soul，用户 Store 隔离，
删除/替换不改变原始 Session 字节，scratch 快照独立且错误回退。

尚未实施 Context/ReAct 自动接入、按来源选择消息、检索/事实条目更新、后台
提取与内置 Memory Tool。日期字符串只验证日历合法性，不验证来源可信。
目标平台目前验证装配与模拟行为，未完成原生固件、介质掉电与磨损验收；
Session 提交同步失败的歧义仍是文件核验日志 FS-11 的独立任务。
文档装配示例已通过 C99 编译检查；本机缺少 Node.js，未验证 Docusaurus 站点构建。
