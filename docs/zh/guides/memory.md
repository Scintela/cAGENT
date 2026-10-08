# Memory 与 Markdown 文档

Session 保存原始对话事实，Memory 保存应用选择的长期内容。
目前库不会自动从 Session 总结或提取 Memory；这类任务由应用调度，使用独立预算。

## 最小文件布局

下面是产品约定，不是 Core 硬编码路径：

```text
workspace/
├── SOUL.md
├── USER.md
├── MEMORY.md
└── memory/
    └── 2026-10-08.md
```

| 逻辑种类 | 内容 | 普通 Memory API 是否允许修改 |
|---|---|---|
| SOUL | 可信身份与长期行为规则 | 否 |
| USER | 当前用户资料 | 是，受信任应用命令 |
| FACTS | MEMORY.md 的长期事实与偏好 | 是 |
| NOTE | 以合法日期标识的每日笔记 | 是 |

身份文件、用户目录和日期由应用授权，不能直接采用模型提供的任意路径。
多用户使用不同 root/绑定，Session ID 不会自动替你隔离 Memory。

## 初始化文档后端

先准备文档 root 的 File Store 与独立 notes root 的 File Store，
启用 `AGENT_BUILD_MARKDOWN_MEMORY` 并链接 `cagent::memory_markdown`：

```c
#include <agent_markdown_memory.h>

agent_error_t attach_documents(agent_t* agent, agent_markdown_memory_t* state,
    agent_file_store_t documents, agent_file_store_t notes)
{
    const agent_markdown_memory_config_t config = {
        .soul = {
            .store = documents, .name = AGENT_SV_LITERAL("SOUL.md"),
            .max_bytes = 512u
        },
        .user = {
            .store = documents, .name = AGENT_SV_LITERAL("USER.md"),
            .max_bytes = 512u
        },
        .facts = {
            .store = documents, .name = AGENT_SV_LITERAL("MEMORY.md"),
            .max_bytes = 1024u
        },
        .notes = notes,
        .note_max_bytes = 1024u
    };
    agent_memory_t binding;
    agent_error_t status = agent_markdown_memory_init(state, &config);
    if (status == AGENT_OK)
        status = agent_markdown_memory_bind(state, &binding);
    if (status == AGENT_OK)
        status = agent_set_memory(agent, &binding);
    return status;
}
```

三个固定文档可以共用受信任文档 root；notes 是独立平面命名空间。
未使用的文档将 max_bytes 设置为 0。state、Store 状态和路径 scratch
至少存活到所有消费者结束；以上函数不负责创建文件内容。

## 装配步骤

1. 应用挂载文件系统，准备受信任目录。
2. 初始化平台 File Store，准备不重叠的路径 scratch。
3. 配置 `agent_markdown_memory_config_t` 的 soul/user/facts 与独立 notes root。
4. `agent_markdown_memory_init()` 后通过 `agent_markdown_memory_bind()` 导出领域绑定。
5. `agent_set_memory()` 绑定 Agent。
6. 用 `agent_register_memory_context()` 显式选择要投影的文档。

例如选择长期事实：

```c
#include <agent/context.h>

agent_error_t select_memory(agent_t* agent)
{
    const agent_memory_context_t source = {
        .name = AGENT_SV_LITERAL("user-facts"),
        .key = { .kind = AGENT_MEMORY_FACTS },
        .required = false,
        .max_bytes = 512u
    };
    return agent_register_memory_context(agent, &source);
}
```

仅绑定 Memory 不会自动注入所有文件。SOUL 投影为指令，其他文档为参考数据；
必需文件读失败终止 Turn，可选文件整段跳过。

## 读写纪律

`agent_memory_read()` 将完整文档复制到调用方缓冲，不静默截断。
`replace()` 是整文替换，不是局部 patch；`forget()` 不是安全擦除。
这些公共命令仅在 Agent 空闲时调用，不能从 Tool/Context 回调重入。

错误返回需要同时看 `agent_memory_change_t`：
UNCHANGED、APPLIED、UNKNOWN 描述内容变化事实，不是另一个错误码。
失败但 APPLIED/UNKNOWN 时不能盲目重试。SOUL 更新应走受信任应用部署流程，
不是绕过保护把它包装为普通模型 Tool。

整文格式、字段和所有权见[Memory 参考](../api/memory.md)；
提取方案背景见 [ADR 0024](../adr/0024-session-to-memory-extraction.md)。
