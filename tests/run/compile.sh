#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
for tools in 0 12; do
  options=()
  if (( tools )); then options+=(-DAGENT_BUILD_OPENAI_PROVIDER=ON -DAGENT_BUILD_JSON_CODEC=ON); fi
  cmake -S "$root" -B "$build/$tools" -DCMAKE_C_COMPILER="${CC:-cc}" \
    -DCMAKE_C_FLAGS="-Wall -Wextra -Werror -pedantic ${CFLAGS:-}" \
    -DCONFIG_AGENT_MAX_TOOLS="$tools" -DAGENT_BUILD_SESSION_RAM=ON "${options[@]}" > /dev/null
  cmake --build "$build/$tools" -j2 > /dev/null
  libs=("$build/$tools/libcagent_core.a"
        "$build/$tools/providers/storage/ram/libcagent_session_ram.a")
  if (( tools )); then libs+=("$build/$tools/codecs/json/libcagent_json_reader.a"); fi
  "${CC:-cc}" -std=c99 -Wall -Wextra -Werror -pedantic -pthread ${CFLAGS:-} \
    -I"$root/include" -I"$root/src" -I"$root/providers/storage/ram/include" \
    -I"$build/$tools/generated/include" '-DAGENT_BUILD_CONFIG_HEADER="agent_build_config.h"' \
    "$root/tests/run/contract.c" "${libs[@]}" -o "$build/run-$tools"
  "$build/run-$tools"
  if (( tools )); then
    "${CC:-cc}" -std=c99 -Wall -Wextra -Werror -pedantic ${CFLAGS:-} \
      -I"$root/include" -I"$root/providers/model/openai/include" \
      -I"$build/$tools/generated/include" '-DAGENT_BUILD_CONFIG_HEADER="agent_build_config.h"' \
      "$root/tests/run/openai.c" "$build/$tools/providers/model/openai/libcagent_provider_openai.a" \
      "${libs[@]}" "$build/$tools/codecs/json/libcagent_json_jsmn.a" \
      "$build/$tools/codecs/json/libcagent_json_reader.a" -o "$build/openai-run"
    "$build/openai-run"
  fi
done
printf 'PASS: synchronous Run, paired tools, storage faults, deadlines, output ownership and zero-tool build\n'
