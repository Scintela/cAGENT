/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 tanglinjie */
#include <agent_rtthread_transport.h>
#include <webclient.h>
#include "run/run_internal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SV(s) agent_string_view((s), sizeof(s)-1u)
static uint64_t clock_ms;
static int status, read_error, close_error, creates, closes, posts, reads, headers_seen, bodies_seen;
static bool allocation_failure, expire_post, cancel_read, recurse, chunk, malformed, unknown;
static size_t offset, wire_size;
static const char* fields;
static const char* extra_field;
static const char body[] = "hello";
static char sent[1024], received[64];
static agent_error_t header_error, body_error;
static agent_cancel_token_t token;
static agent_transport_t transport;
static agent_http_request_t request;
static agent_http_sink_t sink;
static unsigned delivered_status;
static uint64_t now(void* ctx) { (void)ctx; return clock_ms; }
struct webclient_session* webclient_session_create(size_t cap)
{
    struct webclient_session* s;
    ++creates;
    if (allocation_failure) return NULL;
    s = calloc(1u, sizeof(*s)); assert(s);
    s->header = calloc(1u, sizeof(*s->header)); assert(s->header);
    s->header->buffer = calloc(1u, cap); assert(s->header->buffer);
    s->header->size = cap;
    return s;
}
static void line(struct webclient_session* s, const char* text)
{
    size_t n = strlen(text) + 1u;
    assert(n < s->header->size - s->header->length);
    memcpy(s->header->buffer + s->header->length, text, n); s->header->length += n;
}
int webclient_post(struct webclient_session* s, const char* url, const void* data, size_t n)
{
    char first[64];
    ++posts;
    assert(strstr(url, "://") && n == 2u && !memcmp(data, "{}", n));
    assert(s->header->length == strlen(s->header->buffer));
    assert(s->header->length < sizeof(sent));
    memcpy(sent, s->header->buffer, s->header->length + 1u);
    if (expire_post) clock_ms = 100u;
    if (status < 0) return status;
    s->header->length = 0;
    snprintf(first, sizeof(first), "HTTP/1.1 %03d Result", status);
    line(s, malformed ? "HTTP/1.1 000 bad" : first);
    if (fields) line(s, fields);
    else if (chunk) line(s, "Transfer-Encoding: chunked");
    else if (!unknown) {
        snprintf(first, sizeof(first), "Content-Length: %zu", wire_size);
        line(s, first);
    }
    if (extra_field) line(s, extra_field);
    s->content_length = unknown || chunk ? -1 : (int)wire_size;
    s->chunk_sz = chunk ? (int)wire_size : 0;
    return status;
}
int webclient_read(struct webclient_session* s, void* data, size_t n)
{
    size_t available = sizeof(body)-1u-offset;
    ++reads;
    if (cancel_read) token.requested = true;
    if (read_error) return read_error;
    if (n > available) n = available;
    if (n > 2u) n = 2u;
    memcpy(data, body + offset, n); offset += n;
    if (chunk && offset == wire_size) s->chunk_sz = -1;
    return (int)n;
}
int webclient_close(struct webclient_session* s)
{ ++closes; free(s->header->buffer); free(s->header); free(s); return close_error; }
static agent_error_t on_headers(void* ctx, unsigned code, const agent_http_header_t* h, size_t n)
{
    (void)ctx; (void)h; (void)n;
    ++headers_seen; delivered_status = code;
    if (recurse) assert(transport.ops->request(transport.context, &request, &sink) == AGENT_ERROR_BUSY);
    return header_error;
}
static agent_error_t on_body(void* ctx, const void* data, size_t n)
{
    (void)ctx;
    assert(headers_seen == 1 && n <= 2u && strlen(received)+n < sizeof(received));
    memcpy(received+strlen(received), data, n); ++bodies_seen;
    return body_error;
}
static void reset(void)
{
    static const agent_http_header_t h[] = {{ {"Authorization",13u}, {"Bearer test",11u} }};
    status = 200; read_error = close_error = 0; offset = 0; wire_size = 5u; fields = extra_field = NULL;
    clock_ms = 0; allocation_failure = expire_post = cancel_read = recurse = chunk = malformed = unknown = false;
    creates = closes = posts = reads = headers_seen = bodies_seen = 0;
    header_error = body_error = AGENT_OK;
    memset(received, 0, sizeof(received)); memset(sent, 0, sizeof(sent));
    agent_cancel_token_init(&token, NULL);
    request = (agent_http_request_t){0};
    request.method = SV("POST"); request.url = SV("http://example.com/v1/chat/completions");
    request.headers = h; request.header_count = 1u; request.body = "{}"; request.body_size = 2u;
    request.max_response_bytes = 32u; request.max_response_header_bytes = 512u;
    request.cancel = &token;
    sink = (agent_http_sink_t){on_headers,on_body,NULL};
}
static agent_error_t run(void) { return transport.ops->request(transport.context, &request, &sink); }
int main(void)
{
    char url[256], io[16]; agent_http_header_t response[8];
    agent_port_rtthread_transport_t state;
    agent_port_rtthread_transport_config_t c = {0};
    c.runtime.now_ms = now; c.sdk_header_capacity = 1024u;
    c.url_buffer = url; c.url_capacity = sizeof(url); c.io_buffer = io; c.io_capacity = sizeof(io);
    c.response_headers = response; c.response_header_capacity = 8u;
    assert(agent_port_rtthread_transport_init(NULL, &state, &c) == AGENT_ERROR_INVALID);
    { char* old = c.io_buffer; c.io_buffer = url;
      assert(agent_port_rtthread_transport_init(&transport, &state, &c) == AGENT_ERROR_INVALID); c.io_buffer = old; }
    assert(agent_port_rtthread_transport_init(&transport, &state, &c) == AGENT_OK);
    reset(); recurse = true; assert(run() == AGENT_OK && !strcmp(received, "hello"));
    assert(creates == 1 && closes == 1 && headers_seen == 1 && bodies_seen == 3);
    assert(strstr(sent, "Host: example.com\r\n") && strstr(sent, "Content-Length: 2\r\n"));
    assert(strstr(sent, "Accept-Encoding: identity\r\n") && strstr(sent, "Authorization: Bearer test\r\n"));
    reset(); status = 401; assert(run() == AGENT_OK && delivered_status == 401);
    reset(); status = 429; assert(run() == AGENT_OK && delivered_status == 429);
    reset(); status = 503; assert(run() == AGENT_OK && delivered_status == 503);
    reset(); status = 302; assert(run() == AGENT_ERROR_NOT_SUPPORTED && !headers_seen && closes == 1);
    reset(); request.method = SV("GET"); assert(run() == AGENT_ERROR_NOT_SUPPORTED && !creates);
    reset(); request.body_size = 0u; assert(run() == AGENT_ERROR_NOT_SUPPORTED);
    reset(); request.url = SV("https://example.com/v1/chat/completions");
#if defined(WEBCLIENT_USING_SAL_TLS) || defined(WEBCLIENT_USING_MBED_TLS)
    assert(run() == AGENT_ERROR_AUTH && !creates);
    state.config.authenticated_tls = true; assert(run() == AGENT_OK);
#else
    assert(run() == AGENT_ERROR_NOT_SUPPORTED && !creates);
#endif
    reset(); request.url = SV("http://user@example.com/path"); assert(run() == AGENT_ERROR_INVALID);
    reset(); request.url = SV("http://example.com/\r\nbad"); assert(run() == AGENT_ERROR_INVALID);
    reset(); request.url = SV("http://example.com/#fragment"); assert(run() == AGENT_ERROR_INVALID);
    reset(); request.url = agent_string_view(url, 1u); assert(run() == AGENT_ERROR_INVALID);
    reset(); request.body = io; assert(run() == AGENT_ERROR_INVALID);
    { agent_http_header_t h[2] = {{SV("X-Test"),SV("Host: in value")},{SV("accept"),SV("application/json")}};
      reset(); request.headers = h; request.header_count = 2u;
      assert(run() == AGENT_OK && strstr(sent, "Host: example.com\r\n") && strstr(sent, "Accept: application/json\r\n"));
      reset(); h[0].value = SV("bad\r\nInjected: yes"); request.headers = h; request.header_count = 1u;
      assert(run() == AGENT_ERROR_INVALID && !creates);
      reset(); h[0].name = SV("Host"); request.headers = h; assert(run() == AGENT_ERROR_INVALID);
      reset(); h[0].name = SV("accept"); h[0].value = SV("json"); request.headers = h; request.header_count = 2u;
      assert(run() == AGENT_ERROR_INVALID);
      reset(); h[0].name = SV("X-Test"); h[0].value = agent_string_view("x", SIZE_MAX); request.headers = h;
      assert(run() == AGENT_ERROR_CAPACITY); }
    reset(); state.config.sdk_header_capacity = 100u; assert(run() == AGENT_ERROR_CAPACITY && !creates);
    state.config.sdk_header_capacity = 1024u;
    reset(); allocation_failure = true; assert(run() == AGENT_ERROR_NOMEM && !closes);
    reset(); status = -WEBCLIENT_TIMEOUT; assert(run() == AGENT_ERROR_TIMEOUT && closes == 1);
    reset(); status = -WEBCLIENT_CONNECT_FAILED; assert(run() == AGENT_ERROR_IO && closes == 1);
    reset(); header_error = AGENT_ERROR_POLICY_DENIED; assert(run() == header_error && !reads && closes == 1);
    reset(); header_error = AGENT_ERROR_STATE; close_error = -WEBCLIENT_TIMEOUT;
    assert(run() == header_error && closes == 1);
    reset(); body_error = AGENT_ERROR_STATE; assert(run() == body_error && reads == 1 && closes == 1);
    reset(); token.requested = true; assert(run() == AGENT_ERROR_CANCELLED && !creates);
    reset(); request.deadline_ms = 1u; clock_ms = 1u; assert(run() == AGENT_ERROR_TIMEOUT && !creates);
    reset(); request.deadline_ms = 10u; expire_post = true; assert(run() == AGENT_ERROR_TIMEOUT && !headers_seen && closes == 1);
    reset(); cancel_read = true; assert(run() == AGENT_ERROR_CANCELLED && !bodies_seen && closes == 1);
    reset(); request.max_response_bytes = 4u; assert(run() == AGENT_ERROR_CAPACITY && !headers_seen);
    reset(); request.max_response_header_bytes = 1u; assert(run() == AGENT_ERROR_CAPACITY && !headers_seen);
    reset(); wire_size = 6u; assert(run() == AGENT_ERROR_IO && closes == 1);
    reset(); unknown = true; assert(run() == AGENT_OK && !strcmp(received,"hello"));
    reset(); unknown = true; request.max_response_bytes = 4u; assert(run() == AGENT_ERROR_CAPACITY);
    reset(); chunk = true; assert(run() == AGENT_OK);
    reset(); chunk = true; wire_size = 6u; assert(run() == AGENT_ERROR_IO);
    reset(); chunk = true; wire_size = 0u; assert(run() == AGENT_OK && !reads);
    reset(); read_error = -WEBCLIENT_DISCONNECT; assert(run() == AGENT_ERROR_IO && closes == 1);
    reset(); close_error = -WEBCLIENT_ERROR; assert(run() == AGENT_ERROR_IO);
    reset(); malformed = true; assert(run() == AGENT_ERROR_IO && !headers_seen);
    reset(); fields = "Content-Length: -1"; assert(run() == AGENT_ERROR_IO);
    reset(); extra_field = "Content-Length: 5"; assert(run() == AGENT_ERROR_IO && !headers_seen);
    reset(); extra_field = "Transfer-Encoding: chunked"; assert(run() == AGENT_ERROR_IO && !headers_seen);
    reset(); state.config.response_header_capacity = 1u; extra_field = "X-Test: 1";
    assert(run() == AGENT_ERROR_CAPACITY && !headers_seen); state.config.response_header_capacity = 8u;
    reset(); fields = "Content-Encoding: gzip"; assert(run() == AGENT_ERROR_NOT_SUPPORTED);
    reset(); fields = "Transfer-Encoding: Chunked"; assert(run() == AGENT_ERROR_NOT_SUPPORTED);
    reset(); fields = "bad header"; assert(run() == AGENT_ERROR_IO);
    assert(!state.active);
    return 0;
}
