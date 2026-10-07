/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 tanglinjie */
#pragma once
#include <rtthread.h>
#define WEBCLIENT_SW_VERSION_NUM 0x20300
enum WEBCLIENT_STATUS { WEBCLIENT_OK, WEBCLIENT_ERROR, WEBCLIENT_TIMEOUT, WEBCLIENT_NOMEM,
    WEBCLIENT_NOSOCKET, WEBCLIENT_NOBUFFER, WEBCLIENT_CONNECT_FAILED, WEBCLIENT_DISCONNECT, WEBCLIENT_FILE_ERROR };
struct webclient_header { char* buffer; size_t length, size; };
struct webclient_session { struct webclient_header* header; int socket, resp_status;
    char *host, *req_url; int chunk_sz, chunk_offset, content_length; size_t content_remainder;
    int (*handle_function)(char*, int); rt_bool_t is_tls; };
struct webclient_session* webclient_session_create(size_t);
int webclient_post(struct webclient_session*, const char*, const void*, size_t);
int webclient_close(struct webclient_session*);
int webclient_read(struct webclient_session*, void*, size_t);
