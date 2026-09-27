/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* OpenVela webclient adapter contract tests with a minimal NuttX API fake. */

#include <agent_openvela_transport.h>
#include <agent/run.h>
#include "run/run_internal.h"

#include <netutils/webclient.h>

#include <string.h>

static uint64_t fake_now_ms = 1000u;
static unsigned int fake_status = 200u;
static unsigned int fake_calls;
static bool fake_empty_body;
static bool fake_bad_buffer;
static const struct webclient_tls_ops fake_tls_ops = {0};

void webclient_set_defaults(struct webclient_context* client)
{
    *client = (struct webclient_context){0};
}

void webclient_set_static_body(struct webclient_context* client, const void* body, size_t size)
{
    client->body = body;
    client->bodylen = size;
}

int webclient_perform(struct webclient_context* client)
{
    char body[] = "{}";
    char* data = body;
    int buflen = (int)sizeof(body);

    ++fake_calls;
    if (client->method == NULL || client->url == NULL || client->buffer == NULL ||
        client->buflen == 0 || client->protocol_version != WEBCLIENT_PROTOCOL_VERSION_HTTP_1_1 ||
        client->nheaders != 1u || strcmp(client->headers[0], "Accept: application/json") != 0 ||
        client->bodylen != 2u)
    {
        return -1;
    }
    client->http_status = fake_status;
    if (client->header_callback(fake_status == 302u ? "Location: https://example.invalid/other" :
                                "Content-Type: application/json", false,
                                client->header_callback_arg) != 0)
    {
        return -1;
    }
    if (fake_status != 302u && !fake_empty_body &&
        client->sink_callback(&data, 0, 2,
                              fake_bad_buffer ? NULL : &buflen,
                              client->sink_callback_arg) != 0)
    {
        return -1;
    }
    return 0;
}

static uint64_t test_now_ms(void* context)
{
    (void)context;
    return fake_now_ms;
}

typedef struct {
    unsigned int header_count;
    unsigned int body_count;
    unsigned int status;
    char body[8];
} test_sink_t;

static agent_error_t test_headers(void* context, unsigned int status,
                                  const agent_http_header_t* headers, size_t count)
{
    test_sink_t* state = (test_sink_t*)context;
    if (status != fake_status || count != 1u || headers == NULL ||
        headers[0].name.size != 12u ||
        memcmp(headers[0].name.data, "Content-Type", 12u) != 0)
    {
        return AGENT_ERROR;
    }
    ++state->header_count;
    state->status = status;
    return AGENT_OK;
}

static agent_error_t test_body(void* context, const void* data, size_t size)
{
    test_sink_t* state = (test_sink_t*)context;
    if (state->header_count == 0u || size > sizeof(state->body))
    {
        return AGENT_ERROR;
    }
    memcpy(state->body, data, size);
    ++state->body_count;
    return AGENT_OK;
}

int main(void)
{
    static char url_buffer[128];
    static char request_header_buffer[128];
    static const char* request_headers[4];
    static char io_buffer[256];
    static agent_http_header_t response_headers[4];
    static char response_header_buffer[128];
    static const agent_http_header_t headers[] = {
        {AGENT_SV_LITERAL("Accept"), AGENT_SV_LITERAL("application/json")},
    };
    agent_port_openvela_transport_config_t config = {0};
    agent_port_openvela_transport_t state;
    agent_transport_t transport;
    agent_http_request_t request = {0};
    agent_cancel_token_t token;
    test_sink_t sink_state = {0};
    agent_http_sink_t sink = {test_headers, test_body, &sink_state};

    config.runtime.now_ms = test_now_ms;
    config.tls_ops = &fake_tls_ops;
    config.request_timeout_ms = 5000u;
    config.url_buffer = url_buffer;
    config.url_buffer_size = sizeof(url_buffer);
    config.request_header_buffer = request_header_buffer;
    config.request_header_buffer_size = sizeof(request_header_buffer);
    config.request_headers = request_headers;
    config.request_header_capacity = 4u;
    config.io_buffer = io_buffer;
    config.io_buffer_size = sizeof(io_buffer);
    config.response_headers = response_headers;
    config.response_header_capacity = 4u;
    config.response_header_buffer = response_header_buffer;
    config.response_header_buffer_size = sizeof(response_header_buffer);
    if (agent_port_openvela_transport_init(&transport, &state, &config) != AGENT_OK)
    {
        return 1;
    }
    request.method = agent_string_view("POST", 4u);
    request.url = agent_string_view("https://example.invalid/api",
                                    sizeof("https://example.invalid/api") - 1u);
    request.headers = headers;
    request.header_count = 1u;
    request.body = "{}";
    request.body_size = 2u;
    request.max_response_bytes = 8u;
    request.max_response_header_bytes = 64u;

    if (transport.ops->request(transport.context, &request, &sink) != AGENT_OK ||
        sink_state.header_count != 1u || sink_state.body_count != 1u ||
        sink_state.status != 200u || memcmp(sink_state.body, "{}", 2u) != 0)
    {
        return 2;
    }
    fake_status = 401u;
    fake_empty_body = true;
    if (transport.ops->request(transport.context, &request, &sink) != AGENT_OK ||
        sink_state.status != 401u || sink_state.header_count != 2u ||
        sink_state.body_count != 1u)
    {
        return 9;
    }
    fake_status = 200u;
    fake_empty_body = false;
    request.max_response_bytes = 1u;
    if (transport.ops->request(transport.context, &request, &sink) != AGENT_ERROR_CAPACITY)
    {
        return 3;
    }
    request.max_response_bytes = 8u;
    fake_status = 302u;
    if (transport.ops->request(transport.context, &request, &sink) != AGENT_ERROR_NOT_SUPPORTED)
    {
        return 4;
    }
    fake_status = 200u;
    state.config.tls_ops = NULL;
    if (transport.ops->request(transport.context, &request, &sink) != AGENT_ERROR_NOT_SUPPORTED)
    {
        return 5;
    }
    state.config.tls_ops = &fake_tls_ops;
    request.deadline_ms = fake_now_ms;
    if (transport.ops->request(transport.context, &request, &sink) != AGENT_ERROR_TIMEOUT)
    {
        return 6;
    }
    if (fake_calls != 4u)
    {
        return 7;
    }
    request.deadline_ms = 0u;
    fake_bad_buffer = true;
    if (transport.ops->request(transport.context, &request, &sink) != AGENT_ERROR_IO)
    {
        return 10;
    }
    fake_bad_buffer = false;
    agent_cancel_token_init(&token, NULL);
    request.cancel = &token;
    agent_cancel_token_request_locked(&token);
    if (!agent_cancel_token_is_set(&token) ||
        transport.ops->request(transport.context, &request, &sink) != AGENT_ERROR_CANCELLED)
    {
        return 8;
    }
    return 0;
}
