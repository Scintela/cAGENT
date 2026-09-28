# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie

add_library(cagent_json_jsmn STATIC reader.c writer.c jsmn.c)
add_library(cagent::json_jsmn ALIAS cagent_json_jsmn)
target_include_directories(cagent_json_jsmn PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/third_party/jsmn")
target_compile_features(cagent_json_jsmn PRIVATE c_std_99)
set_target_properties(cagent_json_jsmn PROPERTIES
    C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
