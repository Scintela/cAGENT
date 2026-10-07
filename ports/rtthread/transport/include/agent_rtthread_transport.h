/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 tanglinjie */
#pragma once
#include <agent/runtime.h>
#include <agent/transport.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    agent_runtime_t runtime;
    bool authenticated_tls; /* Application attests CA-chain AND hostname validation in the TLS backend. */
    size_t sdk_header_capacity; /* WebClient allocation ceiling, 1..65535, shared by request/response. */
    char* url_buffer;
    size_t url_capacity;
    char* io_buffer;
    size_t io_capacity;
    agent_http_header_t* response_headers;
    size_t response_header_capacity;
} agent_port_rtthread_transport_config_t;

typedef struct {
    agent_port_rtthread_transport_config_t config;
    bool active;
} agent_port_rtthread_transport_t;

/* Nonempty POST only; init has no I/O; serialize calls, preserve buffers; deadlines are cooperative. */
agent_error_t agent_port_rtthread_transport_init(agent_transport_t* out,
    agent_port_rtthread_transport_t* state, const agent_port_rtthread_transport_config_t* config);
#ifdef __cplusplus
}
#endif
