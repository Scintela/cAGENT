# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie

if(CONFIG_AGENT_PORT_OPENVELA_TRANSPORT)
    if(NOT CONFIG_NETUTILS_WEBCLIENT)
        message(FATAL_ERROR "OpenVela Transport requires CONFIG_NETUTILS_WEBCLIENT")
    endif()
    if(NOT COMMAND nuttx_add_library)
        message(FATAL_ERROR "OpenVela Transport must be built in a NuttX application build")
    endif()

    nuttx_add_library(cagent_openvela_transport STATIC)
    target_sources(cagent_openvela_transport PRIVATE transport/src/transport.c)
    target_include_directories(cagent_openvela_transport
        PUBLIC
            ${CMAKE_CURRENT_LIST_DIR}/include
            ${CMAKE_CURRENT_LIST_DIR}/../../include)
    target_compile_features(cagent_openvela_transport PUBLIC c_std_99)
endif()
