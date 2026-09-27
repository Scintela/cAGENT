/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* Internal transport contract tests. */

#include <agent/transport.h>

#include "transport/transport_internal.h"

static unsigned int request_count;
static unsigned int header_count;
static unsigned int body_count;

static agent_error_t test_headers(void* context, unsigned int status,
                                  const agent_http_header_t* headers, size_t count)
{
    (void)context;
    (void)headers;

    if (status != 200u || count != 0u)
    {
        return AGENT_ERROR;
    }

    ++header_count;
    return AGENT_OK;
}

static agent_error_t test_body(void* context, const void* data, size_t size)
{
    (void)context;

    if (data == NULL || size != 2u)
    {
        return AGENT_ERROR;
    }

    ++body_count;
    return AGENT_OK;
}

static agent_error_t test_request(void* context, const agent_http_request_t* request,
                                  const agent_http_sink_t* sink)
{
    static const char response[] = "ok";

    if (context != &request_count || request->method.size != 3u || request->url.size == 0u)
    {
        return AGENT_ERROR;
    }

    ++request_count;
    if (sink->headers(sink->context, 200u, NULL, 0u) != AGENT_OK)
    {
        return AGENT_ERROR;
    }

    return sink->body(sink->context, response, sizeof(response) - 1u);
}

int main(void)
{
    static const agent_http_header_t headers[] = {
        {AGENT_SV_LITERAL("Accept"), AGENT_SV_LITERAL("application/json")},
    };
    static const agent_transport_ops_t ops = {test_request};
    agent_transport_t transport = {&ops, &request_count};
    agent_http_request_t request = {
        AGENT_SV_LITERAL("GET"),
        AGENT_SV_LITERAL("https://example.invalid/v1/test"),
        headers,
        1u,
        NULL,
        0u,
        128u,
        128u,
        0u,
        NULL,
    };
    agent_http_sink_t sink = {test_headers, test_body, NULL};
    agent_error_t status;

    status = agent_transport_request(&transport, &request, &sink);
    if (status != AGENT_OK || request_count != 1u || header_count != 1u || body_count != 1u)
    {
        return 1;
    }

    request.body_size = 1u;
    if (agent_transport_request(&transport, &request, &sink) != AGENT_ERROR_INVALID)
    {
        return 2;
    }

    request.body_size = 0u;
    request.max_response_bytes = 0u;
    if (agent_transport_request(&transport, &request, &sink) != AGENT_ERROR_INVALID)
    {
        return 3;
    }

    request.max_response_bytes = 128u;
    transport.ops = NULL;
    if (agent_transport_request(&transport, &request, &sink) != AGENT_ERROR_INVALID)
    {
        return 4;
    }

    return 0;
}
