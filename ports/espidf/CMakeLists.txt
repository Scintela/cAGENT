# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie

set(_agent_port_sources)
set(_agent_private_requires)

if(CONFIG_AGENT_PORT_ESPIDF_RUNTIME)
    list(APPEND _agent_port_sources runtime/src/runtime.c)
    list(APPEND _agent_private_requires esp_timer)
endif()

if(CONFIG_AGENT_PORT_ESPIDF_TRANSPORT)
    list(APPEND _agent_port_sources transport/src/transport.c)
    list(APPEND _agent_private_requires esp_http_client)
    if(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)
        list(APPEND _agent_private_requires mbedtls)
    endif()
endif()

if(CONFIG_AGENT_PORT_ESPIDF_FILE_STORE)
    if(NOT CONFIG_AGENT_FILE_STORE OR NOT CONFIG_AGENT_POSIX_FILE_STORE)
        message(FATAL_ERROR "ESP-IDF file store requires common and POSIX file-store components")
    endif()
    list(APPEND _agent_port_sources storage/src/file_store.c)
    list(APPEND _agent_private_requires vfs)
endif()

if(_agent_private_requires)
    idf_component_register(
        SRCS ${_agent_port_sources}
        INCLUDE_DIRS runtime/include transport/include storage/include
        REQUIRES cagent
        PRIV_REQUIRES ${_agent_private_requires})
else()
    idf_component_register(
        SRCS ${_agent_port_sources}
        INCLUDE_DIRS runtime/include transport/include storage/include
        REQUIRES cagent)
endif()
