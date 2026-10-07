/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 tanglinjie */
/* WebClient supplies HTTP/TLS; this adapter bounds inputs and consumes one POST synchronously. */
#include <agent_rtthread_transport.h>
#include <agent.h>
#include <webclient.h>
#include "core/text_internal.h"
#include "transport/transport_internal.h"
#include <limits.h>
#include <string.h>

#if !defined(WEBCLIENT_SW_VERSION_NUM) || WEBCLIENT_SW_VERSION_NUM < 0x20300
#error The RT-Thread Transport port requires the WebClient 2.3 API or later
#endif
#ifdef WEBCLIENT_DEBUG
#error Disable WEBCLIENT_DEBUG: upstream debug logging exposes request credentials
#endif

static bool equal_views(agent_string_view_t text, agent_string_view_t other)
{
    size_t i;
    if (text.size != other.size) return false;
    for (i = 0u; i < text.size; ++i) {
        unsigned char c = (unsigned char)text.data[i], d = (unsigned char)other.data[i];
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (d >= 'A' && d <= 'Z') d += 'a' - 'A';
        if (c != d) return false;
    }
    return true;
}

static bool equal(agent_string_view_t text, const char* literal)
{
    return equal_views(text, agent_string_view(literal, strlen(literal)));
}

static bool header_name(agent_string_view_t text)
{
    size_t i;
    for (i = 0u; i < text.size; ++i) {
        unsigned char c = (unsigned char)text.data[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || strchr("!#$%&'*+-.^_`|~", c))) return false;
        if (!c) return false;
    }
    return text.size != 0u;
}

static bool header_value(agent_string_view_t text)
{
    size_t i;
    for (i = 0u; i < text.size; ++i) {
        unsigned char c = (unsigned char)text.data[i];
        if (c == 0x7fu || (c < 0x20u && c != '\t')) return false;
    }
    return true;
}

static agent_error_t poll(const agent_port_rtthread_transport_t* state, const agent_http_request_t* request)
{
    if (agent_cancel_token_is_set(request->cancel)) return AGENT_ERROR_CANCELLED;
    if (request->deadline_ms && state->config.runtime.now_ms(state->config.runtime.clock_context) >= request->deadline_ms)
        return AGENT_ERROR_TIMEOUT;
    return AGENT_OK;
}

static agent_error_t sdk_error(int code)
{
    if (code == -WEBCLIENT_TIMEOUT) return AGENT_ERROR_TIMEOUT;
    if (code == -WEBCLIENT_NOMEM) return AGENT_ERROR_NOMEM;
    if (code == -WEBCLIENT_NOBUFFER) return AGENT_ERROR_CAPACITY;
    return AGENT_ERROR_IO;
}

static bool safe_buffer(const agent_port_rtthread_transport_t* state, const void* data, size_t bytes)
{
    const agent_port_rtthread_transport_config_t* c = &state->config;
    return !agent_bytes_overlap(data, bytes, state, sizeof(*state)) &&
        !agent_bytes_overlap(data, bytes, c->url_buffer, c->url_capacity) &&
        !agent_bytes_overlap(data, bytes, c->io_buffer, c->io_capacity) &&
        !agent_bytes_overlap(data, bytes, c->response_headers, c->response_header_capacity * sizeof(agent_http_header_t));
}

static agent_error_t prepare(agent_port_rtthread_transport_t* state, const agent_http_request_t* request)
{
    size_t i, authority, end, bytes = 0u;
    bool https;
    if (!equal(request->method, "POST") || request->method.size != 4u ||
        memcmp(request->method.data, "POST", 4u) || !request->body_size) return AGENT_ERROR_NOT_SUPPORTED;
    if (request->body_size > INT_MAX || request->url.size >= state->config.url_capacity)
        return AGENT_ERROR_CAPACITY;
    if (!safe_buffer(state, request, sizeof(*request)) || !safe_buffer(state, request->body, request->body_size) ||
        !safe_buffer(state, request->url.data, request->url.size) ||
        request->header_count > SIZE_MAX / sizeof(*request->headers) ||
        !safe_buffer(state, request->headers, request->header_count * sizeof(*request->headers))) return AGENT_ERROR_INVALID;
    https = request->url.size >= 8u && !memcmp(request->url.data, "https://", 8u);
    authority = https ? 8u : 7u;
    if (!https && (request->url.size < 7u || memcmp(request->url.data, "http://", 7u))) return AGENT_ERROR_INVALID;
    end = authority;
    for (i = 0u; i < request->url.size; ++i) {
        unsigned char c = (unsigned char)request->url.data[i];
        if (c <= 0x20u || c >= 0x7fu || c == '#' || c == '\\') return AGENT_ERROR_INVALID;
    }
    while (end < request->url.size && request->url.data[end] != '/') {
        if (request->url.data[end] == '@' || request->url.data[end] == '?') return AGENT_ERROR_INVALID;
        ++end;
    }
    if (end == authority) return AGENT_ERROR_INVALID;
    if (https) {
#if !defined(WEBCLIENT_USING_MBED_TLS) && !defined(WEBCLIENT_USING_SAL_TLS)
        return AGENT_ERROR_NOT_SUPPORTED;
#else
        if (!state->config.authenticated_tls) return AGENT_ERROR_AUTH;
#endif
    }
    for (i = 0u; i < request->header_count; ++i) {
        const agent_http_header_t* h = &request->headers[i];
        size_t j;
        if (h->name.size > state->config.sdk_header_capacity || h->value.size > state->config.sdk_header_capacity)
            return AGENT_ERROR_CAPACITY;
        if (!safe_buffer(state, h->name.data, h->name.size) || !safe_buffer(state, h->value.data, h->value.size) ||
            !header_name(h->name) || !header_value(h->value)) return AGENT_ERROR_INVALID;
        if (equal(h->name, "host") || equal(h->name, "content-length") ||
            equal(h->name, "transfer-encoding") || equal(h->name, "connection") ||
            equal(h->name, "expect") || equal(h->name, "accept-encoding")) return AGENT_ERROR_INVALID;
        for (j = 0u; j < i; ++j)
            if (equal_views(h->name, request->headers[j].name)) return AGENT_ERROR_INVALID;
        if (h->name.size > SIZE_MAX - bytes || h->value.size > SIZE_MAX - bytes - h->name.size ||
            SIZE_MAX - bytes - h->name.size - h->value.size < 4u) return AGENT_ERROR_CAPACITY;
        bytes += h->name.size + h->value.size + 4u;
    }
    /* Include all SDK-generated request text; never rely on snprintf truncation in WebClient. */
    if (bytes > state->config.sdk_header_capacity || request->url.size > state->config.sdk_header_capacity - bytes ||
        state->config.sdk_header_capacity - bytes - request->url.size < 160u) return AGENT_ERROR_CAPACITY;
    memcpy(state->config.url_buffer, request->url.data, request->url.size);
    state->config.url_buffer[request->url.size] = '\0';
    return AGENT_OK;
}

static void add(struct webclient_session* session, const char* data, size_t size)
{
    memcpy(session->header->buffer + session->header->length, data, size);
    session->header->length += size;
    session->header->buffer[session->header->length] = '\0';
}

static void request_headers(struct webclient_session* session, const agent_http_request_t* request)
{
    char digits[sizeof(size_t) * 3u];
    size_t size = request->body_size, n = 0u, i, start, end;
    bool user_agent = false, accept = false;
    const char tail[] = "\r\nConnection: close\r\nAccept-Encoding: identity\r\n";
    start = request->url.data[4] == 's' ? 8u : 7u;
    end = start;
    while (end < request->url.size && request->url.data[end] != '/') ++end;
    /* The SDK searches substrings, not field names: always supply a real Host field. */
    add(session, "Host: ", 6u);
    add(session, request->url.data + start, end - start);
    add(session, "\r\n", 2u);
    for (i = 0u; i < request->header_count; ++i) {
        if (equal(request->headers[i].name, "user-agent")) {
            add(session, "User-Agent", 10u); user_agent = true;
        } else if (equal(request->headers[i].name, "accept")) {
            add(session, "Accept", 6u); accept = true;
        } else add(session, request->headers[i].name.data, request->headers[i].name.size);
        add(session, ": ", 2u);
        add(session, request->headers[i].value.data, request->headers[i].value.size);
        add(session, "\r\n", 2u);
    }
    if (!user_agent) add(session, "User-Agent: cagent\r\n", sizeof("User-Agent: cagent\r\n") - 1u);
    if (!accept) add(session, "Accept: */*\r\n", sizeof("Accept: */*\r\n") - 1u);
    add(session, "Content-Length: ", 16u);
    do { digits[n++] = (char)('0' + size % 10u); size /= 10u; } while (size);
    while (n) add(session, &digits[--n], 1u);
    add(session, tail, sizeof(tail) - 1u);
}

static agent_error_t response_headers(agent_port_rtthread_transport_t* state, struct webclient_session* session,
    const agent_http_request_t* request, unsigned status, size_t* count, bool* known, size_t* length, bool* chunked)
{
    size_t pos = 0u, lines = 0u, wire_bytes = 0u;
    struct webclient_header* header = session->header;
    if (!header || !header->buffer || header->length >= header->size ||
        header->length > request->max_response_header_bytes) return AGENT_ERROR_CAPACITY;
    *count = 0u; *known = *chunked = false; *length = 0u;
    while (pos < header->length) {
        char* data = header->buffer + pos;
        const char* zero = memchr(data, 0, header->length - pos);
        size_t size, colon = 0u, start, end;
        agent_http_header_t h;
        if (!zero) return AGENT_ERROR_IO;
        size = (size_t)(zero - data); pos += size + 1u;
        if (size > request->max_response_header_bytes - wire_bytes ||
            request->max_response_header_bytes - wire_bytes - size < 2u) return AGENT_ERROR_CAPACITY;
        wire_bytes += size + 2u;
        if (!lines++) {
            if (size < 12u || (memcmp(data, "HTTP/1.1 ", 9u) && memcmp(data, "HTTP/1.0 ", 9u)) ||
                data[9] != (char)('0' + status / 100u) || data[10] != (char)('0' + status / 10u % 10u) ||
                data[11] != (char)('0' + status % 10u) || (size > 12u && data[12] != ' ')) return AGENT_ERROR_IO;
            continue;
        }
        while (colon < size && data[colon] != ':') ++colon;
        if (colon == size) return AGENT_ERROR_IO;
        h.name = agent_string_view(data, colon);
        start = colon + 1u; end = size;
        while (start < end && (data[start] == ' ' || data[start] == '\t')) ++start;
        while (end > start && (data[end - 1u] == ' ' || data[end - 1u] == '\t')) --end;
        h.value = agent_string_view(data + start, end - start);
        if (!header_name(h.name) || !header_value(h.value)) return AGENT_ERROR_IO;
        if (*count == state->config.response_header_capacity) return AGENT_ERROR_CAPACITY;
        state->config.response_headers[(*count)++] = h;
        if (equal(h.name, "content-length")) {
            size_t number = 0u, i;
            if (!h.value.size || *known) return AGENT_ERROR_IO;
            for (i = 0u; i < h.value.size; ++i) {
                unsigned c = (unsigned char)h.value.data[i];
                if (c < '0' || c > '9' || number > (SIZE_MAX - (c - '0')) / 10u) return AGENT_ERROR_IO;
                number = number * 10u + c - '0';
            }
            if (number > request->max_response_bytes || number > INT_MAX) return AGENT_ERROR_CAPACITY;
            *known = true; *length = number;
        }
        if (equal(h.name, "transfer-encoding")) {
            if (*chunked || h.value.size != 7u || memcmp(h.value.data, "chunked", 7u)) return AGENT_ERROR_NOT_SUPPORTED;
            *chunked = true;
        }
        if (equal(h.name, "content-encoding") && !equal(h.value, "identity")) return AGENT_ERROR_NOT_SUPPORTED;
    }
    if (!lines || (*known && *chunked) || (*known && session->content_length != (int)*length)) return AGENT_ERROR_IO;
    return AGENT_OK;
}

static agent_error_t perform(void* context, const agent_http_request_t* request, const agent_http_sink_t* sink)
{
    agent_port_rtthread_transport_t* state = context;
    struct webclient_session* session = NULL;
    agent_transport_t binding;
    agent_error_t result, checked;
    size_t count, length, total = 0u;
    bool known, chunked;
    int code;
    if (!state) return AGENT_ERROR_INVALID;
    if (state->active) return AGENT_ERROR_BUSY;
    binding = (agent_transport_t){NULL, state};
    /* Use the same portable descriptor checks as every Provider-facing Transport. */
    { static const agent_transport_ops_t ops = {perform}; binding.ops = &ops; }
    result = agent_transport_validate(&binding, request, sink);
    if (result != AGENT_OK) return result;
    if (!safe_buffer(state, sink, sizeof(*sink))) return AGENT_ERROR_INVALID;
    result = prepare(state, request);
    if (result != AGENT_OK) return result;
    state->active = true;
    result = poll(state, request);
    if (result != AGENT_OK) goto done;
    session = webclient_session_create(state->config.sdk_header_capacity);
    if (!session) { result = AGENT_ERROR_NOMEM; goto done; }
    if (!session->header || !session->header->buffer ||
        session->header->size < state->config.sdk_header_capacity || session->header->length) {
        result = AGENT_ERROR_IO; goto done;
    }
    request_headers(session, request);
    code = webclient_post(session, state->config.url_buffer, request->body, request->body_size);
    result = poll(state, request);
    if (result != AGENT_OK) goto done;
    if (code < 0) { result = sdk_error(code); goto done; }
    if (code < 200 || code > 599) { result = AGENT_ERROR_IO; goto done; }
    if (code >= 300 && code < 400) { result = AGENT_ERROR_NOT_SUPPORTED; goto done; }
    result = response_headers(state, session, request, (unsigned)code, &count, &known, &length, &chunked);
    if (result != AGENT_OK) goto done;
    result = sink->headers(sink->context, (unsigned)code, state->config.response_headers, count);
    if (result != AGENT_OK) goto done;
    for (;;) {
        size_t amount = state->config.io_capacity;
        result = poll(state, request);
        if (result != AGENT_OK) break;
        if ((known && total == length) || (chunked && session->chunk_sz <= 0)) break;
        if (known && amount > length - total) amount = length - total;
        code = webclient_read(session, state->config.io_buffer, amount);
        result = poll(state, request);
        if (result != AGENT_OK) break;
        if (code < 0) { result = sdk_error(code); break; }
        if (!code) {
            if ((known && total != length) || (chunked && session->chunk_sz > 0)) result = AGENT_ERROR_IO;
            break;
        }
        if ((size_t)code > amount) { result = AGENT_ERROR_IO; break; }
        if ((size_t)code > request->max_response_bytes - total) { result = AGENT_ERROR_CAPACITY; break; }
        total += (size_t)code;
        result = sink->body(sink->context, state->config.io_buffer, (size_t)code);
        if (result != AGENT_OK) break;
    }
done:
    if (session) {
        code = webclient_close(session);
        if (result == AGENT_OK && code < 0) result = sdk_error(code);
    }
    checked = poll(state, request);
    if (result == AGENT_OK) result = checked;
    state->active = false;
    return result;
}

agent_error_t agent_port_rtthread_transport_init(agent_transport_t* out,
    agent_port_rtthread_transport_t* state, const agent_port_rtthread_transport_config_t* config)
{
    static const agent_transport_ops_t ops = {perform};
    const void* pointers[5]; size_t sizes[5], i, j;
    if (!out || !state || !config || !config->runtime.now_ms || !config->sdk_header_capacity ||
        config->sdk_header_capacity > UINT16_MAX || !config->url_buffer || !config->url_capacity ||
        !config->io_buffer || !config->io_capacity || config->io_capacity > INT_MAX ||
        !config->response_headers || !config->response_header_capacity ||
        config->response_header_capacity > SIZE_MAX / sizeof(agent_http_header_t)) return AGENT_ERROR_INVALID;
    pointers[0] = out; sizes[0] = sizeof(*out);
    pointers[1] = state; sizes[1] = sizeof(*state);
    pointers[2] = config->url_buffer; sizes[2] = config->url_capacity;
    pointers[3] = config->io_buffer; sizes[3] = config->io_capacity;
    pointers[4] = config->response_headers; sizes[4] = config->response_header_capacity * sizeof(agent_http_header_t);
    for (i = 0u; i < 5u; ++i) {
        if (agent_bytes_overlap(pointers[i], sizes[i], config, sizeof(*config))) return AGENT_ERROR_INVALID;
        for (j = 0u; j < i; ++j)
            if (agent_bytes_overlap(pointers[i], sizes[i], pointers[j], sizes[j])) return AGENT_ERROR_INVALID;
    }
    *state = (agent_port_rtthread_transport_t){*config, false};
    *out = (agent_transport_t){&ops, state};
    return AGENT_OK;
}
