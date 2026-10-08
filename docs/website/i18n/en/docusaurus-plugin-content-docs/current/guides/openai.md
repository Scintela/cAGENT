# OpenAI-Compatible Services

The Provider implements synchronous, non-streaming Chat Completions. Responses
API, SSE, multimodal input, usage reporting and configurable sampling parameters
are not supported. Compatibility requires testing the endpoint's request fields
and Tool protocol; it is not a guarantee for every vendor.

## Build and Prerequisites

Enable `AGENT_BUILD_JSON_CODEC` and `AGENT_BUILD_OPENAI_PROVIDER`; link
`cagent::provider_openai`. Enable the HTTP Port separately in platform builds.

First establish networking and certificate/hostname verification for HTTPS.
Core and Provider Runtime must share the same monotonic clock reference.

## Initialize and Bind

This integration function is not a complete network application. The platform
initializes `transport` and `runtime`; trusted application configuration supplies
endpoint, model and authorization rather than hardcoding them in the library.

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

These capacities are starting examples, not guarantees for every context. Keep
`app_openai_t` in static or application-managed persistent memory, not a temporary
stack frame. Destroy borrowed resources in this order: Agent -> wrapper ->
Provider/Transport storage. Authorization is the full `Bearer ...` value, not a
bare key. Authenticated requests must use HTTPS.

## Data Flow

```text
Context system_prompt / messages[] / tools[]
  -> Bounded JSON writer -> request_buffer
  -> Transport.request(POST) -> response_buffer
  -> Strict JSON reader; reuse request_buffer for tokens
  -> Validate finish_reason, messages and calls before delivery
  -> Decode text/calls into decoded_buffer
  -> Model sink -> Run collection and execution
```

The three buffers must not overlap; configuration text and input/sink state must
not overlap writable buffers either. Provider owns no sockets/TLS and allocates
no heap memory. SDK memory is budgeted separately.

## Token-Field Compatibility

Currently `max_output_tokens > 0` serializes as `max_completion_tokens`. There is
no `max_tokens` switch or endpoint-based guessing. Services that reject this
field are not verified compatible; explicit dual-mode support is not implemented.
Zero omits the parameter and is not a substitute for server-side budget control.

## Failures and Diagnostics

| Condition | Status |
|---|---|
| 401 / 403 | AUTH |
| 429 | MODEL_RATE_LIMIT |
| 408 / 504 | TIMEOUT |
| Other 5xx | MODEL_UNAVAILABLE |
| Unsupported/malformed response | MODEL_PARSE |
| Malformed Tool arguments | TOOL_ARGUMENT |
| Insufficient working buffer | CAPACITY |

Never log keys, Authorization, complete private documents or request bodies.
The application bounds retries and deadlines. Do not blindly rerun a Turn with
side-effecting Tools. See [Model](../api/model.md) and [HTTP](../api/transport.md).
