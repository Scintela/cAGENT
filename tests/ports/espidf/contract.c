/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* ESP-IDF Port contract tests with a minimal ESP-IDF API fake. */

#include <agent_espidf_runtime.h>
#include <agent_espidf_transport.h>
#include <agent.h>

#include <esp_http_client.h>
#include <esp_timer.h>

#include <string.h>

struct esp_http_client {
    esp_http_client_config_t config;
    esp_http_client_method_t method;
    unsigned int headers_set;
    int body_size;
};

static struct esp_http_client fake_client;
static uint64_t fake_now_ms = 1000u;
static int64_t fake_timer_us = 1234567;
static unsigned int fake_status = 200u;
static bool fake_empty_body;

esp_err_t esp_crt_bundle_attach(void* conf)
{
    (void)conf;
    return ESP_OK;
}

int64_t esp_timer_get_time(void)
{
    return fake_timer_us;
}

esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t* config)
{
    fake_client.config = *config;
    fake_client.headers_set = 0u;
    fake_client.body_size = 0;
    return &fake_client;
}

esp_err_t esp_http_client_cleanup(esp_http_client_handle_t client)
{
    return client == &fake_client ? ESP_OK : ESP_ERR_INVALID_ARG;
}

esp_err_t esp_http_client_set_method(esp_http_client_handle_t client,
                                     esp_http_client_method_t method)
{
    if (client != &fake_client)
    {
        return ESP_ERR_INVALID_ARG;
    }
    client->method = method;
    return ESP_OK;
}

esp_err_t esp_http_client_set_header(esp_http_client_handle_t client, const char* key,
                                     const char* value)
{
    if (client != &fake_client || key == NULL || value == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    ++client->headers_set;
    return ESP_OK;
}

esp_err_t esp_http_client_set_post_field(esp_http_client_handle_t client, const char* data,
                                         int length)
{
    if (client != &fake_client || data == NULL || length <= 0)
    {
        return ESP_ERR_INVALID_ARG;
    }
    client->body_size = length;
    return ESP_OK;
}

esp_err_t esp_http_client_perform(esp_http_client_handle_t client)
{
    static char header_name[] = "content-type";
    static char header_value[] = "application/json";
    static char body[] = "{}";
    esp_http_client_event_t event = {0};

    event.client = client;
    event.user_data = client->config.user_data;
    event.event_id = HTTP_EVENT_ON_HEADER;
    event.header_key = header_name;
    event.header_value = header_value;
    if (client->config.event_handler(&event) != ESP_OK)
    {
        return ESP_FAIL;
    }

    if (!fake_empty_body)
    {
        event.event_id = HTTP_EVENT_ON_DATA;
        event.data = body;
        event.data_len = (int)(sizeof(body) - 1u);
        if (client->config.event_handler(&event) != ESP_OK)
        {
            return ESP_FAIL;
        }
    }

    event.event_id = HTTP_EVENT_ON_FINISH;
    return client->config.event_handler(&event);
}

int esp_http_client_get_status_code(esp_http_client_handle_t client)
{
    return client == &fake_client ? (int)fake_status : -1;
}

static uint64_t test_now_ms(void* context)
{
    (void)context;
    return fake_now_ms;
}

typedef struct {
    unsigned int headers;
    unsigned int bodies;
    unsigned int status;
    char body[8];
    size_t body_size;
} test_sink_t;

static agent_error_t test_headers(void* context, unsigned int status,
                                  const agent_http_header_t* headers, size_t count)
{
    test_sink_t* sink = (test_sink_t*)context;

    if (status != fake_status || count != 1u || headers == NULL ||
        headers[0].name.size != strlen("content-type"))
    {
        return AGENT_ERROR;
    }
    ++sink->headers;
    sink->status = status;
    return AGENT_OK;
}

static agent_error_t test_body(void* context, const void* data, size_t size)
{
    test_sink_t* sink = (test_sink_t*)context;

    if (size > sizeof(sink->body) || data == NULL)
    {
        return AGENT_ERROR;
    }
    memcpy(sink->body, data, size);
    sink->body_size = size;
    ++sink->bodies;
    return AGENT_OK;
}

int main(void)
{
    static char url_buffer[128];
    static char request_header_name_buffer[32];
    static char request_header_value_buffer[64];
    static agent_http_header_t response_headers[4];
    static char response_header_buffer[128];
    static const agent_http_header_t request_headers[] = {
        {AGENT_SV_LITERAL("Accept"), AGENT_SV_LITERAL("application/json")},
        {AGENT_SV_LITERAL("X-Optional"), AGENT_SV_LITERAL("")},
    };
    agent_runtime_t runtime;
    agent_port_espidf_transport_t state;
    agent_port_espidf_transport_config_t config;
    agent_transport_t transport;
    agent_http_request_t request;
    agent_http_sink_t sink;
    test_sink_t sink_state = {0};

    if (agent_port_espidf_runtime_init(&runtime) != AGENT_OK ||
        runtime.now_ms(runtime.clock_context) != 1234u)
    {
        return 1;
    }

    config = (agent_port_espidf_transport_config_t){0};
    config.runtime.now_ms = test_now_ms;
    config.cert_pem = "test-ca-pem";
    config.request_timeout_ms = 5000u;
    config.url_buffer = url_buffer;
    config.url_buffer_size = sizeof(url_buffer);
    config.request_header_name_buffer = request_header_name_buffer;
    config.request_header_name_buffer_size = sizeof(request_header_name_buffer);
    config.request_header_value_buffer = request_header_value_buffer;
    config.request_header_value_buffer_size = sizeof(request_header_value_buffer);
    config.response_headers = response_headers;
    config.response_header_capacity = sizeof(response_headers) / sizeof(response_headers[0]);
    config.response_header_buffer = response_header_buffer;
    config.response_header_buffer_size = sizeof(response_header_buffer);
    if (agent_port_espidf_transport_init(&transport, &state, &config) != AGENT_OK)
    {
        return 2;
    }

    request = (agent_http_request_t){0};
    request.method = agent_string_view("POST", 4u);
    request.url = agent_string_view("https://example.invalid/v1/test",
                                    sizeof("https://example.invalid/v1/test") - 1u);
    request.headers = request_headers;
    request.header_count = 2u;
    request.body = "{}";
    request.body_size = 2u;
    request.max_response_bytes = 8u;
    request.max_response_header_bytes = 64u;
    sink = (agent_http_sink_t){test_headers, test_body, &sink_state};

    if (transport.ops->request(transport.context, &request, &sink) != AGENT_OK ||
        sink_state.headers != 1u || sink_state.bodies != 1u || sink_state.body_size != 2u ||
        memcmp(sink_state.body, "{}", 2u) != 0 || fake_client.method != HTTP_METHOD_POST ||
        fake_client.headers_set != 2u || fake_client.body_size != 2 ||
        !fake_client.config.disable_auto_redirect || fake_client.config.timeout_ms != 5000)
    {
        return 3;
    }

    request.max_response_bytes = 1u;
    if (transport.ops->request(transport.context, &request, &sink) != AGENT_ERROR_CAPACITY)
    {
        return 4;
    }

    request.max_response_bytes = 8u;
    request.deadline_ms = fake_now_ms;
    if (transport.ops->request(transport.context, &request, &sink) != AGENT_ERROR_TIMEOUT)
    {
        return 5;
    }

    request.deadline_ms = 0u;
    state.config.cert_pem = NULL;
    if (transport.ops->request(transport.context, &request, &sink) != AGENT_ERROR_INVALID)
    {
        return 6;
    }

    state.config.use_crt_bundle = true;
#if defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)
    if (transport.ops->request(transport.context, &request, &sink) != AGENT_OK ||
        fake_client.config.crt_bundle_attach != esp_crt_bundle_attach)
    {
        return 7;
    }
#else
    if (transport.ops->request(transport.context, &request, &sink) != AGENT_ERROR_NOT_SUPPORTED ||
        agent_port_espidf_transport_init(&transport, &state, &state.config) !=
            AGENT_ERROR_NOT_SUPPORTED)
    {
        return 7;
    }
    state.config.cert_pem = "test-ca-pem";
    state.config.use_crt_bundle = false;
#endif

    fake_status = 401u;
    fake_empty_body = true;
    {
        unsigned int previous_headers = sink_state.headers;
        unsigned int previous_bodies = sink_state.bodies;
    if (transport.ops->request(transport.context, &request, &sink) != AGENT_OK ||
        sink_state.status != 401u || sink_state.headers != previous_headers + 1u ||
            sink_state.bodies != previous_bodies)
        {
            return 9;
        }
    }

    request.url = agent_string_view("ftp://example.invalid", sizeof("ftp://example.invalid") - 1u);
    if (transport.ops->request(transport.context, &request, &sink) != AGENT_ERROR_NOT_SUPPORTED)
    {
        return 8;
    }

    return 0;
}
