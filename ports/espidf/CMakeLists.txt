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
endif()

if(_agent_private_requires)
    idf_component_register(
        SRCS ${_agent_port_sources}
        INCLUDE_DIRS runtime/include transport/include
        REQUIRES cagent
        PRIV_REQUIRES ${_agent_private_requires})
else()
    idf_component_register(
        SRCS ${_agent_port_sources}
        INCLUDE_DIRS runtime/include transport/include
        REQUIRES cagent)
endif()
