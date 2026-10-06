#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie
set -euo pipefail
root="$(cd "$(dirname "$0")/../../.." && pwd)"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
for idf in OFF ON; do
  for slots in 0 8; do
    dir="$build/$idf-$slots"
    cmake -S "$root/tests/build/context" -B "$dir" -DCONTEXT_BUILD_IDF="$idf" \
      -DCONFIG_AGENT_MAX_TOOLS=0 -DCONFIG_AGENT_MAX_CONTEXTS="$slots" \
      -DCONFIG_AGENT_MAX_SKILLS="$slots" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      -DCMAKE_C_FLAGS='-Wall -Wextra -Werror -pedantic' > "$build/configure.log"
    cmake --build "$dir" -j4 > "$build/build.log"
    "$dir/context_contract"
    if rg -q '/codecs/json/' "$dir/compile_commands.json"; then
      printf 'FAIL: Skill/Context unexpectedly depend on JSON\n' >&2
      exit 1
    fi
    printf 'PASS: Skill/Context build/link (idf=%s slots=%s)\n' "$idf" "$slots"
  done
done
