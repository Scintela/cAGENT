#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
for slots in 0 1 8; do
  "${CC:-cc}" -std=c99 -Wall -Wextra -Werror -pedantic ${CFLAGS:-} \
    -I"$root/include" -I"$root/src" -DAGENT_MAX_TOOLS=0 -DAGENT_MAX_SKILLS="$slots" \
    "$root/tests/skill/contract.c" "$root/src/skill/skill_registry.c" "$root/src/context/context_registry.c" \
    "$root/src/tool/tool_registry.c" "$root/src/tool/tool_schema.c" \
    "$root/src/core/lifecycle.c" "$root/src/core/arena.c" \
    "$root/src/model/model.c" "$root/src/runtime/runtime.c" -o "$build/skill-$slots"
  "$build/skill-$slots"
done
printf 'PASS: Skill zero/single/default profiles, UTF-8, priority and required reservation\n'
