# 连接 OpenAI 兼容服务

当前 Provider 实现同步、非流式 Chat Completions。它不支持 Responses API、SSE、
多模态、usage 上报或可配置采样参数。“兼容服务”需要实际验证请求字段与 Tool 协议，
不代表任意厂商端点都能直接接入。

## 构建与前置条件

普通 CMake 同时启用 `AGENT_BUILD_JSON_CODEC` 和 `AGENT_BUILD_OPENAI_PROVIDER`，
链接 `cagent::provider_openai`。平台构建单独启用 HTTP Port。

先完成设备联网、可信时钟基准和 HTTPS 证书/主机名验证。
Core 与 Provider 的 Runtime 必须使用同一单调时间基准。

## 初始化与绑定

以下是集成函数，不是完整网络应用。`transport` 与 `runtime` 由平台初始化；
endpoint/model/authorization 由受信任应用配置提供，不写死到库中。

```c
#include <agent_openai_model.h>

typedef struct {
    agent_openai_provider_t provider;
    agent_model_workspace_t wrapper;
    agent_model_t* model;
    char request[16384];
    char response[8192];
    char decoded[4096];
} app_openai_t;

agent_error_t attach_openai(agent_t* agent, app_openai_t* state,
    agent_transport_t transport, agent_runtime_t runtime,
    agent_string_view_t endpoint, agent_string_view_t model,
    agent_string_view_t authorization)
{
    const agent_openai_config_t config = {
        .transport = transport,
        .runtime = runtime,
        .endpoint = endpoint,
        .model = model,
        .authorization = authorization,
        .request_buffer = state->request,
        .request_capacity = sizeof(state->request),
        .response_buffer = state->response,
        .response_capacity = sizeof(state->response),
        .decoded_buffer = state->decoded,
        .decoded_capacity = sizeof(state->decoded),
        .response_token_capacity = 512u,
        .max_response_header_bytes = 4096u,
        .max_json_depth = 16u
    };
    agent_error_t status = agent_openai_provider_init(&state->provider, &config);
    if (status != AGENT_OK) return status;
    status = agent_model_init(&state->model, &state->wrapper,
        agent_openai_model_ops(), &state->provider);
    if (status != AGENT_OK) return status;
    status = agent_set_model(agent, state->model);
    if (status != AGENT_OK) {
        agent_model_destroy(state->model);
        state->model = NULL;
    }
    return status;
}
```

容量是起点示例，不是针对所有上下文的保证；应将 `app_openai_t` 放在静态或应用管理的持久内存，
而不是临时函数栈上。借用绑定销毁顺序为 Agent → wrapper → Provider/Transport 存储。
认证字符串是完整 `Bearer ...` 值，不是裸 Key；携带认证时必须使用 HTTPS。

## 数据流

```text
Context 的 system_prompt / messages[] / tools[]
  → 有界 JSON writer
  → request_buffer
  → Transport.request(POST)
  → response_buffer
  → 严格 JSON reader；复用 request_buffer 存 token
  → 完整校验 finish_reason、消息与调用
  → decoded_buffer 中解码文本/调用
  → Model sink
  → Run 收集和执行
```

三块缓冲必须互不重叠，配置文本和输入/sink 状态也不能与可写缓冲重叠。
Provider 不持有 socket 或 TLS，不申请 Heap；底层 SDK 的内存另计。

## Token 字段兼容性

`max_output_tokens > 0` 时当前序列化为 `max_completion_tokens`。
尚无切换到 `max_tokens` 的配置项，也不按 endpoint 猜测字段。
不接受该字段的服务不能宣称已兼容；显式双模式是待实现能力。
设置 0 仅省略参数，不能当作服务器预算控制的替代。

## 失败与诊断

| 情况 | 返回 |
|---|---|
| 401 / 403 | AUTH |
| 429 | MODEL_RATE_LIMIT |
| 408 / 504 | TIMEOUT |
| 其他 5xx | MODEL_UNAVAILABLE |
| 不支持或畸形的响应 | MODEL_PARSE |
| 畸形 Tool arguments | TOOL_ARGUMENT |
| 任何工作缓冲不足 | CAPACITY |

不打印 API Key、Authorization、完整用户文档和请求体。
重试需由应用限定次数与时间预算；含副作用工具的 Turn 不允许直接整体重跑。

详见[Model 参考](../api/model.md)、[HTTP 契约](../api/transport.md)。
