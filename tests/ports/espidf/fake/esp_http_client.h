/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdbool.h>

#include <esp_err.h>

typedef struct esp_http_client* esp_http_client_handle_t;

typedef enum {
    HTTP_METHOD_GET,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT,
    HTTP_METHOD_PATCH,
    HTTP_METHOD_DELETE,
    HTTP_METHOD_HEAD
} esp_http_client_method_t;

typedef enum {
    HTTP_EVENT_ERROR,
    HTTP_EVENT_ON_CONNECTED,
    HTTP_EVENT_HEADER_SENT,
    HTTP_EVENT_ON_HEADER,
    HTTP_EVENT_ON_DATA,
    HTTP_EVENT_ON_FINISH
} esp_http_client_event_id_t;

typedef struct esp_http_client_event {
    esp_http_client_event_id_t event_id;
    esp_http_client_handle_t client;
    void* data;
    int data_len;
    void* user_data;
    char* header_key;
    char* header_value;
} esp_http_client_event_t;

typedef esp_err_t (*esp_http_client_event_cb_t)(esp_http_client_event_t* event);

typedef struct {
    const char* url;
    const char* cert_pem;
    esp_err_t (*crt_bundle_attach)(void* conf);
    int timeout_ms;
    bool disable_auto_redirect;
    esp_http_client_event_cb_t event_handler;
    void* user_data;
} esp_http_client_config_t;

esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t* config);
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t client);
esp_err_t esp_http_client_set_method(esp_http_client_handle_t client,
                                     esp_http_client_method_t method);
esp_err_t esp_http_client_set_header(esp_http_client_handle_t client, const char* key,
                                     const char* value);
esp_err_t esp_http_client_set_post_field(esp_http_client_handle_t client, const char* data,
                                         int length);
esp_err_t esp_http_client_perform(esp_http_client_handle_t client);
int esp_http_client_get_status_code(esp_http_client_handle_t client);
