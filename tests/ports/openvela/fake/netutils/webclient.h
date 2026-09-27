/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#define WEBCLIENT_PROTOCOL_VERSION_HTTP_1_1 1

struct webclient_tls_ops {
    int placeholder;
};

struct webclient_context {
    int protocol_version;
    const char* method;
    const char* url;
    const char* const* headers;
    unsigned int nheaders;
    unsigned int timeout_sec;
    char* buffer;
    int buflen;
    const struct webclient_tls_ops* tls_ops;
    void* tls_ctx;
    int (*header_callback)(const char* line, bool truncated, void* arg);
    void* header_callback_arg;
    int (*sink_callback)(char** buffer, int offset, int datend, int* buflen, void* arg);
    void* sink_callback_arg;
    unsigned int http_status;
    const void* body;
    size_t bodylen;
};

void webclient_set_defaults(struct webclient_context* client);
void webclient_set_static_body(struct webclient_context* client, const void* body, size_t size);
int webclient_perform(struct webclient_context* client);
