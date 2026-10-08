# Session 与历史保存

一个 Session 是以 `session_id` 标识的对话；一个 Turn 是一次用户请求及其模型/工具迭代。
Turn、模型迭代和持久化文件行不是可以任意互换的概念。

## 选择保存方式

| 场景 | 方案 |
|---|---|
| 无历史、只执行单次请求 | 不绑定 Storage |
| 测试或易失对话 | `providers/storage/ram`，应用提供数组与 payload |
| 文件系统长期对话 | JSONL Provider + File Store bridge + 平台后端 |
| NVS/Flash 或远端服务 | 应用实现 `agent_session_storage_ops_t`，保留完整组语义 |

Core 只知道 Storage Ops，不认识路径、JSONL、NVS 或挂载方式。
RAM 后端是可选实现，不是 Core 必须使用的中间缓存。

## 绑定 RAM 后端

启用 `AGENT_BUILD_SESSION_RAM` 并链接 `cagent::session_ram`。以下存储是
一个应用实例的示例预算，不是所有产品的推荐默认值：

```c
#include <agent_session_ram.h>

typedef struct {
    agent_session_ram_t backend;
    agent_session_ram_turn_t turns[4];
    agent_session_ram_message_t messages[32];
    agent_session_ram_call_t calls[8];
    char payload[8192];
    agent_message_view_t read_messages[32];
    agent_tool_call_view_t read_calls[8];
} app_history_t;

agent_error_t attach_ram_history(agent_t* agent, app_history_t* state)
{
    const agent_session_ram_config_t config = {
        .turns = state->turns, .turn_capacity = 4u,
        .messages = state->messages, .message_capacity = 32u,
        .calls = state->calls, .call_capacity = 8u,
        .payload = state->payload, .payload_capacity = sizeof(state->payload),
        .read_messages = state->read_messages, .read_message_capacity = 32u,
        .read_calls = state->read_calls, .read_call_capacity = 8u
    };
    agent_session_storage_t storage;
    agent_error_t status = agent_session_ram_init(&state->backend, &config);
    if (status == AGENT_OK)
        status = agent_session_ram_bind(&state->backend, &storage);
    if (status == AGENT_OK)
        status = agent_set_session_storage(agent, &storage);
    return status;
}
```

将 state 放在持续存活的应用内存中。数组和 payload 都可能先耗尽；
这个后端不会自动无限增长或轮转，应用在空闲时决定清理策略。

## 绑定 JSONL 后端

启用完整 JSON codec、JSONL、File Store 与所选平台文件后端。
链接 Session JSONL 和其 file bridge；平台 Store 已由应用初始化。
下面的函数完成领域装配，Store 和每块缓冲都必须长期有效：

```c
#include <agent_session_jsonl_files.h>

typedef struct {
    agent_session_jsonl_t backend;
    agent_session_jsonl_files_t files;
    char name[256];
    char write_line[16384];
    char read_line[16384];
    char decoded[12288];
    /* The public contract requires int alignment, not a private token type. */
    int tokens[2048];
    agent_message_view_t messages[32];
    agent_tool_call_view_t calls[16];
    char session_id[65];
} app_jsonl_history_t;

agent_error_t attach_jsonl_history(agent_t* agent, app_jsonl_history_t* state,
    const agent_file_store_t* store)
{
    agent_session_jsonl_config_t config = {
        .write_line = state->write_line,
        .write_line_capacity = sizeof(state->write_line),
        .read_line = state->read_line,
        .read_line_capacity = sizeof(state->read_line),
        .decoded = state->decoded, .decoded_capacity = sizeof(state->decoded),
        .token_buffer = state->tokens, .token_buffer_bytes = sizeof(state->tokens),
        .read_messages = state->messages, .read_message_capacity = 32u,
        .read_calls = state->calls, .read_call_capacity = 16u,
        .session_id = state->session_id,
        .session_id_capacity = sizeof(state->session_id)
    };
    agent_session_storage_t storage;
    agent_error_t status = agent_session_jsonl_files_init(&state->files, store,
        state->name, sizeof(state->name), &config);
    if (status == AGENT_OK)
        status = agent_session_jsonl_init(&state->backend, &config);
    if (status == AGENT_OK)
        status = agent_session_jsonl_bind(&state->backend, &storage);
    if (status == AGENT_OK)
        status = agent_set_session_storage(agent, &storage);
    return status;
}
```

该工作区比 Core 大也可能是合理的：它需要保存和解码一条完整 Turn，
不计入 Core 的 32 KiB。示例容量不保证最大配置一定能编码；需验证最坏组大小，
可将适合的后端工作区放在应用选择的 PSRAM，或缩小物理上限。
文件 bridge 将 Session ID 转为十六进制文件名，name 缓冲还要容纳前后缀和终止符，
不能仅按原始 ID 长度配置。

## 请求与历史窗口

```c
#include <agent.h>

agent_error_t ask_with_history(agent_t* agent, agent_response_t* response)
{
    agent_request_t request = {
        .session_id = AGENT_SV_LITERAL("living-room"),
        .input = AGENT_SV_LITERAL("What did we discuss last time?")
    };
    agent_limits_t limits = AGENT_LIMITS_DEFAULT;
    limits.max_history_turns = 3u;
    request.limits = &limits;
    /* The caller initializes response.output and response.output_size. */
    return agent_run(agent, &request, response);
}
```

默认 `max_history_turns=0`：不投影历史，但绑定的 Storage 仍可保存新 Turn。
非 NULL 的请求 limits 是完整覆盖，不是只替换非零字段。

Storage 的 `recent()` 返回最近的完整 Turn 组，最新组在前；Core 选择有界窗口，
再以模型所需的时间顺序组装。不会切断 Tool call/result 配对来凑预算。

## 写入与读取链路

```text
请求 → Core scratch 当前 Turn
     → Storage begin / append
     → 模型与工具迭代继续 append
     → finish(COMPLETE 或 ABORTED)
     → JSONL 记录编码、文件追加与 sync

后续请求 → recent(有限候选)
         → Provider 解码一个完整组
         → Core 复制选中的历史到 scratch
         → Context 投影
```

当前 JSONL 在结束时保存一个版本化 Turn 对象，文件按行分隔。
它不是每个事件一行，也不是工具操作之前的持久化执行日志。
中途掉电无法据此保证物理动作 exactly-once。

## 失败与保留

检查 `response.session_status`。存储失败不能推导设备操作未发生，
`sync()` 失败也不能推导记录一定没落盘；禁止盲目重跑整个 Turn。

JSONL 对未终止尾行有读取与修复规则，不能替代底层文件系统掉电测试。
保留周期、容量轮转、压缩、加密和 Flash 磨损由 Storage Provider/应用负责，
`max_history_turns` 只控制上下文窗口，不删除历史。

接入配置见[Storage Provider 参考](../api/storage.md)，领域契约见[Session 参考](../api/session.md)。
