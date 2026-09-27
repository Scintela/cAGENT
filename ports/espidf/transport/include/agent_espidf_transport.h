/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* ESP-IDF esp_http_client Transport adapter. */
#pragma once

#include <agent/runtime.h>
#include <agent/transport.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Caller-owned buffers and services for one non-reentrant ESP-IDF Transport. */
typedef struct {
    agent_runtime_t runtime;                  /* Copied; now_ms must be set. */
    const char* cert_pem;                     /* Optional borrowed PEM trust store. */
    bool use_crt_bundle;                      /* Use the ESP-IDF certificate bundle instead of cert_pem. */
    uint32_t request_timeout_ms;              /* Required nonzero per-request ceiling. */
    char* url_buffer;                         /* Caller NUL-termination buffer. */
    size_t url_buffer_size;                   /* URL buffer capacity. */
    char* request_header_name_buffer;         /* Caller NUL-termination buffer. */
    size_t request_header_name_buffer_size;   /* Header-name buffer capacity. */
    char* request_header_value_buffer;        /* Caller NUL-termination buffer. */
    size_t request_header_value_buffer_size;  /* Header-value buffer capacity. */
    agent_http_header_t* response_headers;    /* Caller response-header descriptor array. */
    size_t response_header_capacity;          /* Descriptor count. */
    char* response_header_buffer;             /* Caller response-header text storage. */
    size_t response_header_buffer_size;       /* Text storage capacity. */
} agent_port_espidf_transport_config_t;

/* Persistent state; one request may execute at a time. */
typedef struct {
    agent_port_espidf_transport_config_t config; /* Copied configuration, buffers remain borrowed. */
    bool active;                                  /* Rejects recursive use on the calling task. */
} agent_port_espidf_transport_t;

/* Bind an ESP-IDF Transport to caller-owned state and buffers. */
agent_error_t agent_port_espidf_transport_init(
    agent_transport_t* out, agent_port_espidf_transport_t* state,
    const agent_port_espidf_transport_config_t* config);

#ifdef __cplusplus
}
#endif
