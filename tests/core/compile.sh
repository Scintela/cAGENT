#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cc="${CC:-cc}"
binary="$(mktemp)"
trap 'rm -f "$binary"' EXIT

"$cc" -std=c99 -Wall -Wextra -Werror ${CFLAGS:-} -I"$root/include" -I"$root/src" \
    -I"$root/codecs/json" -I"$root/codecs/json/vendor/jsmn" \
    "$root/src/tool/tool_registry.c" "$root/src/tool/tool_schema.c" "$root/codecs/json/reader.c" \
    "$root/tests/core/lifecycle.c" \
    "$root/src/core/arena.c" \
    "$root/src/skill/skill_registry.c" \
    "$root/src/core/lifecycle.c" \
    "$root/src/core/event.c" \
    "$root/src/core/error.c" \
    "$root/src/model/model.c" \
    "$root/src/run/cancel.c" \
    "$root/src/runtime/runtime.c" \
    -o "$binary"

"$binary"
printf 'PASS: Core workspace and Model wrapper lifecycle (C99, %s).\n' "$cc"
