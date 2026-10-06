#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie
set -euo pipefail
root="$(cd "$(dirname "$0")/../../.." && pwd)"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
for idf in OFF ON; do
  for codec in OFF ON; do
    for slots in 0 12; do
      dir="$build/$idf-$codec-$slots"
      cmake -S "$root/tests/build/tool" -B "$dir" -DTOOL_BUILD_IDF="$idf" \
        -DTOOL_WITH_CODEC="$codec" -DCONFIG_AGENT_MAX_TOOLS="$slots" \
        -DCMAKE_C_FLAGS='-Wall -Wextra -Werror -pedantic' \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON > "$build/configure.log"
      cmake --build "$dir" -j4 > "$build/build.log"
      "$dir/tool_smoke"
      if [[ "$slots" != 0 || "$codec" == ON ]]; then
        test "$(rg -c '"file": ".*/codecs/json/reader.c"' "$dir/compile_commands.json")" = 1
      elif rg -q '"file": ".*/codecs/json/' "$dir/compile_commands.json"; then
        printf 'FAIL: zero-slot Core unexpectedly includes JSON\n' >&2
        exit 1
      fi
      if [[ "$codec" == OFF ]] && rg -q '/codecs/json/writer.c' "$dir/compile_commands.json"; then
        printf 'FAIL: Tool reader unexpectedly includes writer\n' >&2
        exit 1
      fi
      printf 'PASS: Tool build/link (idf=%s codec=%s slots=%s)\n' "$idf" "$codec" "$slots"
    done
  done
done
