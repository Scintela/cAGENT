# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie

add_library(cagent_provider_mock STATIC src/model_mock.c)
target_link_libraries(cagent_provider_mock PUBLIC cagent_core)
