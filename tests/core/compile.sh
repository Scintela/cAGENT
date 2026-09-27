#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cc="${CC:-cc}"
binary="$(mktemp)"
trap 'rm -f "$binary"' EXIT

"$cc" -std=c99 -Wall -Wextra -Werror -I"$root/include" -I"$root/src" \
    "$root/tests/core/lifecycle.c" \
    "$root/src/core/arena.c" \
    "$root/src/core/agent_core.c" \
    "$root/src/core/agent_event.c" \
    "$root/src/model/model.c" \
    "$root/src/runtime/runtime.c" \
    -o "$binary"

"$binary"
printf 'PASS: Core workspace and Model wrapper lifecycle (C99, %s).\n' "$cc"
