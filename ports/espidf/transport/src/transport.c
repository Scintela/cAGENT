/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* ESP-IDF esp_http_client implementation of the synchronous Transport contract. */

#include <agent_espidf_transport.h>

#include <agent.h>

#include <esp_err.h>
#include <esp_http_client.h>
#if defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)
#include <esp_crt_bundle.h>
#endif

#include <limits.h>
#include <string.h>

typedef struct {
    agent_port_espidf_transport_t* transport;
    const agent_http_request_t* request;
    const agent_http_sink_t* sink;
    size_t response_header_bytes;
    size_t response_header_text_used;
    size_t response_body_bytes;
    size_t response_header_count;
    bool headers_emitted;
    agent_error_t status;
} agent_port_espidf_exchange_t;

static agent_error_t agent_port_espidf_map_error(esp_err_t error)
{
    if (error == ESP_OK)
    {
        return AGENT_OK;
    }
    if (error == ESP_ERR_TIMEOUT)
    {
        return AGENT_ERROR_TIMEOUT;
    }
    if (error == ESP_ERR_NO_MEM)
    {
        return AGENT_ERROR_NOMEM;
    }
    if (error == ESP_ERR_INVALID_ARG)
    {
        return AGENT_ERROR_INVALID;
    }
    return AGENT_ERROR_IO;
}

static bool agent_port_espidf_view_equal(agent_string_view_t value, const char* literal)
{
    size_t size = strlen(literal);
    return value.data != NULL && value.size == size && memcmp(value.data, literal, size) == 0;
}

static agent_error_t agent_port_espidf_url_scheme(agent_string_view_t url, bool* https)
{
    size_t index;

    if (url.data == NULL || https == NULL)
    {
        return AGENT_ERROR_INVALID;
    }
    if (url.size > 8u && memcmp(url.data, "https://", 8u) == 0)
    {
        *https = true;
    }
    else if (url.size > 7u && memcmp(url.data, "http://", 7u) == 0)
    {
        *https = false;
    }
    else
    {
        return AGENT_ERROR_NOT_SUPPORTED;
    }
    for (index = 0u; index < url.size; ++index)
    {
        if ((unsigned char)url.data[index] <= 0x20u || (unsigned char)url.data[index] == 0x7fu)
        {
            return AGENT_ERROR_INVALID;
        }
    }
    return AGENT_OK;
}

static bool agent_port_espidf_http_text_valid(agent_string_view_t value, bool required)
{
    size_t index;

    if (value.size == 0u)
    {
        return !required;
    }
    if (value.data == NULL)
    {
        return false;
    }

    for (index = 0u; index < value.size; ++index)
    {
        if (value.data[index] == '\0' || value.data[index] == '\r' || value.data[index] == '\n')
        {
            return false;
        }
    }
    return true;
}

static agent_error_t agent_port_espidf_copy_text(char* output, size_t output_size,
                                                 agent_string_view_t input, bool required)
{
    if (!agent_port_espidf_http_text_valid(input, required) || output == NULL)
    {
        return AGENT_ERROR_INVALID;
    }
    if (input.size >= output_size)
    {
        return AGENT_ERROR_CAPACITY;
    }

    if (input.size != 0u)
    {
        memcpy(output, input.data, input.size);
    }
    output[input.size] = '\0';
    return AGENT_OK;
}

static agent_error_t agent_port_espidf_poll(const agent_port_espidf_exchange_t* exchange)
{
    uint64_t now;

    if (exchange->request->cancel != NULL &&
        agent_cancel_token_is_set(exchange->request->cancel))
    {
        return AGENT_ERROR_CANCELLED;
    }
    if (exchange->request->deadline_ms == 0u)
    {
        return AGENT_OK;
    }

    now = exchange->transport->config.runtime.now_ms(
        exchange->transport->config.runtime.clock_context);
    return now >= exchange->request->deadline_ms ? AGENT_ERROR_TIMEOUT : AGENT_OK;
}

static agent_error_t agent_port_espidf_timeout_ms(const agent_port_espidf_exchange_t* exchange,
                                                   int* out)
{
    uint64_t timeout = exchange->transport->config.request_timeout_ms;
    agent_error_t status = agent_port_espidf_poll(exchange);

    if (status != AGENT_OK)
    {
        return status;
    }
    if (exchange->request->deadline_ms != 0u)
    {
        uint64_t now = exchange->transport->config.runtime.now_ms(
            exchange->transport->config.runtime.clock_context);
        if (now >= exchange->request->deadline_ms)
        {
            return AGENT_ERROR_TIMEOUT;
        }
        uint64_t remaining = exchange->request->deadline_ms - now;
        if (remaining < timeout)
        {
            timeout = remaining;
        }
    }
    if (timeout == 0u)
    {
        return AGENT_ERROR_TIMEOUT;
    }

    *out = timeout > (uint64_t)INT_MAX ? INT_MAX : (int)timeout;
    return AGENT_OK;
}

static agent_error_t agent_port_espidf_set_method(esp_http_client_handle_t client,
                                                   agent_string_view_t method)
{
    esp_http_client_method_t value;

    if (agent_port_espidf_view_equal(method, "GET"))
    {
        value = HTTP_METHOD_GET;
    }
    else if (agent_port_espidf_view_equal(method, "POST"))
    {
        value = HTTP_METHOD_POST;
    }
    else if (agent_port_espidf_view_equal(method, "PUT"))
    {
        value = HTTP_METHOD_PUT;
    }
    else if (agent_port_espidf_view_equal(method, "PATCH"))
    {
        value = HTTP_METHOD_PATCH;
    }
    else if (agent_port_espidf_view_equal(method, "DELETE"))
    {
        value = HTTP_METHOD_DELETE;
    }
    else if (agent_port_espidf_view_equal(method, "HEAD"))
    {
        value = HTTP_METHOD_HEAD;
    }
    else
    {
        return AGENT_ERROR_NOT_SUPPORTED;
    }

    return agent_port_espidf_map_error(esp_http_client_set_method(client, value));
}

static agent_error_t agent_port_espidf_set_headers(esp_http_client_handle_t client,
                                                    agent_port_espidf_transport_t* transport,
                                                    const agent_http_request_t* request)
{
    size_t index;
    size_t character;
    agent_error_t status;

    for (index = 0u; index < request->header_count; ++index)
    {
        if (request->headers[index].name.data == NULL || request->headers[index].name.size == 0u)
        {
            return AGENT_ERROR_INVALID;
        }
        for (character = 0u; character < request->headers[index].name.size; ++character)
        {
            unsigned char value = (unsigned char)request->headers[index].name.data[character];
            if (value <= 0x20u || value >= 0x7fu || value == ':')
            {
                return AGENT_ERROR_INVALID;
            }
        }
        status = agent_port_espidf_copy_text(transport->config.request_header_name_buffer,
                                             transport->config.request_header_name_buffer_size,
                                             request->headers[index].name, true);
        if (status != AGENT_OK)
        {
            return status;
        }
        status = agent_port_espidf_copy_text(transport->config.request_header_value_buffer,
                                             transport->config.request_header_value_buffer_size,
                                             request->headers[index].value, false);
        if (status != AGENT_OK)
        {
            return status;
        }
        status = agent_port_espidf_map_error(esp_http_client_set_header(
            client, transport->config.request_header_name_buffer,
            transport->config.request_header_value_buffer));
        if (status != AGENT_OK)
        {
            return status;
        }
    }

    return AGENT_OK;
}

static agent_error_t agent_port_espidf_store_header(agent_port_espidf_exchange_t* exchange,
                                                    const char* name, const char* value)
{
    agent_port_espidf_transport_config_t* config = &exchange->transport->config;
    size_t name_size;
    size_t value_size;
    size_t required;
    char* text;

    if (name == NULL || value == NULL || exchange->response_header_count >= config->response_header_capacity)
    {
        return AGENT_ERROR_CAPACITY;
    }

    name_size = strlen(name);
    value_size = strlen(value);
    if (exchange->response_header_bytes > exchange->request->max_response_header_bytes ||
        name_size > exchange->request->max_response_header_bytes - exchange->response_header_bytes ||
        value_size > exchange->request->max_response_header_bytes - exchange->response_header_bytes - name_size)
    {
        return AGENT_ERROR_CAPACITY;
    }
    if (name_size >= config->response_header_buffer_size ||
        config->response_header_buffer_size - name_size < 2u ||
        value_size > config->response_header_buffer_size - name_size - 2u)
    {
        return AGENT_ERROR_CAPACITY;
    }
    required = name_size + 1u + value_size + 1u;
    if (exchange->response_header_text_used > config->response_header_buffer_size ||
        required > config->response_header_buffer_size - exchange->response_header_text_used)
    {
        return AGENT_ERROR_CAPACITY;
    }

    text = config->response_header_buffer + exchange->response_header_text_used;
    memcpy(text, name, name_size);
    text[name_size] = '\0';
    config->response_headers[exchange->response_header_count].name =
        agent_string_view(text, name_size);
    text += name_size + 1u;
    memcpy(text, value, value_size);
    text[value_size] = '\0';
    config->response_headers[exchange->response_header_count].value =
        agent_string_view(text, value_size);

    exchange->response_header_count += 1u;
    exchange->response_header_text_used += required;
    exchange->response_header_bytes += name_size + value_size;
    return AGENT_OK;
}

static agent_error_t agent_port_espidf_emit_headers(agent_port_espidf_exchange_t* exchange,
                                                    esp_http_client_handle_t client)
{
    int status_code;

    if (exchange->headers_emitted)
    {
        return AGENT_OK;
    }
    status_code = esp_http_client_get_status_code(client);
    if (status_code < 0)
    {
        return AGENT_ERROR_IO;
    }

    exchange->status = exchange->sink->headers(exchange->sink->context, (unsigned int)status_code,
                                                exchange->transport->config.response_headers,
                                                exchange->response_header_count);
    if (exchange->status == AGENT_OK)
    {
        exchange->headers_emitted = true;
    }
    return exchange->status;
}

static esp_err_t agent_port_espidf_event(esp_http_client_event_t* event)
{
    agent_port_espidf_exchange_t* exchange;
    agent_error_t status;
    size_t data_size;

    if (event == NULL || event->user_data == NULL)
    {
        return ESP_FAIL;
    }
    exchange = (agent_port_espidf_exchange_t*)event->user_data;
    if (exchange->status != AGENT_OK)
    {
        return ESP_FAIL;
    }

    status = agent_port_espidf_poll(exchange);
    if (status != AGENT_OK)
    {
        exchange->status = status;
        return ESP_FAIL;
    }

    if (event->event_id == HTTP_EVENT_ON_HEADER)
    {
        exchange->status = agent_port_espidf_store_header(exchange, event->header_key,
                                                           event->header_value);
    }
    else if (event->event_id == HTTP_EVENT_ON_DATA)
    {
        exchange->status = agent_port_espidf_emit_headers(exchange, event->client);
        if (exchange->status == AGENT_OK)
        {
            if (event->data_len < 0 || (event->data_len != 0 && event->data == NULL))
            {
                exchange->status = AGENT_ERROR_IO;
            }
            else
            {
                data_size = (size_t)event->data_len;
                if (exchange->response_body_bytes > exchange->request->max_response_bytes ||
                    data_size > exchange->request->max_response_bytes - exchange->response_body_bytes)
                {
                    exchange->status = AGENT_ERROR_CAPACITY;
                }
                else if (data_size != 0u)
                {
                    exchange->status = exchange->sink->body(exchange->sink->context, event->data,
                                                             data_size);
                    if (exchange->status == AGENT_OK)
                    {
                        exchange->response_body_bytes += data_size;
                    }
                }
            }
        }
    }
    else if (event->event_id == HTTP_EVENT_ON_FINISH)
    {
        exchange->status = agent_port_espidf_emit_headers(exchange, event->client);
    }

    return exchange->status == AGENT_OK ? ESP_OK : ESP_FAIL;
}

static agent_error_t agent_port_espidf_request(void* context,
                                                const agent_http_request_t* request,
                                                const agent_http_sink_t* sink)
{
    agent_port_espidf_transport_t* transport = (agent_port_espidf_transport_t*)context;
    agent_port_espidf_exchange_t exchange;
    esp_http_client_config_t config;
    esp_http_client_handle_t client;
    agent_error_t status;
    esp_err_t error;
    int timeout_ms;
    bool https;

    if (transport == NULL || request == NULL || sink == NULL || sink->headers == NULL ||
        sink->body == NULL || request->method.data == NULL || request->method.size == 0u ||
        request->url.data == NULL || request->url.size == 0u ||
        (request->header_count != 0u && request->headers == NULL) ||
        (request->body_size != 0u && request->body == NULL) ||
        request->max_response_bytes == 0u || request->max_response_header_bytes == 0u)
    {
        return AGENT_ERROR_INVALID;
    }
    if (transport->active)
    {
        return AGENT_ERROR_BUSY;
    }
    if (request->body_size > (size_t)INT_MAX)
    {
        return AGENT_ERROR_LIMIT;
    }

    status = agent_port_espidf_url_scheme(request->url, &https);
    if (status != AGENT_OK)
    {
        return status;
    }
    if (https && transport->config.cert_pem == NULL && !transport->config.use_crt_bundle)
    {
        return AGENT_ERROR_INVALID;
    }
#if !defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)
    if (https && transport->config.use_crt_bundle)
    {
        return AGENT_ERROR_NOT_SUPPORTED;
    }
#endif

    status = agent_port_espidf_copy_text(transport->config.url_buffer,
                                         transport->config.url_buffer_size, request->url, true);
    if (status != AGENT_OK)
    {
        return status;
    }

    exchange = (agent_port_espidf_exchange_t){0};
    exchange.transport = transport;
    exchange.request = request;
    exchange.sink = sink;
    exchange.status = AGENT_OK;
    status = agent_port_espidf_timeout_ms(&exchange, &timeout_ms);
    if (status != AGENT_OK)
    {
        return status;
    }

    config = (esp_http_client_config_t){0};
    config.url = transport->config.url_buffer;
    config.cert_pem = transport->config.cert_pem;
#if defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)
    if (transport->config.use_crt_bundle)
    {
        config.crt_bundle_attach = esp_crt_bundle_attach;
    }
#endif
    config.timeout_ms = timeout_ms;
    config.disable_auto_redirect = true;
    config.event_handler = agent_port_espidf_event;
    config.user_data = &exchange;
    transport->active = true;
    client = esp_http_client_init(&config);
    if (client == NULL)
    {
        transport->active = false;
        return AGENT_ERROR_NOMEM;
    }

    status = agent_port_espidf_set_method(client, request->method);
    if (status == AGENT_OK)
    {
        status = agent_port_espidf_set_headers(client, transport, request);
    }
    if (status == AGENT_OK && request->body_size != 0u)
    {
        status = agent_port_espidf_map_error(esp_http_client_set_post_field(
            client, (const char*)request->body, (int)request->body_size));
    }
    if (status == AGENT_OK)
    {
        error = esp_http_client_perform(client);
        status = exchange.status != AGENT_OK ? exchange.status : agent_port_espidf_map_error(error);
        if (status == AGENT_OK && !exchange.headers_emitted)
        {
            status = AGENT_ERROR_IO;
        }
        if (status == AGENT_OK)
        {
            status = agent_port_espidf_poll(&exchange);
        }
    }

    esp_http_client_cleanup(client);
    transport->active = false;
    return status;
}

static const agent_transport_ops_t agent_port_espidf_transport_ops = {
    agent_port_espidf_request,
};

agent_error_t agent_port_espidf_transport_init(
    agent_transport_t* out, agent_port_espidf_transport_t* state,
    const agent_port_espidf_transport_config_t* config)
{
    if (out == NULL || state == NULL || config == NULL || config->runtime.now_ms == NULL ||
        config->request_timeout_ms == 0u || config->url_buffer == NULL ||
        config->url_buffer_size == 0u || config->request_header_name_buffer == NULL ||
        config->request_header_name_buffer_size == 0u || config->request_header_value_buffer == NULL ||
        config->request_header_value_buffer_size == 0u || config->response_headers == NULL ||
        config->response_header_capacity == 0u || config->response_header_buffer == NULL ||
        config->response_header_buffer_size == 0u)
    {
        return AGENT_ERROR_INVALID;
    }
    if ((config->runtime.allocator.alloc == NULL) != (config->runtime.allocator.free == NULL) ||
        (config->runtime.cancel_sync.enter == NULL) != (config->runtime.cancel_sync.leave == NULL))
    {
        return AGENT_ERROR_INVALID;
    }
    if ((config->cert_pem != NULL && config->cert_pem[0] == '\0') ||
        (config->cert_pem != NULL && config->use_crt_bundle))
    {
        return AGENT_ERROR_INVALID;
    }
#if !defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)
    if (config->use_crt_bundle)
    {
        return AGENT_ERROR_NOT_SUPPORTED;
    }
#endif

    state->config = *config;
    state->active = false;
    out->ops = &agent_port_espidf_transport_ops;
    out->context = state;
    return AGENT_OK;
}
