/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 tanglinjie */
#include <agent.h>
#include <agent_openai_model.h>
#include <assert.h>
#include <string.h>

static unsigned exchanges, executions;
static uint64_t now(void* data)
{
    (void)data;
    return 1u;
}
static agent_error_t http(void* data, const agent_http_request_t* request,
                          const agent_http_sink_t* sink)
{
    static const char call[] =
        "{\"choices\":[{\"finish_reason\":\"tool_calls\",\"message\":{\"role\":\"assistant\","
        "\"content\":null,\"tool_calls\":[{\"id\":\"one\",\"type\":\"function\","
        "\"function\":{\"name\":\"lamp\",\"arguments\":\"{}\"}}]}}]}";
    static const char final[] = "{\"choices\":[{\"finish_reason\":\"stop\","
                                "\"message\":{\"role\":\"assistant\",\"content\":\"Light on\"}}]}";
    char body[4096];
    const char* reply;
    size_t i, size;
    agent_error_t status;
    (void)data;
    assert(request->body_size < sizeof(body));
    memcpy(body, request->body, request->body_size);
    body[request->body_size] = '\0';
    if (++exchanges == 1u)
    {
        assert(strstr(body, "\"tools\"") && strstr(body, "lamp"));
        reply = call;
        size = sizeof(call) - 1u;
    } else
    {
        assert(exchanges == 2u && executions == 1u);
        assert(strstr(body, "\"tool_call_id\":\"one\"") && strstr(body, "switched"));
        assert(!strstr(body, "\"tools\""));
        reply = final;
        size = sizeof(final) - 1u;
    }
    status = sink->headers(sink->context, 200u, NULL, 0u);
    for (i = 0u; status == AGENT_OK && i < size; ++i)
        status = sink->body(sink->context, reply + i, 1u);
    return status;
}
static agent_error_t execute(void* data, const agent_tool_context_t* context,
                             const agent_text_sink_t* sink)
{
    (void)data;
    assert(context->call.id.size == 3u && !memcmp(context->call.id.data, "one", 3u));
    ++executions;
    return sink->write(sink->context, agent_string_view("switched", 8u));
}
static agent_policy_decision_t allow(void* data, const agent_policy_request_t* request)
{
    (void)data;
    (void)request;
    return AGENT_POLICY_ALLOW;
}

int main(void)
{
    static agent_workspace_t workspace;
    static agent_model_workspace_t model_workspace;
    static char request_bytes[8192], response_bytes[4096], decoded[4096];
    agent_config_t config = agent_config_default();
    agent_openai_config_t openai = {0};
    agent_openai_provider_t provider;
    const agent_transport_ops_t ops = {http};
    agent_tool_t tool = {0};
    agent_t* agent;
    agent_model_t* model;
    char output[64];
    agent_request_t request = {{0}, AGENT_SV_LITERAL("turn on the lamp"), {0}, NULL, NULL};
    agent_response_t response = {0};
    config.runtime.now_ms = now;
    config.limits.max_tool_calls = 1u;
    openai.runtime = config.runtime;
    openai.transport.ops = &ops;
    openai.endpoint = agent_string_view("https://example.invalid/v1/chat/completions", 43u);
    openai.model = agent_string_view("fixture", 7u);
    openai.request_buffer = request_bytes;
    openai.request_capacity = sizeof(request_bytes);
    openai.response_buffer = response_bytes;
    openai.response_capacity = sizeof(response_bytes);
    openai.decoded_buffer = decoded;
    openai.decoded_capacity = sizeof(decoded);
    openai.response_token_capacity = 128u;
    openai.max_response_header_bytes = 1024u;
    openai.max_json_depth = 16u;
    assert(agent_openai_provider_init(&provider, &openai) == AGENT_OK);
    assert(agent_model_init(&model, &model_workspace, agent_openai_model_ops(), &provider) ==
           AGENT_OK);
    assert(agent_init(&agent, &workspace, &config) == AGENT_OK);
    assert(agent_set_model(agent, model) == AGENT_OK);
    tool.name = agent_string_view("lamp", 4u);
    tool.input_schema_json = agent_string_view("{}", 2u);
    tool.execute = execute;
    assert(agent_register_tool(agent, &tool) == AGENT_OK);
    assert(agent_set_policy_callback(agent, allow, NULL) == AGENT_OK);
    assert(agent_start(agent) == AGENT_OK);
    response.output = output;
    response.output_size = sizeof(output);
    assert(agent_run(agent, &request, &response) == AGENT_OK);
    assert(!strcmp(output, "Light on") && exchanges == 2u && executions == 1u);
    assert(response.summary.final_valid && response.summary.tool_succeeded == 1u);
    agent_destroy(agent);
    agent_model_destroy(model);
    return 0;
}
