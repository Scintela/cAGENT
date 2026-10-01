/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
#include <agent_openai_model.h>

#include "json_internal.h"
#include "run/run_internal.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define SV(value) agent_string_view((value), sizeof(value) - 1u)

typedef struct {
    const char* response;
    size_t response_size;
    size_t chunk_size;
    unsigned int status;
    size_t requests;
    uint64_t deadline;
    bool authorized;
    bool omit_headers;
    bool ignore_body_errors;
    agent_cancel_token_t* cancel_after_headers;
    uint64_t clock_after_headers;
    char sent[4096];
    size_t sent_size;
} fake_http_t;

typedef struct {
    char text[2048];
    size_t text_size;
    char id[128];
    char name[128];
    char arguments[1024];
    size_t calls;
    agent_error_t sink_error;
} collected_t;

static char request_buffer[4096];
static char response_buffer[4096];
static char decoded_buffer[4096];
static uint64_t clock_ms = 1000u;

static uint64_t now_ms(void* context)
{
    (void)context;
    return clock_ms;
}

static void copy_view(char* output, size_t capacity, agent_string_view_t value)
{
    assert(value.size < capacity);
    memcpy(output, value.data, value.size);
    output[value.size] = '\0';
}

static agent_error_t fake_request(void* context, const agent_http_request_t* request,
                                  const agent_http_sink_t* sink)
{
    fake_http_t* fake = context;
    agent_json_document_t document;
    jsmntok_t tokens[256];
    size_t offset;
    size_t i;

    ++fake->requests;
    assert(request->method.size == 4u &&
           memcmp(request->method.data, "POST", 4u) == 0);
    assert(request->max_response_bytes != 0u &&
           request->max_response_bytes <= sizeof(response_buffer));
    assert(request->header_count == 1u || request->header_count == 2u);
    for (i = 0u; i < request->header_count; ++i) {
        if (request->headers[i].name.size == sizeof("Authorization") - 1u &&
            memcmp(request->headers[i].name.data, "Authorization",
                   sizeof("Authorization") - 1u) == 0) {
            fake->authorized = request->headers[i].value.size ==
                                   sizeof("Bearer test-secret") - 1u;
        }
    }
    fake->deadline = request->deadline_ms;
    assert(request->body_size < sizeof(fake->sent));
    memcpy(fake->sent, request->body, request->body_size);
    fake->sent[request->body_size] = '\0';
    fake->sent_size = request->body_size;
    assert(agent_json_parse(agent_string_view(fake->sent, fake->sent_size), tokens,
                            256u, 16u, &document) == AGENT_OK);
    if (fake->status == 0u) fake->status = 200u;
    if (fake->omit_headers) return AGENT_OK;
    {
        agent_error_t status = sink->headers(sink->context, fake->status, NULL, 0u);
        if (status != AGENT_OK) return status;
    }
    if (fake->cancel_after_headers != NULL) {
        agent_cancel_token_request_locked(fake->cancel_after_headers);
    }
    if (fake->clock_after_headers != 0u) {
        clock_ms = fake->clock_after_headers;
    }
    for (offset = 0u; offset < fake->response_size;) {
        size_t length = fake->response_size - offset;
        agent_error_t status;

        if (fake->chunk_size != 0u && length > fake->chunk_size) {
            length = fake->chunk_size;
        }
        status = sink->body(sink->context, fake->response + offset, length);
        if (status != AGENT_OK && !fake->ignore_body_errors) return status;
        offset += length;
    }
    return AGENT_OK;
}

static const agent_transport_ops_t http_ops = {fake_request};

static agent_error_t collect_text(void* context, agent_string_view_t value)
{
    collected_t* output = context;

    if (output->sink_error != AGENT_OK) return output->sink_error;
    assert(value.size <= sizeof(output->text) - output->text_size);
    memcpy(output->text + output->text_size, value.data, value.size);
    output->text_size += value.size;
    return AGENT_OK;
}

static agent_error_t collect_call(void* context, const agent_tool_call_view_t* call)
{
    collected_t* output = context;

    if (output->sink_error != AGENT_OK) return output->sink_error;
    ++output->calls;
    copy_view(output->id, sizeof(output->id), call->id);
    copy_view(output->name, sizeof(output->name), call->name);
    copy_view(output->arguments, sizeof(output->arguments), call->arguments_json);
    return AGENT_OK;
}

static agent_openai_config_t config_for(fake_http_t* fake)
{
    agent_openai_config_t config = {0};

    config.transport.ops = &http_ops;
    config.transport.context = fake;
    config.runtime.now_ms = now_ms;
    config.endpoint = SV("https://example.invalid/v1/chat/completions");
    config.model = SV("test-model");
    config.authorization = SV("Bearer test-secret");
    config.request_buffer = request_buffer;
    config.request_capacity = sizeof(request_buffer);
    config.response_buffer = response_buffer;
    config.response_capacity = sizeof(response_buffer);
    config.decoded_buffer = decoded_buffer;
    config.decoded_capacity = sizeof(decoded_buffer);
    config.response_token_capacity = 128u;
    config.max_response_header_bytes = 512u;
    config.max_json_depth = 16u;
    return config;
}

static agent_model_request_t request_for(const agent_message_view_t* messages,
                                         size_t message_count)
{
    agent_model_request_t request = {0};

    request.system_prompt = SV("Be concise.");
    request.messages = messages;
    request.message_count = message_count;
    request.timeout_ms = 250u;
    request.max_output_tokens = 64u;
    return request;
}

static agent_error_t complete(agent_openai_provider_t* provider,
                              const agent_model_request_t* request,
                              collected_t* collected)
{
    agent_model_sink_t sink = {collect_text, collect_call, collected};

    return agent_openai_model_ops()->complete(provider, request, &sink);
}

static void test_text_and_history(void)
{
    static const char answer[] =
        "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"Hello\\nworld\","
        "\"tool_calls\":null},"
        "\"finish_reason\":\"stop\"}]}";
    agent_tool_call_view_t old_call = {SV("call_old"), SV("set_light"),
                                       SV("{\"on\":true}")};
    agent_message_view_t messages[3] = {
        {AGENT_MESSAGE_ROLE_USER, SV("Turn on light"), {NULL, 0u}, NULL, 0u},
        {AGENT_MESSAGE_ROLE_ASSISTANT, {NULL, 0u}, {NULL, 0u}, &old_call, 1u},
        {AGENT_MESSAGE_ROLE_TOOL, SV("done"), SV("call_old"), NULL, 0u}
    };
    agent_tool_view_t tool = {SV("set_light"), SV("Toggle light"),
                              SV("{\"type\":\"object\",\"properties\":{\"on\":{\"type\":\"boolean\"}}}"), 0u};
    fake_http_t fake = {0};
    collected_t collected = {0};
    agent_openai_provider_t provider;
    agent_openai_config_t config = config_for(&fake);
    agent_model_request_t request = request_for(messages, 3u);
    agent_model_workspace_t model_workspace;
    agent_model_t* model = NULL;

    fake.response = answer;
    fake.response_size = sizeof(answer) - 1u;
    fake.chunk_size = 3u;
    request.tools = &tool;
    request.tool_count = 1u;
    request.deadline_ms = 1300u;
    assert(agent_openai_provider_init(&provider, &config) == AGENT_OK);
    assert(agent_model_init(&model, &model_workspace, agent_openai_model_ops(),
                            &provider) == AGENT_OK);
    assert(complete(&provider, &request, &collected) == AGENT_OK);
    assert(fake.requests == 1u && fake.authorized && fake.deadline == 1250u);
    assert(collected.text_size == sizeof("Hello\nworld") - 1u);
    assert(memcmp(collected.text, "Hello\nworld", collected.text_size) == 0);
    assert(collected.calls == 0u);
    assert(strstr(fake.sent, "\"max_completion_tokens\":64") != NULL);
    assert(strstr(fake.sent, "\"n\":1,\"stream\":false") != NULL);
    assert(strstr(fake.sent, "\"arguments\":\"{\\\"on\\\":true}\"") != NULL);
    assert(strstr(fake.sent, "\"parameters\":{\"type\":\"object\"") != NULL);
    agent_model_destroy(model);
}

static void test_tool_call_and_failures(void)
{
    static const char tool_answer[] =
        "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":null,"
        "\"tool_calls\":[{\"id\":\"call_1\",\"type\":\"function\",\"function\":{"
        "\"name\":\"set_light\",\"arguments\":\"{\\\"on\\\":true}\"}}]},"
        "\"finish_reason\":\"tool_calls\"}]}";
    static const char bad_arguments[] =
        "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":null,"
        "\"tool_calls\":[{\"id\":\"call_1\",\"type\":\"function\",\"function\":{"
        "\"name\":\"set_light\",\"arguments\":\"{bad}\"}}]},"
        "\"finish_reason\":\"tool_calls\"}]}";
    static const char two_calls[] =
        "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"Using tools\","
        "\"tool_calls\":[{\"id\":\"c1\",\"type\":\"function\",\"function\":{"
        "\"name\":\"set_light\",\"arguments\":\"{}\"}},{\"id\":\"c2\","
        "\"type\":\"function\",\"function\":{\"name\":\"set_light\","
        "\"arguments\":\"{\\\"on\\\":true}\"}}]},\"finish_reason\":\"tool_calls\"}]}";
    agent_message_view_t message = {AGENT_MESSAGE_ROLE_USER, SV("Light on"),
                                     {NULL, 0u}, NULL, 0u};
    fake_http_t fake = {0};
    collected_t collected = {0};
    agent_openai_provider_t provider;
    agent_openai_config_t config = config_for(&fake);
    agent_model_request_t request = request_for(&message, 1u);

    fake.response = tool_answer;
    fake.response_size = sizeof(tool_answer) - 1u;
    fake.chunk_size = 5u;
    assert(agent_openai_provider_init(&provider, &config) == AGENT_OK);
    assert(complete(&provider, &request, &collected) == AGENT_OK);
    assert(collected.calls == 1u && strcmp(collected.id, "call_1") == 0);
    assert(strcmp(collected.name, "set_light") == 0);
    assert(strcmp(collected.arguments, "{\"on\":true}") == 0);

    memset(&collected, 0, sizeof(collected));
    fake.response = two_calls;
    fake.response_size = sizeof(two_calls) - 1u;
    assert(complete(&provider, &request, &collected) == AGENT_OK);
    assert(collected.calls == 2u && strcmp(collected.id, "c2") == 0);
    assert(collected.text_size == sizeof("Using tools") - 1u);

    memset(&collected, 0, sizeof(collected));
    fake.response = bad_arguments;
    fake.response_size = sizeof(bad_arguments) - 1u;
    assert(complete(&provider, &request, &collected) == AGENT_ERROR_TOOL_ARGUMENT);
    assert(collected.calls == 0u && collected.text_size == 0u);

    fake.response = "{broken}";
    fake.response_size = strlen(fake.response);
    assert(complete(&provider, &request, &collected) == AGENT_ERROR_MODEL_PARSE);
    assert(collected.calls == 0u);

    fake.response = tool_answer;
    fake.response_size = sizeof(tool_answer) - 1u;
    fake.status = 429u;
    assert(complete(&provider, &request, &collected) == AGENT_ERROR_MODEL_RATE_LIMIT);
    fake.status = 401u;
    assert(complete(&provider, &request, &collected) == AGENT_ERROR_AUTH);
    fake.status = 503u;
    assert(complete(&provider, &request, &collected) == AGENT_ERROR_MODEL_UNAVAILABLE);
}

static void test_limits_and_validation(void)
{
    static const char answer[] =
        "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"ok\"},"
        "\"finish_reason\":\"stop\"}]}";
    agent_message_view_t message = {AGENT_MESSAGE_ROLE_USER, SV("Hi"),
                                     {NULL, 0u}, NULL, 0u};
    agent_tool_view_t tool = {SV("set_light"), SV("Toggle"), SV("{invalid}"), 0u};
    fake_http_t fake = {0};
    collected_t collected = {0};
    agent_openai_provider_t provider;
    agent_openai_config_t config = config_for(&fake);
    agent_model_request_t request = request_for(&message, 1u);

    config.endpoint = SV("http://example.invalid/v1/chat/completions");
    assert(agent_openai_provider_init(&provider, &config) == AGENT_ERROR_INVALID);
    config = config_for(&fake);
    config.endpoint = SV("https://example.invalid/bad url");
    assert(agent_openai_provider_init(&provider, &config) == AGENT_ERROR_INVALID);
    config = config_for(&fake);
    config.decoded_buffer = response_buffer + 1u;
    assert(agent_openai_provider_init(&provider, &config) == AGENT_ERROR_INVALID);
    config = config_for(&fake);
    config.response_buffer = request_buffer;
    assert(agent_openai_provider_init(&provider, &config) == AGENT_ERROR_INVALID);
    config = config_for(&fake);
    config.model = agent_string_view(request_buffer, 4u);
    assert(agent_openai_provider_init(&provider, &config) == AGENT_ERROR_INVALID);
    config = config_for(&fake);
    config.response_token_capacity = 1000u;
    assert(agent_openai_provider_init(&provider, &config) == AGENT_ERROR_CAPACITY);
    config = config_for(&fake);
    assert(agent_openai_provider_init(&provider, &config) == AGENT_OK);
    fake.response = answer;
    fake.response_size = sizeof(answer) - 1u;
    request.tools = &tool;
    request.tool_count = 1u;
    assert(complete(&provider, &request, &collected) == AGENT_ERROR_INVALID);
    assert(fake.requests == 0u);
    request.tool_count = 0u;
    request.message_count = AGENT_MAX_PROJECTED_MESSAGES + 1u;
    assert(complete(&provider, &request, &collected) == AGENT_ERROR_LIMIT);
    request.message_count = 1u;

    request.deadline_ms = 999u;
    assert(complete(&provider, &request, &collected) == AGENT_ERROR_TIMEOUT);
    assert(fake.requests == 0u);
    request.deadline_ms = 0u;
    config = config_for(&fake);
    config.response_token_capacity = 2u;
    assert(agent_openai_provider_init(&provider, &config) == AGENT_OK);
    assert(complete(&provider, &request, &collected) == AGENT_ERROR_CAPACITY);
    config = config_for(&fake);
    config.response_capacity = 16u;
    assert(agent_openai_provider_init(&provider, &config) == AGENT_OK);
    assert(complete(&provider, &request, &collected) == AGENT_ERROR_CAPACITY);
    fake.ignore_body_errors = true;
    assert(complete(&provider, &request, &collected) == AGENT_ERROR_CAPACITY);
    fake.ignore_body_errors = false;

    config = config_for(&fake);
    assert(agent_openai_provider_init(&provider, &config) == AGENT_OK);
    collected.sink_error = AGENT_ERROR_TRUNCATED;
    assert(complete(&provider, &request, &collected) == AGENT_ERROR_TRUNCATED);
}

static void test_lifecycle_and_partial_failures(void)
{
    static const char answer[] =
        "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"ok\"},"
        "\"finish_reason\":\"stop\"}]}";
    static const char bad_second_call[] =
        "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":null,"
        "\"tool_calls\":[{\"id\":\"c1\",\"type\":\"function\",\"function\":{"
        "\"name\":\"set_light\",\"arguments\":\"{}\"}},{\"id\":\"c2\","
        "\"type\":\"function\",\"function\":{\"name\":\"set_light\","
        "\"arguments\":\"{bad}\"}}]},\"finish_reason\":\"tool_calls\"}]}";
    static const char refusal[] =
        "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":null,"
        "\"refusal\":\"I cannot help with that.\",\"tool_calls\":null},"
        "\"finish_reason\":\"stop\"}]}";
    agent_message_view_t message = {AGENT_MESSAGE_ROLE_USER, SV("Hi"),
                                     {NULL, 0u}, NULL, 0u};
    fake_http_t fake = {0};
    collected_t collected = {0};
    agent_openai_provider_t provider;
    agent_openai_config_t config = config_for(&fake);
    agent_model_request_t request = request_for(&message, 1u);
    agent_cancel_token_t cancel;

    fake.response = answer;
    fake.response_size = sizeof(answer) - 1u;
    clock_ms = 1000u;
    agent_cancel_token_init(&cancel, NULL);
    request.cancel = &cancel;
    assert(agent_openai_provider_init(&provider, &config) == AGENT_OK);
    agent_cancel_token_request_locked(&cancel);
    assert(complete(&provider, &request, &collected) == AGENT_ERROR_CANCELLED);
    assert(fake.requests == 0u && !provider.active);

    agent_cancel_token_init(&cancel, NULL);
    fake.cancel_after_headers = &cancel;
    assert(complete(&provider, &request, &collected) == AGENT_ERROR_CANCELLED);
    assert(collected.text_size == 0u && !provider.active);
    fake.cancel_after_headers = NULL;
    request.cancel = NULL;
    fake.clock_after_headers = 1250u;
    assert(complete(&provider, &request, &collected) == AGENT_ERROR_TIMEOUT);
    assert(collected.text_size == 0u && !provider.active);
    fake.clock_after_headers = 0u;
    clock_ms = 1000u;

    fake.omit_headers = true;
    assert(complete(&provider, &request, &collected) == AGENT_ERROR_IO);
    fake.omit_headers = false;
    fake.response = bad_second_call;
    fake.response_size = sizeof(bad_second_call) - 1u;
    assert(complete(&provider, &request, &collected) == AGENT_ERROR_TOOL_ARGUMENT);
    assert(collected.calls == 0u && collected.text_size == 0u);

    fake.response = answer;
    fake.response_size = sizeof(answer) - 1u;
    assert(complete(&provider, &request, &collected) == AGENT_OK);
    assert(collected.text_size == 2u && memcmp(collected.text, "ok", 2u) == 0);

    memset(&collected, 0, sizeof(collected));
    fake.response = refusal;
    fake.response_size = sizeof(refusal) - 1u;
    assert(complete(&provider, &request, &collected) == AGENT_OK);
    assert(collected.text_size == sizeof("I cannot help with that.") - 1u);

    config = config_for(&fake);
    config.authorization = (agent_string_view_t){NULL, 0u};
    config.endpoint = SV("http://example.invalid/v1/chat/completions");
    assert(agent_openai_provider_init(&provider, &config) == AGENT_OK);
    assert(complete(&provider, &request, &collected) == AGENT_OK);
}

int main(void)
{
    test_text_and_history();
    test_tool_call_and_failures();
    test_limits_and_validation();
    test_lifecycle_and_partial_failures();
    puts("PASS: OpenAI non-streaming provider contract");
    return 0;
}
