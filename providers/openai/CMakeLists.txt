# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie

add_library(cagent_provider_openai STATIC src/model_openai.c)
target_link_libraries(cagent_provider_openai PUBLIC cagent_core)
